-- Token-based access control for the device_data schema.
--
-- The dashboard is an anonymous browser client, so there is no Supabase Auth
-- user to hang row level security off. Instead a device token is exchanged for
-- a random session token; the browser sends that session token in the
-- x-device-session request header on every read, and the policies below resolve
-- it back to a single device_id.
--
-- Rows in device_data.devices and device_data.readings are therefore only ever
-- visible to a caller holding a live session for that device.

create table device_data.sessions (
  session_token text primary key
    check (session_token ~ '^[0-9a-f]{64}$'),
  device_id integer not null
    references device_data.devices (device_id) on delete cascade,
  created_at timestamptz not null default now(),
  expires_at timestamptz not null
    check (expires_at > created_at)
);

create index sessions_device_id_idx
  on device_data.sessions (device_id);

create index sessions_expires_at_idx
  on device_data.sessions (expires_at);

alter table device_data.sessions enable row level security;

-- Read x-device-session out of the PostgREST request headers. PostgREST
-- exposes the whole header bag as a JSON object; when the client sends none
-- (direct psql access, for example) current_setting returns an empty string,
-- which is not valid JSON, so fall back to an empty object instead of raising.
--
-- Defined before session_device_id because Postgres validates SQL function
-- bodies at creation time, so a caller cannot reference a function that has
-- not been created yet.
create or replace function device_data.request_session_token()
returns text
language sql
stable
set search_path = pg_catalog
as $$
  select nullif(
    (
      case
        when coalesce(current_setting('request.headers', true), '') ~ '^\s*\{'
          then current_setting('request.headers')::jsonb
        else '{}'::jsonb
      end
    ) ->> 'x-device-session',
    ''
  );
$$;

comment on function device_data.request_session_token() is
  'Reads the x-device-session header sent by the dashboard. Returns null when absent.';

-- Resolve the caller's session token to a device_id. Marked SECURITY DEFINER
-- so it can read device_data.sessions, which has no client-visible policies.
--
-- SECURITY DEFINER functions must pin their search_path, otherwise a caller
-- could shadow pg_catalog.sha512 or pg_catalog.gen_random_uuid with their own
-- objects in a writable schema.
create or replace function device_data.session_device_id()
returns integer
language sql
stable
security definer
set search_path = pg_catalog, device_data
as $$
  select s.device_id
  from device_data.sessions s
  where s.session_token = device_data.request_session_token()
    and s.expires_at > now()
  limit 1;
$$;

comment on function device_data.session_device_id() is
  'Device id owning the caller session token, or null when no live session exists.';

-- Exchange a device token for a session. Runs as the table owner because it
-- must read token_hash, which the anon role is never granted.
--
-- The comparison against token_hash is an indexed equality test on a
-- high-entropy SHA-512 digest, so it does not leak the stored hash through
-- timing. A wrong token and an unknown token raise the same error so the
-- function is not an oracle for which devices exist.
create or replace function device_data.authenticate(p_token text)
returns table (
  session_token text,
  device_id integer,
  bottle_count integer,
  owner_name text,
  owner_email text,
  expires_at timestamptz
)
language plpgsql
security definer
set search_path = pg_catalog, device_data
as $$
declare
  requested_hash text;
  matched_device device_data.devices%rowtype;
  new_session_token text;
  new_expires_at timestamptz;
begin
  if p_token is null or p_token = '' then
    raise exception 'A device token is required.'
      using errcode = '22023';
  end if;

  requested_hash := encode(sha512(convert_to(p_token, 'UTF8')), 'hex');

  select *
  into matched_device
  from device_data.devices d
  where d.token_hash = requested_hash;

  if not found then
    raise exception 'Invalid device token.'
      using errcode = '28000';
  end if;

  -- Drop this device's dead sessions on login rather than relying purely on
  -- expiry checks, so the table cannot accumulate rows indefinitely.
  delete from device_data.sessions s
  where s.device_id = matched_device.device_id
    and s.expires_at <= now();

  -- 256 bits of randomness rendered as the 64 hex characters the sessions
  -- check constraint requires. gen_random_uuid() is used as a text seed rather
  -- than cast to bytea, because PostgreSQL provides no uuid -> bytea cast.
  new_session_token := encode(
    sha256(
      convert_to(gen_random_uuid()::text || gen_random_uuid()::text, 'UTF8')
    ),
    'hex'
  );
  new_expires_at := now() + interval '24 hours';

  insert into device_data.sessions (session_token, device_id, expires_at)
  values (new_session_token, matched_device.device_id, new_expires_at);

  return query
  select
    new_session_token,
    matched_device.device_id,
    matched_device.bottle_count,
    matched_device.owner_name,
    matched_device.owner_email,
    new_expires_at;
end;
$$;

comment on function device_data.authenticate(text) is
  'Exchanges a plaintext device token for a session token scoped to that device.';

-- End the caller's own session. Uses the same SECURITY DEFINER read path as
-- session_device_id so it works without exposing device_data.sessions.
create or replace function device_data.revoke_session()
returns void
language plpgsql
security definer
set search_path = pg_catalog, device_data
as $$
declare
  caller_device_id integer;
  caller_token text;
begin
  caller_token := device_data.request_session_token();
  caller_device_id := device_data.session_device_id();

  if caller_token is not null and caller_device_id is not null then
    delete from device_data.sessions s
    where s.session_token = caller_token
      and s.device_id = caller_device_id;
  end if;
end;
$$;

comment on function device_data.revoke_session() is
  'Deletes the caller''s session so the token stops working for that browser.';

alter table device_data.devices enable row level security;
alter table device_data.readings enable row level security;

create policy "session can read own device"
on device_data.devices
for select
to anon, authenticated
using (device_id = device_data.session_device_id());

create policy "session can read own device readings"
on device_data.readings
for select
to anon, authenticated
using (device_id = device_data.session_device_id());

-- Column level grants rather than a table grant: token_hash is a secret and
-- must stay unreachable through PostgREST, which will refuse any select that
-- names a column the role has not been granted.
grant usage on schema device_data to anon, authenticated;

revoke all on table device_data.devices from anon, authenticated;
grant select (device_id, bottle_count, owner_name, owner_email)
  on table device_data.devices to anon, authenticated;

revoke all on table device_data.readings from anon, authenticated;
grant select on table device_data.readings to anon, authenticated;

-- Sessions are reachable only through the SECURITY DEFINER functions above.
revoke all on table device_data.sessions from anon, authenticated;

revoke all on function device_data.authenticate(text) from public;
grant execute on function device_data.authenticate(text) to anon, authenticated;

revoke all on function device_data.revoke_session() from public;
grant execute on function device_data.revoke_session() to anon, authenticated;

revoke all on function device_data.session_device_id() from public;
grant execute on function device_data.session_device_id() to anon, authenticated;

revoke all on function device_data.request_session_token() from public;

notify pgrst, 'reload schema';
