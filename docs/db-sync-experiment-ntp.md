# DB experiment sync + NTP (change record)

Backlog #1 (real DB fetch), #9 (experiment number from DB latest+1 with mismatch warn), #12 (NTP sync).

> **Status 2026-11-17:** this records an intermediate step and is superseded by
the shipped code. The firmware no longer takes `SUPABASE_URL`,
`SUPABASE_ANON_KEY` or `DEVICE_TOKEN` as build flags; those values are in
`main.cpp` and only the development device token is used (`network-master.md`
§6.1). The `DB` line now carries the full configuration set, not just
`latest`/`time`/`bottles`/`drops` (`link-serial-protocol.md` §3.2.1), and the
functions named below were renamed: `sbFetchLatestExp` ->
`sbFetchLatestReadingExp`, `sbPushReadings` -> `sbPushOneRound`, and the
periodic driver is now `dbPassTick()`. Read the current state from
`network-master.md` and `link-serial-protocol.md`.

## Auth used

Reuses `device_data.authenticate(p_token)` (`dashboard/supabase/migrations/20260930162000_add_device_data_token_auth.sql`). No schema change. RLS scopes reads to the session device, so the firmware sends no `device_id` filter.

## Master (`firmware/network-master/main.cpp`)

Build flags, never committed: `SUPABASE_URL`, `SUPABASE_ANON_KEY`, `DEVICE_TOKEN`, optional `RUSIM_DEVICE_ID` (default 0, log only; RLS selects the device).

Flow:
- `ensureNtpSync()` after DHCP: `configTime(0, 0, "pool.ntp.org", "time.google.com")`, waits 10 s for `time() > 1700000000`. Sets `ntpSynced`, reported as `time=1` on the `DB` line.
- `sbAuthenticate()`: `POST <url>/rest/v1/rpc/authenticate` with `{"p_token":...}`, extracts the 64-hex `session_token` into RAM. Also keeps `bottle_count`, `owner_name`, `owner_email` from the same reply (bounded parse, no longer discarded). TLS pinned via `setCACert()` (`supabase_ca.h`, 3 roots); no `setInsecure()` fallback on this path.
- `sbFetchLatestExp()`: `GET <url>/rest/v1/readings?select=experiment_id&order=experiment_id.desc&limit=1` with `apikey` + `x-device-session`. Returns latest id, `-1` when empty, `-2` on failure. Re-auths once on 401/403. Same pinned TLS.
- `syncDbLatest()` every 30 s in NEED_CREDS and RUN, forced once after LOGIN. `link_emit_db()` sends one `DB latest=<n|-> time=<0|1> bottles=<n|-> drops=<n>` line (`-` = unknown, `latest=0` = no rows yet, `drops` is a saturating counter), then `sbPushReadings()` drains up to 2 buffered rounds (no-op until the sensor chain enqueues).

Portal verdict still uses the lyrics `fetchData()`; Supabase fetch is the real-data path.

## Link (`rusim-line` v1)

The register map is gone; DB state travels as one `DB` line per heartbeat and
after each DB pass (`link-serial-protocol.md` §3.1). Fields:

| Field | Meaning |
|---|---|
| `latest=` | latest experiment id, `0` = no rows yet, `-` = unknown |
| `time=` | `1` when NTP is synced |
| `bottles=` | `bottle_count` from `authenticate()`, `-` = unknown |
| `drops=` | dropped push rows, always a number |

## Display (`firmware/display-v9/`)

- `link_serial.cpp`: stores `s_dbLatest` (`-2` unknown, `-1` none), `s_timeOk`, `s_dbBottles` (`-1` unknown), `s_pushDrops`; readable via `link_serial_get_db_latest()` / `link_serial_time_ok()` / `link_serial_get_db_bottles()` / `link_serial_get_push_drops()`.
- `screen_control.cpp`: `s_dbWarn` label replaces the static note. `db_refresh_warn()` auto-sets the spinbox to `latest+1` once per boot, then shows `WARN: exp N already in DB (latest M)` when `local <= latest`, else `DB latest M, next M+1 OK`. `screen_control_poll()` (called from `main.cpp` loop) applies late syncs to the open screen.
- `screen_info.cpp`: DB tile shows `latest (Nbottles)`, e.g. `12 (24bt)`, or `--` when never synced.

## Build

`pio run` accepts no positional build flags (tested with PIO 6.1: `Got unexpected extra argument`), so pass them through the `PLATFORMIO_BUILD_FLAGS` env var. String-valued defines must be double-escaped inside the var: PlatformIO splits it with shlex, which would otherwise strip the quotes the C++ `String(...)` macros need. The var is global, so build only the targeted env (`-e network-master`).

```
PLATFORMIO_BUILD_FLAGS='-DSUPABASE_URL=\"https://xyz.supabase.co\" -DSUPABASE_ANON_KEY=\"...\" -DDEVICE_TOKEN=\"...\"' pio run -e network-master -t upload
```

Optional `-DRUSIM_DEVICE_ID=n` (integer, no quote escaping) pins a device id; RLS selects the device from the token regardless.

Without flags every DB pass aborts with `[DB] Supabase not configured (SUPABASE_URL/ANON_KEY/DEVICE_TOKEN empty)` and the display blocks run starts with the `DB configuration not loaded` modal; portal login + experiment state still work.

---
Updated at 2026-11-18-00:12.
