# Dashboard public-deploy security report

**Date:** 2026-10-01
**Scope:** `dashboard/` as a public Netlify static site + linked Supabase project `rusim-dashboard`
**Method:** read-only inspection via Supabase CLI (`bunx supabase migration list --linked`, `db advisors --linked`, `db query --linked`), plus local code/migration review. No secrets read, no rows with PII/tokens selected (counts only).

## Live state (verified against remote)

- Project is `ACTIVE_HEALTHY`, region `ap-northeast-1`, Postgres 17. All 15 local migrations are applied remotely (`migration list --linked`: local == remote, no drift).
- `device_data`: 1 device, token provisioned (`devices_with_token = 1`), 6 unexpired sessions, 192 readings.
- `db advisors --linked`: no `ERROR`s. Only `WARN`s (listed below); most are intentional-by-design linter flags.

## Already fixed / verified good (keep these)

1. **Session-scoped RLS on `device_data`.** Live `pg_policies` shows exactly two policies, both `device_id = device_data.session_device_id()` for `anon,authenticated`:
   `device_data.devices "session can read own device"`, `device_data.readings "session can read own device readings"`. Source: `dashboard/supabase/migrations/20260930162000_add_device_data_token_auth.sql:188-198`.
2. **`token_hash` unreachable via API.** Live column grants on `device_data.devices` cover only `device_id, bottle_count, owner_name, owner_email` for `anon/authenticated`. `device_data.sessions` has no table grants to client roles. Verified via `information_schema.role_column_grants` / `role_table_grants`.
3. **RLS enabled everywhere.** `relrowsecurity = true` on all `device_data` + `public` tables (15/15 checked).
4. **Function hardening.** `authenticate`, `revoke_session`, `session_device_id` are `SECURITY DEFINER` with pinned `search_path = pg_catalog, device_data`; `request_session_token` has `search_path = pg_catalog` and no public `EXECUTE` grant. Verified via `pg_proc`. Authenticate hashes client-side token with SHA-512 and returns generic "Invalid device token" (no device-existence oracle beyond distinct `errcode`s — see §3).
5. **No plaintext credentials in git.** No `.env` tracked (`git ls-files` shows none); `.env` ignored (`dashboard/.gitignore:19`). Dev token hash applied out-of-band via `bun run dev-db` (`dashboard/scripts/dev-db.ts`, `dashboard/supabase/migrations/20260930162100_add_development_device_token.sql`). Time-series key is `(device_id, bottle_id, timestamp)` with nullable sensors (`20260930170000_make_readings_a_time_series.sql`), so no invented data.
6. **Realtime correctly avoided for `device_data`.** Custom `x-device-session` header cannot be sent over websockets, so app polls (`src/data.ts:60`, `src/App.tsx:126-130`). `device_data` is not in the `supabase_realtime` publication.

## Findings (ordered by severity)

### 1. Blocker — legacy `public` schema is public-read (live, confirmed)

Live `pg_policies` on the remote still contains 7 `USING (true)` policies for `anon,authenticated`:

- `public.devices, nodes, chambers, experiment_chambers, telemetry_readings, experiments, experiment_sessions` — all `"dev anon authenticated can read *"`.

Sources: `20260927142738_allow_dashboard_read_access.sql`, `20260927145948_allow_dashboard_read_experiments.sql`, `20260927160200_allow_dashboard_read_experiment_sessions.sql`. Each file self-marks as "temporary development-only … replace before production".

Impact: the Netlify bundle embeds `VITE_SUPABASE_URL` + publishable key (`src/supabase.ts:4-5`), so anyone can query PostgREST directly. Current app code only reads `device_data` (`src/data.ts:136-163`), but PostgREST exposes `public` too (`supabase/config.toml:13`). The old telemetry/equipment tables remain readable without any token. Realtime publication also includes `public.telemetry_readings, devices, nodes, chambers, experiments` (`20260927152135_enable_realtime_monitoring.sql`), compounding the leak.

Fix: before any public URL, drop the dev policies/grants or drop the dead schema. Minimal migration sketch:

```sql
drop policy if exists "dev anon authenticated can read devices" on public.devices;
-- repeat for nodes, chambers, experiment_chambers, telemetry_readings, experiments, experiment_sessions
revoke all on all tables in schema public from anon, authenticated;
-- or: drop schema public legacy tables entirely if rollback no longer needed
```

Also remove `public` from API exposed schemas if nothing still needs it, and remove those tables from the `supabase_realtime` publication. The 5 remaining `public` tables with RLS on but zero policies (`device_status_events, node_status_events, chamber_status_events, experiment_devices, experiment_nodes` — confirmed live: policy query returns `[]`) are already deny-by-default; leave them denied.

### 2. High — `authenticate()` has no brute-force protection

`device_data.authenticate(p_token)` is executable by `anon` by design (flagged as `WARN` by `db advisors`: `anon_security_definer_function_executable`). There is no rate limit, lockout, or CAPTCHA on the RPC — Supabase Auth rate limits do not apply to custom RPCs. SHA-512 is fast and unsalted; security rests entirely on token entropy.

Fix: issue only high-entropy random tokens (≥128 bits, e.g. `openssl rand -hex 32`), never human-memorable passwords; add edge throttling (failed-attempt counter + Turnstile) before public launch; consider shortening session TTL from 24h for prod. Note minor oracle: empty token raises `22023`, wrong token raises `28000` — normalize to one code/message.

### 3. High — Netlify bundle + env handling

- `VITE_*` vars bake into `dist/assets/*.js` at build. Only set `VITE_SUPABASE_URL` / `VITE_SUPABASE_PUBLISHABLE_KEY` in Netlify. Never set `DATABASE_URL` (direct Postgres write access used by `scripts/dev-db.ts`) or `DEV_0_TOKEN_HASH` on the site.
- `netlify.toml` has no security headers. Add `Content-Security-Policy`, `X-Frame-Options: DENY`, `HSTS`, `Referrer-Policy`, `X-Content-Type-Options`. Session tokens live in `localStorage` (`src/auth.ts:5,31-41`), so any XSS = session theft; CSP + no inline secrets matter.
- Working-tree note: uncommitted annotations `# Filled.` / `# Unfilled.` in `dashboard/.env.example` leak which local secrets exist. Revert before committing. `dist/` is correctly ignored, do not publish sourcemaps with secrets.

### 4. Medium — session hygiene and PII

- Live DB holds 6 unexpired sessions for 1 device. `authenticate` prunes only expired rows for the logging-in device; devices that never log in again accumulate expired rows. Add a scheduled cleanup (`delete from device_data.sessions where expires_at <= now()` via pg_cron/edge).
- Authenticated reads return `owner_name/owner_email`, rendered in UI (`src/App.tsx:193`). Any token holder sees PII. Confirm this is required; otherwise drop those columns from the grant/RPC return.
- `session_device_id()` is granted to `anon/authenticated` (flagged by advisors). Required for RLS policies to execute, so keep — but do not call it directly from the client; it acts as a session-validity oracle.
- `public.set_updated_at` has mutable `search_path` (`db advisors WARN function_search_path_mutable`); `public.rls_auto_enable()` is an anon-executable `SECURITY DEFINER` function. Neither is used by the app — pin/remove before public.

### 5. Low — functional / ops

- `fetchReadings` requests `.limit(5000)` (`src/data.ts:141`) but local `config.toml:18` sets `max_rows = 1000` (remote PostgREST cap may similarly truncate). Page or raise the cap deliberately.
- `DATABASE_URL` in local config allows `0.0.0.0/0`; ensure prod uses a least-privilege role and `dev-db.ts` never runs against prod (it `DELETE`s per experiment).
- One token per device, no rotation UI; rotation is manual SQL/`dev-db --only token`. Document rotation runbook; use a separate prod Supabase project and never reuse the device-0 dev token.

## Pre-public checklist

- [ ] Remove/replace the 7 `USING (true)` public policies; verify with `pg_policies` query that only `device_data` session policies remain.
- [ ] Unexpose `public` from PostgREST + Realtime if unused.
- [ ] Rotate prod device tokens (random ≥128-bit); confirm `devices_with_token` only for prod devices.
- [ ] Add Netlify security headers; set only `VITE_*` env vars.
- [ ] Add brute-force throttling/CAPTCHA on login; normalize auth errcodes.
- [ ] Add expired-session cleanup job; decide on owner PII retention.
- [ ] Fix `set_updated_at` search_path; restrict `rls_auto_enable`.
- [ ] Re-run `db advisors --linked` and the policy/grant queries below; expect zero public-schema `USING (true)` rows.

## Verification queries used (read-only)

```sql
-- policies
select schemaname, tablename, policyname, roles, cmd, qual
from pg_policies where schemaname in ('public','device_data')
order by schemaname, tablename, policyname;
-- rls status
select n.nspname, c.relname, c.relrowsecurity
from pg_class c join pg_namespace n on n.oid = c.relnamespace
where n.nspname in ('public','device_data') and c.relkind = 'r';
-- device_data grants (token_hash must be absent)
select grantee, privilege_type, column_name
from information_schema.role_column_grants
where table_schema='device_data' and table_name='devices';
-- function hardening
select n.nspname, p.proname, p.prosecdef, p.proconfig from pg_proc p
join pg_namespace n on n.oid = p.pronamespace
where p.proname in ('authenticate','revoke_session','session_device_id','request_session_token');
-- counts only, no PII
select (select count(*) from device_data.devices) as devices,
  (select count(*) from device_data.sessions where expires_at > now()) as live,
  (select count(*) from device_data.devices where token_hash is not null) as with_token,
  (select count(*) from device_data.readings) as readings;
```

---
Content generated by `meta/muse-spark-1.3-contributor` using `opencode`.
