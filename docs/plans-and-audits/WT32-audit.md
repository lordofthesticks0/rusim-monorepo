# WT32 Database Authentication & Startup Latency Audit

Scope: investigation of why `firmware/network-master/main.cpp` (WT32-ETH01) can take up to ~5 minutes to authenticate with Supabase and reach `ready=1`, blocking experiment starts on `display-v9`.

## 1. Symptom and Display Gate

- The display blocks starting an experiment until `link_serial_db_ready()` is true (`firmware/display-v9/ui_shell.cpp` L177, L725–L734).
- `link_serial_db_ready()` becomes true only when the display receives a `DB ... ready=1 ...` line from the WT32 (`firmware/display-v9/link_serial.cpp` L258–L261).
- On the WT32, `dbConfigReady` is set to `true` only at the end of `DB_PASS_LATEST` (`firmware/network-master/main.cpp` L2662) and reported in `dbPassFinish()` via `link_emit_db()` (L2542–L2545).

A single healthy pass takes seconds. Reaching ~5 minutes means multiple cycles are failing and retrying, with each failed cycle costing ~40–60 seconds.

## 2. Startup Path (Boot to `ready=1`)

1. `setup()` delays 1 s (L2798) and starts Ethernet (`ETH.begin()`).
2. `STAGE_WAIT_IP` waits for DHCP (`eth_connected`), then enters `STAGE_NEED_CREDS` (L2886–L2895).
3. `STAGE_NEED_CREDS` probes for an existing portal session or waits for web login, then enters `STAGE_RUN` (L2897–L2952).
4. `STAGE_RUN` runs `Ping.ping(1.1.1.1, 3)` and `probeOnline()`, then sets `dbPassRequested = true` (L2969–L2992).
5. `dbPassTick()` runs `ensureNtpSync()`, then five sequential HTTPS requests—`DB_PASS_AUTH`, `DB_PASS_DEVICES`, `DB_PASS_CONFIG`, `DB_PASS_EXPERIMENTS`, `DB_PASS_LATEST`—one per `loop()` iteration (L2559–L2715).
6. `dbPassFinish()` emits `DB ... ready=1` and records `netCycleEndMs = millis()` (L2542–L2545).

## 3. Root Causes

### 3.1 30-second cooldown on every startup failure
- `STAGE_RUN` gates every cycle on `kIntervalMs = 30000` ms measured from the end of the previous pass (L70, L2964), even while `dbConfigReady == false`.
- Any failure during NTP sync or any of the five HTTPS calls aborts the pass via `dbPassFinish()` and forces a full 30 s wait before retrying. Five to six failed cycles easily reach 4–5 minutes.

### 3.2 Late, blocking NTP sync that resets SNTP on every retry
- `sbAuthenticateImpl()` refuses to run while `!ntpSynced` (L2033).
- `configTime(0, 0, "pool.ntp.org", "time.google.com")` is not started on `ARDUINO_EVENT_ETH_GOT_IP`; it is called for the first time inside `ensureNtpSync()` during `DB_PASS_IDLE` (L1377–L1397, L2568).
- `ensureNtpSync()` blocks in a 1 s polling loop for at most 10 s. LwIP SNTP waits ~15 s before failing over from `pool.ntp.org` to `time.google.com`. If `pool.ntp.org` is slow or drops UDP/123, `ensureNtpSync()` times out at 10 s, aborts the pass (`DB_FAULT_CLOCK`), waits 30 s (`kIntervalMs`), and calls `configTime()` again—restarting SNTP from `pool.ntp.org`.

### 3.3 Five all-or-nothing cold-TLS HTTPS calls before `ready=1`
- `dbRequest()` creates a fresh local `WiFiClientSecure` and calls `setCACert(kSupabaseCaPem)` (parsing 3 PEM roots from `firmware/network-master/supabase_ca.h`) on every request (L1918–L1968). There is no TLS session reuse or HTTP keep-alive.
- `dbConfigReady` is not set until all five sequential requests (`rpc/authenticate`, `devices`, `device_config`, `experiments`, `readings`) succeed in a single unbroken pass.
- Between steps, `sens_poll()` runs every 5 s and can block up to `kSensReplyMs = 1500` ms if the Mega slave times out (L259, L1775).
- If any of the five requests fails or hits `http.setTimeout(10000)` (10 s), the pass aborts and sleeps for 30 s.

### 3.4 Pre-pass delays and redundant portal probes
- **DHCP-to-probe idle gap**: `STAGE_WAIT_IP` sets `stageAtMs = 0` intending an immediate tick (L2890), but `STAGE_NEED_CREDS` checks `millis() - stageAtMs >= kCredPollMs` (`5000` ms, L2907). If DHCP finishes before 5 s uptime, the board idles until `millis() == 5000`.
- **Duplicate 204 probes**: On an already-open network, `STAGE_NEED_CREDS` calls `fetchPortalUrl()` and immediately calls `probeOnline()` on the same URL (`http://connectivitycheck.gstatic.com/generate_204`, L2917–L2918). Upon entering `STAGE_RUN`, it runs `Ping.ping(1.1.1.1, 3)` (L2969) and calls `probeOnline()` a third time (L2982) before requesting the DB pass.
- **Single-probe fallback to `STAGE_NEED_CREDS`**: One transient failure of `probeOnline()` in `STAGE_RUN` drops the board back to `STAGE_NEED_CREDS` (L2982–L2986) without resetting `lastAuthProbeMs`, delaying auto-recovery by up to `kAuthProbeMs = 30000` ms (L209, L2912).

### 3.5 Potential socket-timeout stall on chunked responses
- In `readBodyCapped()` (L1868–L1890), when a response omits `Content-Length` (`http.getSize() < 0`, e.g., `Transfer-Encoding: chunked`), the loop reads from `stream->readBytes()` until `want` (4095 bytes) is filled or `http.connected()` becomes false. If the server delays closing the connection, `readBytes()` can stall up to `http.setTimeout(10000)` (10 s).

## 4. Verification on Hardware

Flash `[env:network-master-metrics]` (`-D RUSIM_METRICS=1`, see [verification-toolkit.md](../verification-toolkit.md)) and capture the serial log from boot until `ready=1`:
- `MET BOOT stage=got_ip|portal_ok|ntp|auth|ready ms=...` identifies which stage gap dominates.
- `MET HTTP op=auth|devices|device_config|experiments|latest code=... ms=... tls_ms=...` separates TLS handshake time (`tls_ms`) from response read time (`ms - tls_ms`).
- `MET AUTH why=initial ok=... ms=... fault=...` and `MET PASS ms=... fault=... ready=...` show retry counts and fault classes.

## 5. Recommended Fixes

1. Start `configTime()` once on `ARDUINO_EVENT_ETH_GOT_IP` (or first online state) and do not call `configTime()` again on retry; check `time(nullptr) > 1700000000` without a 10 s blocking loop.
2. Use a short retry interval (e.g., 3–5 s) while `!dbConfigReady`, keeping `kIntervalMs = 30000` ms only for steady-state polling after `dbConfigReady == true`.
3. Fix the `stageAtMs = 0` initial tick in `STAGE_NEED_CREDS`, eliminate redundant `probeOnline()` calls, and skip `Ping.ping()` before the first DB pass after portal auth succeeds.
4. Require consecutive `probeOnline()` failures (or a short retry) before dropping from `STAGE_RUN` back to `STAGE_NEED_CREDS`.
5. Reuse a persistent `WiFiClientSecure` (with keep-alive / session reuse) across the requests in a single DB pass to avoid five back-to-back cold TLS handshakes.

---
Updated at 2026-11-25-19:50.
