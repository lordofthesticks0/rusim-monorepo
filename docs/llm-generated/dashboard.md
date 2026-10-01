# Dashboard — full walkthrough

**Scope:** `dashboard/` in its entirety — application source, Supabase schema, migrations, dev tooling, and deploy config.
**Method:** direct read of every source file, migration, and config in `dashboard/`, plus a grep of `firmware/` to check for any writer. No secrets read, no live database queries.

This document explains what the dashboard is, what it does from page load to sign-out, how it authenticates, how it reads and shapes data, and where the implementation diverges from what its own comments claim. Findings and bugs are collected in §12.


## 1. What it is

`dashboard/` is a single-page React application (Vite + React 19 + TypeScript) that lets an operator holding a device token watch rumen-simulation sensor telemetry from the Supabase database.

It is **read-only**. There is no write path in the application. It talks to Supabase over PostgREST and RPC only.

There is no UI framework, no state library, and no chart library. The chart is hand-built SVG path data computed in a 20-line function. The whole client is about 1,100 lines across five modules.

### 1.1 File map

| File | Lines | Responsibility |
|---|---|---|
| `index.tsx` | 5 | Mounts `<App/>` into `#root`, imports the stylesheet |
| `src/App.tsx` | 537 | Everything visible: login screen, both views, all components, all chart math |
| `src/data.ts` | 305 | Every Supabase read, the auth RPCs, and all pure domain logic |
| `src/auth.ts` | 110 | Session lifecycle: localStorage persistence, expiry timer, retry backoff |
| `src/supabase.ts` | 62 | Client construction, schema pinning, session header injection |
| `src/csv.ts` | 90 | RFC 4180 CSV serialisation and download |
| `src/styles.css` | 53 | All styling, one declaration block per line |
| `scripts/dev-db.ts` | 694 | Dev-only database seeding and token provisioning |
| `supabase/migrations/*.sql` | 16 files | Schema, authentication, lockdown |
| `netlify.toml` | 23 | Build config and security headers |
| `src/types/domain.ts` | 56 | Hand-written domain types and experiment-id helpers |
| `src/types/database.ts` | 840 | Supabase-generated schema types for `device_data` and legacy `public` |

### 1.2 What it is not

- **Not a device control surface.** There is no write RPC, no command queue, no configuration UI. It observes.
- **Not realtime.** No websocket, no subscription. See §5.4.
- **Not fed by firmware.** See §11.

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

`src/supabase.ts` **throws at module load** if either `VITE_*` variable is missing. This is a hard crash of the whole bundle, not a rendered error state.

### 2.2 The static host

`netlify.toml` builds with `bun run build`, publishes `dist`, and rewrites `/*` to `/index.html` with status 200 for client-side routing. There is no router in the app, so this is defensive only.

---

## 3. The database

The application reads exactly one PostgreSQL schema: **`device_data`**. The client pins itself to it (`db: { schema: "device_data" }`) and `supabase/config.toml` exposes only it through PostgREST.

A second, much richer schema, `public`, exists from an earlier design and is retained for rollback. The application does not read it. See §10.

### 3.1 `device_data.devices`

| Column | Type | Rules and meaning |
|---|---|---|
| `device_id` | `integer` | Primary key, `CHECK (device_id >= 0)`. Factory assigned. `0` is the development device; production numbering starts at `1`. |
| `bottle_count` | `integer` | Required, no range check. How many bottles the UI offers in its selector. Dev device: 24. |
| `owner_name` | `text` | Nullable. Rendered in the UI. |
| `owner_email` | `text` | Nullable. Rendered in the UI. |
| `token_hash` | `text` | Nullable, `UNIQUE`, constrained to `NULL` or exactly 128 lowercase hex characters. |

`token_hash` holds only the SHA-512 hex digest of the device token. A `NULL` means the device has no token and therefore cannot be logged into at all — an empty string is not a valid substitute, because it fails the check constraint.

Device 0 is provisioned by migration `20260930162100` with `bottle_count = 24`, `owner_name = 'RUSIM team'`, and `owner_email = 'example@example.com'`, deliberately leaving `token_hash` NULL. The `ON CONFLICT DO UPDATE` clause preserves any hash already applied by `bun run dev-db`, so applying the migration cannot silently lock the device out.

### 3.2 `device_data.readings`

| Column | Type | Rules and meaning |
|---|---|---|
| `device_id` | `integer` | FK → `devices.device_id` |
| `bottle_id` | `integer` | Required, no range check. Convention is `0`–`23` for 24 bottles. |
| `experiment_id` | `integer` | Required, `CHECK (>= 0)`. `0` is the testing experiment. **Not in the primary key.** |
| `timestamp` | `timestamptz` | Required, timezone-aware. **In the primary key.** |
| `ph`, `pressure`, `temp`, `co2`, `ch4` | `real` | Nullable four-byte floats. |

**Primary key: `(device_id, bottle_id, timestamp)`.**

This is the single most consequential schema decision, changed by `20260930170000_make_readings_a_time_series.sql`. The previous key was `(device_id, bottle_id, experiment_id)`, which permitted exactly one row per bottle per experiment and made repeated sampling within an experiment impossible. Dropping `experiment_id` from the key turns the table into a genuine time series.

`experiment_id` itself is unchanged: still `NOT NULL`, still a grouping column, still reserving `0` for the testing experiment. Only its role in the key was removed.

A direct consequence, stated in the migration: **one bottle cannot hold the same timestamp in two experiments.** A conflicting insert fails loudly on the primary key rather than overwriting. The rationale is that a device cannot genuinely report two experiments for one bottle at the same instant.

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
| **No row** at a timestamp | The bottle sent nothing at all — it is offline | `splitBottlesByPresence` → "Disconnected" card, disconnection banner |
| **Row present, field `NULL`** | The bottle reported, but that one sensor produced no value | `-` in the metric card, a break in the chart line, an empty CSV field |

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

### 4.5 Client-side rate limiting

There is **no server-side throttle**, by design. The lockdown migration explains why a database counter cannot work: a failed `authenticate` raises, which rolls back its own transaction, so any counter written inside the function would never accumulate.

The intended layers are:

1. **Client backoff** in `auth.ts` — module-scope `consecutiveFailures` and `lastAttemptAt`. After two consecutive failures, the next attempt waits `min(1000 × 2^(failures-1), 5000)` ms minus time already elapsed. This spaces out accidental rapid resubmits, nothing more.
2. **Edge rate limit or Turnstile** on the login path, which the migration names as a deployment responsibility and the security report lists as a pre-public checklist item.

Security therefore rests entirely on token entropy. The stated guidance is to issue only high-entropy random tokens of at least 128 bits (`openssl rand -hex 32`) and never human-memorable passwords.

---

## 5. The application, from load to sign-out

### 5.1 Page load

`index.html` loads `/index.tsx`, which calls `createRoot(...).render(<App/>)`.

`App()` calls `useAuth()`, whose `useState` initialiser calls `readStoredSession()` **synchronously**. It parses `localStorage["rusim.device_session"]`, returns null if the token is not a string or `expiresAt` is in the past, and otherwise returns the stored session. A valid stored session therefore renders the dashboard with no network call and no loading flash.

### 5.2 Session expiry timer

`useAuth` sets a `setTimeout` for exactly the remaining lifetime. On fire it clears localStorage, nulls the session, and sets the message "Your session expired. Sign in again to continue." The operator is never left looking at a dead dashboard.

### 5.3 First data load

`Dashboard` mounts and `loadData()` runs once, from a `useEffect` keyed on the `useCallback`-memoised `loadData` (memoised on `sessionToken`).

`fetchReadings` pages through PostgREST:

```js
for (let offset = 0; offset < MAX_READING_ROWS; offset += PAGE_SIZE) {
  .from("readings")
  .select("device_id, bottle_id, experiment_id, timestamp, ph, pressure, temp, co2, ch4")
  .order("timestamp", { ascending: false })
  .range(offset, Math.min(offset + PAGE_SIZE - 1, MAX_READING_ROWS - 1))
  if (data.length < PAGE_SIZE) break;
}
```

| Constant | Value | Reasoning in the source |
|---|---|---|
| `PAGE_SIZE` | 1000 | *"Must stay at or below the server's `max_rows` setting, otherwise every page returns a truncated response and the paging loop misreads the last one as the end of the data."* |
| `MAX_READING_ROWS` | 5000 | Client-side ceiling. Callers hitting it receive a partial result, so anything presenting itself as complete must check and say so. |

`config.toml` sets `max_rows = 1000`, matching `PAGE_SIZE` exactly.

There is **no device filter in the query**. `fetchExperimentReadings` documents that RLS already restricts to the session's own device, so adding `.eq("device_id", ...)` would be redundant.

### 5.4 Polling

```js
useEffect(() => {
  if (view !== "monitoring" || pollSuspended || !liveExperiment) return;
  const timer = window.setInterval(() => void loadData(), POLL_INTERVAL_MS);
  return () => window.clearInterval(timer);
}, [view, pollSuspended, liveExperiment?.experiment_id, loadData]);
```

`POLL_INTERVAL_MS` is 5 minutes 30 seconds — deliberately longer than the 10-minute staleness threshold, so each tick re-decides from fresh data rather than assuming continuity.

Polling stops when: the view is not Monitoring, no experiment looks live, or a fetch has failed. `pollSuspended` is set on any error and cleared only on the next success, so a transient failure halts the live feed until the operator clicks "Refresh data". The inline comment frames this as intent: *"A failed fetch is treated as the end of the live feed, so polling stops until the operator refreshes by hand."*

**Realtime is deliberately unused.** The `x-device-session` custom header cannot be sent over a websocket, so a subscription could not be scoped to a device. Migration `20261001000000` explicitly removed the legacy `public` tables from the `supabase_realtime` publication to match.

The consequence is a worst-case latency of 5m30s for a device that samples every 5 minutes.

### 5.5 The Monitoring view

The default view, reached by the "Monitoring" nav tab.

**Top bar** — connection status (`Loading` / `Unable to load` / `Data loaded` / `No readings`), the latest sample's localised timestamp, `Auto-refreshing` or `No live experiment`, and a sign-out link. The green `.pulse` dot beside the status is a static CSS colour and does not reflect state.

**Bottle selector** — a `<select>` with `bottleCount` options, defaulting to `Bottle 0`. This is the only client-side drill-down control in the whole app.

**Device identity strip** — `Device {id} | {owner_name} ({owner_email})`, with "Unnamed owner" when null.

**Four status cards** — Bottle, Samples (that bottle's row count), Latest sample, and Connection. Connection resolves in priority order: `Disconnected` if the bottle is absent from the newest sample, else `Concluded` if the newest sample is older than 10 minutes, else `Live`, else `No data`.

**Disconnection banner** — rendered only when a latest sample exists *and* at least one configured bottle is missing from it. With the default dev configuration (24 configured, 8 reporting) this reads: "Disconnected at …: bottles Bottle 8, Bottle 9, … Bottle 23 sent no data."

**Five metric cards** — pH, Temp, CO2, CH4, Pressure, read from the selected bottle's newest sample. Decimal places come from `PARAMETER_DIGITS`: pH 2, Temp 2, CO2 1, CH4 2, Pressure 2. A `null` renders as `-` with no unit and no "Latest" tag.

**One chart** — with a parameter picker defaulting to CH4, plotting the selected bottle's **entire** history across all experiments, which is why the panel heading reads "Across experiments". Fewer than two rows renders "Not enough data for chart."

### 5.6 Live vs concluded is inferred, never stored

There is no `experiments` table and no status column. `concluded` is derived purely from sample time:

```js
export const CONCLUDED_AFTER_MS = 10 * 60 * 1000;
export function isConcluded(timestamp, now = Date.now()) {
  const sampled = Date.parse(timestamp);
  if (Number.isNaN(sampled)) return true;   // an unparseable timestamp is treated as concluded
  return now - sampled > CONCLUDED_AFTER_MS;
}
```

`summarizeExperiments` groups rows by `experiment_id` and flips `concluded` back to false if *any* sample in the group is fresh. So an experiment is live while any part of it is still reporting.

Summaries are sorted by `experiment_id` **descending**, which puts the testing experiment (0) last.

### 5.7 The Experiment History view

Reached by the "Experiment History" nav tab. Two-stage drill-down.

**Stage 1 — the list.** Built from the *same* in-memory `readings` array as the Monitoring view, not a separate query. Each item shows the formatted experiment id, its reading count, a `Testing experiment` marker when the id is 0, Concluded/Live, and the last sample time.

Selection defaults to the newest: `selectedExperimentId ?? experiments[0]?.experiment_id ?? null`.

**Stage 2 — the detail.** A `useEffect` keyed on `activeExperimentId` fires `fetchExperimentReadings`, which pages identically but adds `.eq("experiment_id", id)` and orders by `bottle_id` ascending then `timestamp` ascending. A `cancelled` closure flag prevents state updates from a superseded request or an unmounted component.

That ordering is load-bearing, and the comment says so: it keeps a multi-page read contiguous, so each bottle's samples stay together.

**"Bottle Readings" sub-panel** — five metric cards and five per-parameter charts in a grid.

**"Results" sub-panel** — three sections: a four-card overview (Experiment, Status, Bottles, Readings), a per-parameter Min/Avg/Max panel across bottles, and a per-bottle card list.

**CSV download** — see §8.

### 5.8 Truncation is surfaced, not hidden

`isTruncated(readings.length)` compares against `MAX_READING_ROWS`. When a read hit the cap, the detail pane shows: *"Only the most recent 5,000 readings were loaded, so this experiment is incomplete on screen and any CSV export is truncated too."*

The export is deliberately of what is on screen rather than a fresh query, and the comment explains the tradeoff: *"This exports what is already on screen rather than re-querying, so the file and the charts always describe the same rows. A truncated read therefore produces a truncated file, which is why the limit is called out on screen next to the button."*

---

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

```jsx
<div className="gridlines"><i /><i /><i /><i /></div>
<svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label={`${parameterLabel(parameter)} reading chart`}>
  <path d={chartPath(points)} />
</svg>
<div className="axis">{labels.map(...)}</div>
```

Four dashed horizontal gridlines, the SVG absolutely positioned over them, and `vector-effect: non-scaling-stroke` keeping the 3px stroke constant despite the deliberately non-uniform `preserveAspectRatio="none"`.

`createTimeLabels` picks five evenly-spaced **indexes** and renders `HH:MM` for each, so the axis reads as a time axis while actually being a sample axis.

### 6.4 What the chart is not

No y-axis, no value labels, no gridline values, no tooltips, no zoom, no point markers, no crosshair. The only way to read a value off a chart is the metric cards.

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

`computeBottleStats` is intended to align charts and tables by bottle, but as implemented it does not reduce; see §12.1.

---

## 8. CSV export

`src/csv.ts` produces RFC 4180 output.

**Columns:** `bottle_id, timestamp, pH, Temp (°C), CO2 (ppm), CH4 (ppm), Pressure (kPa)`.

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

Sensor values get a **stable per-bottle offset** (`bottle_id * 0.11` for pH, `* 0.28` for pressure, and so on) so bottles remain visually distinguishable across samples instead of being pure noise, plus a configurable `null_rate` (default 0.0005) to exercise the null-rendering paths.

The script also reports what landed: row count, bottles reporting, first and last sample in UTC, null counts for pH and methane, and a list of configured-but-never-reporting bottles — described as what "the dashboard reads as 'disconnected'".

### 9.3 Why a script and not a migration

Documented in both the script header and the seed file: `supabase db push` applies pending migrations once and records them, so a second run would be a no-op. Regenerating on demand needs a direct connection. Keeping a second copy of the same SQL in a `.sql` file *"would only let the two drift apart."*

### 9.4 Safety guards

The script **deletes and rewrites data**, so it refuses any non-loopback host without an explicit `--allow-remote`. TLS is inferred from the hostname — Supabase requires it, a local `supabase start` does not — with `--tls` / `--no-tls` overrides.

It runs with no flags it prompts for every setting through readline, reading via the `line` event rather than `readline.question()` because the latter never settles once the stream ends, which made piped input hang. Out-of-range or non-integer answers are rejected rather than clamped, *"so a typo cannot quietly produce surprising data."*

### 9.5 Initial data

Migration `20260930172000` populates experiment 0 (24 samples, 5-minute gap, 8 of 24 bottles, `null_rate` 0.0005) with the same timestamp logic inlined, so a `supabase db reset` still produces a dashboard with data in it.

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
| `Content-Security-Policy` | `default-src 'self'`; `script-src 'self'`; `connect-src 'self' https://*.supabase.co wss://*.supabase.co`; `object-src 'none'`; `base-uri 'self'`; `frame-ancestors 'none'` | The important one |

The stated motivation: session tokens live in localStorage, so any XSS equals session theft. The CSP permits only self-hosted scripts and only Supabase as a connect target.

`style-src 'unsafe-inline'` is the one relaxation, needed because the stylesheet is a single imported file with inline style attributes on chart elements.

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

## 11. The missing writer

A grep of `firmware/` for `supabase`, `postgrest`, `readings`, `device_data`, and `https://` returns **no database integration**. The firmware targets are sensor calibration sketches and a display/UI project.

**Nothing in this repository writes readings except `scripts/dev-db.ts`.** The dashboard is fully built on the read side, but every row it displays is synthetic.

This is consistent with the unchecked root README item *"Integrate to data collection system"*, and with the `device_data.readings` design: `real` columns, nullable sensors, a real time-series primary key, and comments anticipating that *"a device can report that a sensor produced no value"* all describe an expected hardware ingest path that does not exist yet.

---

## 12. Findings

### 12.1 `computeBottleStats` is misnamed and mislabelled

Its doc comment reads *"Pairs each bottle with its reading, so charts and tables can align by bottle."* The implementation does not reduce:

```js
function computeBottleStats(readings) {
  return [...readings]
    .sort((left, right) => left.bottle_id - right.bottle_id)
    .map((reading) => ({ bottle_id: reading.bottle_id, reading }));
}
```

`bottleStats.length` is a **row count, not a bottle count**. `ExperimentResultsView` then renders:

- the overview card as `Bottles: {bottleStats.length}`
- the parameter summary tag as `{bottleStats.length} bottles`
- the bottle-results tag on every `BottleChart` as `{readings.length} bottles`

With the default dev configuration of 24 samples across 8 bottles, the Results tab reads **"Bottles 192"** and tags the charts **"192 bottles"** when there are 8.

The "Bottle results" section likewise renders 192 cards for an 8-bottle experiment, 191 of them stale intermediate samples. Correct behaviour needs a reduction keeping the last row per `bottle_id`.

### 12.2 The Results "per bottle" charts are not per bottle

`BottleChart` receives all of `readings` and passes them straight to `Chart`. Because the read is ordered `bottle_id ASC, timestamp ASC`, the x-axis is a composite walk — bottle 0 across all its samples, then bottle 1 across all of its samples, and so on — with time labels drawn from five index positions in that composite order.

The panel headings read "Across bottles", so the intent is per-bottle comparison, but the rendering concatenates bottles instead of comparing them.

### 12.3 History metric cards show the wrong sample

The "Bottle Readings" sub-panel renders its five metric cards from `readings[0]`. With `bottle_id ASC, timestamp ASC` ordering, that is the **oldest sample of bottle 0**, not a latest value. It should be the newest sample of the highest bottle, or a per-bottle latest.

### 12.4 The experiment list is bounded by the monitoring fetch's row cap

The History list is derived from the same in-memory `readings` array, which is capped at 5,000 rows ordered newest-first. On a device with more than 5,000 readings, the **oldest experiments silently vanish from the list** — including ones that would otherwise be selectable.

The truncation notice only appears in the *detail* pane, so a vanished experiment is never explained. Only the Monitoring view has a per-experiment path to the full data, and it is the path that cannot reach the older rows.

### 12.5 The status dot does not reflect state

The `.pulse` element beside the connection status is a hardcoded green in `styles.css`. It is purely decorative, yet it sits immediately left of a label that *does* reflect state (`Unable to load` next to a green "live" dot).

### 12.6 Dead code

| Symbol | Location | Status |
|---|---|---|
| `fetchDevice` | `data.ts:147` | Exported, never imported |
| `ReadingWithStatus` | `data.ts:36` | Exported, never imported |
| `Metadata` | `types/domain.ts:1` | Exported, never imported |
| `getMetricValue` | `App.tsx:460` | Pure pass-through wrapper over `getParameterValue` |
| `.session-list`, `.session-item` | `styles.css:24-27` | No matching JSX |
| `.experiment-status-card`, `.status-label`, `.experiment-code`, `.experiment-name` | `styles.css:6-10` | No matching JSX |
| `.line-secondary` | `styles.css` | No matching JSX |

`types/database.ts` also carries 840 lines of generated types for the entire legacy `public` schema that the app no longer reads.

### 12.7 Stale comments and docs

| Claim | Reality |
|---|---|
| `auth.ts:80` — *"server throttle in `device_data.authenticate` (30 failures / 5 min globally)"* | No such throttle exists. It was dropped because a failed call rolls back its own counter. |
| `data.ts:113` — handles `42900` "too many attempts" | No migration raises `42900`. The branch is dead. |
| `signOut` comment — *"the session expires on its own within a day"* | TTL is now 12 hours. |
| `docs/llm-generated/database.md` — "Currently set to 24 hours after login" | 12 hours. |
| `docs/llm-generated/database.md` — "exposes `public`, `graphql_public`, and `device_data` through the API" | `config.toml` now lists only `device_data`. |
| `docs/llm-generated/database.md` — "The legacy `public` tables still carry their original broad read policies" | Dropped by the lockdown migration. |

### 12.8 Security posture

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

### 12.9 Functional gaps

- **No tests.** No test runner, no test files, no `test` script. The pure functions in `data.ts` and `csv.ts` are well-shaped for unit tests but untested.
- **No per-bottle comparison in Monitoring.** The only cross-bottle view is the history charts described in §12.2.
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
| Pressure | `pressure` | `kPa` | 2 |

`PARAMETER_NAMES` is the single source of truth. It drives the metric cards, the chart parameter picker, the statistics panel, the CSV columns, and `getParameterValue`'s switch.

### 13.2 Constants

| Constant | Value | Location |
|---|---|---|
| `CONCLUDED_AFTER_MS` | 600,000 (10 min) | `data.ts:57` |
| `POLL_INTERVAL_MS` | 330,000 (5m30s) | `data.ts:60` |
| `MAX_READING_ROWS` | 5,000 | `data.ts:70` |
| `PAGE_SIZE` | 1,000 | `data.ts:79` |
| `TESTING_EXPERIMENT_ID` | `0` | `types/domain.ts:48` |
| Session TTL | 12 hours | migration `20261001000000` |
| Session token | 64 hex / 256 bits | `sha256(uuid ‖ uuid)` |
| Server `max_rows` | 1,000 | `supabase/config.toml` |
| Exposed schemas | `["device_data"]` | `supabase/config.toml` |
| Chart viewBox | `0 0 800 220` | `App.tsx:439` |
| Chart plot band | y ∈ [30, 190] | `App.tsx:491` |
| Flat-series y | 110 | `App.tsx:491` |
| localStorage key | `rusim.device_session` | `auth.ts:5` |
| Session header | `x-device-session` | `supabase.ts:26` |

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

### 13.4 The 30-second version

Paste a device token → Postgres SHA-512s it, matches the digest against a stored hash, and returns a 256-bit session token scoped to that one device, valid 12 hours. The browser stores it in localStorage and sends it as `x-device-session` on every request. RLS resolves the header back to a `device_id`, and the query returns that device's rows and nothing else.

The app pulls up to 5,000 rows newest-first, derives an experiment list by grouping on `experiment_id` — there is no experiments table — infers Live versus Concluded purely from whether the newest sample is under 10 minutes old, works out which configured bottles are missing from the newest sample and calls them disconnected, then renders four status cards, five metric cards, and one hand-built SVG chart for the selected bottle. It re-polls every 5m30s only while an experiment looks live, and halts permanently on any fetch error until Refresh is pressed.

The History tab reuses the same in-memory array to list experiments, then issues a second paged read for the selected one and shows per-parameter charts, cross-bottle min/average/max, and a CSV export of exactly the rows on screen.

The entire design rests on one distinction it never blurs: **a missing row means a bottle went silent; a null field means a single sensor failed.** Charts break the line, statistics exclude the value, and CSV writes an empty cell — never a zero.

---
Content generated by `stealth/space-bunny-alpha` using `opencode`.
