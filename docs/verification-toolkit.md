# Verification toolkit (report data collection)

Scope: the instrumentation and tools used to collect the final-report
verification data for the Display (`display-v9`), the network ESP
(`network-master`), the database (Supabase) and the dashboard. It covers the
two metrics firmware builds, the `MET` line format, the host tools in
`verification/`, the procedure for each measurement, and what each number does
and does not include.

The measurement plan itself (what the committee will check, proposal Table 2.1)
is the operator's checklist, kept outside the repository. Section numbers in
the form "checklist §N" refer to it.

## 1. Overview

```
 Mega slave (or verification/fake-slave.ts)
     |  UART1 9600, "#1:PING*" -> JSON
     v
 WT32 network-master-metrics ---- HTTPS ----> Supabase  <---- dashboard (?metrics)
     |  rusim-line 115200: SENS/NET/DB + "LOG MET ..."            ^
     v                                                            |
 Display display-v9-metrics                          verification/db-check.ts
     |  USB 115200: "MET ...", "[W32] MET ...", "[LINK<] ..."     db-replay.ts
     v                                                         dashboard-load.ts
 verification/capture.ts -> runs/<run>/raw.log -> analyze.ts -> summary.md + CSVs
```

One USB cable to the display records both boards: the display prints its own
`MET` lines, forwards every WT32 log line as `[W32] ...`, and the metrics build
echoes every line it receives from the WT32 as `[LINK<] ...`.

| Part | Path |
|---|---|
| Display instrumentation | `firmware/display-v9/metrics.h`, `metrics.cpp`, hooks in `main.cpp`, `link_serial.cpp`, `fake_data.cpp`, `ui_hold_button.cpp`, `serial_cmd.cpp` |
| WT32 instrumentation | `firmware/network-master/main.cpp`, every block under `#if RUSIM_METRICS` |
| Host tools | `verification/*.ts`, shared code in `verification/lib/` |
| SQL suites | `verification/sql/integrity.sql`, `security.sql`, `performance.sql` |
| Dashboard timing | `dashboard/src/metrics.ts` (opt-in with `?metrics`) |
| Ingest-time column | `dashboard/supabase/migrations/20261124000000_add_readings_inserted_at.sql` |
| Run output | `verification/runs/` (git-ignored; archive it with the report evidence) |

## 2. Metrics builds

| Environment | Base | Added |
|---|---|---|
| `display-v9-metrics` | `display-v9` + `-D RUSIM_METRICS=1` | `MET` lines, `[LINK<]` echo, `REBOOT`/`BOOTLOG`/`HEAPCHK` console commands, 8 KB USB TX buffer |
| `network-master-metrics` | `network-master` + `-D RUSIM_METRICS=1` | `LOG MET` lines, `DBG` fault-injection commands, `seq=` on `SENS` lines, timed TLS connect |

```bash
pio run -e display-v9-metrics -t upload
pio run -e network-master-metrics -t upload   # through the display's PASSTH tunnel, see link-serial-protocol.md §7
```

With `RUSIM_METRICS` unset (all other environments), every hook is an empty
inline function (display) or compiles to nothing (WT32), so production
firmware is unchanged. Flash use measured on 2026-11-24: `network-master-metrics`
79.5 % of the 1.25 MB app partition; `display-v9-metrics` 21.8 %.

Behaviour differences of a metrics build, to state in the report:

- WT32: a Supabase request first calls `client.connect()` so the TCP + TLS
  handshake can be timed (`tls_ms`). `HTTPClient` then reuses the connected
  client, so the number of connections is the same as in production. A failed
  connect returns `-1` without the request, which is what `HTTPClient` returns
  for a refused connection.
- WT32: `SENS` lines start with `seq=<n>`. Every display build ignores a field
  that comes before the first `b<i>` token, so the line stays valid
  `rusim-line` v1.
- Display: the USB console carries roughly 3 to 4 times more text. The 8 KB TX
  buffer keeps those writes from blocking the UI loop.

Use `IS_DEBUG=1` (the default) on the display: the `W32CMD` console command,
which sends `DBG` words to the WT32, only exists in debug builds.

## 3. `MET` line reference

Format: `MET <KIND> key=value ...`, values without spaces. On the display USB
console a WT32 line appears as `[W32] MET <KIND> ...`. Times are the board's
own `millis()`/`micros()`; `_us` = microseconds, `_ms` = milliseconds.

### 3.1 Display

| Kind | Fields | Emitted |
|---|---|---|
| `RESET` | `reason` (`poweron`, `sw`, `panic`, `int_wdt`, `task_wdt`, `brownout`, ...) | once per boot, after LVGL init |
| `BOOT` | `stage`, `ms` (since the app started) | once per stage: `setup`, `panel`, `ui_built`, `first_frame`, `touch_ready`, `w32_first_line`, `w32_hello`, `loginreq`, `net_up`, `db_first`, `db_ready`, `login_ok`, `sens_first` |
| `TOUCH` | `edge` (`down`/`up`), `lat_us`, `inv_us`, `render_us`, `nf`, `stuck`, `wake`, `from`, `to` (routes), `x`, `y` | every touch edge |
| `HOLD` | `event` (`start`/`commit`/`cancel`), `held_ms`, `route` | hold-to-confirm buttons |
| `SENSRX` | `wseq` (WT32 seq), `seq` (display seq), `apply_us`, `render_us`, `noframe`, `route` | every `SENS` frame the UI applied |
| `FPS` | `window_ms`, `frames`, `avg_render_us`, `max_render_us`, `px`, `idle_pct` | every 5 s with at least one frame |
| `MEM` | `uptime_s`, `heap_free/min`, `psram_free/min` | every 60 s |
| `LINK` | `window_ms`, `rx`, per-word counts (`hello`, `loginreq`, `result`, `net`, `db`, `sens`, `log`, `other`), `gap`, `ovf`, `net_gap_max_ms`, `db_gap_max_ms`, `touch_drop` | every 60 s, then reset |
| `HEAPCHK` | `ok`, `ms` | console `HEAPCHK` |

`[LINK<] <line>` repeats every inbound link line except `LOG` (`NET`, `DB`,
`SENS`, `HELLO`, `LOGINREQ`, `RESULT`), as received.

### 3.2 WT32

| Kind | Fields | Emitted |
|---|---|---|
| `RESET` | `reason` | once per boot |
| `BOOT` | `stage`, `ms` | once per stage: `setup`, `eth_start`, `eth_link`, `got_ip`, `ntp`, `portal_ok`, `auth`, `ready`, `sens_first`, `enq_first`, `ack_first` |
| `POLL` | `ok`, `why` (`timeout`, `overflow`, `nojson`, `nobottle`), `reply_ms`, `bytes`, `present`, `valid` | every Mega poll (5 s) |
| `SENSTX` | `seq`, `src` (`poll`/`snapshot`), `proc_us`, `bytes` | every `SENS` line sent |
| `ENQ` | `unix`, `exp`, `rows`, `queued`, `drops`, `dt_ms` (spacing since the previous enqueue of the run, `-1` first), `period_ms` (configured) | every enqueued round |
| `HTTP` | `op` (`auth`, `devices`, `device_config`, `experiments`, `latest`, `push`), `code`, `ms`, `tls_ms`, `req`, `resp`, `trunc` | every Supabase request |
| `AUTH` | `why` (`initial`, `renew`, `reauth`), `ok`, `ms`, `fault` | every authentication |
| `ACK` | `rows`, `inserted`, `queued` | every acknowledged upload |
| `BACKOFF` | `fault`, `wait_ms`, `retry_after_s`, `queued` | every upload backoff |
| `PASS` | `ms`, `pushes`, `queued`, `drops`, `acks`, `fault`, `ready`, `run` | end of every DB pass |
| `LINK` | `window_ms`, `rx`, `exp`, `hello`, `other`, `gap`, `ovf`, `exp_gap_max_ms` | every 60 s |
| `LOOP` | `window_ms`, `iters`, `max_iter_ms`, `max_link_gap_ms` | every 60 s |
| `MEM` | `uptime_s`, `free`, `min`, `maxblock` | every 60 s |
| `TIME` | `unix_ms`, `ntp` | every 60 s |
| `DBG` | `ok=<word>` or `err=...` | reply to a `DBG` command |

## 4. Commands

Display USB console (metrics build):

| Command | Effect |
|---|---|
| `REBOOT` | `ESP.restart()` (software reset) |
| `BOOTLOG` | re-prints the `RESET` and `BOOT` lines of this boot with `replay=1` |
| `HEAPCHK` | `heap_caps_check_integrity_all()`. Walks every heap block: on a corrupted heap it may hang or reset the board, which is the finding (`plans-and-audits/display-info-reboot.md`) |
| `MEM` | existing O(1) heap/PSRAM counters (all builds) |
| `W32CMD <line>` | sends one line to the WT32 (`IS_DEBUG` builds) |

WT32 `DBG` words, sent as `W32CMD DBG <word> [arg]` (metrics build only; every
effect is RAM-only and ends at reboot):

| Word | Effect | Tests |
|---|---|---|
| `REBOOT` | `ESP.restart()`; queued rows are lost | WT32 reboot mid-run, RAM-only queue loss |
| `BOOTLOG` | replays this boot's milestones | boot loop |
| `SESSION_CORRUPT` | overwrites the session token with zeros | forced 401/403, re-auth, batch replay |
| `SESSION_EXPIRE` | moves the session expiry inside the 5 min renewal margin | session renewal |
| `CLOCK_STEP <s>` | steps the system clock by `<s>` seconds (negative = back) | timestamp-reuse refusal |
| `IDENTITY [OFF]` | binds the queue to another device id / restores it | `identity` fault, no upload |
| `BADPUSH` | the next upload sends a malformed body (server `22023`) | `payload` fault, batch kept |
| `BADPATH` | the next upload calls a missing function (`PGRST202`) | `schema` fault |
| `DBDOWN ON\|OFF` | Supabase requests go to the unroutable `10.255.255.1` | Supabase unreachable while Ethernet is up |

The `net` fault class needs no command: unplug Ethernet or use `DBDOWN`. The
`clock` class needs NTP blocked at the network (firewall), because the firmware
resynchronises on every pass.

## 5. Host tools

All tools run with Bun from the repository root and print their usage when
passed `--help` or when a required argument is missing. `capture.ts`,
`boot-loop.ts` and `fake-slave.ts` exit 0 on `--help`; the rest have no
dedicated help branch and exit 1 after printing usage.
Type-check them with `dashboard/node_modules/.bin/tsc -p verification/tsconfig.json`.

| Tool | Purpose | Output |
|---|---|---|
| `capture.ts` | records the display USB console with host timestamps; markers from stdin; periodic commands (`--every "60:MEM"`) | `runs/<run>/raw.log`, `meta.json` (the test-run log: operator, git commit and dirty flag, firmware envs, latest local migration, interval, experiment, bottles, network) |
| `analyze.ts` | turns one or more runs into tables and CSVs | `runs/<run>/analysis/summary.md`, `summary.json`, `touch.csv`, `boot.csv`, `internal_latency.csv`, `poll.csv`, `enq.csv`, `http.csv`, `pass.csv`, `db_state.csv`, `net_state.csv`, `episodes.csv`, `link_*.csv`, `loop_wt32.csv`, `mem_*.csv`, `clock_wt32.csv`, `fps.csv`, `hold.csv`, `backoff.csv` |
| `boot-loop.ts` | N boots by command or by manual power cycle | `boot-loop.csv`, `boot-loop.md` |
| `fake-slave.ts` | answers `#1:PING*` in place of the Mega with scripted scenarios | `expected.csv` |
| `fidelity.ts` | compares `expected.csv` with `SENS` lines and a database/dashboard CSV | `fidelity.md`, `fidelity.csv` |
| `db-check.ts` | runs `sql/*.sql` inside a READ ONLY transaction | `db-<suite>.md`, `.json` |
| `db-replay.ts` | live `push_readings` contract test (writes test rows) | `db-replay.md` |
| `dashboard-load.ts` | concurrent viewers repeating the dashboard's reads | `dashboard-load.md`, `requests.csv` |

Serial ports: the display's CH340 resets the board when opened by pyserial or a
plain `pio device monitor`. The tools configure the port with `stty` and open
it as a file, which does not reset it (`link-serial-protocol.md` §7). Close any
other monitor first; only one program can read the port.

Markers: in `capture.ts`, a line starting with `#` is stored as `#MARK <text>`.
`analyze.ts` groups touch results by the latest marker, and treats markers
that start with `fault` and `restore` as fault-injection episodes (§6.4).

Credentials: `db-check.ts` reads `DATABASE_URL`, `db-replay.ts` and
`dashboard-load.ts` read `RUSIM_DEVICE_TOKEN`, all from the environment. None
of them opens a `.env` file or prints a token. Bun itself loads a `.env` file
from the directory it is started in, so copy `.env.example` (repository root)
to `.env`, fill in both values, and run the tools from the repository root
instead of exporting shell variables. `.env` is git-ignored.

## 6. Procedures

Every run: flash both metrics builds, keep the host NTP-synced, start
`capture.ts` with the run's configuration, and keep `meta.json` with the raw
log. Run `analyze.ts` on the run directory afterwards.

```bash
bun verification/capture.ts --label <name> --operator "<name>" \
    --interval-s 30 --experiment 4 --bottles 4 --network campus [--duration 24h]
bun verification/analyze.ts verification/runs/<stamp>-<name>
```

### 6.1 Display (checklist §3)

- **GUI latency, per interaction (Table 2.1 #1).** Type a marker naming the
  interaction (`#tab-tap`, `#swipe`, `#button-control-start`, `#spinbox-duration`,
  `#route-info`), then perform it at least 100 times; one marker per
  interaction. `summary.md` gives n, min, mean, p50, p95, p99, max and the count
  above 200 ms per marker and edge; `touch.csv` has every sample for a
  histogram or CDF. Buttons act on release, so the `up` edge is the action
  latency and `down` is the pressed-state feedback.
- **Under stress.** Repeat with a `#stress-...` marker while tapping fast, and
  while a `SENS` frame renders (frames arrive every 5 s; a run of 100 taps
  covers many). Count resets in the `Resets` table and keep a screen recording.
- **Hold-to-confirm.** `HOLD` lines: commits held below 2000 ms are false
  triggers; short taps produce `cancel`. Time to first feedback is the `down`
  edge `TOUCH` on the hold button.
- **Wake from sleep.** Wait 60 s, tap: the `TOUCH` with `wake=1` is the waking
  tap. The backlight turns on in the same touch read; the framebuffer is
  retained, so no frame is needed (`nf=1` is expected for that edge).
- **Render time and FPS.** `FPS` lines (`fps.csv`).
- **Timestamp validation.** Build with `-D RUSIM_MET_GPIO=<free pin>` (choose a
  pin that `hardware.md` lists as unused): the pin goes HIGH at the touch edge
  and LOW when the answering frame is flushed. Compare a subset with a logic
  analyser, or film the screen at 240 fps.
- **Boot time (Table 2.1 #4).** `bun verification/boot-loop.ts --board display
  --method manual --reps 20` (cold boot, operator power-cycles) and `--method
  command` (software restart). `touch_ready` is the first touch-responsive
  screen. Login time at `/setup` is outside it by construction; the network
  milestones (`net_up`, `db_ready`, `login_ok`) are reported separately.
- **Heap and resets over 24 h.** `MEM` lines every 60 s; the summary gives
  first, last, minimum and the slope per hour. `HEAPCHK` at the start and the
  end of a soak. `MEM` carries counter-API values only: `getMaxAllocHeap()` and
  `getMaxAllocPsram()` walk every heap block under the spinlock and trip the
  interrupt watchdog when the heap is corrupt
  (`plans-and-audits/display-info-reboot.md`), so they are never sampled
  periodically. Fragmentation is judged with `HEAPCHK` or the console `MEM`.

### 6.2 Internal data latency (Table 2.1 #2)

With both metrics builds, every `SENS` frame yields one joined row in
`internal_latency.csv`: WT32 reply-complete -> `SENS` written (`proc_us`), wire
time (`bytes x 10 / 115200`), and display receive -> rendered frame. Each part
is measured on a single clock, so no clock synchronisation between boards is
needed. The display copies new frames into the UI on a 1 s timer
(`UI_SENS_APPLY_MS`), so the display part is between 0 and about 1 s and
dominates the total. `poll_reply_ms` (request -> reply complete) is listed
separately: it includes the Mega's own reply time.

### 6.3 Network ESP (checklist §4)

- **Sensor poll.** `POLL` lines: success ratio, failure causes, reply time vs
  the 1500 ms budget, frame size vs 512 B. The NULL rate per bottle and field
  comes from the `SENS` lines.
- **Sample spacing vs `sample_interval_s`.** `ENQ` lines: `dt_ms - period_ms`
  per round. Single spacings are quantised to the 5 s poll; the mean shows
  drift (§8.1).
- **Display link loss proxies (Table 2.1 #3).** `LINK` lines on both boards:
  heartbeat lines received vs expected (one per 5 s), the longest gap between
  heartbeats, partial-line and over-length discards, unknown lines.
- **HTTPS.** `HTTP` lines: status distribution per operation, round trip,
  TLS connect time, body sizes vs the 6144 B / 4096 B caps. `AUTH` gives
  initial, renew and reauth counts.
- **Failure classes.** Induce one at a time with §4 (`SESSION_CORRUPT`,
  `BADPUSH`, `BADPATH`, `IDENTITY`, `DBDOWN`, Ethernet unplug) and read
  `BACKOFF`, `PASS` and `[LINK<] DB ... state=` in `db_state.csv`. Expected:
  capped exponential backoff with jitter (minimum wait above 0, so no hot loop),
  the batch kept (`queued` does not drop), recovery after the fault ends.
- **Queue capacity.** Unplug Ethernet during a run for longer than the ring
  lasts (1200 rows = 300 four-bottle rounds; 150 min at 30 s, 25 min at 5 s).
  `pass.csv` and `db_state.csv` show `queued` reaching 1200 and `drops`
  counting; after reconnecting, the drain rate is the slope of `queued`.
- **RAM-only loss.** Note `queued` from the last `DB` line, then
  `W32CMD DBG REBOOT`; those rows are lost (the reboot `DBG ok=REBOOT` line
  records `queued`).
- **Clock.** `TIME` lines: offset to the host clock and drift per hour.
  `W32CMD DBG CLOCK_STEP -120` during a run must produce
  `[PUSH] refused: timestamp reuse` and no new `ENQ` until the clock passes the
  last queued second.
- **Responsiveness while HTTPS is slow.** `LOOP max_link_gap_ms` with `DBDOWN ON`
  (each request waits for the 5 s connect timeout).
- **Run snapshot.** Start a run, change the `experiments` row in the database,
  check `[LINK<] DB aexp/adur` stays and `pexp/pdur` changes; the next start
  uses the new row.

### 6.4 Reconnect and fault-injection matrix (Table 2.1 #5, checklist §7)

Type `#fault <name>` when the fault is applied and `#restore <name>` when it is
removed. `episodes.csv` gives, in ms after each marker: `NET up=0`, first DB
fault state and first failed poll (detection), then `NET up=1`, first good
poll, `DB state=none` and the first acknowledged upload (recovery). The
reconnect time is the first successful action after `#restore`; Table 2.1 asks
for 20 repetitions per scenario.

| Scenario | Apply | Restore |
|---|---|---|
| Ethernet | unplug the cable | plug it back |
| Supabase unreachable | `W32CMD DBG DBDOWN ON` | `W32CMD DBG DBDOWN OFF` |
| Sensor link | pull the Mega TX wire (or stop `fake-slave.ts`) | reconnect / restart |
| Display link | pull the WT32 TX -> display wire | reconnect |
| Session | `W32CMD DBG SESSION_CORRUPT` | (self-healing; recovery = next `AUTH why=reauth ok=1`) |
| WT32 reboot | `W32CMD DBG REBOOT` | (automatic) |
| Display reboot | console `REBOOT` | (automatic; the capture tool keeps the port) |

The DB pass runs once per 30 s cycle, so database recovery after a restore
includes up to one cycle of waiting.

### 6.5 Value fidelity with the fake slave (checklist §8)

Replace the Mega with a 3.3 V USB-UART adapter on WT32 IO5/IO17 and run
`fake-slave.ts` next to `capture.ts`:

```bash
bun verification/fake-slave.ts --port /dev/ttyUSB1 --scenario mixed --seed 7
bun verification/capture.ts --port /dev/ttyUSB0 --label fake-mixed ...
bun verification/fidelity.ts --expected <fake run>/expected.csv \
    --capture <capture run>/raw.log --db <dashboard CSV export> --db-bottle-base 1
```

Scenarios cover missing bottles, invalid markers, values next to the markers,
malformed, oversize (600 B), slow (1.7 s) and silent replies. Expected: markers
become NULL end to end, values next to markers stay values, the display link
differs from the sent value only by `%.4g` rounding, the database only by
float32 and the upload's `%.6g` formatting.

### 6.6 Database (checklist §5)

```bash
export DATABASE_URL='postgresql://...?...sslmode=require'
bun verification/db-check.ts --suite all --device 0 --exp 4 --interval-s 30
RUSIM_DEVICE_TOKEN=... bun verification/db-replay.ts --exp 0 --yes
```

- `integrity`: completeness per bottle against the observed span and the
  configured duration, spacing statistics and the longest gaps, bottles per
  round (missing rows), NULL fields (sensor failed), rows of other
  experiments inside the run window, out-of-range timestamps, and ingest
  latency (`inserted_at - timestamp`) once the migration is applied.
- `security`: policies, RLS state, column and table grants, function
  hardening, legacy `public` grants, session expiry state. Run it before and
  after a change and keep both outputs.
- `performance`: table size and bytes per row, rows per experiment,
  `EXPLAIN ANALYZE` of the dashboard's two reads (look for
  `readings_experiment_timestamp_idx`), index usage. Synthetic 1k/10k/100k-row
  and 24-chamber data come from `bun run dev-db` (`dashboard.md` §9).
- `db-replay.ts` checks the write contract case by case (first insert, exact
  replay = 0, same key with different values = 0 and the stored value
  unchanged, mixed keys, one bad row rolls back the batch, time window,
  non-array, 25 rows, forged `device_id`, wrong token, garbage session) and
  times `--latency-n` inserts. It writes rows about one hour in the past into
  experiment 0 (the testing experiment); a large unused id would move the
  display's "next experiment" hint.

**Ingest latency.** `timestamp` is the WT32's sample time and cannot show
when a row arrived. Migration `20261124000000_add_readings_inserted_at.sql`
adds `inserted_at` (server `now()` at the first insert; earlier rows stay
NULL). It is not applied by this change: apply it with `supabase db push`,
then regenerate `dashboard/src/types/database.ts`. Until then, ingest
latency can only be estimated from the WT32 side (`ENQ` -> `ACK`).

### 6.7 Dashboard (checklist §6)

- Open the dashboard with `?metrics` (for example
  `http://localhost:5173/?metrics`). The console prints `[metrics]` records:
  `page_load`, `fetch` (duration, rows, age of the newest sample) and `render`
  (data arrival -> next painted frame); `copy(window.__rusimMetricsCsv())`
  copies them as CSV. The deployed Netlify site is frozen on an older build,
  so measure on `bun run dev` or `bun run preview`.
- Data age and Live/Concluded flip: `newest_age_s` on each poll (every 5 min
  30 s). The 10 min Concluded threshold must stay well above the run's
  `sample_interval_s`.
- Concurrent viewers: `RUSIM_DEVICE_TOKEN=... bun verification/dashboard-load.ts --viewers 20 --duration 5m --exp 4`.
- Row caps as built: `fetchReadings` (Monitoring) stops at 5 000 rows;
  `fetchExperimentReadings` (History and CSV export) stops at 50 000 rows
  (`MAX_EXPERIMENT_ROWS`).

## 7. What each number includes

| Metric | Starts | Ends | Not included |
|---|---|---|---|
| `TOUCH lat_us` | first indev read that sees the new touch state | `LV_EVENT_RENDER_READY` of the frame holding the touch's invalidated areas (all flush calls done) | GT911 scan time and the up-to-33 ms indev polling period before the read; panel scan-out after the framebuffer write |
| `TOUCH inv_us` | same start | first invalidation made while the indev timer runs | |
| `nf=1` | | no touch-caused invalidation within 500 ms (the touch changed nothing on screen) | excluded from latency statistics, counted |
| `SENSRX render_us` | `SENS` line parsed on the display | frame flushed after the UI applied it | |
| `SENSTX proc_us` | WT32 reply line complete | `SENS` handed to the UART driver | UART transmission (added by `analyze.ts` as wire time) |
| `POLL reply_ms` | request written to the Mega | reply line complete | |
| `HTTP ms` / `tls_ms` | before `connect()` | after `http.end()` / after `connect()` | |
| `BOOT ms` | application start | the milestone | ROM and second-stage bootloader time (measure once on video) |
| `TIME` offset | | | the display-forwarding delay (tens of ms) |

Touch results are attributed only to invalidations made inside the indev read
timer, so a status-bar or sensor redraw a few ms after a tap cannot end the
measurement early. Results are queued and printed from the main loop, never
from inside a render.

## 8. Firmware changes made for data collection

### 8.1 `sample_interval_s` now sets the enqueue spacing

Before this change the WT32 froze `sample_interval_s` at run start, logged it
and never used it: rows were always enqueued once per 30 s. Now
(`network-master.md` §2.3):

- During a run the spacing is the run's frozen `sample_interval_s`; idle
  rounds and a run whose `experiments` row has a NULL interval keep 30 s.
  An interval below the 5 s poll is raised to 5 s and logged.
- Due times sit on a fixed grid (`due += period`) and the poll nearest a due
  time takes it (half a poll period of tolerance). Measuring from the
  previous round instead would let reply-time jitter push a round to the next
  poll (35 s instead of 30 s) and the average would drift late.
- The first accepted round after a run start is enqueued immediately.
- Upload rounds per DB pass scale with the interval (2 at 30 s and above,
  `2 x rounds per 30 s + 1` below, at most 16), so the queue drains as fast
  as it fills.

### 8.2 Findings while preparing

- The Mega is polled every 5 s, not 30 s; 30 s was only the enqueue and
  `[SENS]` log cadence.
- The display link prints values with `%.4g`. Pressure in Pa has six
  significant digits, so the display shows it rounded to about +-50 Pa
  (`1.011e+05`). The upload path uses `%.6g` and the database stores float32.
- The dashboard's experiment reads are capped at 50 000 rows, not 5 000;
  5 000 applies to the Monitoring view's newest-first read.

## 9. Related documents

- `network-master.md` — WT32 firmware, sample interval, metrics build.
- `display-docs.md` — display firmware; §16 metrics hooks.
- `link-serial-protocol.md` — wire rules, `seq=` and `DBG` in metrics builds, `PASSTH`.
- `platformio-environments.md` — the two metrics environments.
- `database.md` — `readings.inserted_at`.
- `dashboard.md` — the app, `dev-db`, row caps.

---
Updated at 2026-11-24-15:10.
