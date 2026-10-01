-- Public-deploy lockdown for the legacy `public` schema plus auth hardening.
--
-- Context: the dashboard app now reads only `device_data` (see src/data.ts and
-- src/supabase.ts, both pinned to `db: { schema: "device_data" }`). The seven
-- `USING (true)` policies created by 20260927142738, 20260927145948 and
-- 20260927160200 were marked "temporary development-only" in their own headers
-- and must not survive on a public URL, where the publishable key is embedded
-- in the JS bundle and anyone can call PostgREST directly.
--
-- This migration:
--   1. drops the seven dev read policies,
--   2. revokes client grants on every `public` table / sequence / function,
--   3. removes the legacy tables from the `supabase_realtime` publication,
--   4. pins `public.set_updated_at()` search_path and revokes direct execute,
--   5. hardens `device_data.authenticate`: single errcode/message (no
--      empty-vs-wrong oracle), 12h session TTL, global expired-session prune,
--   6. adds `device_data.cleanup_expired_sessions()` for scheduled cleanup.
--
-- Brute force is rate-limited outside the database: a failed `authenticate`
-- call raises, which rolls back its own transaction, so a counter written
-- inside the function could never accumulate. The throttle therefore lives
-- in the client backoff (src/auth.ts) plus a deployment-level edge limit /
-- Turnstile on the login path (see pre-public checklist in
-- docs/llm-summary/security-report.md), not in a DB table.
--
-- The five `public` tables that already have RLS enabled with zero policies
-- (device_status_events, node_status_events, chamber_status_events,
-- experiment_devices, experiment_nodes) stay deny-by-default; nothing grants
-- them back here.

-- ---------------------------------------------------------------------------
-- 1. Drop the dev `USING (true)` policies.
-- ---------------------------------------------------------------------------

drop policy if exists "dev anon authenticated can read devices"
  on public.devices;
drop policy if exists "dev anon authenticated can read nodes"
  on public.nodes;
drop policy if exists "dev anon authenticated can read chambers"
  on public.chambers;
drop policy if exists "dev anon authenticated can read experiment chambers"
  on public.experiment_chambers;
drop policy if exists "dev anon authenticated can read telemetry readings"
  on public.telemetry_readings;
drop policy if exists "dev anon authenticated can read experiments"
  on public.experiments;
drop policy if exists "dev anon authenticated can read experiment sessions"
  on public.experiment_sessions;

-- ---------------------------------------------------------------------------
-- 2. Revoke every client grant in `public`. The app uses `device_data` only,
--    so nothing is granted back. RLS stays enabled, hence deny-by-default.
-- ---------------------------------------------------------------------------

revoke all on all tables in schema public from anon, authenticated;
revoke all on all sequences in schema public from anon, authenticated;
revoke all on all functions in schema public from anon, authenticated, public;

-- Re-assert RLS so a future `create table public.*` without policies is still
-- deny-by-default even if a later migration forgets `enable row level security`.
-- `rls_auto_enable()` below is the helper for that; it must not be callable by
-- clients (revoked above), only by the table owner / migrations.
do $$
declare
  target_table text;
begin
  for target_table in
    select c.relname
    from pg_class c
    join pg_namespace n on n.oid = c.relnamespace
    where n.nspname = 'public'
      and c.relkind = 'r'
      and c.relrowsecurity = false
  loop
    execute format('alter table public.%I enable row level security;', target_table);
  end loop;
end
$$;

-- ---------------------------------------------------------------------------
-- 3. Remove legacy tables from Realtime. The app polls `device_data` (custom
--    `x-device-session` header cannot ride a websocket) and subscribes to
--    nothing, so no table needs to be in `supabase_realtime`.
-- ---------------------------------------------------------------------------

do $$
declare
  target_table text;
begin
  for target_table in
    select table_name
    from (values
      ('telemetry_readings'),
      ('devices'),
      ('nodes'),
      ('chambers'),
      ('experiments')
    ) as requested(table_name)
  loop
    if exists (
      select 1
      from pg_publication_tables
      where pubname = 'supabase_realtime'
        and schemaname = 'public'
        and tablename = target_table
    ) then
      execute format(
        'alter publication supabase_realtime drop table public.%I',
        target_table
      );
    end if;
  end loop;
end
$$;

-- ---------------------------------------------------------------------------
-- 4. Pin `public.set_updated_at()` and keep trigger helpers out of the API.
--    (Grants were revoked in §2; the pin fixes the
--    `function_search_path_mutable` advisor flag.)
-- ---------------------------------------------------------------------------

alter function public.set_updated_at() set search_path = pg_catalog;

-- If a `rls_auto_enable()` helper exists in this project (Supabase advisor
-- flag `anon_security_definer_function_executable`), pin it and keep it
-- owner-only. The `if exists` guards keep this idempotent across projects
-- where the helper was never created.
do $$
begin
  if exists (
    select 1 from pg_proc p
    join pg_namespace n on n.oid = p.pronamespace
    where n.nspname = 'public' and p.proname = 'rls_auto_enable'
  ) then
    execute 'alter function public.rls_auto_enable() set search_path = pg_catalog';
    execute 'revoke all on function public.rls_auto_enable() from anon, authenticated, public';
  end if;
end
$$;

-- ---------------------------------------------------------------------------
-- 5. Harden `device_data.authenticate`.
--
--    - Single errcode/message for null, empty, and wrong tokens, so the
--      function is not an oracle distinguishing "no token sent" from
--      "wrong token" (previously 22023 vs 28000).
--    - Session TTL 12 hours (was 24 hours) to shrink the window of a leaked
--      session token taken from browser localStorage.
--    - Global expired-session prune (was per-device), so devices that never
--      log in again cannot accumulate dead rows.
--    - Tokens themselves remain high-entropy server-generated values
--      (≥128 bits, e.g. `openssl rand -hex 32`); SHA-512 comparison is an
--      indexed equality test on the digest. Brute-force protection lives
--      outside this transaction (client backoff in src/auth.ts plus an edge
--      rate limit / Turnstile on the login path), because a counter written
--      before the raise would roll back with the failed call.
-- ---------------------------------------------------------------------------

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
    raise exception 'Invalid device token.'
      using errcode = '28000';
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

  -- Drop every dead session, not just this device's, so devices that never
  -- log in again cannot accumulate expired rows indefinitely.
  delete from device_data.sessions s
  where s.expires_at <= now();

  -- 256 bits of randomness rendered as the 64 hex characters the sessions
  -- check constraint requires. gen_random_uuid() is used as a text seed
  -- rather than cast to bytea, because PostgreSQL provides no uuid -> bytea
  -- cast.
  new_session_token := encode(
    sha256(
      convert_to(gen_random_uuid()::text || gen_random_uuid()::text, 'UTF8')
    ),
    'hex'
  );
  new_expires_at := now() + interval '12 hours';

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
  'Exchanges a plaintext device token for a session token scoped to that device. Single error for all bad tokens; 12h sessions.';

revoke all on function device_data.authenticate(text) from public;
grant execute on function device_data.authenticate(text) to anon, authenticated;

-- ---------------------------------------------------------------------------
-- 6. Scheduled cleanup entry point. Call from pg_cron (e.g.
--    `select cron.schedule('cleanup-expired-sessions', '0 * * * *',
--    'select device_data.cleanup_expired_sessions()')`) or an edge worker.
--    Owner-only: no client grants, so a leaked publishable key cannot abuse
--    it and `session_device_id()` remains the only anon-reachable helper,
--    as required by the RLS policies.
-- ---------------------------------------------------------------------------

create or replace function device_data.cleanup_expired_sessions()
returns integer
language plpgsql
security definer
set search_path = pg_catalog, device_data
as $$
declare
  removed_count integer;
begin
  delete from device_data.sessions s
  where s.expires_at <= now();
  get diagnostics removed_count = row_count;

  return removed_count;
end;
$$;

comment on function device_data.cleanup_expired_sessions() is
  'Deletes expired device sessions. Intended for pg_cron / edge scheduler, not for browser clients.';

revoke all on function device_data.cleanup_expired_sessions() from public, anon, authenticated;

notify pgrst, 'reload schema';
