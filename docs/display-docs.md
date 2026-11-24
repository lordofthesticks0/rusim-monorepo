# Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)

## 1. Overview

`firmware/display-v9/` is the Display MCU firmware: Sunton 800×480 ST7262
(RGB565) + GT911 touch, ESP32-S3, LVGL v9.6.0, Arduino_GFX flush path.

The WT32 serves the login page and supplies network status, database state,
experiment state, and bottle readings over `rusim-line`
(`link-serial-protocol.md`). The display shows a QR for the WT32's reported IP;
it never serves a web page, provisions Wi-Fi, or receives login credentials.
Sensor values start unavailable and change only when a real `SENS` frame arrives;
there is no fake seed, random jitter, or simulated sensor fallback. The
legacy filenames/types `fake_data` and `BottleFake` remain for compatibility.
Setpoint, stirrers, water pump and POST controls still lack real hardware
transport; the screen labels them "Display only" / "Not connected yet".

Locked decisions honoured: no staleness UI, no PIN on Control,
hold-to-confirm 2000 ms on binary toggles, no local log storage, 41 °C warn /
45 °C breaker two-tier overheat, 60 s backlight sleep, duration 1–99 h,
5 sensors + trend per bottle, NULL-field bell with live list and no history.
The 2026-11-21 UX audit (`plans-and-audits/lvgl-ui-ux-audit.md`) reworked
the layout, the colour rules and the refresh model; §15 describes the result.

## 2. Hardware / platform notes

| Item | Detail |
|---|---|
| Panel | `Arduino_RPi_DPI_RGBPanel`, 800×480, 16 MHz PCLK, `auto_flush` (`main.cpp`) |
| Bus | `Arduino_ESP32RGBPanel`, DE 40 / VSYNC 41 / HSYNC 39 / PCLK 42, R 45/48/47/21/14, G 5/6/7/15/16/4, B 8/3/46/9/1 |
| Touch | GT911, SDA 19 / SCL 20 (`touch.h`), read via `my_touchpad_read()` |
| Backlight | `TFT_BL = 2`. Notes say GPIO45, but **GPIO45 is R0 of the RGB bus** — do not move without EE sign-off (`ui_config.h`, `screen_sleep.cpp`) |
| Draw buffer | 800·480/4·2 ≈ 187.5 KB `MALLOC_CAP_INTERNAL`, `RENDER_MODE_PARTIAL`, RGB565 |
| Env | `display-v9` in `platformio.ini`: esp32-s3-devkitc-1, qio_opi, 16 MB flash, opi PSRAM, lvgl 9.6.0 |

## 3. File map

| File | Role |
|---|---|
| `main.cpp` | HW init (gfx, touch, flush, draw buf, indev) + `fake_init()` + `link_serial_init()` + `ui_init()`; `loop()` polls USB, link, screens, `ui_poll()`, sleep, and LVGL; all LVGL work is suspended while the `PASSTH` tunnel is open. The touch read passes through `sleep_filter_touch()` (§15.4) |
| `lv_conf.h` | Enables `BUTTON, SPINBOX (+TEXTAREA, required by spinbox), SWITCH, CHART, BUTTONMATRIX, QRCODE (+CANVAS, required by qrcode)`; fonts 14/20/28/40 (12 is off: unused). `KEYBOARD` is 0 because the display has no credential entry form (§12). **`SLIDER` deliberately left 0** (not needed). `MSGBOX/LIST/TABLE` left 0 — modals and lists are hand-built from `obj+label+button` |
| `link_serial.h/.cpp` | `rusim-line` v1 peer on `Serial1` (RX 18 / TX 17, 115200 8N1). Line parser, 5 s heartbeats, link gate words (`LOGINREQ`, `RESULT`), network/database/sensor state, log forwarding, and `PASSTH` tunnel. Credentials are not part of the wire protocol. `Serial` stays USB debug only. Full wire spec in §10 and `link-serial-protocol.md` |
| `screen_setup.h/.cpp` | `/setup` first-class route: 280 px QR in a white 316 px box, numbered steps and a 28 px typed URL for `http://<WT32-IP>/`. The QR stays hidden (box shows a waiting note) until the WT32 reports an IP via `NET`; it is refreshed only when that IP changes. `ui_init()` opens the route first; the first `RESULT OK` routes home without touching run state. A later `LOGINREQ` (session expired) shows a modal + status chip instead of pulling the operator away (§11). `RESULT FAIL` shows retry text. No credential form or Wi-Fi provisioning. |
| `touch.h` | Unchanged GT911 glue |
| `ui_config.h` | Capacity (6 clusters × 4 bottles; all offline until reported), ranges, 2000 ms / 60 s timings, 1 s sensor apply / 90 s expiry, thresholds, pins, palette and colour rules, layout grid (`UI_BAR_H`, `UI_PAGE_TOP`, ...), `UI_DEG_C`, link-silence limit, `FW_VERSION`, `IS_DEBUG` |
| `fake_data.h/.cpp` | `AppState g`: controls (incl. display-only `stirrerOn`/`pumpOn`, `expNumUserSet`, `runStartUnknown`) and real bottle values; `fake_alarm_count()` counts NULL fields on online nodes only (bell); `fake_init()` starts all nodes offline, bottles absent, readings/history unavailable; `sens_apply_real()` applies WT32 frames to node 0 and clears missing/expired data. Every sensor keeps its own 24-sample history (`trend[S_COUNT][UI_TREND_PTS]`, ~11.5 KB for all 24 slots). Bottle presence is independent of database capacity |
| `serial_cmd.h/.cpp` | Human-typeable debug protocol over USB serial (see §5). Display-local only; no `CREDS` or `AP` commands. |
| `provision_ap.h/.cpp` | Removed. The display no longer provisions Wi-Fi or serves web pages; login is hosted by the WT32 (§12). |
| `ui_shell.h/.cpp` | Status bar (labelled tabs, link chip, bell), content router with in-place refresh, severity-ranked modal, bell logic, overheat eval, boot run adoption and experiment-number suggestion (`ui_poll()`), shared experiment status text, `ui_mk_*` styled helpers and stepper auto-repeat (no LVGL theme is enabled, so all styling is explicit) |
| `screen_home.h/.cpp` | `/home` "Overview": run card (progress arc, start/stop hold button, duration stepper locked while running, experiment status line) and status card (bottles, alerts, setpoint, chamber, stirrers, water pump) |
| `screen_data.h/.cpp` | `/data` "Nodes": six cards (`Node 1`..`Node 6`, bottles reporting, missing readings, status dot); `/data/X`: back button + four bottle tiles (40 px temperature, state line); `/data/X/bottle_Y`: back button, five sensor tiles (selected = 3 px accent outline) and a trend card with an auto-ranged Y axis, min/max labels and the latest value. Operator-facing numbers are 1-based |
| `screen_control.h/.cpp` | `/control`: setpoint card (−/+ 0.1 °C, 35.0–40.0, display only), experiment card (number and duration steppers with auto-repeat, status line), and a run card with the run hold button plus two equipment hold buttons, **Stirrers** and **Water pump** (display only, logic in the backlog). A title-row button sends one immediate `EXPERIMENT` line (development tool) |
| `screen_info.h/.cpp` | `/info` "System info": IP, network (same priority as the link chip), firmware, latest DB experiment, active run, upload queue, sensor-feed age, LVGL UI-thread load |
| `metrics.h/.cpp` | Verification instrumentation, compiled only with `RUSIM_METRICS=1` (`display-v9-metrics`); empty inline hooks otherwise (§16) |
| `screen_sleep.h/.cpp` | Idle timer on last-touch millis; backlight off after 60 s (`UI_SLEEP_MS`). The waking touch is swallowed (`sleep_filter_touch`); `sleep_wake()` turns it on for modals |

## 4. Screen tree / state machine (as built)

```
/setup (first route after boot; QR to WT32 login page) -> /home on first RESULT OK
/home -> /data (node picker) -> /data/X (4 bottle tiles) -> /data/X/bottle_Y
/home -> /control (setpoint, experiment number/duration, run, stirrers, pump)
/home -> /info (link, database, run, queue, feed, UI load)
[later LOGINREQ]  modal "Network login needed" + chip -> /setup -> back on RESULT OK
[any, post-login] chamber>=41 -> warn modal (ack; re-arms <41)
                  chamber>=45 -> trip modal (stirrers off; re-arms <45)
[any]  60 s no touch -> backlight off -> touch wakes only (swallowed) -> back on
[status bar] chip = link state (tap: /info or /setup); bell red iff alert fields
1 s lv_timer -> sens_apply_real() -> bell + in-place refresh of the open route
```

There is no boot gate on the UI side: `ui_init()` calls `ui_show_setup()` and
that route remains up until the WT32 reports a successful login or `IS_DEBUG`
bypasses it. Swipes switch the four top-level tabs; drill-down pages and
`/setup` ignore them. `ui_refresh_current()` calls the open route's
`screen_*_refresh()`, which changes text and colours only; widgets are rebuilt
only on a route change or a user tap that changes the page (e.g. picking a
sensor to chart). The modal overlay is a sibling of the content container.

## 5. USB control and diagnostic protocol

USB serial, 115200, newline-terminated, case-insensitive command word:

```
HELP | STATUS | LINK
PUSH                 request the real WT32 snapshot (HELLO); no generated values
SETPOINT 37.5        35.0-40.0, mirrors the /control stepper
DURATION 24          1-99 whole hours; pushes EXPERIMENT to the WT32
RUN ON|OFF           use the touchscreen DB-ready gate; keep the selected EXP
EXP 3                set the experiment number
STIRRER ON|OFF       also logs [FAKE->WT32] like the on-screen toggle
POST ON|OFF

MEM                  heap/PSRAM free, low-water and largest block
PASSTH ON|OFF        raw USB <-> IO17/IO18 tunnel (§10)
W32CMD <line>        send one arbitrary line to the WT32 (IS_DEBUG builds)
```

`TEMP`, `NODE`, `NULL`, and `UNNULL` are rejected: measured values are
read-only and cannot be replaced by synthetic USB commands.
`STATUS` prints run state + node list.
`LINK` prints the link state (`req/pair/result/user/exp`, `ip/ping/up/age`,
`db`, passthrough). `PASSTH ON|OFF` opens the raw USB <-> IO17/IO18 tunnel
used to flash or watch the WT32 (§10). `W32CMD <line>` forwards one arbitrary
line and exists only in `IS_DEBUG=1` builds.

`PUSH` only requests a snapshot; if the WT32 has no current sensor frame,
readings remain unavailable. It does not poll the Mega directly or force a
database read.

## 6. Bell / NULL semantics

At boot all 24 bottle slots are absent and all 120 sensor fields are NULL
(`fake_null_count()`, used by USB `STATUS`). The bell does not use that
number: `bellRed = fake_alarm_count() > 0`, which counts NULL fields only on
nodes that are online. A slot on an offline node is "not connected", not a
fault; a bottle missing from a live node's frame counts (5 fields). The bell
shows the count; tapping it lists `Node n, bottle m: <sensors> missing` (or
`no data`), at most 10 bottles. Missing sensors show `--` in
`UI_COL_ALARM` on the bottle tiles; absent or expired bottles show no live
data. Charts use gaps for missing samples and never manufacture points.

The separate chamber temperature is `--` ("No chamber sensor connected"):
`SENS` contains bottle readings, not a chamber sensor. No bottle temperature
is silently substituted into the chamber overheat logic. That logic is not a
working hardware safety system.

## 7. Hold-to-confirm implementation

`ui_hold_button.cpp` owns every binary toggle. `PRESSED` starts a 20 ms
progress timer and records the start tick; the timer fills the arc and calls
the commit callback once `lv_tick_elaps(start) >= UI_HOLD_TO_CONFIRM_MS`
(2000 ms). `RELEASED` before that deletes the timer and leaves the state
unchanged. The widget keeps its own `state`, so the caller only reacts to the
commit callback. Fill colour: `UI_COL_OK` when on, `UI_COL_IDLE` (grey) when
off; red is reserved for faults. Every caller shows the state in words next
to the button. Because routes are refreshed in place, a sensor frame arriving
mid-hold no longer deletes the button and cancels the hold.

## 8. Verify without hardware signals

```sh
python3 firmware/tests/display-sensors/run_tests.py
pio run -e display-v9 -e display-sim   # build only; no board required
SDL_VIDEODRIVER=dummy SIM_SHOTS=<dir> .pio/build/display-sim/program  # screenshots
```

The host suite compiles the actual model, link parser, and USB command
sources against a buffered serial/clock shim. It covers unavailable startup,
partial/malformed/nonfinite fields, successive frame sequences, observed-only
trends, missing bottles, expiry/recovery, HELLO invalidation, wire command
format/clamps/heartbeat, and USB command routing. Login credentials are not a
display-side concern and are not sent over the serial link.
Its UI spy verifies that USB RUN delegates to the shared gate; it does not
execute LVGL or exercise the real gate itself. It also covers the EXPERIMENT
boot gate (§10). Known failure since before 2026-11-21: "trend / one point per
new frame" (see `backlog.md`).

On device: before sensor frames arrive, confirm zero online bottles and NULL
readings. After a real Mega reply, check node 0 values and subsequent updates;
after 90 s without a fresh frame, check NULL/offline and empty charts. Verify
the experiment number stays selected across start/stop. Builds and host tests
do not replace this hardware check.

## 9. Known gaps / next pass

- The current real sensor transport covers only the Mega's four bottles on
  node 0; other nodes remain offline. Database capacity does not imply a
  sensor is connected. A separate chamber-temperature source is not wired.
- Real `→WT32` transport for setpoint, stirrers, water pump and POST
  (experiment state is real; the rest only prints `[FAKE->WT32] …`).
- After a display reboot an adopted run has no known start time: `/home`
  shows "elapsed unknown" until the WT32 reports a start time (not on the wire).
- Trend window is 12 min and a 90 s outage clears it (backlog).
- Backlight GPIO45 conflict resolution with EE.
- `LV_MEM_SIZE` is still 64 KB on the device (the sim uses 4 MB for snapshots).
- Duration sub-hour steps, breaker power-domain scope: parked open questions.
- Development tools still in normal builds (decision 2026-11-21): LVGL
  performance overlay, `IS_DEBUG` login bypass, "Push EXPERIMENT".

## 10. WT32 link (`rusim-line` v1, production)

The display and the WT32 are peers on a raw line protocol. The full wire spec
lives in `link-serial-protocol.md`; this section is the display-end summary.
USB `Serial` is debug only (§5 plus the `LINK` command). The link is
`Serial1`: RX IO18, TX IO17, 115200 8N1, common GND. `link_serial_init()`
drains boot noise and sends `HELLO`; `link_serial_poll()` runs every `loop()`
before `screen_setup_poll()`.

One ASCII line per message, `\n` terminated, 1024 bytes maximum. A partial
line is dropped after 50 ms of silence. Unknown words and display-direction
words arriving on RX are ignored silently, so power-on noise and a loopback
copy produce no action. There is no CRC and no ack: both ends repeat their
full truth every 5 s (`LINK_BEAT_MS`), and changes push immediately.

Received from the WT32:

| Word | Meaning |
|---|---|
| `HELLO` | WT32 booted; reset display-side link state |
| `LOGINREQ` | before the first login: route to `/setup`; after it: modal + `Login needed` chip (§11). Idempotent |
| `RESULT OK` / `RESULT FAIL` | login verdict: route home / stay on setup with retry text |
| `NET ip= up= [ping=]` | `/info` IP, link and latency store |
| `DB latest= time= bottles= drops= ready= aexp= adur= asrc= pexp= pdur= cfgmax= queued= reads= ack= state=` | `/info` DB, run-configuration and queue stores; `/control` start gate, source label and next-id hint. `-` = unknown, `latest=0` = no rows |
| `SENS b<i> ph=|nan ch4=|nan t=|nan co2=|nan p=|nan ...` | real node-0 bottle values from the Mega poll; `nan` = invalid sensor, absent `b<i>` = absent bottle (wire spec §3.1.1) |
| `LOG <text>` | print `[W32] <text>` on the display USB console, verbatim |

Sent to the WT32:

| Word | Meaning |
|---|---|
| `HELLO` | display booted; the WT32 replies with its full snapshot |
| `EXPERIMENT run= num= dur=` | on every change and every heartbeat, but only after the boot gate opens (below) |
| `DUMP` | debug-build bench command via `W32CMD`, never automatic |

**EXPERIMENT boot gate.** A freshly booted display does not know whether the
WT32 is in the middle of a run, and `EXPERIMENT run=0` would stop it. So after
`link_serial_init()` no `EXPERIMENT` line is sent (no change push, no
heartbeat). The WT32 answers the display's `HELLO` with its snapshot, whose
`DB` line carries `aexp`/`adur` for an active run. `link_serial_take_first_db()`
reports that first `DB` line once; `ui_poll()` then adopts an active run
(running, experiment number, duration; start time unknown) and calls
`link_serial_exp_release()`, which sends the current state and resumes normal
emission. A WT32 `HELLO` (WT32 reboot) does not close the gate again.

The WT32 serves the login page and collects the credentials locally. The
serial link carries only the gate request and verdict: `LOGINREQ` routes to
`/setup`; `RESULT OK` routes home but does NOT start the run; `RESULT FAIL`
keeps `/setup` up with retry guidance. The QR and typed URL use the WT32's
reported IP. The run starts from `/control` or `/home` (hold-to-confirm), which
flips `run=1` without incrementing `num`.

**Start gate and run configuration.** `ui_experiment_toggle()` refuses to start
while `link_serial_db_ready()` is false, and shows a modal explaining that the
WT32 has not completed a database configuration pull since boot. This is the
2026-11-10 decision: no cached or local configuration is substituted. The
operator still chooses the experiment number; on start the WT32 freezes that
run's configuration from the `device_data.experiments` row for that id (or the
display's duration when no row exists).

`/control` labels the next start as `exp N: DB <dur>h` or `exp N: local
<dur>h`, warns when the id already has readings, and during a run shows the
frozen snapshot (`run N: <dur>h DB|local`) so edits on that screen are visibly
pending for the next run. `/info` adds a **Run config** tile (frozen experiment/
duration/source, or `ready`/`waiting`) and a **Queue** tile (queued rows and the
ages of the last successful configuration read and acknowledged upload).

Every outbound command is echoed locally as one `[LINK>] <word> [args]` line.
A WT32 `HELLO` resets link state; credentials are not retained or re-pushed by
the display. A display `HELLO` makes the WT32 re-send `NET`/`DB`, `SENS`
(a frame fresh enough to be called current) and `RESULT OK` if it is running.

**Real sensor values.** `link_serial` stores each accepted `SENS` line as one
`LinkSensFrame` with a `seq` counter. A 1 s lv_timer (`on_sens_timer`,
`UI_SENS_APPLY_MS`) calls `sens_apply_real()`; the WT32 sends a frame about
every 5 s, so a frame reaches the screen within about 1 s. It copies new frames into
node 0 (`UI_REAL_CLUSTER`) bottle slots 0..3 — columns match `SensorId` order,
absent bottles and `nan` become NULL flags for that frame only (trend history is kept and shows a gap), and each trend ring shifts one
point per `UI_TREND_STEP_MS` (30 s), so its 24 points cover 12 minutes; between
steps the newest point is overwritten with the live value. Each stored frame receives the incremented sequence
number; otherwise later frames would be mistaken for duplicates and ignored.
Missing fields start as NAN, not zero, and numeric suffixes are rejected.
All nodes start offline; only node 0 currently receives real frames. A frame
older than 90 s (`UI_SENS_STALE_MS`) clears values, bottle presence, and trends
until the poll recovers. WT32 HELLO also invalidates its old cached frame.
There is never a fake fallback.

`PASSTH ON` suspends both parsers and turns the display into a raw USB <->
IO17/IO18 tunnel: use it to watch unprefixed WT32 ROM output or to flash the
WT32 through the display's USB, using the same `IO0` jumper and power-cycle
drill as a bare 4-wire adapter. `PASSTH OFF` at the start of a line closes it;
after binary traffic send a newline first. Runtime only. The USB port is a
CH340 bridge whose auto-reset circuit reboots the display on any ordinary
serial open, so an open monitor ends the tunnel and the flash tool must not
open the port itself (`link-serial-protocol.md` §7).

## 11. Setup route

`/setup` is a normal content route, not an overlay. `ui_refresh_current()`
does nothing on it; `screen_setup_poll()` keeps the QR and status current.

First login: the route is raised at boot and lowered to `/home` on the first
`RESULT OK`. The run state is not changed there (it is false at boot unless a
run was adopted from the WT32, §10).

Later logins (portal session expired, WT32 sends `LOGINREQ` again while
`g.loggedIn` is true): the operator is not pulled away. A WARN modal "Network
login needed" offers "Open login", and the status-bar chip shows
`Login needed` (tap opens `/setup`). The experiment keeps running. On
`RESULT OK` the modal closes and, if `/setup` is open, the previous route is
restored (`ui_show_previous()`). `RESULT FAIL` only changes the status text
unless the first login has not happened yet.

Verify: `pio run -e display-v9`; on device send `LINK` over USB serial to
print the link state. Short display IO18 to IO17 with the WT32 absent to
loop back lines: each emitted command must still produce no parse reply, which
proves the direction check is in place.

## 12. WT32 login page QR

The display does not run a web server or provision Wi-Fi. `/setup` shows a
280 px QR containing `http://<WT32-IP>/` and a typed URL for fallback. The IP
comes from the WT32's `NET ip=` message. Until an address is available, the QR
is hidden and the display says it is waiting for the WT32. The QR is re-encoded
only when the reported IP changes.

The phone opens the login form served by the WT32 on port 80. Credentials are
submitted directly to that board and never enter display memory or the serial
protocol. A `RESULT FAIL` received over `rusim-line` keeps `/setup` visible and
shows retry guidance; `RESULT OK` routes home. The WT32's web page handles its
own form result. See `network-master.md` for the HTTP login and security notes.

The page uses plain HTTP. The password is unencrypted between the phone and
WT32, so use a trusted LAN. The phone must be able to route to the WT32, and AP
client isolation can block that traffic. `display-provisioning-ap.md` records
the retired display-hosted provisioning design.

## 13. Light theme and the /setup QR

Two UI changes sit outside the screen-tree logic.

**Light theme.** `LV_USE_THEME_DEFAULT` stays `0`, so nothing is themed and
styling stays explicit in the `ui_mk_*` helpers. The palette and its colour
rules are in `ui_config.h` and §15.

**Setup QR.** `/setup` encodes only the WT32's HTTP URL. It is hidden until a
non-empty IP arrives and changes only when that address changes. `/info` shows
the reported network state and address as ordinary status data; it does not
start a SoftAP or host a login QR.

Verify: build `display-v9`; on hardware, confirm `/setup` shows a waiting state
before `NET ip=` arrives, then the matching `http://<ip>/` QR and typed URL
once the WT32 reports an address. This hardware check was not performed as part
of this documentation update.

## 14. Debug flag `IS_DEBUG`

`IS_DEBUG` is the compile-time gate for debug-only shortcuts in this UI.
Defined in `ui_config.h`, currently defaulting to `1` for development,
overridable per build with `-DIS_DEBUG=0` (a commented-out line sits in
`[env:display-v9]` in `platformio.ini`; `[env:display-sim]` inherits it).
Full detail lives in `display-debug-flag.md`.

Current effects: when `1`, `/setup` gains a `DEBUG: bypass login` button
(`screen_setup.cpp`) that runs the normal success path — `g.loggedIn`,
route to `/home` — without the WT32 `RESULT=success`, and
the USB console accepts `W32CMD <line>`. Boot still starts at the login
screen either way. Guarded by `#if IS_DEBUG`, so builds with `0` contain
none of the debug code.

Note for future agents: use `IS_DEBUG` for anything intentionally gated,
whether for safety (skipping auth, forcing outputs) or any other reason.
Branch on the existing flag instead of adding a new one, mark debug UI
with a `DEBUG:` prefix, and never enable it in a production flash.


## 15. Layout, colour and refresh rules

These rules came out of the 2026-11-21 UX audit
(`plans-and-audits/lvgl-ui-ux-audit.md`). New screens must follow them.

### 15.1 Render cost

The ESP32-S3 renders in software through a 1/4-screen buffer. Allowed: flat
fills, 1–3 px borders, rounded corners, labels, arcs, small circles, opaque
objects. Not used: animations, transitions, blur, shadows, gradients, partial
opacity over large areas, image scaling. The modal backdrop is opaque
(`UI_COL_SCRIM`) for that reason.

### 15.2 Grid

| Item | Value |
|---|---|
| Status bar | 800 × 48 (`UI_BAR_H`), `UI_COL_BAR` + 1 px `UI_COL_LINE` divider |
| Content | 800 × 432 below the bar |
| Page title | 20 px at y = 15; back button 56 × 40 at (16, 6) on drill-down pages |
| Cards | from y = 56 (`UI_PAGE_TOP`) to y = 416 (`UI_PAGE_BOTTOM`), 16 px margins and gaps, 12 px padding |
| Tabs | 100 × 40, icon + word, hit area extended to the bar height |
| Steppers | at least 56 × 52; −/+ symbols; auto-repeat (400 ms, then 100 ms) |
| Hold buttons | 96 px (run) and 80 px (equipment) |

### 15.3 Colour and text

| Token | Meaning |
|---|---|
| `UI_COL_ACCENT` | tappable outline, active tab, selected tile, links |
| `UI_COL_OK` | running, healthy, valid |
| `UI_COL_WARN` | needs attention (login needed, no internet, existing experiment id) |
| `UI_COL_ALARM` | faults only (missing readings, no WT32, start blocked) |
| `UI_COL_IDLE` | idle/off hold button fill |
| `UI_COL_LINE` | grey outline of information cards, chart grid |

All text colours pass 4.5:1 on white and on `UI_COL_BG`. Type scale: 14 px
captions and detail lines, 20 px titles and word values, 28 px numbers, 40 px
hero numbers (run progress, bottle temperature). Card captions are UPPERCASE;
everything else is sentence case. Temperatures use `UI_DEG_C` (°C). The
built-in fonts lack U+00B7, U+2026 and U+2212, so those characters are not
used. Changing numbers in status rows are right-aligned so only one edge moves.

### 15.4 Refresh and input

- Each screen builds its widgets in `show()`, keeps static pointers, nulls
  them in `hide()`, and updates text/colours in `refresh()`.
- `sleep_filter_touch()` reports the touch that wakes a dark screen as
  released until the finger lifts, so it cannot press a widget.
- `ui_modal_show_sev()` ranks INFO < WARN < ALARM; a lower-ranked modal never
  replaces a higher one. Every modal turns the backlight on.
- Status-bar chip priority: `No WT32` (no line for 15 s) > `Login needed` >
  `No internet` > `DB: <state>` > `Online`.
- The experiment-number suggestion (`max(latest, cfgmax) + 1`) runs from
  `ui_poll()`, only while idle, only when the DB answer changes, and never
  after the operator chose a number (stepper or USB `EXP`); the choice is
  cleared when a run starts.

## 16. Verification metrics build (`display-v9-metrics`)

`-D RUSIM_METRICS=1` compiles `metrics.cpp` and turns the hooks in
`metrics.h` from empty inline functions into real ones. Production
(`display-v9`) and the simulator compile the empty versions.

| Hook | Called from | Measures |
|---|---|---|
| `met_init_early()` | `setup()`, before `Serial.begin()` | sets an 8 KB USB TX buffer so console output does not block the loop |
| `met_boot(stage)` | `setup()`, the link, the render callback | boot milestones (`MET BOOT`) |
| `met_attach(display, indev)` | `setup()` | registers `LV_EVENT_INVALIDATE_AREA`, `RENDER_START`, `RENDER_READY` on the display and wraps the indev read timer callback |
| `met_touch(raw, swallowed, x, y)` | `my_touchpad_read()` | starts a touch-latency measurement on every press or release edge |
| `met_flush(px)` | `my_disp_flush()` | pixels written (`MET FPS px=`) |
| `met_link_line()`, `met_link_discard()` | `link_serial.cpp` | per-word line counts, discards, heartbeat gaps, the `[LINK<]` echo |
| `met_sens_rx/commit/applied()` | `handle_sens()`, `sens_apply_real()` | `SENS` receive -> rendered frame (`MET SENSRX`) |
| `met_hold()` | `ui_hold_button.cpp` | hold start, commit, cancel |
| `met_tick()` | end of `loop()` | prints queued results; 5 s `FPS`, 60 s `MEM` and `LINK` lines |
| `met_console()` | `serial_cmd.cpp` | `REBOOT`, `BOOTLOG`, `HEAPCHK` |

A touch measurement ends at the first `RENDER_READY` after an invalidation
made while the indev read timer was running, so only the touch's own redraw
counts; an edge that redraws nothing within 500 ms is logged with `nf=1`.
`ui_route_name()` (`ui_shell.cpp`) supplies the route for each result.
`-D RUSIM_MET_GPIO=<pin>` drives a free pin HIGH at the edge and LOW at the
answering frame, for logic-analyser validation. Definitions, line reference
and procedures: `verification-toolkit.md`.


---
Updated at 2026-11-24-12:34.
