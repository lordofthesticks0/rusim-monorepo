-- Corrects session token generation in device_data.authenticate.
--
-- The original implementation seeded randomness with gen_random_uuid()::bytea.
-- PostgreSQL has no uuid -> bytea cast, so every call to authenticate failed
-- with SQLSTATE 42846 (cannot cast type uuid to bytea) and no session could ever
-- be issued. This replaces the function with an equivalent that hashes two
-- UUIDs as text, still producing 64 hex characters to satisfy the sessions
-- check constraint.
--
-- The function body is otherwise identical to the version in
-- 20260930162000_add_device_data_token_auth.sql.

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