# Database schema catalog — LLM Documentation

**Project:** `mdjpqjkduyexrlimepmb` (dashboard Supabase project)  
**Source:** Read-only inventory through the Supabase MCP (`list_tables` and SQL catalog queries), plus verification of `device_data` through the Supabase CLI (`db dump`, `gen types`, `migration list`).

This document catalogs the persistent schemas present in the live database. “Schema” here means a PostgreSQL namespace, not just an application data model. The database also has PostgreSQL system namespaces (`pg_catalog`, `information_schema`, `pg_toast`) and session/temporary namespaces; these are infrastructure internals and are not listed as project schemas below. Object counts include tables, views, routines, and custom types visible in the catalog at inspection time. Supabase-managed schemas may change as the platform is upgraded.

## Schema inventory

| Schema | Objects observed | Purpose / notes |
|---|---|---|
| `auth` | 27 tables, 1 sequence, 4 routines, 36 custom types | Supabase Auth users, identities, sessions, MFA, OAuth, SSO/SAML, SCIM, WebAuthn, and auth migration state. All 27 tables report RLS enabled. |
| `extensions` | 2 views, 55 routines, 2 custom types | Installed PostgreSQL extensions and their objects. The views are `pg_stat_statements` and `pg_stat_statements_info`. |
| `graphql` | No relations or routines observed | Reserved GraphQL API namespace; currently empty. |
| `graphql_public` | 1 routine; no relations | Public GraphQL entry point. |
| `device_data` | 3 tables, 4 routines | **Active application schema.** Device registry, time-series readings, and token sessions. All 3 tables report RLS enabled. Exposed through the API and used exclusively by the dashboard. |
| `public` | 12 tables, 4 sequences, 2 routines, 12 custom types | Legacy dashboard application schema, superseded by `device_data` and no longer read by the app. Tables and relationships described below. All 12 tables report RLS enabled. Retained for rollback. |
| `realtime` | 11 tables (including one partitioned parent and eight date partitions), 1 sequence, 15 routines, 16 custom types | Supabase Realtime messages, subscriptions, and migration state. The partitioned message parent is `messages`; partitions observed cover 2026-09-26 through 2026-10-03. |
| `storage` | 8 tables, 19 routines, 9 custom types | Supabase Storage buckets, objects, multipart uploads, vector/analytics bucket metadata, and migration state. All 8 tables report RLS enabled. |
| `supabase_migrations` | 2 tables, 2 custom types | Database migration history and seed-file tracking. Tables: `schema_migrations`, `seed_files`. |
| `vault` | 1 table, 1 view, 5 routines, 2 custom types | Supabase Vault encrypted secrets. Relations: `secrets`, `decrypted_secrets`; the decrypted view exposes decrypted secret content and should be treated as privileged. |

## `device_data`: active application schema

The dashboard reads only this schema. It is a separate PostgreSQL namespace because the legacy `public.devices` table has a different UUID-based structure, which allowed both structures to coexist during the transition.

- `devices`: integer device registry, bottle count, owner contact details, and a hashed device token.
- `readings`: sensor samples per device/bottle/timestamp, grouped into experiments.
- `sessions`: short-lived token sessions created by a successful login, used to scope every read to one device.

The full column reference is in the [Reference](#reference-device_data-column-detail) section below, which records the live state.

## `public`: legacy application schema (no longer used)

The original application schema models equipment, experiments, experiment assignments, run sessions, telemetry, and status history. UUIDs identify most entities; telemetry and status-event rows use generated bigint identities. It is described here for rollback reference only.

### Equipment hierarchy

- `devices`: device identity/codes, current status, last-seen timestamp, and JSONB metadata.
- `nodes`: child nodes linked to devices, with identity, status, last-seen timestamp, and metadata.
- `chambers`: child chambers linked to nodes, with enabled state, status, and metadata.

### Experiments and assignments

- `experiments`: experiment code/name, description, lifecycle state, planned and actual timing, creator identifier, and metadata.
- `experiment_devices`, `experiment_nodes`, `experiment_chambers`: link experiments to equipment and record assignment/removal timestamps plus metadata. Chamber assignment rows have UUID `id` values referenced by telemetry.

### Sessions and telemetry

- `experiment_sessions`: numbered sessions for an experiment, lifecycle state, start/end times, interruption details, and optional self-reference for resumed sessions.
- `telemetry_readings`: timestamped CH4, CO2, pH, temperature, and pressure readings, quality, optional source sequence, received time, and metadata. Composite foreign keys tie both the session and chamber assignment to the same experiment.

### Status history

- `device_status_events`, `node_status_events`, `chamber_status_events`: timestamped status transitions with optional experiment context, message, and metadata. Current status is also stored on each equipment row.

### Integrity model

Foreign keys connect the equipment hierarchy, experiments, assignments, sessions, telemetry, and status events. Checks constrain lifecycle/status and telemetry quality values, require positive session numbers, and validate end/removal timestamps. Unique constraints cover equipment codes, experiment codes, assignment pairs, session numbering within an experiment, and telemetry identity `(experiment_chamber_id, session_id, recorded_at)`. Indexes cover status/last-seen lookups, assignment foreign keys, event histories by entity and time, and telemetry by experiment/session/chamber and time. Some legacy constraint/index names retain “bottle” terminology despite the current chamber naming.

## Relation inventories

The following inventories name every table and view found in the persistent project schemas at inspection time. Internal Supabase relations are listed so that the catalog covers the whole project rather than only application-owned data.

- **`auth` tables:** `audit_log_entries`, `custom_oauth_providers`, `flow_state`, `identities`, `instances`, `mfa_amr_claims`, `mfa_challenges`, `mfa_factors`, `mfa_recovery_code_sets`, `mfa_recovery_codes`, `oauth_authorizations`, `oauth_client_states`, `oauth_clients`, `oauth_consents`, `one_time_tokens`, `refresh_tokens`, `saml_providers`, `saml_relay_states`, `schema_migrations`, `scim_tokens`, `scim_users`, `sessions`, `sso_domains`, `sso_providers`, `users`, `webauthn_challenges`, `webauthn_credentials`.
- **`extensions` views:** `pg_stat_statements`, `pg_stat_statements_info`.
- **`device_data` tables:** `devices`, `readings`, `sessions`.
- **`public` tables (legacy, unused):** `chamber_status_events`, `chambers`, `device_status_events`, `devices`, `experiment_chambers`, `experiment_devices`, `experiment_nodes`, `experiment_sessions`, `experiments`, `node_status_events`, `nodes`, `telemetry_readings`.
- **`realtime` relations:** partitioned table `messages` (partitions `messages_2026_09_26` through `messages_2026_10_03`), `schema_migrations`, `subscription`.
- **`storage` tables:** `buckets`, `buckets_analytics`, `buckets_vectors`, `migrations`, `objects`, `s3_multipart_uploads`, `s3_multipart_uploads_parts`, `vector_indexes`.
- **`supabase_migrations` tables:** `schema_migrations`, `seed_files`.
- **`vault` relations:** table `secrets`; view `decrypted_secrets`.
- **`graphql` and `graphql_public`:** no tables or views.

## Exposure and scope

The dashboard's local Supabase configuration exposes `public`, `graphql_public`, and `device_data` through the API; it adds `public`, `extensions`, and `device_data` to the request search path. `device_data` must also be listed in the deployed project's API exposed schemas, which is a project setting rather than something migrations control. Without it, PostgREST resolves `device_data.*` against `public` and returns `PGRST205`.

RLS being enabled is a table property and does not, by itself, describe the policies or grants. The legacy `public` tables still carry their original broad read policies, so they remain reachable by any caller who can query them even though the dashboard no longer reads them. Treat that schema as dead-but-exposed until it is dropped.

The inventory records schema structure, not a general operational health assessment. It does not include every column of Supabase-managed internals, routine definitions, grants/policies, extension package versions, logs, or database contents. Use the live catalogs/MCP again before relying on this snapshot for migrations or access-control decisions.

## Reference: `device_data` column detail

Created by `dashboard/supabase/migrations/20260930082137_create_device_data_schema.sql`. **This schema is applied to the live project** and is now the only schema the dashboard reads. Migrations `20260930162000` through `20260930172000` added token authentication, made readings a time series, and populated development data.

### `device_data.devices`

| Column | Type | Rules / meaning |
|---|---|---|
| `device_id` | `integer` | Primary key; non-negative. Factory assigned. `0` is the development device; production numbering starts at `1`. |
| `bottle_count` | `integer` | Required, with no range check. Current development device has 24. |
| `owner_name` | `text` | Nullable. |
| `owner_email` | `text` | Nullable. |
| `token_hash` | `text` | Nullable until token provisioning; unique when present and checked for 128 lowercase hexadecimal characters (SHA-512 hex digest). Store no plaintext token, and never commit a hash. |

Device `0` is provisioned by migration `20260930162100` with `bottle_count = 24`, `owner_name = 'RUSIM team'`, and `owner_email = 'example@example.com'`, and with `token_hash` left `NULL`. The hash is applied separately from the untracked `dashboard/.env` by `bun run dev-db`, so no device credential is committed to the repository and rotating a token never requires editing a migration. An empty string is not a valid substitute for `NULL`, because the column is constrained to either `NULL` or 128 lowercase hexadecimal characters. No other devices are registered.

### `device_data.readings`

| Column | Type | Rules / meaning |
|---|---|---|
| `device_id` | `integer` | Required foreign key to `device_data.devices.device_id`. |
| `bottle_id` | `integer` | Required; no range check, so bottle counts can grow for future devices. Current convention is IDs `0`–`23` for 24 bottles. |
| `experiment_id` | `integer` | Required, non-negative; `0` is the testing experiment and `1+` are regular experiments. There is no experiments table. **Not part of the primary key.** |
| `timestamp` | `timestamptz` | Required, timezone-aware sample time. **Part of the primary key.** |
| `ph`, `pressure`, `temp`, `co2`, `ch4` | `real` | Nullable four-byte floating-point sensor values. A null means the bottle reported but that sensor produced no value, which differs from the bottle being absent from the sample entirely (disconnected). |

The primary key is `(device_id, bottle_id, timestamp)`, changed from `(device_id, bottle_id, experiment_id)` by migration `20260930170000` so a bottle can report repeatedly within one experiment. An index on `(experiment_id, timestamp DESC)` supports experiment time-series reads.

Because `experiment_id` is not in the key, one bottle cannot hold the same timestamp in two experiments. That is correct behaviour: a device cannot genuinely report two experiments for one bottle at the same instant, and a conflicting insert fails loudly.

### `device_data.sessions`

| Column | Type | Rules / meaning |
|---|---|---|
| `session_token` | `text` | Primary key; 64 lowercase hex characters (256 bits of randomness). |
| `device_id` | `integer` | Required foreign key to `devices.device_id`, cascading on delete. |
| `created_at` | `timestamptz` | Required, defaults to `now()`. |
| `expires_at` | `timestamptz` | Required, must be later than `created_at`. Currently set to 24 hours after login. |

### Access control

The dashboard is an anonymous browser client with no Supabase Auth user, so access is granted per device by token exchange:

- `authenticate(p_token)` hashes the supplied token with SHA-512 and matches it against `devices.token_hash`, then inserts a session row and returns the session token plus device details. `SECURITY DEFINER`, because `anon` cannot read `token_hash`. A wrong token and an unknown token raise the same error, so the function is not an oracle for which devices exist.
- `revoke_session()` deletes the caller's own session.
- `request_session_token()` reads the `x-device-session` request header. `session_device_id()` resolves it to a `device_id`, running `SECURITY DEFINER` so it can read the policy-free `sessions` table.

The browser sends the session token in `x-device-session` on every read. RLS policies then restrict `devices` and `readings` to `device_id = device_data.session_device_id()`. `anon` and `authenticated` hold column-scoped `SELECT` on `devices` covering only `device_id`, `bottle_count`, `owner_name`, and `owner_email`, so `token_hash` is unreachable through PostgREST. `sessions` has no grants to client roles and is reachable only through the functions above.

Verified against the live project using only the anon publishable key: reads without a session return no rows, a valid session sees only its own device, `token_hash` and `sessions` both return permission denied, and a forged or expired token sees nothing.

Realtime subscriptions are not used. Custom headers cannot be sent over the realtime websocket, so the session token cannot scope a subscription; the dashboard polls instead.

### Development tooling

`bun run dev-db` (`dashboard/scripts/dev-db.ts`) prepares the development database: it applies the device token from `.env` and generates dummy readings, either together or one half at a time with `--only token` or `--only readings`. Run with no flags it prompts for every setting; pass flags to skip the prompts.

It connects directly with `DATABASE_URL` using Bun's built-in Postgres client, so `psql` is not required. It deliberately avoids `supabase db push`, which applies pending migrations once and would make a second run a no-op, and there is no `seed_readings.sql` because a second copy of the same SQL in a seed file would only let the two drift apart. Migration `20260930172000` seeds experiment 0, so a `supabase db reset` still produces a dashboard with data in it.

Every value reaches the database as a bound parameter rather than interpolated text. The readings are written in a single transaction, so a failure part-way through leaves the database untouched. Experiment `N` is placed one full span (`sample_count * gap`) further into the past than experiment `N-1`, so experiments occupy disjoint timestamp windows and generating one leaves the others intact. Bottles reporting are those below `bottle_count`; higher-numbered bottles never appear, which the dashboard reports as disconnected.

---
Content generated by `stealth/space-bunny` using `opencode`.
