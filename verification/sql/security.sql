-- Access-control verification (report checklist §5 "Security and access").
-- Run before and after any change; keep both outputs as evidence.
--   bun verification/db-check.ts --suite security
-- Queries adapted from docs/plans-and-audits/security-report.md.

-- name: policies
-- doc: Every RLS policy in public and device_data. Expected in device_data: only the session-scoped read policies.
select schemaname, tablename, policyname, roles::text, cmd, qual
from pg_policies where schemaname in ('public', 'device_data')
order by schemaname, tablename, policyname;

-- name: rls_enabled
-- doc: RLS state of every table in public and device_data (expected: true everywhere).
select n.nspname as schema, c.relname as table_name, c.relrowsecurity as rls_enabled
from pg_class c join pg_namespace n on n.oid = c.relnamespace
where n.nspname in ('public', 'device_data') and c.relkind = 'r'
order by 1, 2;

-- name: devices_column_grants
-- doc: Column grants on device_data.devices to client roles. token_hash must not appear.
select grantee, privilege_type, column_name
from information_schema.role_column_grants
where table_schema = 'device_data' and table_name = 'devices' and grantee in ('anon', 'authenticated')
order by grantee, column_name;

-- name: table_grants
-- doc: Table privileges of anon/authenticated in device_data. Expected: SELECT on readings/experiments/device_config; nothing on sessions; no INSERT on readings.
select table_name, grantee, string_agg(privilege_type, ',' order by privilege_type) as privileges
from information_schema.role_table_grants
where table_schema = 'device_data' and grantee in ('anon', 'authenticated')
group by table_name, grantee order by table_name, grantee;

-- name: functions
-- doc: device_data routines: SECURITY DEFINER flag and pinned search_path.
select p.proname, p.prosecdef as security_definer, p.proconfig::text as config,
       has_function_privilege('anon', p.oid, 'execute') as anon_execute
from pg_proc p join pg_namespace n on n.oid = p.pronamespace
where n.nspname = 'device_data' order by p.proname;

-- name: public_client_grants
-- doc: Remaining client grants on the legacy public schema (pre-public checklist: expected none).
select table_name, grantee, string_agg(privilege_type, ',') as privileges
from information_schema.role_table_grants
where table_schema = 'public' and grantee in ('anon', 'authenticated')
group by table_name, grantee order by table_name;

-- name: sessions_state
-- doc: Session rows by state (no token values). Shows expiry and revocation working.
select device_id, count(*) as sessions,
       count(*) filter (where expires_at > now()) as live,
       min(expires_at) as earliest_expiry, max(expires_at) as latest_expiry
from device_data.sessions group by device_id order by device_id;
