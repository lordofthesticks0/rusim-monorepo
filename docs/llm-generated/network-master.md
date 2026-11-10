# network-master (`firmware/network-master`) â€” production WT32 firmware

Scope: the WT32-ETH01 firmware that ships. It brings up wired Ethernet,
takes the captive portal username/password from the display over the
`rusim-line` serial link, logs in to the portal, then holds the connection up
with ping plus an HTTPS data fetch. Re-authentication reuses the same display
path; there is no serial-console credential entry.

`link-serial-protocol.md` is the wire specification. `display-docs.md` Â§10
documents the display end of the same link.
`captive-portal-workaround.md` documents the portal relay plan.

## 1. Environment

| Property | Value |
|---|---|
| Environment | `[env:network-master]` (production) |
| Board | `wt32-eth01` |
| Platform / framework | `espressif32` / `arduino` |
| `monitor_speed` | 115200 |
| Libraries | `dvarrel/ESPping@^1.0.5` |
| Source | `firmware/network-master/main.cpp` |

Build and flash:

```bash
pio run -e network-master
pio run -e network-master -t upload
pio device monitor -e network-master
```

## 2. Wiring

### 2.1 Display link (UART0)

| WT32 pin | Display pin | Direction |
|---|---|---|
| Debug-header TXD (IO1), UART0 TX | IO18 (RX) | WT32 -> display |
| Debug-header RXD (IO3), UART0 RX | IO17 (TX) | display -> WT32 |
| GND | GND | common, wired directly |

Settings: 115200 8N1. `Serial` is both the link and the log sink. The WT32
prints every diagnostic as a `LOG <text>` line and the display forwards it to
its own USB console; nothing else may write raw text to `Serial`.

`IO2` remains an optional bench LED (external LED + 1k to GND) that blinks on
any line received from the display. The `IO11 -> IO14` poll-request wire and
its ISR are gone.

If no lines arrive, swap the two data wires first, then check the direct GND.
Do not rely on shared USB ground. `IO1`/`IO3` carry no boot strapping, so
power-up order does not matter; a UART transmitter idles HIGH.

### 2.2 Mega slave link (UART1)

Direct TTL, no MAX485: the legacy Mega answers on its own `Serial1`, and the
link needs no direction control.

| Signal | WT32 pin | Mega pin | Direction |
|---|---|---|---|
| Data | IO5 (UART1 RX) | D18 (TX1) | Mega -> WT32 |
| Data | IO17 (UART1 TX) | D19 (RX1) | WT32 -> Mega |
| Ground | GND | GND | common |

Settings: 9600 8N1, matching `Serial1.begin(9600)` in
`legacy/slave/new-slave.cpp`. `Serial1.begin(9600, SERIAL_8N1, 5, 17)` remaps
the pins, because the ESP32 UART1 defaults (`IO9`/`IO10`) are flash pins on
this module. `IO14` stays spare.

Electrical warning: the Mega drives D18 at 5 V and the WT32 pins are 3.3 V
only, so D18 -> IO5 needs a divider (2k2/3k3) or a level shifter. IO17 -> D19
at 3.3 V sits close to the ATmega's worst-case `VIH` (about 3.0 V at a 5 V
supply); it works in practice but is not guaranteed. The common ground is
mandatory; two USB grounds are not a substitute.

### 2.3 Mega slave poll behaviour

- `sens_poll()` is called from `loop()` before the stage switch, so it runs in
every state, including while the portal gate is open. One round per 30 s
(`kSensPollMs`).
- Each round drains stale RX bytes, writes `#1:PING*` (8 bytes), and reads
until `\n`, a full buffer, or 1500 ms (`kSensReplyMs`: 50 ms capture window
plus about 310 ms of line time for a ~500-byte reply). A frame that does not
begin with `{` is rejected before parsing.
- The frame is one fixed `char sensRx[513]`. No `String`, no JSON library: the
shape is fixed and flash is tight. The parser locates `"id":`, then `"ph":`,
`"methane":`, `"temp":`, `"co2":` and `"press":` with `strstr` + `strtof`,
bounded to one bottle object and to the buffer's NUL. Ids outside 1-4 are
ignored.
- The slave's invalid markers become `nullMask` bits instead of numbers:
`ph`/`methane`/`co2`/`press` `-1` and `temp` `-999`, mapped to bits 0/4/3/1/2
the same way `PushRow` does. `ph` is pH units: the slave applies its per-bottle
3-point calibration, so no table exists in this firmware.
- One `LOG [SENS]` line per round, for example
`LOG [SENS] 4 bottles ok=20 fail=0 b1 ph=2.51 ch4=812.4 t=38.5 co2=1204 p=1013 ...`.
`ok=` counts accepted values, `fail=` counts bottle slots with no object in
the frame. Failed rounds (timeout, overlong frame, non-JSON) are logged on the
first failure and then every 20th, so a disconnected Mega cannot flood the
display's log.
- Every accepted frame is enqueued while an experiment run is active. The
round keeps explicit bottle ids (Mega 1-4 -> DB 0-3) so a missing bottle is a
hole and never a renumbered or fabricated sample. A present bottle with an
unavailable sensor still contributes a row with `null` fields.
- Enqueue is refused for idle rounds (no experiment-0 samples), for an
unsynced clock, for timestamps outside 2025-01-01..now+10 min, and for a
sample time that does not advance (second-resolution primary keys, and a
clock step could otherwise collide silently). Those are counted separately
from queue overflow and reported by `DUMP`.
- The Mega frame has no per-sensor timestamp. The WT32 collection time is a
proxy for when the sensors measured, not proof; per-sensor acquisition times
would need a wire-protocol change.
- Measurement values are stored and uploaded exactly as the slave reports
them. Pressure is the slave's `press` value, which `new-slave.cpp` produces
as `sensorValue * PA_PER_UNIT` (Pa). Nothing here rescales or recalibrates.

## 3. Line protocol

Must match `firmware/display-v9/link_serial.h`. One ASCII line per message,
`\n` terminated, 1024 bytes maximum, 50 ms partial-line discard, no CRC and no
acks. Full rules: `link-serial-protocol.md` Â§2.

### 3.1 Received from the display

| Word | Payload | Meaning |
|---|---|---|
| `HELLO` | - | display booted; reply with the full snapshot |
| `EXPERIMENT` | `run=<0\|1> num=<n> dur=<1-99>` | experiment truth; log on change |
| `CREDS` | `user=<pct> pass=<pct>` | staged portal pair; decode, trim, log in |
| `DUMP` | - | emit a `LOG [DUMP] ...` state snapshot, including the sensor counters `sensOk`, `sensFail` and `sensAge` (read-only, ungated) |

### 3.2 Sent to the display

| Word | Payload | Cadence |
|---|---|---|
| `HELLO` | - | once at WT32 boot |
| `LOGINREQ` | - | every heartbeat while credentials are needed |
| `RESULT` | `OK` / `FAIL` | after every login attempt, plus the HELLO snapshot in RUN |
| `NET` | `ip=<str> up=<0\|1> [ping=<n\|->]` | every heartbeat and ETH event |
| `DB` | `latest=<n\|-> time=<0\|1> bottles=<n\|-> drops=<n> ready=<0\|1> aexp=<n\|-> adur=<n\|-> asrc=<db\|local\|-> pexp=<n\|-> pdur=<n\|-> cfgmax=<n\|-> queued=<n> reads=<ms\|-> ack=<ms\|-> state=<word>` | every heartbeat and after each DB pass |
| `LOG` | `<text>` | every diagnostic line |

`DB latest=0` means "no experiments yet" (the display maps it to -1), `-`
means unknown. `drops=` and `queued=` are always numbers; `0` is meaningful.

- `ready=1` means at least one full configuration pull (devices +
device_config + experiments + latest reading) succeeded since boot. The
display refuses to start a run while it is 0.
- `aexp`/`adur`/`asrc` describe the FROZEN active run snapshot: experiment id,
duration and whether the configuration came from the `experiments` row (`db`)
or from the display (`local`). They are `-` while idle.
- `pexp`/`pdur` are the operator-selected pending experiment and the DB
duration for that id (`-` when the DB has no duration for it). The display
shows this before a start; changes are only applied at the next run start.
- `cfgmax` is the highest CONFIGURED experiment id in the pulled history; it
can exceed `latest`, which only counts experiments with readings. The display
uses `max(latest, cfgmax) + 1` for its next-id hint.
- `reads`/`ack` are milliseconds since the last successful configuration read
and the last acknowledged upload; `-` means never. They are never refreshed by
re-sending cached values, so they show link freshness, not report freshness.
- `state` is the current DB fault category: `none`, `net`, `auth`, `payload`,
`schema`, `clock` or `identity`.

### 3.3 Credential state

Fixed 64-byte buffers, no Arduino `String`. A new pair is decoded from
percent-encoding, trimmed, and copied to the working buffers and to the
last-attempted pair. An identical pair answers with the stored verdict instead
of logging in again. The working pair is overwritten with `x` then zeroed
after each attempt; the last-attempted pair stays in RAM for the repeat rule.
Nothing is written to NVS or flash.

The display keeps its own copy for the session, so a WT32 reboot (`HELLO`)
makes the display re-push the same pair without retyping.

## 4. State machine

```
WAIT_IP -> NEED_CREDS -> LOGIN -> RUN
   ^           ^                    |
   |___________|____________________|
   link lost    login fail / walled mid-run
```

- `WAIT_IP`: heartbeat `NET up=0`. Logs `[ETH] Waiting for IP address (DHCP)...`
  every 2 s. `ETH_GOT_IP` moves to `NEED_CREDS`.
- `NEED_CREDS`: every 5 s emits `NET up=1`, `DB` and `LOGINREQ`, and runs the
  30 s already-authenticated probe (portal trigger + data fetch). A `CREDS`
  line moves to `LOGIN` immediately. The display re-pushes its pair on every
  heartbeat until a verdict arrives.
- `LOGIN`: `doLogin()` (Â§5) with the RAM-only pair, then `RESULT OK`/`FAIL`.
  The working pair is wiped. Success moves to `RUN`; failure returns to
  `NEED_CREDS`, where a corrected pair from the display is acted on at once.
- `RUN`: every 5 s prints the IP, pings `1.1.1.1` 3x, fetches the data URL,
  runs the DB/push pass, and emits `NET` + `DB`. A WALLED fetch returns to
  `NEED_CREDS` for re-auth. A ping failure alone does not trigger re-auth.

`loop()` is non-blocking between stage ticks: it pulls and dispatches link
lines before anything else, then checks `millis()` gates. The database pass is
a cooperative state machine (`dbPassTick()`): each loop iteration performs at
most ONE HTTPS operation (renew/auth, one read, or one upload round), then
returns so the link, heartbeat and sensor poll are serviced before the next
step. A 10 s socket timeout therefore delays the UART once, not for the whole
pass. `doLogin()` still blocks for its duration; the UART FIFO absorbs a
heartbeat and the next iteration drains it. The sensor poll blocks for at most
1.5 s once per 30 s, and it pulls the display link from inside that wait so a
`CREDS` line arriving during a reply is not left in the FIFO. The queue has a
single owner: every pass step runs in `loop()`, so no lock or task handoff is
needed.

There is no `DONE` halt state: every failure path returns to the display.
Reset is never required to retry.

## 5. Portal login (`doLogin()`)

The request flow was prototyped in the removed standalone ping demo. The
production firmware receives username/password over the line link:

1. Trigger: GET `http://connectivitycheck.gstatic.com/generate_204` with
   redirects disabled, parse `window.location="..."`. Empty means no
   interception; a genuine data fetch then means already authenticated.
2. Login page: HTTPS GET of the portal URL with `setInsecure()`,
   rolling-window scan for form `action`, `4Tredir`, `magic`.
   Abort when the form or token is missing.
3. Submit: POST form-urlencoded
   `4Tredir=<enc>&magic=<enc>&username=<enc>&password=<enc>` with
   `Content-Type` and `Referer`. A 3xx or a body mentioning success counts
   as acceptance; the verifier below decides.
4. Verify: `fetchData()`. Genuine lyrics JSON means success.

Body reads are capped (`kMaxBody = 4096`) and the login page is scanned
through a 320-byte rolling window because the WT32-ETH01 has no PSRAM.

## 6. Memory and limits

- Flash used: 1,006,409 B of 1,310,720 B (76.8%). RAM used: 116,896 B of
  327,680 B (35.7%). The tight flash is why the SoftAP credential form lives
  on the display rather than here: a web server does not fit in the
  remaining ~300 KB. See `display-provisioning-ap.md`. The Supabase CA bundle
  (`supabase_ca.h`, 3 roots, ~4 KB flash) is included in those figures.
  The sensor poll adds one fixed 513-byte RX frame and ~20 B of counters.
- Push queue: the 1200-row ring is now materialized (~48 KB `.bss`, 40 B per
  row). The configuration pull adds a 4,096-byte response buffer, a 320-byte
  URL buffer, 16 experiment slots (`DbExperiment`, ~108 B each, ~1.7 KB) and
  ~200 B of session/counter state. Nominal capacity at 30 s sampling is 300
  four-bottle rounds (~150 min); RAM contents are lost on reboot.
- The line link adds two fixed 1 KB log buffers, one 1 KB RX buffer, and
  ~130 B of state. No heap allocation is added.
- DHCP only, no static-IP fallback.
- `setInsecure()` skips TLS verification on the captive-portal path only
  (interception sits above TLS termination, so it cannot be verified).
  Supabase calls (`sbAuthenticate`, `sbFetchLatestExp`, `sbPushReadings`)
  pin the CA bundle via `setCACert()` instead: chain + hostname checking
  on, no insecure fallback. See `esp-db-sync.md` Â§5.
- Sample push queue: fixed 1200-row ring, oldest-drop with a saturating drop
  counter, immutable sample-time stamps, clock-gated on NTP. Enqueued only
  from an accepted sensor frame during an active run; flushed 2 rounds per
  30 s cycle, and only while a run is active. Stopping pauses retries but
  keeps the rows; a later run flushes them with their original experiment id
  and timestamp. See Â§6.1 and `esp-db-sync.md` Â§7-8.
- Credential lifetime: RAM only. The working pair is overwritten with `x`
  before release. The last-attempted pair is kept for the same-pair rule.
  Never written to flash or NVS.
- This firmware does not know where the credentials came from. The display's
  SoftAP form and its USB `CREDS` command both land in
  `link_serial_set_credentials()`, which sends `CREDS` immediately, and this
  side only ever sees a staged pair.
- Link limits: 1024-byte lines, 50 ms gap discard. A lost or corrupted line is
  replaced by the next heartbeat within 5 s.

### 6.1 Database path (configuration, runs and uploads)

Full rationale and acceptance criteria: `network-master-database-plan.md`.
Implementation: the `sbAuthenticate()` / `dbPullConfig()` / `sbPushOneRound()`
block in `main.cpp`, with the pure parsing and envelope code in
`db_contract.h` (shared with the native `db-contract` test).

**Requests.** Every call is HTTPS to `SUPABASE_URL` with `sbApplyTls()`
(pinned CA, no insecure fallback). Common headers are `apikey`, `Accept`; GETs
add `Accept-Profile: device_data` and POSTs add `Content-Profile: device_data`
plus `Content-Type: application/json`. After authentication every request also
sends `x-device-session`. That is a custom device token, not a Supabase Auth
JWT, so it never goes in `Authorization`. No service-role key or database
password is stored on the board.

| Call | Purpose |
|---|---|
| `POST /rest/v1/rpc/authenticate` `{"p_token":...}` | one-row array with `session_token`, `device_id`, `bottle_count`, owner fields and `expires_at` |
| `GET /rest/v1/devices?select=device_id,bottle_count,owner_name,owner_email` | authoritative bottle count; an empty array means the session is not live |
| `GET /rest/v1/device_config?select=bottle_count,owner_name,owner_email,updated_at` | copy for comparison only; disagreements are logged once and `devices` wins |
| `GET /rest/v1/experiments?select=experiment_id,duration_h,sample_interval_s,setpoints&order=experiment_id.desc&limit=16` | bounded local history used for run configuration |
| `GET /rest/v1/readings?select=experiment_id&order=experiment_id.desc&limit=1` | display hint only; never treated as the running experiment |
| `POST /rest/v1/rpc/push_readings` `{"p_readings":[...]}` | one collected round, max 24 objects |

Pipeline: `dbPassTick()` runs at most one of renew/auth, devices, device_config,
experiments, latest-reading or one upload round per `loop()` iteration, then
reports once with `link_emit_db()`. On a session rejection the pass
reauthenticates once and replays the unchanged read/upload.

All responses are read into one bounded 4,096-byte buffer. A body that does not
fit is a payload fault: the firmware never parses a prefix. The session token is
validated as a hex string, and `expires_at` is parsed from the response and
renewed 5 minutes before it lapses (the SQL currently issues 12 hours).

**Run snapshot.** The display remains the start/stop interface and the operator
chooses the experiment number. On a false -> true `run` edge the WT32 refuses
the start unless `ready=1` (one successful pull since boot). It then freezes
experiment id, duration, sample interval and setpoints: the `experiments` row
for that id supplies them, and the display values are used only when no row
exists or its duration is null. Every queued row in the run carries the frozen
experiment id. A DB update during the run only changes the pending
configuration for the next run; it never relabels queued samples or changes the
active duration. Starting does not increment the experiment number.

**Uploads.** A request contains at most 24 rows sharing one collection time.
`null` means a present bottle's sensor was unavailable; an absent bottle
contributes no row. The response must be a bare integer between 0 and the sent
row count (a replay can legitimately answer 0). Only then is the batch removed
from the queue. The primary key is `(device_id, bottle_id, timestamp)`, so a
replayed round with different values is ignored server-side and cannot be
detected from the scalar acknowledgement; queued rows are therefore immutable.

**Failures.** `net` (timeout, TLS, 429, 5xx) keeps the batch and retries with
capped exponential backoff plus jitter, honouring `Retry-After`. `auth`
reauthenticates once and replays the unchanged batch. `payload`/`schema` stop
hot-looping and keep the batch for inspection; there is no automatic discard
policy beyond the existing oldest-drop bound. `clock` means NTP or a timestamp
is untrustworthy, and `identity` means the authenticated device differs from
the identity the queue was collected under, in which case uploads are blocked
rather than delivering rows to the wrong device.

## 7. Expected output

Lines prefixed `LOG ` reach the display, which prints them as `[W32] ...` on
its USB console. A bare adapter on the WT32 Debug header shows the prefix.

```
LOG network-master (production WT32 portal + rusim-line peer)
LOG Ping target: 1.1.1.1
LOG Data URL: https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ
LOG Credentials come from the display over UART0, never stored.
HELLO
LOG [LINK] rusim-line v1 on UART0 IO1/IO3 115200 8N1
LOG [SENS] UART1 on IO5 RX / IO17 TX, 9600 8N1
LOG [ETH] Waiting for link + DHCP...
LOG [ETH] Started
LOG [ETH] Link up
LOG [ETH] Got IP 192.168.1.10 gw ... mask ... mac ...
LOG [LINK] link up, waiting for credentials from the display
LOG [LINK] captured creds user='...' (password in RAM only)
LOG [LOGIN] Verifying with a real data fetch...
LOG [LOGIN] SUCCESS: port is authenticated.
LOG [PING] 1.1.1.1 OK, avg time = 15 ms
LOG [SENS] 4 bottles ok=20 fail=0 b1 ph=2.51 ch4=812.4 t=38.5 co2=1204 p=1013 b2 ph=2.49 ch4=790.1 t=38.2 co2=1198 p=1013 b3 ph=2.53 ch4=824.9 t=38.7 co2=1210 p=1014 b4 ph=2.47 ch4=801.3 t=38.4 co2=1202 p=1013
LOG [DB] authenticated device=1 bottles=4 expires_in=43200s owner='...'
LOG [DB] config pull ok: bottles=4 experiments=2 latest=3
LOG [DB] run exp=4 started: src=db dur=48h interval=30s setpoints={"t":39.5}
LOG [RUN] exp=4 duration=48h running=1 dbLatest=3 bottles=4 ntp=1 queued=0 drops=0
```

An idle round is counted but not queued, and a refused start is explicit:

```
LOG [DB] run start refused: no successful DB configuration pull since boot
LOG [PUSH] acked 3 rows (server inserted 3, 0 queued)
```

With the Mega absent or its TX wire open, a failure line is logged on the
first failure and then every 20th:

```
LOG [SENS] poll failed: reply timeout (fail=1)
```

A field the slave reported invalid prints as `null`:

```
LOG [SENS] 2 bottles ok=5 fail=2 b1 ph=null ch4=null t=null co2=null p=null b2 ph=2.5 ch4=700 t=37 co2=1000 p=1000 ...
```

On a rejected pair:

```
LOG [LOGIN] FAILED: still walled (wrong credentials or portal changed).
LOG [LOGIN] will wait for a corrected pair from the display
```

The display shows the same verdict on `/setup` and on the phone's form page.

## 8. Related documents

- `link-serial-protocol.md` â€” the wire spec, log forwarding, `PASSTH`, and
  the Mega link that replaced the RS485 pin reservation.
- `display-docs.md` Â§10, Â§12 â€” display end, login gate, SoftAP form.
- `display-provisioning-ap.md` â€” the phone login path on the display.
- `wt32-eth01-captive-recon.md` â€” portal recon sketch.
- `captive-portal-workaround.md` â€” portal relay plan.
- `migration-plan.md` â€” the sensor pull this implements, plus the deferred
  enqueue and in-slave pH conversion.
- `legacy-slave.md` â€” the Mega program and its JSON reply.
- `platformio-environments.md` â€” build environments.
- `../hardware.md` â€” board pinouts (read-only reference).

---
Content generated by LLM in Freebuff.
