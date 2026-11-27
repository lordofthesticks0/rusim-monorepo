# Dashboard — full walkthrough

**Scope:** `dashboard/` in its entirety — application source, Supabase schema, migrations, dev tooling, and deploy config.
**Method:** direct read of every source file, migration, and config in `dashboard/`, plus a grep of `firmware/` to check for any writer. No secrets read, no live database queries.

This document explains what the dashboard is, what it does from page load to sign-out, how it authenticates, how it reads and shapes data, and where the implementation diverges from what its own comments claim. Findings and bugs are collected in §12.


## 1. What it is

`dashboard/` is a single-page React application (Vite + React 19 + TypeScript) that lets an operator holding a device token watch rumen-simulation sensor telemetry from the Supabase database.

It is **read-only**. There is no write path in the application. It talks to Supabase over PostgREST and RPC only.

There is no UI framework, no state library, and no chart library. The chart is hand-built SVG path data computed in a 20-line function. The client and its pure modules are about 2,055 lines, excluding the 840-line generated type file.

### 1.1 File map

| File | Lines | Responsibility |
|---|---|---|
| `index.tsx` | 5 | Mounts `<App/>` into `#root`, imports the stylesheet |
| `index.html` | 18 | Page shell, tab title ("In Vitro Rumen Fermentation Dashboard · IAIS Lab"), fonts, theme colour, and favicon links |
| `public/favicon.svg` | 7 | Browser-tab icon: the `BrandMark` glyph on an off-white tile, served at `/favicon.svg` |
| `public/favicon-32.png` | — | 32×32 raster fallback of the same mark for browsers without SVG favicon support |
| `public/unpad-mono.svg` | 3 | One-colour Universitas Padjadjaran emblem for the login screen, black ink (§5.11) |
| `src/App.tsx` | 1736 | Everything visible: login screen (§5.11), all three views, all components, all chart math, the transition stage (§5.12) |
| `src/brandTransition.ts` | 347 | The logout and sign-in transitions: phase choreography with the Web Animations API, plus pure geometry helpers (§5.12) |
| `src/chartInteraction.ts` | 86 | Pure chart-interaction helpers: nearest sample from pointer x, keyboard stepping, nearest series, when to draw sample dots (§6.3) |
| `src/data.ts` | 410 | Every Supabase read (latest row, experiment rows, experiment list), the auth RPCs, and the non-numbering domain logic |
| `src/readingCache.ts` | 145 | Pure reading-cache helpers: merge, dedupe and 24 h window trim, truncation flag, incremental-poll start time, folding the newest row into the experiment list (§5.3) |
| `src/useReadingCache.ts` | 90 | `useReadingCache` hook: the in-memory per-experiment store shared by all three views, plus per-experiment load progress (§5.3, §5.7) |
| `src/chambers.ts` | 58 | Maps stored 0-based bottle ids to displayed chamber numbers and splits current presence |
| `src/history.ts` | 132 | Pure history helpers: reading sort, experiment/chamber selection, per-point details, chamber grouping, selection toggle |
| `src/monitoring.ts` | 62 | Pure monitoring helpers: poll eligibility and synthetic preview data |
| `src/compare.ts` | 165 | Pure compare helpers: selection cap, duplicate check, new-row defaults, in-place row edits, add/remove, series labels and colours |
| `src/auth.ts` | 115 | Session lifecycle: localStorage persistence, expiry timer, retry backoff |
| `src/supabase.ts` | 72 | Client construction, schema pinning, session header injection |
| `src/csv.ts` | 95 | RFC 4180 CSV serialisation and download |
| `src/styles.css` | 555 | All styling: design tokens, login screen, shell, presence strip, KPI strip, charts, tables, states, the transition stage (§5.12), and the motion layer (§5.10) |
| `src/motion.ts` | 98 | Motion helpers: the sliding tab and nav indicator (`useIndicator`) and the once-per-visit entrance flag (`useEntranceOnce`, which can replay, §5.12) |
| `scripts/dev-db.ts` | 697 | Dev-only database seeding and token provisioning |
| `supabase/migrations/*.sql` | 17 files | Schema, authentication, lockdown |
| `netlify.toml` | 23 | Build config and security headers |
| `src/types/domain.ts` | 60 | Hand-written domain types and experiment-id helpers |
| `src/types/database.ts` | 840 | Supabase-generated schema types for `device_data` and legacy `public` |

### 1.2 What it is not

- **Not a device control surface.** There is no write RPC, no command queue, no configuration UI. It observes.
- **Not realtime.** No websocket, no subscription. See §5.4.
- **Only indirectly fed by firmware.** Devices can push through `push_readings()` (§11), but the firmware's sensor poll does not enqueue yet, so the rows on screen are still synthetic unless something else writes them.

---

## 2. Running it

```bash
cd dashboard
bun install
bun run dev          # vite dev server
bun run build        # tsc --noEmit && vite build
bun run preview      # serve the production build
bun run dev-db       # prepare the development database
```

### 2.1 Environment

`.env.example` documents four variables, which fall into two groups that must never be mixed:

| Variable | Consumed by | Purpose |
|---|---|---|
| `VITE_SUPABASE_URL` | App, baked into the bundle | PostgREST endpoint |
| `VITE_SUPABASE_PUBLISHABLE_KEY` | App, baked into the bundle | Anon API key |
| `DATABASE_URL` | `scripts/dev-db.ts` only | Direct Postgres connection for writing |
| `DEV_0_TOKEN_HASH` | `scripts/dev-db.ts` only | SHA-512 hex digest of the device token |

`netlify.toml` states the rule plainly: only the two `VITE_*` variables may be set on the deployed site. `DATABASE_URL` is direct Postgres write access, and `DEV_0_TOKEN_HASH` is a credential.

`src/supabase.ts` reads the two `VITE_*` variables on first use, then throws if either is missing. A missing variable is still a hard failure of the app, not a rendered error state, but importing a pure module for a test no longer constructs the client or requires the variables.

### 2.2 The static host

`netlify.toml` builds with `bun run build`, publishes `dist`, and rewrites `/*` to `/index.html` with status 200 for client-side routing. There is no router in the app, so this is defensive only.

**Deploying.** Production deploys are started by a Netlify build hook: a `POST` to a private URL, which is kept in `AGENTS.md` rather than in the docs. The hook makes Netlify fetch the site's configured GitHub branch and build it on Netlify's servers with the site's own environment variables, so only committed and pushed code is deployed. The deployed site is a development build: it keeps `DEV_FORCE_LIVE_EXPERIMENT` at `1` (§5.5), so its Monitoring view shows Preview data whenever no experiment is live. Make sure the site defines `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY`. Vite inlines both at build time; a build without them succeeds but fails on its first Supabase call.

---

### 2.3 Timing records (`?metrics`)

`src/metrics.ts` records timings only when the URL contains `?metrics`; for
everyone else `metricsEnabled` is false and every call returns at once. When
on, it prints `[metrics] {...}` to the browser console and keeps the records
in `window.__rusimMetrics`: `page_load` (navigation timing, transfer size,
viewport), `fetch` (duration, rows and age of the newest sample for
`loadLatestExperiment`, `fetchExperimentReadings` and `fetchExperimentList`) and `render` (data handed to
React -> two animation frames later, i.e. after the browser painted it).
Chrome adds `heap_bytes`. `copy(window.__rusimMetricsCsv())` in the console
copies all records as CSV. Every record is also POSTed (`fetch`, `no-cors`,
`text/plain`, errors ignored) to `http://127.0.0.1:8787/` or to the port in
`?metrics=<port>`, where `verification/dashboard-log.ts` stores it. With no
receiver running the request fails silently and the page is unaffected. Used
for the report's dashboard measurements (`verification-toolkit.md` §6.7).

## 3. The database

The application reads exactly one PostgreSQL schema: **`device_data`**. The client pins itself to it (`db: { schema: "device_data" }`) and `supabase/config.toml` exposes only it through PostgREST.

A second, much richer schema, `public`, exists from an earlier design and is retained for rollback. The application does not read it. See §10.

### 3.1 `device_data.devices`

| Column | Type | Rules and meaning |
|---|---|---|
| `device_id` | `integer` | Primary key, `CHECK (device_id >= 0)`. Factory assigned. `0` is the development device; production numbering starts at `1`. |
| `bottle_count` | `integer` | Required, no range check. How many chambers the UI offers in its selector. Chamber ids run `1` through this value. Dev device: 24. |
| `owner_name` | `text` | Nullable. Rendered in the UI. |
| `owner_email` | `text` | Nullable. Rendered in the UI. |
| `token_hash` | `text` | Nullable, `UNIQUE`, constrained to `NULL` or exactly 128 lowercase hex characters. |

`token_hash` holds only the SHA-512 hex digest of the device token. A `NULL` means the device has no token and therefore cannot be logged into at all — an empty string is not a valid substitute, because it fails the check constraint.

Device 0 is provisioned by migration `20260930162100` with `bottle_count = 24`, `owner_name = 'RUSIM team'`, and `owner_email = 'example@example.com'`, deliberately leaving `token_hash` NULL. The `ON CONFLICT DO UPDATE` clause preserves any hash already applied by `bun run dev-db`, so applying the migration cannot silently lock the device out.

### 3.2 `device_data.readings`

| Column | Type | Rules and meaning |
|---|---|---|
| `device_id` | `integer` | FK → `devices.device_id` |
| `bottle_id` | `integer` | Required internal id, retained as stored by Supabase and ESP32 (`0..bottle_count-1`). The dashboard displays `bottle_id + 1` as the chamber number. |
| `experiment_id` | `integer` | Required, `CHECK (>= 0)`. `0` is the testing experiment. **Not in the primary key.** |
| `timestamp` | `timestamptz` | Required, timezone-aware. **In the primary key.** |
| `ph`, `pressure`, `temp`, `co2`, `ch4` | `real` | Nullable four-byte floats. |

**Primary key: `(device_id, bottle_id, timestamp)`.**

This is the single most consequential schema decision, changed by `20260930170000_make_readings_a_time_series.sql`. The previous key was `(device_id, bottle_id, experiment_id)`, which permitted exactly one row per chamber per experiment and made repeated sampling within an experiment impossible. Dropping `experiment_id` from the key turns the table into a genuine time series.

`experiment_id` itself is unchanged: still `NOT NULL`, still a grouping column, still reserving `0` for the testing experiment. Only its role in the key was removed.

A direct consequence, stated in the migration: **one chamber cannot hold the same timestamp in two experiments.** A conflicting insert fails loudly on the primary key rather than overwriting. The rationale is that a device cannot genuinely report two experiments for one chamber at the same instant.

Stored `bottle_id` values remain unchanged. The dashboard adds one only when presenting an id to the user; data queries, Supabase values, and ESP32 payloads keep their existing internal ids.

All five measurement columns became nullable in the same migration. The stated intent: *"a row is now present-but-empty rather than absent when a reading fails."*

An index on `(experiment_id, "timestamp" DESC)` supports the per-experiment history read.

### 3.3 `device_data.sessions`

| Column | Type | Rules |
|---|---|---|
| `session_token` | `text` | Primary key, `CHECK ~ '^[0-9a-f]{64}$'` — 256 bits of randomness |
| `device_id` | `integer` | FK → `devices.device_id`, `ON DELETE CASCADE` |
| `created_at` | `timestamptz` | Defaults to `now()` |
| `expires_at` | `timestamptz` | `CHECK (expires_at > created_at)`. Set to `now() + 12 hours` |

Indexed on `device_id` and on `expires_at`. RLS is enabled but the table has **no policies and no client grants** — it is reachable only through the `SECURITY DEFINER` functions in §4.2.

### 3.4 The distinction the whole design rests on

Two different failure modes exist, and every function in the codebase keeps them apart:

| State | Database meaning | UI surface |
|---|---|---|
| **No row** at a timestamp | The chamber sent nothing at all - it is offline | `splitBottlesByPresence` → "Disconnected" card, disconnection banner |
| **Row present, field `NULL`** | The chamber reported, but that one sensor produced no value | `-` in the metric card, a break in the chart line, an empty CSV field |

Three separate places enforce this explicitly:

- `summarizeParameter` filters nulls out before computing min/avg/max: *"a bottle that reported no value should lower the sample count, not drag the average toward nothing."*
- `createChartPoints` emits a `null` y for a missing measurement, and `chartPath` lifts the pen, producing a break in the line rather than a drop to zero.
- `toCsv` writes an empty field rather than `0`: *"a missing measurement and a measurement of zero are different facts."*

---

## 4. Authentication, end to end

The dashboard is an **anonymous browser client**. There is no Supabase Auth user, so there is no JWT to hang row-level security off. Access is granted per device by exchanging a long-lived device token for a short-lived session token.

### 4.1 Sign-in

`auth.ts` → `data.ts:authenticate()` → RPC `device_data.authenticate(p_token)`.

The database function is `SECURITY DEFINER`, because `anon` has no grant on `token_hash`:

```sql
requested_hash := encode(sha512(convert_to(p_token, 'UTF8')), 'hex');
select * into matched_device from device_data.devices d where d.token_hash = requested_hash;
if not found then raise exception 'Invalid device token.' using errcode = '28000';
```

Because the comparison is an indexed equality test on a high-entropy SHA-512 digest, it does not leak the stored hash through timing.

A `NULL`, empty, and wrong token all raise the identical `28000` / "Invalid device token." The lockdown migration normalised the earlier `22023` code precisely so the function is not an oracle distinguishing "no token sent" from "wrong token", and therefore not an oracle for which devices exist.

On success the function:

1. Prunes **all** expired sessions globally — the earlier version pruned only the logging-in device's, which let devices that never logged in again accumulate dead rows.
2. Generates a session token: `sha256(gen_random_uuid()::text || gen_random_uuid()::text)` rendered as 64 hex characters. `gen_random_uuid()` is used as a *text* seed rather than cast to `bytea`, because PostgreSQL provides no `uuid → bytea` cast. That cast was the subject of migration `20260930164500`, whose header records that the original `::bytea` version failed every call with SQLSTATE 42846 and no session could ever be issued.
3. Sets `expires_at = now() + 12 hours` — reduced from 24 hours by the lockdown migration to shrink the window for a token leaked from browser localStorage.
4. Returns the session token plus the device's `device_id`, `bottle_count`, `owner_name`, `owner_email`.

The plaintext token exists only in the operator's hands. The database does the hashing, so it never reaches storage.

### 4.2 Per-request scoping

The session token is sent as a **custom header**, not as `Authorization`:

```
x-device-session: <64 hex characters>
```

`supabase.ts` documents why: PostgREST only accepts JWTs in `Authorization`, and this is deliberately not a JWT.

PostgREST exposes the whole header bag to Postgres as `request.headers`, so:

```sql
create function device_data.request_session_token() ...
  select nullif(
    (case when coalesce(current_setting('request.headers', true), '') ~ '^\s*\{'
          then current_setting('request.headers')::jsonb
          else '{}'::jsonb end) ->> 'x-device-session', '');

create function device_data.session_device_id() returns integer
  language sql stable security definer set search_path = pg_catalog, device_data
  ... from device_data.sessions s
      where s.session_token = device_data.request_session_token()
        and s.expires_at > now()
```

The `^\s*\{` guard handles direct `psql` access, where `request.headers` is empty and therefore not valid JSON; the fallback is `'{}'::jsonb` rather than raising.

`session_device_id` is `SECURITY DEFINER` so it can read the policy-free `sessions` table, and pins its `search_path` so a caller cannot shadow `pg_catalog.sha512` or `gen_random_uuid` with objects in a writable schema.

The two policies that do all the work:

```sql
create policy "session can read own device" on device_data.devices
  for select to anon, authenticated using (device_id = device_data.session_device_id());

create policy "session can read own device readings" on device_data.readings
  for select to anon, authenticated using (device_id = device_data.session_device_id());
```

### 4.3 Grants

| Object | Grant to `anon`, `authenticated` |
|---|---|
| schema `device_data` | `USAGE` |
| `devices` | `SELECT` on four columns only: `device_id, bottle_count, owner_name, owner_email` |
| `readings` | `SELECT` on all columns |
| `sessions` | **none** |
| `authenticate(text)` | `EXECUTE` |
| `revoke_session()` | `EXECUTE` |
| `session_device_id()` | `EXECUTE` |
| `request_session_token()` | revoked from `public`, granted to nobody |
| `cleanup_expired_sessions()` | revoked from `public`, `anon`, `authenticated` |

The column-level grant on `devices` is the mechanism that keeps `token_hash` unreachable: PostgREST refuses any select naming a column the role has not been granted, so a request for `token_hash` fails outright rather than being filtered.

`cleanup_expired_sessions()` is deliberately owner-only so that a leaked publishable key cannot abuse it, and so `session_device_id()` remains the only anon-reachable helper.

### 4.4 Sign-out

`revoke_session()` deletes exactly the caller's own row, using the same `SECURITY DEFINER` read path so it works without exposing `sessions`.

`data.ts:signOut()` wraps the call in a try/catch that swallows the error, and the comment gives the reason: *"A failure here must not trap the operator in the dashboard, since the session expires on its own within a day."* The localStorage clear in `auth.ts` is unconditional regardless.

`auth.ts:signOut(holdUntil?)` takes an optional promise. It revokes the session and clears localStorage straight away, then waits for `holdUntil` before setting the session to `null`. The logout animation passes its own promise here (§5.12), so the dashboard stays on screen while the animation plays, and the server revoke runs at the same time instead of after it.

### 4.5 Client-side rate limiting

There is **no server-side throttle**, by design. The lockdown migration explains why a database counter cannot work: a failed `authenticate` raises, which rolls back its own transaction, so any counter written inside the function would never accumulate.

The intended layers are:

1. **Client backoff** in `auth.ts` — module-scope `consecutiveFailures` and `lastAttemptAt`. After two consecutive failures, the next attempt waits `min(1000 × 2^(failures-1), 5000)` ms minus time already elapsed. This spaces out accidental rapid resubmits, nothing more.
2. **Edge rate limit or Turnstile** on the login path, which the migration names as a deployment responsibility and the security report lists as a pre-public checklist item.

Security therefore rests entirely on token entropy. The stated guidance is to issue only high-entropy random tokens of at least 128 bits (`openssl rand -hex 32`) and never human-memorable passwords.

---

## 5. The application, from load to sign-out

### 5.1 Page load

`index.html` loads `/index.tsx`, which calls `createRoot(...).render(<App/>)`. It also links `public/favicon.svg` and the `public/favicon-32.png` fallback, so the browser tab shows the same rounded-square mark and accent dot as the in-app `BrandMark` (§5.10). Vite copies `public/` verbatim into `dist/`.

`App()` calls `useAuth()`, whose `useState` initialiser calls `readStoredSession()` **synchronously**. It parses `localStorage["rusim.device_session"]`, returns null if the token is not a string or `expiresAt` is in the past, and otherwise returns the stored session. A valid stored session therefore renders the dashboard with no network call and no loading flash.

### 5.2 Session expiry timer

`useAuth` sets a `setTimeout` for exactly the remaining lifetime. On fire it clears localStorage, nulls the session, and sets the message "Your session expired. Sign in again to continue." The operator is never left looking at a dead dashboard.

### 5.3 First data load

`Dashboard` mounts and `loadData()` runs once, from a `useEffect` keyed on the `useCallback`-memoised `loadData` (memoised on `sessionToken`). The `Dashboard` element is keyed by the session token, so a new sign-in remounts it with an empty cache.

The load has two steps:

1. **Latest row.** `fetchLatestReading` asks for one row, ordered by `timestamp` then `bottle_id`, both descending (`LIMIT 1`). Its `experiment_id` names the most recent experiment. Its `timestamp` decides whether that experiment is live (§5.6).
2. **That experiment's last day.** `fetchExperimentReadings(id)` reads the experiment's rows from 24 h before its newest sample, with keyset paging (§5.7), and stores them in the reading cache.

| Constant | Value | Reasoning in the source |
|---|---|---|
| `PAGE_SIZE` | 1000 | *"Must stay at or below the server's `max_rows` setting, otherwise every page returns a truncated response and the paging loop misreads the last one as the end of the data."* |
| `EXPERIMENT_WINDOW_MS` | 86,400,000 (24 h) | How much of each experiment is loaded, measured back from its newest sample. It is a time window, not a row count, because the sample interval is per experiment (`experiments.sample_interval_s`, 30 s when unset; see `network-master.md`) and the number of reporting chambers varies. Every experiment read (Monitoring, History, Compare) uses it. Reads are newest first, so what falls outside the window is the **start** of the experiment. |
| `MAX_CHAMBERS` | 24 | Largest number of chambers a device reports. |
| `MAX_EXPERIMENT_ROWS` | 69,120 | Safety ceiling: one window for 24 chambers at the default 30 s interval (24 × 2,880). The window normally ends a read long before this. It only binds for a run with a shorter interval (the firmware allows down to 5 s), which then loads less than a day. |

With today's hardware (the Mega reports 4 bottles) at 30 s, one day is 11,520 rows, read in 12 requests.

`config.toml` sets `max_rows = 1000`, matching `PAGE_SIZE` exactly.

There is **no device filter in the query**. `fetchExperimentReadings` documents that RLS already restricts to the session's own device, so adding `.eq("device_id", ...)` would be redundant.

**The reading cache.** `useReadingCache` (`useReadingCache.ts`) holds one entry per experiment for the whole session: `{ readings, truncated }`, with readings oldest first and unique per `(bottle_id, timestamp)`. Monitoring, History and Compare all read from it, so each experiment is fetched once per session. Moving between experiments, or between views, re-reads nothing. `load(id)` returns the cached entry at once when there is one. Concurrent calls for the same experiment share one request (an in-flight map), so a double click or a poll that overlaps a History click sends one query. Memory is bounded by the window: a day of a 4-chamber run at 30 s is about 2 MB (roughly 150 bytes a row), and the 69,120-row ceiling is about 10 MB.

The merge and bookkeeping are pure functions in `readingCache.ts`, tested in `readingCache.test.ts`:

- `mergeReadings` dedupes by `(bottle_id, instant)`; the incoming copy wins. It sorts oldest first, drops rows more than 24 h older than the newest row, then keeps at most the ceiling. It reports whether it dropped anything. Polls need this: as new rows arrive, the oldest leave the window.
- `nextCacheEntry` sets `truncated` when the read left older rows out or the merge dropped any; it stays set.
- `incrementalSince` gives the start time of an incremental poll (§5.4).
- `withLatestReading` and `withConcludedState` keep the experiment list current (§5.7).

### 5.4 Polling

```js
useEffect(() => {
  if (!isMonitoringPollingEligible(view, pollSuspended)) return;
  const timer = window.setInterval(() => void loadData(), POLL_INTERVAL_MS);
  return () => window.clearInterval(timer);
}, [view, pollSuspended, liveExperiment?.experiment_id, loadData]);
```

`POLL_INTERVAL_MS` is 5 minutes 30 seconds — deliberately longer than the 10-minute staleness threshold, so each tick re-decides from fresh data rather than assuming continuity. `isMonitoringPollingEligible()` (`monitoring.ts`) only requires the Monitoring view and an unsuspended feed, so a newly started experiment is discovered without a page reload.

Each tick repeats the two-step load of §5.3, but step 2 is **incremental** when the experiment is already cached: `fetchExperimentReadings(id, { since })` reads only rows at or after `since`, which is the newest cached timestamp minus `INCREMENTAL_OVERLAP_MS` (10 minutes). The overlap catches a buffered round the device pushes late, with a timestamp older than rows already stored. `mergeReadings` drops the duplicates. A poll therefore costs one `LIMIT 1` query plus about two rounds of rows, instead of a full re-read. When a new experiment starts, the latest row points at it and it is read in full. The previous experiment stays cached for History.

Polling stops when the view is not Monitoring or a fetch has failed. `pollSuspended` is set on any error and cleared only on the next success, so a transient failure halts the live feed until the operator clicks "Refresh data". The inline comment frames this as intent: *"A failed fetch is treated as the end of the live feed, so polling stops until the operator refreshes by hand."*

**Realtime is deliberately unused.** The `x-device-session` custom header cannot be sent over a websocket, so a subscription could not be scoped to a device. Migration `20261001000000` explicitly removed the legacy `public` tables from the `supabase_realtime` publication to match.

The consequence is a worst-case latency of 5m30s for a device that samples every 5 minutes.

### 5.5 The Monitoring view

The default view, reached by the "Monitoring" nav tab.

**Shell** — a tinted L-frame (sidebar + 56px top bar on an off-white ground) with the work on a white sheet inset 8px from the right and bottom edges, 10px radius, 1px edge, no shadow. The sidebar names the product (RuSim / Telemetry with a chamber mark), scopes to `Device {id}` with owner, and holds the three views (Monitoring, Experiment history, Compare). The active view sits on a white pill with a 2px blue bar; the pill slides between items when the view changes. The top bar holds a breadcrumb (`Device {id} / {view}`), a freshness stamp (`Data loaded · Updated …`) whose green dot pulses twice when a load succeeds, and the black `Refresh data` primary, whose icon spins while a load runs. Sign out lives at the sidebar bottom with the session expiry. Below 860px the frame disappears: the sidebar becomes a drawer (at most 320px wide) behind a menu button, with a dimmed backdrop that closes it, and the sheet becomes the screen.

**Title** — the view name as the page heading, with a one-line summary under it. On Monitoring a line above the heading names the live experiment (`Live experiment Experiment 999`).

**Empty state** — when no experiment looks live, the body is replaced by a panel headed `No live experiment` pointing at Experiment history. The poll keeps running, so starting an experiment repopulates the view.

**Preview flag** — `DEV_FORCE_LIVE_EXPERIMENT` in `monitoring.ts` is `1` while the project is in development, including on the deployed site, so Monitoring can be reviewed without a device. When no real experiment is live, the view shows synthetic fresh data for experiment 999 (`generateSyntheticLiveReadings`: 12 samples per chamber at 5-minute intervals, latest 2 minutes ago, chambers 7 and 19 disconnected, one null sensor) with a `Preview` notice. History always shows real data. Setting the flag to `0` turns the preview off; Monitoring then shows `No live experiment` when nothing is live.

**Chamber presence strip** — one cell per configured chamber (`bottleCount` cells, 24 on the dev device), each labelled with its two-digit display number and a small status dot. Connected cells are white with a green dot; disconnected cells (absent from the newest sample) take a striped amber tint with an amber dot. The selected chamber is filled black. Clicking a cell selects that chamber. The strip has 24 columns on a desktop, 12 below 1100px and 6 below 480px. This is the product's signature element: it shows the whole device at the latest sample at a glance.

**Disconnection notice** — rendered only when a latest sample exists *and* at least one configured chamber is missing from it. With the default dev configuration (24 configured, chambers 1–8 reporting) this reads: "Disconnected at …: chambers Chamber 9, Chamber 10, … Chamber 24 sent no data."

**Five KPI cells as parameter tabs** — pH, Temperature, CO₂, CH₄, Pressure (`PARAMETER_DISPLAY`; stored and exported names stay `PARAMETER_NAMES`), read from the selected chamber's newest sample in the active experiment, in one container split by 1px dividers. Each cell shows its label, unit, value, and a sparkline of the chamber's last 40 samples of that parameter (real rows, gaps kept). Decimal places come from `PARAMETER_DIGITS`: pH 2, Temp 2, CO2 1, CH4 2, Pressure 2. A `null` renders as a grey dash. Selecting a cell switches the chart below to that parameter; a 2px blue bar slides under the selected cell and its sparkline turns blue. The strip has five columns on a desktop, three below 1100px and two below 860px.

**Status line** — chamber, connection state (`Disconnected` / `Concluded` / `Live` / `No data`), sample count, and latest sample time, in one 12px line under the KPI strip.

**One chart** — plotting the selected chamber's rows for the **active experiment only**, headed `Chamber {n} · {parameter}` with the record count. The line is 2px in blue (`--chart-1`) over a soft area fill, with round y ticks and gridlines; a `null` breaks both the line and the fill. Hovering the plot, clicking it, or focusing the chart and using the arrow keys selects the nearest sample, and shows a dashed vertical guide and a tooltip with value and timestamp (§6.3). Fewer than two rows renders "Not enough data for chart."

### 5.6 Live vs concluded is inferred, never stored

There is an `experiments` table for configuration (written through the device pull path), but no status column. `concluded` is derived purely from sample time:

```js
export const CONCLUDED_AFTER_MS = 10 * 60 * 1000;
export function isConcluded(timestamp, now = Date.now()) {
  const sampled = Date.parse(timestamp);
  if (Number.isNaN(sampled)) return true;   // an unparseable timestamp is treated as concluded
  return now - sampled > CONCLUDED_AFTER_MS;
}
```

Monitoring treats the latest row's experiment as live when `isConcluded(latest.timestamp)` is false. In the experiment list, `withConcludedState` recomputes `concluded` from each experiment's last sample on every render, so the pills follow the clock.

The list is sorted by `experiment_id` **descending**, which puts the testing experiment (0) last.

### 5.7 The Experiment History view

Reached by the "Experiment History" nav tab. Two-stage drill-down.

**Stage 1 — the list.** A separate query, `fetchExperimentList`, run the first time History or Compare opens and again on `Refresh data` while either view is open. PostgREST offers this client no `SELECT DISTINCT` or `GROUP BY`, so the list is built with a **skip scan** (`fetchExperimentIds`):

```
select experiment_id order by experiment_id desc limit 1                      -> 5
select experiment_id where experiment_id < 5 order by experiment_id desc limit 1 -> 2
select experiment_id where experiment_id < 2 order by experiment_id desc limit 1 -> 0
select experiment_id where experiment_id < 0 ...                              -> no row, done
```

Each step is one `LIMIT 1` query on the `(experiment_id, timestamp DESC)` index, so the cost grows with the number of experiments, not the number of rows. For each id found, two more `LIMIT 1` queries read its first and last sample time (`fetchExperimentBounds`). These start as soon as the id is found, so they overlap the rest of the scan. In total that is `3N + 1` requests for `N` experiments. There is no row count per experiment, because counting needs an aggregate.

Between list fetches, `withLatestReading` folds each poll's latest row into the list. The latest experiment's last sample moves forward, and an experiment that started after the fetch is added at the top.

Each row shows the formatted experiment id, `Started {first sample}`, `{n} readings loaded` once the experiment is in the cache, a `Testing experiment` marker when the id is 0, a Live/Concluded pill, and the last sample time. The list is capped at six rows with a `Show all N` toggle. The selected row takes the active fill.

Selection starts empty; clicking the selected item clears it (`toggleExperimentSelection()`), and choosing another replaces it. A notice asks the operator to select an experiment until one is chosen.

**Stage 2 — the detail.** Selecting an experiment calls `cache.load(id)`. A cached experiment renders at once with no request. Otherwise `fetchExperimentReadings` runs. Loading is derived (selected, not cached, no error) rather than stored. A load the operator navigates away from still finishes into the cache. The read filters with `.eq("experiment_id", id)` and uses **keyset paging** instead of offsets. Rows are ordered by `timestamp` descending, then `bottle_id` descending. Each page after the first adds a filter for rows older than the last one seen:

```
or=(timestamp.lt."T", and(timestamp.eq."T", bottle_id.lt.B))
```

The pages are reversed before returning, so callers always get rows oldest first. `bottle_id` is the tiebreak because all bottles in one round share a timestamp, so a page boundary can fall inside a round. An offset would shift as new rows land at the front during a run, and could skip or repeat rows; "older than this row" cannot. Every bottle is sampled at the same instants, so the window (or the row ceiling) cuts the experiment off at one point in time for all bottles; it never drops whole bottles.

The window start (24 h before the newest sample) is only known once the first page arrives, so it is applied in the client. Each page is filtered to rows inside the window, and paging stops at the first page that reaches past it. That row older than the window is also the exact signal that the start is missing, so `truncated` needs no extra query. At most one page of older rows is read and discarded. Before 2026-11-27 the read was oldest first with a 50,000-row cap, and earlier that day briefly newest first with a 6,912-row cap. Before 2026-11-21 it ordered by `bottle_id` first and capped at 5,000 rows, so any experiment over about 10 h lost its later bottles.

The ordering is load-bearing: it keeps a multi-page read contiguous, so each chamber's samples stay together.

**"Chamber Readings" sub-panel** — a chamber selector, and five per-parameter charts stacked in one column. The charts are interactive: hovering or clicking the plot, or focusing a chart and using the arrow keys, selects the nearest sample and shows a tooltip above it with timestamp and value (`getHistoryPointDetails()`, §6.3).

**Loading feedback.** While an experiment is read in full, `useReadingCache` exposes the rows received so far in `cache.loading` (keyed by experiment id, updated after each 1,000-row page through `fetchExperimentReadings`' `onProgress`). The clicked list row shows `Loading… {n} readings` in the accent colour and is `aria-busy`. The detail area shows a dashed `.loading-block` with the sliding line and `Loading Experiment {id}… {n} readings so far` (`role="status"`). A day of 4 chambers at 30 s is 12 requests, so the count visibly climbs. Incremental poll reads are not listed, because they are one short request for an experiment already on screen.

**"Results" sub-panel** — one table with a row per chamber (`groupReadingsByChamber`): chamber, sample count, and for each parameter the mean with min – max as a 12px subline in the same right-aligned cell. A dash means no bottle reported that sensor.

**CSV download** — see §8.

### 5.8 Truncation is surfaced, not hidden

The cache entry's `truncated` flag is set when the experiment has rows older than the loaded day: the read saw one, a poll pushed rows out of the window, or the read stopped at `MAX_EXPERIMENT_ROWS`. The detail pane then shows: *"Only the experiment's most recent day is loaded, from {first loaded sample} onward. Earlier samples are missing on screen, in Results, and in any CSV export."* The date comes from the first loaded row, so the message stays accurate when the ceiling cuts the load to less than a day. An experiment of 24 h or less is never flagged.

The export is deliberately of what is on screen rather than a fresh query, and the comment explains the tradeoff: *"This exports what is already on screen rather than re-querying, so the file and the charts always describe the same rows. A truncated read therefore produces a truncated file, which is why the limit is called out on screen next to the button."*

### 5.9 The Compare view

Reached by the "Compare" nav tab. Overlays up to four chamber series on one chart; chambers may come from different experiments.

**Selection** — a panel headed `Chambers to compare`. Each series is its own filters row (`.filters.compare-row`): a colour dot, an Experiment select, a Chamber select (all configured chambers), and a × remove button. Changing either select edits that series in place, and the chart follows at once. Below the rows sit `Add chamber`, which appends a row, and `Clear all`. The header shows `n of 4`.

- **First row.** When the experiment list arrives, the view starts with one row: the newest experiment and its first chamber. This happens once; after `Clear all` the view stays empty (with a short hint) until a row is added.
- **New rows** (`nextSelection`) copy the last row's experiment and take its next free chamber, wrapping past the last chamber. `Add chamber` is disabled at `MAX_COMPARE_SERIES` (4) or when that experiment has no free chamber.
- **Distinct rows.** The same experiment and chamber cannot appear twice, but the same chamber in different experiments can. In each row's Chamber select, chambers that another row already uses with the same experiment are disabled. `updateSelection` also refuses such a change. Switching a row's experiment keeps its chamber when free there, and otherwise moves it to the next free one.
- **Colours** are assigned by row position (`--chart-1` blue, `--chart-2` green, `--chart-3` amber, `--chart-4` violet), so removing a middle row shifts later colours. Rows are keyed by position for the same reason; every field is controlled, so reusing a row element after a removal is safe.
- **Layout.** On a desktop a row is one line, with the × at the right edge. Below 860px each row becomes a small grid: dot and × beside the `Experiment` label, each label above its full-width select, and a rule between rows.

**Data** — each distinct experiment in the selection is loaded through the shared reading cache (§5.3). An experiment already opened in Monitoring or History costs no request. The chart filters each cache entry with `selectChamberReadings`. Compare uses real data only (the synthetic preview experiment 999 never appears in the experiment list) and does not poll.

**Chart** — `CompareChart` draws all series on one SVG with a shared y-axis (ticks computed from the flattened rows of every series) and x aligned by sample order (first sample to first sample, each series stretched across the plot width). The x labels come from the longest series. Each series is a 2px line in its colour. The same hit area as the single-series charts selects a sample: the pointer picks the series whose line is closest, and ↑/↓ switch series from the keyboard (§6.3). The tooltip above the point shows the series label, timestamp, and value. A legend lists each series with its colour dot and sample count. Fewer than two rows in every series renders "Not enough data for chart."

---

### 5.10 Visual design and motion

The look was redesigned with the `design-taste-frontend` skill. Its scripts dealt the palette and the type pairing for a software product. The rest of that skill targets marketing sites (heroes, photographs, one section per screen), so only the parts that fit an operational dashboard were applied.

**Tokens** (`:root` in `styles.css`):

| Role | Value |
|---|---|
| Ground (frame) | `#f7f7f5` off-white |
| Sheet and panels | `#ffffff`, 1px `#e3e3de` edge |
| Ink / muted | `#000000` / `#666662` |
| Accent | `#2563eb` blue: selection, focus ring, the main chart line |
| Primary button | black with white text (the login button uses the accent instead, §5.11) |
| Chart series | `--chart-1` blue `#2563eb`, `--chart-2` green `#0f8a6a`, `--chart-3` amber `#b86200`, `--chart-4` violet `#7c3aed` |
| Type | Chivo (text and headings), Chivo Mono (ids, units, chamber numbers), from Google Fonts in `index.html` |
| Radii | 4px controls, 6px panels, 10px sheet |

Status colours stay semantic: green for live and connected, amber for disconnected, red for errors.

**Motion.** Every motion answers a click, a hover, or new data, and none is driven by scrolling. Only `transform` and `opacity` animate, apart from colour changes, the SVG point radius, and the wordmark's `clip-path` and `filter: blur()` in the sign-in and logout transitions (§5.12).

| Motion | Trigger | Duration | Where |
|---|---|---|---|
| Title rises out of a line mask, then the summary and content fade up 12px | First screen of the visit (login or dashboard), once | ~1.1 s in total | `.is-entering`, `useEntranceOnce()` |
| Login fermentation curve draws left to right | Login entrance | 1.6 s after 520 ms | `.login-trace-line.draw` |
| Login bubbles rise through the identity panel | Always, while the login screen is open | 10–19 s loops | `.login-bubbles span` |
| Login field shakes | A sign-in attempt fails | 360 ms | `shake()` (Web Animations API) |
| Login logo glides into a centred lockup, then the dashboard zooms out of the sidebar mark | A successful sign-in | ~1.3 s | `brandTransition.ts` phases "lift" and "arrive" (§5.12) |
| Camera zooms through the sidebar mark, the wordmark opens under it, then the lockup glides into the login logo | Sign out | ~2.7 s | `brandTransition.ts` phases "outro" and "return" (§5.12) |
| New view settles in (fade, 6px rise) | Switching view | 260 ms | `.view-swap.is-swapping` |
| Nav pill slides to the active view | Switching view | 240 ms | `.nav-indicator`, `useIndicator(…, "box")` |
| Bar slides under the selected KPI cell or tab | Selecting a parameter or a History/Compare tab | 240 ms | `.tab-bar`, `useIndicator(…, "bar")` |
| Chart line draws, then area and points fade in | New chamber or parameter (not on a poll) | 900 ms | `.chart-line.draw`, keyed on `drawKey` |
| Compare series draw one after another | New series or parameter | 900 ms, 90 ms apart | `CompareChart()` |
| KPI value ticks up into its slot | Value or chamber changes | 320 ms | `.tick`, keyed on chamber and value |
| Sparklines draw | Chamber changes | 900 ms, 60 ms apart | `Sparkline()` |
| Status dot ring pulses twice | A load succeeds | 1.2 s × 2 | `.status-dot.pulse`, keyed on `loadedAt` |
| Refresh icon spins; a thin bar slides while loading | Loading | 1 s / 1.1 s loops | `.is-busy`, `.loading-line` |
| Buttons step 1px up on hover and press 1px down at 98.5% | Pointer | 150 / 90 ms | `.btn` |
| Chamber cells lift 1px; rows and table rows tint; row chevrons nudge 3px | Pointer | 150 ms | `.presence-cell`, `.row-button`, `tbody tr` |
| Tooltip pops in | Sample hovered, clicked, or stepped to with the keyboard | 140 ms | `.chart-tooltip-inner` |
| Phone drawer slides in over a fading backdrop | Menu button | 200 ms | `.sidebar.open`, `.backdrop` |

Hover motion applies only to a fine pointer (`@media (hover: hover) and (pointer: fine)`), so a phone never keeps a stuck hover state. Under `prefers-reduced-motion: reduce` every animation is off and only colour transitions remain, so all content is visible at once. The CSS rule cannot stop animations started from JavaScript, so the sign-in and logout transitions check `prefersReducedMotion()` themselves and are skipped.

**How the sliding indicator works.** `useIndicator()` receives the container through a callback ref, finds the child marked `aria-selected="true"` or `aria-current="page"`, and writes its offset and size to CSS variables on the one indicator element. Only `transform` animates. A bar uses a 100px element scaled with `scaleX(width / 100)`; the nav pill takes its size directly and only moves. The first placement, and any placement after a resize, jumps without a transition (`.is-jump`), so the indicator travels only when the selection changes. A callback ref is used because the KPI strip unmounts and mounts again when the view or the loading state changes.

### 5.11 The login screen

`LoginScreen()` in `App.tsx` renders two panes inside the same 8px frame the dashboard uses (`.login-shell`).

| Pane | Content |
|---|---|
| Identity (`.login-aside`, left) | One-colour Universitas Padjadjaran emblem, a thin divider, then "IAIS Lab" and "Teknik Elektro · Universitas Padjadjaran"; the product name "In Vitro Rumen Fermentation Dashboard"; a one-line description; the five measured parameters as chips; a decorative fermentation curve; a copyright line |
| Sign-in (`.login-panel`, right) | The RuSim logo (`RusimLogo`: the `BrandMark` glyph with the "RuSim / In Vitro Rumen Simulator" wordmark), the "Sign in" heading, the device-token field, the submit button, and a note on how the session works |

The whole screen uses one flat background (`--ground`). The two areas are separated only by a 1px `--border` line: vertical on wide screens, horizontal under the identity area on narrow ones. There are no filled blocks or panels. The identity area defines `--aside-ink`, `--aside-muted`, `--aside-faint`, `--aside-line` and `--aside-accent` on `.login-aside`, mapped to the global tokens, so it can be re-tinted in one place. The only strong colour is the accent blue: the live dot, the fermentation curve, the field focus ring, and the sign-in button.

**The Unpad emblem (`public/unpad-mono.svg`).** The original emblem has four colours: an orange field, black outlines and wings, white details, and a red flame and wheel. The one-colour file was derived from the colour SVG on Indonesian Wikipedia (`File:Lambang_Universitas_Padjadjaran.svg`, listed there as fair use) by a one-off conversion:

1. Editor metadata (Inkscape attributes, ids, an unused full-page clip path) was removed.
2. All shapes were moved into an SVG `<mask>`. In a mask, white shows and black hides. The orange shapes were filled black (hidden) and every other shape white (shown).
3. A single rectangle filled `#000000` (the page text colour, `--text`) is drawn through that mask.

So the orange field becomes transparent, the page background shows through, and the emblem is one ink colour that matches the text. The original drawing leaves gaps between the wings, the torch, and the wheel, so these stay distinct without a second colour. A plain `fill` swap would not work, because the orange layers are painted over full black pentagons: making orange transparent would reveal solid black underneath. The file is served from `public/` as an `<img>`, which the Content-Security-Policy allows (`img-src 'self'`). Because an `<img>` cannot read CSS variables, the ink colour is fixed in the file. A version for a dark background needs a second file with a light ink (run the same conversion with, for example, `#f4f4f2`). The comment at the top of the file must not contain `--`, which is illegal inside an XML comment and makes the browser reject the whole image. If the university provides an official one-colour emblem, it can replace this file under the same name.

The `RusimLogo` glyph is the same `BrandMark` used in the sidebar and the favicon. The project has no separate RuSim logo file.

**Field behaviour.**

- The eye button toggles the input between `password` and `text`. Its icon shows the action: a plain eye means "show", a slashed eye means "hide". The slash draws in through `stroke-dashoffset`.
- A typed token is shown in Chivo Mono with wider letter spacing, so characters are easy to check when revealed.
- "Caps Lock is on" appears under the field while Caps Lock is active (`KeyboardEvent.getModifierState("CapsLock")`). It is announced through `aria-live`.
- On a failed attempt the field border turns red, the error is linked to the input with `aria-describedby`, and the field shakes once. `auth.ts` clears the error to `""` before every attempt, so each failure is a new value and triggers a new shake. The shake uses `element.animate()` instead of a CSS class, so it replays without remounting the input and losing focus. It is skipped under `prefers-reduced-motion`.
- The enabled sign-in button is accent blue (`--accent`), not the black primary used in the dashboard, so blue is the only strong colour on the screen. Disabled (empty field) it is grey. While verifying, it stays blue, shows "Verifying token" with a spinning icon, and a thin bar runs along its base.

**Hand-off to the dashboard.** A successful sign-in plays the "lift" and "arrive" transitions (§5.12). A stored session (page reload) opens the dashboard without them.

**Layout by width.**

| Width | Layout |
|---|---|
| > 860px | Two columns (identity 1.1fr, form 1fr); the form is centred, max 380px |
| ≤ 860px | One column: the identity pane becomes a compact header without chips and curve; the form sits below it, the input uses 16px text so iOS does not zoom in, and the eye button grows to 44px |

### 5.12 Sign-in and logout transitions

The logout copies the outro of the product video (`video-demo/hyperframes/compositions/walk.html` from 128.8 s, then `endcard.html`; see `docs/video-demo.md`). The camera zooms through the sidebar's RuSim mark until the mark stands alone in the centre of an empty page. The "RuSim / In Vitro Rumen Simulator" wordmark opens under it, and then the login screen comes back. Sign-in plays the same idea in reverse.

**The stage.** Both transitions play on one full-screen layer, `BrandStage` in `App.tsx` (`.brand-stage` in `styles.css`). It holds three things:

| Part | Element | Role |
|---|---|---|
| Ground | `.brand-stage-ground` | A `--ground` coloured cover. Fading it in hides the page behind the stage; fading it out reveals the page. |
| Lockup | `BrandMark`, "RuSim", "In Vitro Rumen Simulator" | The video's end card: mark on top, wordmark and subtitle centred under it. The mark is `clamp(88px, 9vw, 168px)` (168px is its size in the 1920px video) and the text sizes are fractions of it. |

The stage's CSS lays out the **finished** lockup. No animation changes that layout. Each phase measures where a piece must come from or go to, then moves it there with `transform`. The stage is `100vw` wide, which includes the scrollbar, so its centre does not move when the dashboard (which has a scrollbar) is swapped for the login screen (which has none). While the stage is mounted it covers the page and takes all clicks, and `.app` gets `overflow: clip`, so the magnified dashboard cannot add scrollbars. `clip` is used instead of `hidden` because it does not create a scroll container, so the sticky sidebar keeps working.

**The four phases.** `App()` keeps a `phase` state. `playPhase(phase, stage)` in `brandTransition.ts` starts the animations of one phase and returns `{ finished, cancel }`.

| Phase | Shown under the stage | What moves | Length |
|---|---|---|---|
| `outro` (logout) | Dashboard | The stage mark starts exactly on the sidebar mark. In 380 ms it travels to the screen centre at 1.5× (ease in-out). In the next 420 ms it zooms to full size (power3.in). The dashboard moves with it like a camera, and the ground fades in over the second half. The mark then slides up into the lockup (expo.out). "RuSim" opens from its centre line (`clip-path`) and the subtitle blurs in. The finished lockup holds for 320 ms. | ~1.9 s |
| `return` (logout) | Login screen | The three pieces glide into the login logo's mark, "RuSim" and subtitle, 45 ms apart, mark first. The ground fades out, and the login screen plays its first-load entrance again. | ~0.8 s |
| `lift` (sign-in) | Login screen (busy) | The reverse of `return`: the pieces glide from the login logo into the centred lockup, text first. The ground covers the login screen. | ~0.6 s |
| `arrive` (sign-in) | Dashboard | The wordmark blurs out. The mark shrinks to 1.5× the sidebar mark about the centre (power3.out), then travels into the sidebar mark. The dashboard zooms out with it, and the ground fades out in the first 40%. | ~0.7 s |

`outro` and `arrive` are the same camera move in opposite directions: the video's two-part zoom (centre the mark first, then zoom about that point), which keeps the mark on a straight path to the centre.

**How the camera works.** Scaling about a point P means: set `transform-origin` to P and apply `translate(...) scale(s)`. The point P stays fixed under the scale, and the translate then carries it to its destination. `cameraShift(from, to, scale)` computes that shift. `from` is the sidebar mark's centre, `to` is the screen centre, and `scale` is the stage mark's size divided by the sidebar mark's size (about 4.6× on a 1440px screen). At the end of the zoom, the magnified sidebar mark and the stage mark are the same square in the same place, so the hand-over cannot be seen. The stage mark rides on top the whole way, so the mark stays sharp while the magnified dashboard under it blurs slightly.

The camera does not transform one shared parent. It gives every direct child of `.app` the same transform about the same screen point, each with its own `transform-origin`. A transform on a parent makes that parent the containing block of its `position: fixed` children. The phone drawer and its backdrop would then be placed relative to the top of the page instead of the screen, and on a scrolled page they would jump out of view.

**How the pieces fly.** `boxOnto(box, target)` lays one box on another: translate the top-left onto the target's top-left, then scale by the height ratio. The mark is measured with `getBoundingClientRect()`. A word is measured with a `Range` over its text, which gives the glyph box and ignores `line-height`. So an 80px "RuSim" and the 17px "RuSim" in the login logo compare by font size alone. When the return ends, each piece lies on its counterpart to within 0–3px of width; the small width difference comes from font hinting at different sizes. Then the stage is removed and the real logo is shown in the same render.

**How the phases are wired** (all in `App()`):

- **Logout.** `signOutWithOutro()` creates a promise, stores its `resolve` in `releaseOutro`, sets `phase = "outro"`, and calls `signOut(promise)` (§4.4). The revoke runs while the outro plays. When the outro's animations finish, `releaseOutro()` lets `signOut()` drop the session. In the render where the session is gone, `App()` changes `"outro"` to `"return"` before React commits. So the login screen mounts already knowing it is coming back: `useEntranceOnce(true)` replays the entrance, and the logo is `visibility: hidden` (`.rusim-logo.is-held`) while the stage's copy flies to it. The logo has no entrance step of its own in that case. The heading's entrance steps sit on the title and subtitle, not on the heading, so nothing moves the spot where the stage lands.
- **Sign-in.** `signInWithIntro()` sets `holdLogin` before the attempt. The session is set inside `signIn()` before it resolves, and the flag keeps the login screen up during that gap. On success it sets `phase = "lift"`. The login screen stays mounted (still showing "Verifying token") until `lift` finishes, then `phase = "arrive"` mounts the dashboard under the opaque stage. The dashboard's own title entrance has already played once this visit, so only the zoom-out plays.
- **Running a phase.** A `useLayoutEffect` on `phase` calls `playPhase()` before the browser paints, so the first frame of each phase is already in place. Its cleanup cancels the previous phase's animations in the same commit as the next phase starts, and each phase's first keyframes equal the previous phase's last. A 4 s timer moves on if `finished` never resolves, so the stage cannot trap the user.
- **Fallbacks.** On a phone the drawer is closed after a sign-in, so there is no sidebar mark on screen to zoom out of. The mark fades instead, and the dashboard settles from 1.04× about the screen centre. If an element is missing, a phase still fades the stage in or out. Under `prefers-reduced-motion` no stage is shown at all: sign-in and sign-out switch screens directly.

## 6. From a row to a chart point

This is the most intricate logic in the client, and it lives entirely in `App.tsx`.

### 6.1 Scaling

```js
function createChartPoints(rows, parameter) {
  if (rows.length < 2) return [];
  const values = rows.map((row) => getMetricValue(row, parameter));
  const reported = values.filter((value) => value !== null);
  if (reported.length === 0) return values.map((_, i) => ({ x: (i/(values.length-1))*800, y: null }));

  const min = Math.min(...reported);
  const max = Math.max(...reported);
  const range = max - min;
  return values.map((value, index) => ({
    x: (index / (values.length - 1)) * 800,
    y: value === null ? null : range === 0 ? 110 : 190 - ((value - min) / range) * 160,
  }));
}
```

Properties worth noting:

- **The scale is computed only from samples that reported.** One missing value cannot distort the axis.
- **A null measurement becomes a null y**, not a zero. The comment: *"A null measurement becomes a null y, which `chartPath` renders as a break in the line rather than a drop to zero."*
- **A flat series is pinned to `y = 110`**, the vertical midpoint of the plot band.
- **The plot band is y ∈ [30, 190]** within a `viewBox="0 0 800 220"`.
- **x is index-proportional, not time-proportional.** Irregular sampling still spaces evenly across the width.

### 6.2 Path construction

```js
function chartPath(points) {
  let penDown = false;
  return points.map((point) => {
    if (point.y === null) { penDown = false; return ""; }
    const command = penDown ? "L" : "M";
    penDown = true;
    return `${command}${point.x} ${point.y}`;
  }).join(" ");
}
```

The pen lifts on every null and re-anchors with a new `M`, so gaps render as separate segments instead of a line jumping across missing data.

### 6.3 Rendering

`Chart()` and `CompareChart()` draw everything in one `<svg>`. The `viewBox` width is the chart's measured pixel width (`useMeasuredWidth`, a `ResizeObserver`), so one SVG unit is one screen pixel: axis text stays 11px on a phone instead of shrinking with the drawing. `chartGeometry(width)` sets the plot margins and the height: 260px, or 200px below 560px of width. `createChartPoints()` above only decides which samples are null; the renderers place points with their own `toX`/`toY` from the y ticks.

Inside the SVG, from back to front:

1. Gridlines and y tick labels from `createYTicks()`.
2. The area fill (`chartAreaPath()`): one closed shape per unbroken run of samples, filled with a vertical gradient of the line colour, so a gap in the data is also a gap in the fill. Single-chamber charts only.
3. A dashed vertical guide at the active point.
4. The line (`chartPath()`).
5. Dots on each sample, only when samples are at least 6px apart (`showMarkers`, `MIN_MARKER_SPACING` in `chartInteraction.ts`). They are decorative (`aria-hidden`, no pointer events); denser series show only the line.
6. The active point: one larger dot at the hovered or selected sample.
7. The hit area (`.chart-hit`): one transparent `<rect>` over the plot that takes all pointer input.

**Interaction.** The chart does not create an element per sample. The pointer's x is converted to viewBox units (`viewBoxX`) and mapped to the nearest sample index (`nearestIndex`). Samples are spread evenly across the plot, so this is one division, not a search. In Compare, each series finds its own nearest index, and the series whose point is closest to the pointer's y wins (`nearestSeries`). A mouse hover is transient and a click pins it; leaving the plot clears an unpinned point. A touch drag pins, because lifting the finger also fires `pointerleave`. The `<svg>` is one Tab stop (`role="group"`, `aria-roledescription="chart"`). ← and → step one sample, Page Up and Page Down a tenth of the series, Home and End jump to the ends (`stepIndex`), and Escape clears. In Compare, ↑ and ↓ move to the neighbouring series at the same relative position. The tooltip has `role="status"`, so screen readers announce each value.

Before 2026-11-27 every sample was its own focusable `<circle>`, with five event handlers and an `aria-label` built with `toLocaleString`. A day at 30 s is 2,880 samples per chamber, and History draws five charts, so selecting a chamber built about 14,400 such elements. Measured in Chromium, the labels alone took 867 ms, and the page froze after the data arrived. With the hit area, changing chamber on a 2,881-sample day renders in about 25 ms, and the five charts hold 148 DOM nodes in total.

`createTimeLabels` picks evenly spaced **indexes** (five, or three on charts under 480px wide) and renders `HH:MM` for each, so the axis reads as a time axis while actually being a sample axis.

### 6.4 What the chart is not

No zoom and no panning. The Monitoring, History and Compare charts show a tooltip for the hovered or keyboard-selected sample (value and timestamp; Compare also names the series). Values are read from the KPI strip (Monitoring) or the y ticks and tooltips (History).

---

## 7. Statistics

`summarizeParameter` computes cross-bottle min/average/max for one parameter:

```js
const reported = values.filter((value) => value !== null);
if (reported.length === 0) return null;
return { min: Math.min(...reported),
         average: reported.reduce((sum, v) => sum + v, 0) / reported.length,
         max: Math.max(...reported) };
```

Nulls are excluded rather than treated as zero, and it returns `null` — rendered as "No readings" — when nothing reported at all.

`groupReadingsByChamber` groups one experiment's rows by `bottle_id`, sorts each chamber's readings by time, and computes per-chamber statistics. The Results view therefore has one entry per chamber, not one per row.

---

## 8. CSV export

`src/csv.ts` produces RFC 4180 output.

**Columns:** `bottle_id, timestamp, pH, Temp (°C), CO2 (ppm), CH4 (ppm), Pressure (Pa)`.

Pressure is stored and exported in pascals: `legacy/slave/new-slave.cpp`
produces `press` as `sensorValue * PA_PER_UNIT`. The dashboard does not rescale
it (the earlier `kPa` label was wrong).

The CSV keeps the historical `bottle_id` header, but exports the user-facing chamber number (`bottle_id + 1`). The Supabase row remains unchanged.

Headers and row values are both generated from `PARAMETER_NAMES` and `PARAMETER_UNITS`, with the field lookup delegated to `getParameterValue`, so a sensor cannot be exported under the wrong name or column. The comment states the intent: *"a downloaded file can be read without the dashboard."*

| Behaviour | Detail |
|---|---|
| Line endings | CRLF, as the RFC requires |
| Quoting | Only fields matching `/[",\r\n]/`; internal quotes doubled |
| Null values | Empty field, never `0` |
| Row order | Re-sorted by timestamp then bottle — the order a human reads an experiment in, not database order |
| Empty input | Returns `""`, and the caller returns early without touching the DOM |
| Filename | `experiment-{id}-readings-{ISO-8601}.csv` |

The filename embeds a timestamp so repeated downloads of the same experiment land as distinct files and a later file is never mistaken for an earlier one.

`downloadCsv` creates a blob URL, clicks a synthetic anchor, and revokes the URL on `setTimeout(..., 0)` rather than synchronously. The comment records why: *"revoking it synchronously can cancel the download in some browsers, which would leave the operator with nothing and no error."*

A disabled download button carries an explanatory `title` from `csvDisabledReason` — "Readings are still loading", "Readings failed to load, so there is nothing to export", or "This experiment has no readings to export" — so the reason is never left to guesswork.

---

## 9. Development tooling: `bun run dev-db`

`scripts/dev-db.ts` is the only thing in the repository that writes to the database. It performs both jobs in one connection and one transaction.

### 9.1 Token half

Applies `DEV_0_TOKEN_HASH` to a device's `token_hash`.

- `.env` is read with a hand-rolled line parser, **not executed** — *"so a stray shell line cannot run code."*
- A real environment variable beats the file, because Bun loads `.env` into `process.env` at startup and the file's copy would otherwise silently win.
- The 128-lowercase-hex shape is validated before any write, and only the shape is reported on failure — the value is never echoed.
- Two pre-flight checks run before the update: the device must exist, and the hash must not already belong to a different device. The comment explains the ordering: otherwise the update would fail *after* the readings had committed, which "looks like the seed was lost when it was not."
- `--token-hash ""` clears the token, which is the escape hatch for handing a unique hash to a different device.
- The stored value is a bound parameter, so the hash never lands in a query log.

### 9.2 Readings half

Deletes the target experiment's rows, then inserts `sampleCount × bottleCount` rows, all in a single transaction so a failure part-way leaves the database untouched. Every value reaches the database as a bound parameter.

**The timestamp layout is the subtle part:**

```sql
date_trunc('minute', now())
  - make_interval(mins => experiment_id * (sample_count * sample_gap_minutes))
  - make_interval(mins => (sample_count - 1 - sample_offset) * sample_gap_minutes)
```

Experiment 0 ends at the current minute, so it reads as **live**. Each higher experiment id is shifted a **full span** (`sample_count × sample_gap_minutes`) further into the past, so windows are disjoint and generating one experiment cannot disturb another.

The comments record the bug this fixed: a naive 5-minute stride looks correct for a single sample per experiment but makes neighbours overlap almost completely — with 24 samples every 5 minutes, experiment 1's newest sample lands on experiment 0's second-newest, and the two collide on the primary key.

Sensor values get a **stable per-chamber offset** (`bottle_id * 0.11` for pH, `* 0.28` for pressure, and so on) so chambers remain visually distinguishable across samples instead of being pure noise, plus a configurable `null_rate` (default 0.0005) to exercise the null-rendering paths. The seed generates chamber ids `1..bottleCount` directly.

The script also reports what landed: row count, chambers reporting, first and last sample in UTC, null counts for pH and methane, and a list of configured-but-never-reporting chambers — described as what "the dashboard reads as 'disconnected'".

### 9.3 Why a script and not a migration

Documented in both the script header and the seed file: `supabase db push` applies pending migrations once and records them, so a second run would be a no-op. Regenerating on demand needs a direct connection. Keeping a second copy of the same SQL in a `.sql` file *"would only let the two drift apart."*

### 9.4 Safety guards

The script **deletes and rewrites data**, so it refuses any non-loopback host without an explicit `--allow-remote`. TLS is inferred from the hostname — Supabase requires it, a local `supabase start` does not — with `--tls` / `--no-tls` overrides.

It runs with no flags it prompts for every setting through readline, reading via the `line` event rather than `readline.question()` because the latter never settles once the stream ends, which made piped input hang. Out-of-range or non-integer answers are rejected rather than clamped, *"so a typo cannot quietly produce surprising data."*

### 9.5 Initial data

Migration `20260930172000` populates experiment 0 (24 samples, 5-minute gap, 8 of 24 chambers, `null_rate` 0.0005) using internal ids `0..7`. The dashboard displays those rows as Chambers 1 through 8; the other 16 configured chambers remain disconnected and have no fabricated readings. `bun run dev-db` continues to seed internal ids starting at 0.

---

## 10. Deployment and the legacy schema

### 10.1 Security headers

`netlify.toml` sets, on all paths:

| Header | Value | Purpose |
|---|---|---|
| `X-Frame-Options` | `DENY` | No framing |
| `X-Content-Type-Options` | `nosniff` | No MIME sniffing |
| `Referrer-Policy` | `strict-origin-when-cross-origin` | Referrer containment |
| `Strict-Transport-Security` | `max-age=31536000; includeSubDomains` | HTTPS-only |
| `Permissions-Policy` | `camera=(), microphone=(), geolocation=()` | Feature denial |
| `Content-Security-Policy` | `default-src 'self'`; `script-src 'self'`; `style-src 'self' 'unsafe-inline' https://fonts.googleapis.com`; `font-src 'self' https://fonts.gstatic.com`; `img-src 'self' data:`; `connect-src 'self' https://*.supabase.co wss://*.supabase.co`; `object-src 'none'`; `base-uri 'self'`; `frame-ancestors 'none'` | The important one |

The stated motivation: session tokens live in localStorage, so any XSS equals session theft. The CSP permits only self-hosted scripts and only Supabase as a connect target.

`style-src 'unsafe-inline'` is the one relaxation, needed because the stylesheet is a single imported file with inline style attributes on chart elements.

Google Fonts is the one outside origin for styles and fonts. `index.html` loads the Chivo and Chivo Mono stylesheet from `fonts.googleapis.com` (allowed by `style-src`), and that stylesheet loads the font files from `fonts.gstatic.com` (allowed by `font-src`). Without both entries the browser blocks the fonts and the page falls back to the system font. Scripts are still self-hosted only; neither Google origin may serve a script.

### 10.2 The legacy `public` schema

`20260927093901_create_mvp_schema.sql` built a substantially richer model: `devices → nodes → chambers` as an equipment hierarchy, `experiments`, `experiment_devices` / `experiment_nodes` / `experiment_chambers` assignment tables, `experiment_sessions` with self-referencing resume links, `telemetry_readings` with composite foreign keys preventing a reading from crossing an experiment boundary, and three status-event tables. All UUID-keyed, with CHECK constraints on every lifecycle state.

It is **dead**. The application pins `db: { schema: "device_data" }` and `config.toml` exposes only `device_data` to PostgREST.

It is retained for rollback, and `seed.sql` plus `config.toml` still seed it, so `supabase db reset` produces a fully-populated legacy database the app ignores.

### 10.3 The lockdown migration

`20261001000000_lock_down_public_schema_for_public_deploy.sql` exists because the app was on a public Netlify URL and the publishable key is embedded in the JS bundle, meaning anyone can call PostgREST directly. It:

1. Drops the seven `USING (true)` dev read policies on `public` tables, each of which had self-marked as "temporary development-only … replace before production".
2. Revokes all client grants on every `public` table, sequence, and function, and grants nothing back.
3. Re-asserts RLS on any `public` table missing it, via a `DO` block, so a future migration that forgets `enable row level security` still yields deny-by-default.
4. Removes the legacy tables from the `supabase_realtime` publication.
5. Pins `public.set_updated_at()`'s `search_path` and revokes direct execute on `rls_auto_enable()` if it exists, closing advisor findings.
6. Hardens `authenticate`: single errcode and message, 12-hour TTL, global session prune.
7. Adds `cleanup_expired_sessions()` for pg_cron or an edge worker.

---

## 11. The writer

`firmware/network-master/main.cpp` is the device side: it authenticates with the device token, fetches the latest experiment, and contains a bounded push queue that POSTs to `device_data.push_readings(jsonb)`. The migration `20261016000000_add_push_readings_and_device_tables.sql` grants that RPC and keeps direct `INSERT` on `readings` denied.

What is missing is the enqueue caller. The firmware's Mega poll decodes a sensor round and logs it (`plans-and-audits/migration-plan.md` §6) but does not build the `BottleReading[4]` that `pushEnqueueRound()` expects, so the queue stays dormant. The display still shows fake sensor values (`fake_data.cpp`).

So the write path exists end to end but has nothing feeding it: `scripts/dev-db.ts` remains the only thing that inserts rows today. The schema anticipates the real path — `real` columns, nullable sensors, a time-series primary key — and the RPC is ready for it.

---

## 12. Findings

### 12.1 The experiment list is bounded by the monitoring fetch's row cap (fixed)

Until 2026-11-27 the History list was derived from the Monitoring fetch (the newest 5,000 rows), so on a busy device the oldest experiments disappeared from the list. The list now comes from its own skip-scan query (§5.7) and includes every experiment with readings.

The same change removed `selectActiveExperiment` and `selectMonitoringReadings` from `monitoring.ts`, with their tests. They filtered one mixed array down to the live experiment. The cache now stores each experiment separately, so Monitoring needs no filtering.

### 12.2 The chart heading says "Across experiments" (fixed)

The Monitoring chart shows only the active experiment (its own reading-cache entry, §5.3). The heading now reads `Chamber {n} · {parameter}` (for example `Chamber 1 · CH₄ (ppm)`), so it no longer describes the previous behaviour.

### 12.3 Dead code

| Symbol | Location | Status |
|---|---|---|
| `ReadingWithStatus` | `data.ts:40` | Exported, never imported |
| `Metadata` | `types/domain.ts:1` | Exported, never imported |

`getMetricValue` (`App.tsx:608`) is not dead — `createChartPoints()` calls it on
every row — but it is a one-line pass-through over `getParameterValue` and could
be inlined.

`types/database.ts` also carries 840 lines of generated types for the entire legacy `public` schema that the app no longer reads.

### 12.4 Stale comments and docs

| Claim | Reality |
|---|---|
| `auth.ts` — *"server throttle in `device_data.authenticate` (30 failures / 5 min globally)"* | No such throttle exists. It was dropped because a failed call rolls back its own counter. The comment was corrected on 2026-11-21 to say the backoff is client-side only. |
| `data.ts` handled `42900` "too many attempts" | No migration raises `42900`. The dead branch was removed on 2026-11-21; `authenticate()` now throws one generic "Invalid device token." for every failure. |
| `signOut` comment — *"the session expires on its own within a day"* | TTL is now 12 hours. |


### 12.5 Security posture

Already handled, and worth preserving:

- Session-scoped RLS on `device_data` with exactly two policies.
- `token_hash` unreachable through column-level grants.
- All `SECURITY DEFINER` functions pin `search_path`.
- Single generic error for all bad tokens; no device-existence oracle.
- No plaintext credentials in git; `.env` is untracked and ignored.
- 256-bit session tokens, 12-hour TTL, global expiry prune, owner-only cleanup function.
- No `device_data` table in the Realtime publication.
- No password stretching to attack: SHA-512 is fast, so protection is entirely token entropy plus whatever the edge provides.

Open items:

- **The `authenticate` RPC is unthrottled at the database layer by design.** Security rests on token entropy and an edge rate limit or Turnstile that the checklist requires but which is not evidenced in this repository. Before any public URL, confirm that layer exists.
- **`owner_email` is PII** returned by the RPC, granted as a readable column, and rendered in the UI. Every token holder sees the registered owner's email. Worth an explicit decision.
- **No token rotation UI.** Rotation is a manual `--only token` run or direct SQL.
- **Sessions in localStorage** are readable by any XSS. The CSP is the mitigation; there is no `HttpOnly` alternative, since the token must be readable by JS to set the custom header.

### 12.6 Functional gaps

- **Test coverage is narrow.** `bun test` covers history grouping, monitoring poll eligibility, chamber numbering, Compare selection and row rules, chart interaction (nearest sample, keyboard stepping), CSV output, the reading-cache helpers, and the Supabase reads in `data.ts` against a fake PostgREST server. React rendering remains untested.
- **No per-chamber comparison in Monitoring.** Monitoring shows one selected chamber at a time; the History view is the only place to compare chambers.
- **No realtime**, by design (§5.4), giving up to 5m30s of latency.
- **No error boundary.** A render-time throw blanks the page; only data-fetch errors are caught.
- **No loading state granularity.** One `loading` boolean covers the whole view, so the Monitoring view shows "Loading" rather than a skeleton.

---

## 13. Reference tables

### 13.1 Sensors and units

| Name | Column | Unit | Display digits |
|---|---|---|---|
| pH | `ph` | — | 2 |
| Temp | `temp` | `°C` | 2 |
| CO2 | `co2` | `ppm` | 1 |
| CH4 | `ch4` | `ppm` | 2 |
| Pressure | `pressure` | `Pa` | 2 |

`PARAMETER_NAMES` is the single source of truth. It drives the metric cards, the chart parameter picker, the statistics panel, the CSV columns, and `getParameterValue`'s switch.

### 13.2 Constants

| Constant | Value | Location |
|---|---|---|
| `CONCLUDED_AFTER_MS` | 600,000 (10 min) | `data.ts:61` |
| `POLL_INTERVAL_MS` | 330,000 (5m30s) | `data.ts:64` |
| `EXPERIMENT_WINDOW_MS` | 86,400,000 (24 h) | `data.ts` |
| `MAX_EXPERIMENT_ROWS` | 69,120 (24 chambers × 2,880 rounds of 30 s), safety ceiling | `data.ts` |
| `INCREMENTAL_OVERLAP_MS` | 600,000 (10 min) | `readingCache.ts` |
| `MAX_COMPARE_SERIES` | 4 | `compare.ts` |
| `PAGE_SIZE` | 1,000 | `data.ts:83` |
| `TESTING_EXPERIMENT_ID` | `0` | `types/domain.ts:52` |
| Session TTL | 12 hours | migration `20261001000000` |
| Session token | 64 hex / 256 bits | `sha256(uuid ‖ uuid)` |
| Server `max_rows` | 1,000 | `supabase/config.toml` |
| Exposed schemas | `["device_data"]` | `supabase/config.toml` |
| Chart viewBox | `0 0 {measured width} 260` (200 under 560px) | `chartGeometry()` in `App.tsx` |
| Chart plot band | y ∈ [10, height − 30], x ∈ [56, width − 8] | `chartGeometry()` in `App.tsx` |
| Flat-series y | 110 (only its null-ness is used by the renderers) | `createChartPoints()` in `App.tsx` |
| localStorage key | `rusim.device_session` | `auth.ts:5` |
| Session header | `x-device-session` | `supabase.ts:33` |

### 13.3 Migration timeline

| Migration | Effect |
|---|---|
| `20260927093901` | Creates the legacy `public` MVP schema |
| `20260927095959` | Adds chamber-level telemetry |
| `20260927142738` / `145959` / `160200` | Three `USING (true)` dev read policies on `public` |
| `20260927152135` | Adds legacy tables to the Realtime publication |
| `20260930082137` | Creates the `device_data` schema, `devices` and `readings`; inserts device 0 |
| `20260930162000` | Token auth: `sessions` table, `request_session_token`, `session_device_id`, `authenticate`, `revoke_session`, RLS policies, column-level grants |
| `20260930162100` | Provisions device 0 with a NULL token hash |
| `20260930164500` | Fixes session token generation (the `uuid → bytea` cast bug that blocked every login) |
| `20260930164600` / `164700` | Adds and removes a cross-device isolation fixture; isolation verified |
| `20260930164800` | Clears smoke-test rows and sessions |
| `20260930170000` | Makes `readings` a time series: `experiment_id` out of the primary key, sensors nullable |
| `20260930172000` | Reseeds experiment 0 with disjoint timestamp windows |
| `20261001000000` | Locks down `public`, drops dev policies, removes Realtime, hardens `authenticate` to 12h, adds session cleanup |
| `20261016000000` | Adds `push_readings`, `experiments`, and `device_config` |

### 13.4 The 30-second version

Paste a device token → Postgres SHA-512s it, matches the digest against a stored hash, and returns a 256-bit session token scoped to that one device, valid 12 hours. The browser stores it in localStorage and sends it as `x-device-session` on every request. RLS resolves the header back to a `device_id`, and the query returns that device's rows and nothing else.

The app reads the single newest row, takes its experiment as the most recent one, and loads that experiment's last 24 hours into an in-memory cache. It infers Live versus Concluded purely from whether the newest sample is under 10 minutes old, works out which configured chambers are missing from that sample and calls them disconnected, then renders a chamber presence strip, five KPI cells (which double as the chart parameter tabs), and one hand-built SVG chart for the selected chamber. It re-polls every 5m30s whenever Monitoring is open, reading only rows newer than the cache, and stops polling after any fetch error until Refresh is pressed.

The History tab lists every experiment on the device with a skip scan (one `LIMIT 1` query per experiment id, plus its first and last sample time). Selecting one loads its last 24 hours into the same cache, unless they are already there. It shows a chamber selector, per-parameter charts, a per-chamber results table (mean with min – max), and a CSV export of exactly the rows on screen. Compare draws from the same cache.

The Compare tab overlays up to four (experiment, chamber) series on one chart with a shared y-axis, aligned by sample order. Chambers may come from different experiments; each distinct experiment is fetched once and cached. Series colours are blue, green, amber and violet (`--chart-1` to `--chart-4`) by selection order.

The entire design rests on one distinction it never blurs: **a missing row means a chamber went silent; a null field means a single sensor failed.** Charts break the line, statistics exclude the value, and CSV writes an empty cell — never a zero.

---
Updated at 2026-11-27-11:31.
