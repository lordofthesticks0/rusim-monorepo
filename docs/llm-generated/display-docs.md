# Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)

## 1. Overview

`firmware/display-v9/` is the Display MCU firmware: Sunton 800×480 ST7262
(RGB565) + GT911 touch, ESP32-S3, LVGL v9.6.0, Arduino_GFX flush path.

The WT32 supplies login, network status, database state, experiment state,
and bottle readings over `rusim-line` (`link-serial-protocol.md`). Sensor
values start unavailable and change only when a real `SENS` frame arrives;
there is no fake seed, random jitter, or simulated sensor fallback. The
legacy filenames/types `fake_data` and `BottleFake` remain for compatibility.
Setpoint, stirrer and POST controls still lack real hardware transport.

Locked decisions honoured: no staleness UI, no PIN on Control,
hold-to-confirm 2000 ms on binary toggles, no local log storage, 41 °C warn /
45 °C breaker two-tier overheat, 60 s backlight sleep, duration 1–99 h,
5 sensors + trend per bottle, NULL-field bell with live list and no history.

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
| `main.cpp` | HW init (gfx, touch, flush, draw buf, indev) + `fake_init()` + `link_serial_init()` + `ui_init()`; `loop()` = USB serial poll → link poll → setup/home/control/info polls → clock → SoftAP poll → sleep tick → `lv_timer_handler()`, with all LVGL work suspended while the `PASSTH` tunnel is open |
| `lv_conf.h` | Enables vs stock: `BUTTON, SPINBOX (+TEXTAREA, required by spinbox), SWITCH, CHART, BUTTONMATRIX, QRCODE (+CANVAS, required by qrcode)` = 1; fonts +12/+20/+28. `KEYBOARD` is now 0 (credential entry moved to QR, §12). **`SLIDER` deliberately left 0** (not needed). `MSGBOX/LIST/TABLE` left 0 — modals and lists are hand-built from `obj+label+button` |
| `link_serial.h/.cpp` | `rusim-line` v1 peer on `Serial1` (RX 18 / TX 17, 115200 8N1). Line parser, 5 s heartbeats, all inbound/outbound words, log forwarding, `SENS` real sensor frames, `PASSTH` tunnel. `Serial` stays USB debug only. Full wire spec in §10 and `link-serial-protocol.md` |
| `screen_setup.h/.cpp` | `/setup` first-class route (route 6): 300 px QR (Wi-Fi join code, then the login URL) + SSID and passphrase readout + Start/Stop button. `ui_init()` opens it first; WT32 `LOGINREQ` re-raises it; `RESULT OK` routes home. No keyboard (§12); duration/exp number/run live on `/control`. The SoftAP stays up while either `/setup` or `/info` is showing (§13). `RESULT FAIL` puts the wrong-password text on the panel, and the phone sees the same verdict (§12) |
| `touch.h` | Unchanged GT911 glue |
| `ui_config.h` | Capacity (6 clusters × 4 bottles; all offline until reported), ranges, 2000 ms / 60 s timings, 5 s sensor apply / 90 s expiry, thresholds, pins, palette, `FW_VERSION`, `IS_DEBUG` |
| `fake_data.h/.cpp` | `AppState g`: controls and real bottle values; `fake_init()` starts all nodes offline, bottles absent, readings/history unavailable; `sens_apply_real()` applies WT32 frames to node 0 and clears missing/expired data. Bottle presence is independent of database capacity |
| `serial_cmd.h/.cpp` | Human-typeable debug protocol over USB serial (see §5). Display-local only. Also `CREDS` and `AP` (§12) |
| `provision_ap.h/.cpp` | SoftAP + web credential form on the display, staging into the same link pair as the `CREDS` serial command (§12). `/status` leads with a `waiting`/`success`/`fail` token and a reject gets the error page with a retry link |
| `ui_shell.h/.cpp` | Status bar, content router, single blocking-modal primitive, bell logic, overheat eval, `ui_mk_*` styled helpers (no LVGL theme is enabled, so all styling is explicit) |
| `screen_home.h/.cpp` | `/home`: DATA / CONTROL / INFO nav + run summary + serial hint |
| `screen_data.h/.cpp` | `/data` picker (6 clusters, offline greyed); `/data/X` node page = one 180×180 square per bottle, laid out like the `/info` tiles (bottle temperature on top, `Bottle n` caption below), greyed card when the node is offline, red value when the WT32 reported that temperature as NULL, and the square itself opens the chamber; `/data/X/bottle_Y` 5 sensor rows + `lv_chart` temp trend (values ×10, range 300–450) |
| `screen_control.h/.cpp` | `/control`: setpoint −/+ 0.1 °C (35.0–40.0, logs `[FAKE->WT32]`), duration/experiment-number steppers, and stirrer + run switches built from `ui_hold_button` (2000 ms hold, progress ring), plus a button that sends one immediate `EXPERIMENT` line for bench testing |
| `screen_info.h/.cpp` | `/info`: real link/database/configuration/queue tiles, firmware version, LVGL UI-thread load; storage usage shows `--` because the protocol supplies no measurement |
| `screen_sleep.h/.cpp` | Idle timer on last-touch millis; backlight off after 30 s, on at next touch tick |

## 4. Screen tree / state machine (as built)

```
/setup (first route after boot; QR credential form) -> /home on RESULT OK
/home -> /data (picker) -> /data/X (grid) -> /data/X/bottle_Y (5 sensors + chart)
/home -> /control (setpoint, duration, run/stirrer hold-2000ms)
/home -> /info (fresh on open; network QR + Start/Stop, shares the SoftAP with /setup)
[any, post-confirm]  chamber>=41 -> warn modal (ack; re-arms <41)
                     chamber>=45 -> trip modal (stirrer fake-OFF; re-arms <45)
[any]  30 s no touch -> backlight off -> touch -> back on
[status bar] bell red iff any NULL field -> tap -> live NULL list modal
5-s lv_timer -> sens_apply_real() -> bell + content refresh when data changes
```

There is no boot gate. `ui_init()` calls `ui_show_setup()` and that route
remains up until credentials are accepted or `IS_DEBUG` bypasses it.
Overheat modals reuse the single-modal primitive and can stack over any
screen. `ui_refresh_current()` re-runs the current route builder after
pushes/serial edits; the modal overlay is a sibling of the content container
so refreshes never dismiss it.

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

CREDS <user> <pass>  stage a portal pair (bench path; echoed scrollback)
AP                   toggle the credential SoftAP, print both QR payloads
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

`bellRed = fake_null_count() > 0`. At boot all 24 bottle slots are absent and
all 120 sensor fields are NULL. Only bottles reported by live WT32 frames
count as online. Tapping the bell lists unavailable fields. Missing/invalid
sensors show `-- NULL`; absent or expired bottles show no live data. Charts
use gaps for missing samples and never manufacture historical points.

The separate chamber temperature is `-- C`: `SENS` contains bottle readings,
not a chamber sensor. No bottle temperature is silently substituted into the
chamber overheat logic. That logic is not a working hardware safety system.

## 7. Hold-to-confirm implementation

`ui_hold_button.cpp` owns every binary toggle. `PRESSED` starts a 20 ms
progress timer and records the start tick; the timer fills the arc and calls
the commit callback once `lv_tick_elaps(start) >= UI_HOLD_TO_CONFIRM_MS`
(2000 ms). `RELEASED` before that deletes the timer and leaves the state
unchanged. The widget keeps its own `state`, so the caller only reacts to the
commit callback.

## 8. Verify without hardware signals

```sh
python3 firmware/tests/display-sensors/run_tests.py
pio run -e display-v9 -e display-sim   # build only; no board required
```

The host suite compiles the actual model, link parser, and USB command
sources against a buffered serial/clock shim. It covers unavailable startup,
partial/malformed/nonfinite fields, successive frame sequences, observed-only
trends, missing bottles, expiry/recovery, HELLO invalidation, wire command
format/clamps/heartbeat, credential encoding/redaction, and USB command routing.
Its UI spy verifies that USB RUN delegates to the shared gate; it does not
execute LVGL or exercise the real gate itself.

On device: before sensor frames arrive, confirm zero online bottles and NULL
readings. After a real Mega reply, check node 0 values and subsequent updates;
after 90 s without a fresh frame, check NULL/offline and empty charts. Verify
the experiment number stays selected across start/stop. Builds and host tests
do not replace this hardware check.

## 9. Known gaps / next pass

- The current real sensor transport covers only the Mega's four bottles on
  node 0; other nodes remain offline. Database capacity does not imply a
  sensor is connected. A separate chamber-temperature source is not wired.
- Real `→WT32` transport for setpoint, stirrer and POST (experiment state is
  real; the rest still only prints `[FAKE->WT32] …`).
- Backlight GPIO45 conflict resolution with EE.
- `LV_MEM_SIZE` is still 64 KB — bump if chart/table-heavy screens OOM.
- Duration sub-hour steps, wake-confirm UX, breaker power-domain scope: parked
  open questions, unchanged.

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
| `HELLO` | WT32 booted; clear verdict bookkeeping so the staged pair is re-pushed |
| `LOGINREQ` | route to `/setup` (idempotent); heartbeat while creds are needed |
| `RESULT OK` / `RESULT FAIL` | login verdict: route home / stay on setup with retry text |
| `NET ip= up= [ping=]` | `/info` IP, link and latency store |
| `DB latest= time= bottles= drops= ready= aexp= adur= asrc= pexp= pdur= cfgmax= queued= reads= ack= state=` | `/info` DB, run-configuration and queue stores; `/control` start gate, source label and next-id hint. `-` = unknown, `latest=0` = no rows |
| `SENS b<i> ph=|nan ch4=|nan t=|nan co2=|nan p=|nan ...` | real node-0 bottle values from the Mega poll; `nan` = invalid sensor, absent `b<i>` = absent bottle (wire spec §3.1.1) |
| `LOG <text>` | print `[W32] <text>` on the display USB console, verbatim |

Sent to the WT32:

| Word | Meaning |
|---|---|
| `HELLO` | display booted; the WT32 replies with its full snapshot |
| `EXPERIMENT run= num= dur=` | on every change and every heartbeat |
| `CREDS user= pass=` | percent-encoded pair, sent on staging and every heartbeat until a verdict |
| `DUMP` | debug-build bench command via `W32CMD`, never automatic |

Staging a pair (phone form or USB `CREDS`) calls
`link_serial_set_credentials()`, which sends `CREDS` immediately and repeats it
on each heartbeat until `RESULT` arrives. `LOGINREQ` routes to `/setup`;
`RESULT OK` routes home but does NOT start the run; `RESULT FAIL` keeps
`/setup` up with "wrong username or password" on the panel and on the phone
page (§12). The run starts from `/control` or `/home` (hold-to-confirm), which
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
`CREDS` echoes the word only, never the payload. A WT32 `HELLO` clears the
local verdict and re-pushes the kept pair, so the rebooted master can log in
without retyping. A display `HELLO` makes the WT32 re-send `NET`/`DB`, `SENS`
(a frame fresh enough to be called current) and `RESULT OK` if it is running.

**Real sensor values.** `link_serial` stores each accepted `SENS` line as one
`LinkSensFrame` with a `seq` counter. A 5 s lv_timer (`on_sens_timer`,
`UI_SENS_APPLY_MS`) calls `sens_apply_real()`, which copies new frames into
node 0 (`UI_REAL_CLUSTER`) bottle slots 0..3 — columns match `SensorId` order,
absent bottles and `nan` become NULL flags, and the temp trend ring gains one
point per applied frame. Each stored frame receives the incremented sequence
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
skips route 6, so sensor refreshes never disturb it.

The route was originally a two-textarea form with an `lv_keyboard`. That is
gone; §12 describes what replaced it. What survives: the route is raised by
`LOGINREQ` and lowered to `/home` on `RESULT OK`. The duration
spinbox moved to `/control`, so experiment duration and number can be
changed after login without re-authing.

Verify: `pio run -e display-v9`; on device send `LINK` over USB serial to
print the link state. Short display IO18 to IO17 with the WT32 absent to
loop back lines: each emitted command must still produce no parse reply, which
proves the direction check is in place.

## 12. Credential entry by QR (`provision_ap`)

The `/setup` keyboard has been removed. `LV_USE_KEYBOARD` is `0` and the
route carries no textareas.

`firmware/display-v9/provision_ap.{h,cpp}` raises a WPA2 SoftAP on demand with
a cloaked SSID, and serves a form at `http://192.168.4.1/`. `/setup` shows a
300 px QR that advances through two steps on its own:

1. `WIFI:T:WPA;S:RUSIM-SETUP-<MAC>;P:setup-<MAC>;H:true;;` — scanning joins the
   phone to the AP. Switches when `WiFi.softAPgetStationNum()` reports a station.
2. `http://192.168.4.1/` — scanning opens the credential form. Reverts to step 1
   if the phone disconnects.

`T:WPA` is the only legal token for a passphrase network, and `H:` must be
present when the SSID is cloaked. One constant (`kHidden`) drives both the radio
and the payload's `H:` field, so they cannot disagree.

Controls sit in a right-hand column: the SSID, the passphrase, and the
Start/Stop button. The passphrase is there for a phone that
scans the join code and does nothing — Settings → Other network, then type what
the panel shows. The duration spinbox moved to `/control`; the Confirm button
is gone; there is nothing on the panel to confirm.

Build flags: `PROVISION_AP_HIDDEN=0` broadcasts the SSID,
`PROVISION_AP_PASSWORD="secret"` (8–63 chars) replaces the MAC-derived
passphrase.

USB serial gained `AP` (toggle the AP, print both payloads) and
`CREDS <user> <pass>` (stage directly; the password reaches the scrollback, so
this is the bench path only).

Credentials remain RAM-only on both boards; nothing reaches NVS or flash.
`network-master` is unaware of this path: it only sees a staged pair.

A rejected pair is visible in three places (`display-provisioning-ap.md` §7.1):
the phone's result page turns red with an **Enter them again** link, the form
that link returns to carries a banner until the next pair is staged, and the
`/setup` status line carries the same wording. Retrying calls
`link_serial_set_credentials()`, which sends `CREDS` immediately, so the WT32
acts on the corrected pair on its next loop pass instead of on its next 5 s
heartbeat.

Full detail: the WIFI: payload format and escaping rules, the cloaked-network
tradeoff, the single-canvas re-encode, the `/status` state machine, and the
verification steps are in `display-provisioning-ap.md`.

## 13. Light theme and the /info network QR

Two UI changes sit outside the screen-tree logic.

**Light theme.** `LV_USE_THEME_DEFAULT` stays `0`, so nothing is themed and
styling stays explicit in the `ui_mk_*` helpers. The change is the palette in
`ui_config.h`: `UI_COL_BG` and the status-bar fill are light, `UI_COL_CARD` is
white, and the primary foreground is `UI_COL_TEXT` (near-black). That macro was
renamed from `UI_COL_WHITE`, whose value is no longer white. Two fills were
added: `UI_COL_BAR` for the status bar and `UI_COL_CARD_OFF` for a button that is
present but not usable (an offline node). `ui_mk_button()` gained a 1 px accent
border, which is what now separates a white button from a white card or the light
background. The modal ack button and the bell label keep light text because both
sit on a dark fill.

**Network QR on /info.** `/info` carries a 180 px QR and a Start/Stop button. It
runs the same two-step payload as `/setup`: the Wi-Fi join code first, then
`http://192.168.4.1/` once a phone associates. `screen_info_poll()` repaints it
each loop, so the payload follows association and an idle-timeout stop without a
route change. There is no "phone connected" modal here; the step label carries
that. It is smaller than the `/setup` code because the diagnostic rows share the
same route.

The SoftAP is shared with `/setup`. `screen_setup_poll()` no longer tears it down
when the route is neither `/setup` nor `/info`; `ui_is_info()` is the guard. That
keeps the rule that the credential form does not outlive the screens that offer
it, while letting a phone rejoin from `/info` after the run has started.

Verify: `pio run -e display-v9`; on device open `/info`, tap Start phone login,
scan with a phone — the code changes to the URL after the phone associates, and
the Home button stops the AP. With the debug flag on (the current
`ui_config.h` default), reach `/info` via the bypass and check the same.

## 14. Debug flag `IS_DEBUG`

`IS_DEBUG` is the compile-time gate for debug-only shortcuts in this UI.
Defined in `ui_config.h`, currently defaulting to `1` for development,
overridable per build with `-DIS_DEBUG=0` (a commented-out line sits in
`[env:display-v9]` in `platformio.ini`; `[env:display-sim]` inherits it).
Full detail lives in `display-debug-flag.md`.

Current effects: when `1`, `/setup` gains a `DEBUG: bypass login` button
(`screen_setup.cpp`) that runs the normal success path — `g.loggedIn`,
SoftAP stopped, route to `/home` — without the WT32 `RESULT=success`, and
the USB console accepts `W32CMD <line>`. Boot still starts at the login
screen either way. Guarded by `#if IS_DEBUG`, so builds with `0` contain
none of the debug code.

Note for future agents: use `IS_DEBUG` for anything intentionally gated,
whether for safety (skipping auth, forcing outputs) or any other reason.
Branch on the existing flag instead of adding a new one, mark debug UI
with a `DEBUG:` prefix, and never enable it in a production flash.

### Earlier documentation updates

Update by `opencode/glm-5.3-flash-free` in `opencode`: LVGL version references corrected
v9.3.0 -> v9.6.0, matching `platformio.ini` (`lvgl/lvgl@9.6.0`) and the 9.6 port.

Update by `opencode/deepseek-v4.1-flash-free` in `opencode`: Modbus RTU slave section replaced by the `rusim-line` v1 peer (`link-serial-protocol.md`); file map, serial commands (`LINK`, `PASSTH`, `W32CMD`) and known gaps updated.

Update by `codebuff/deepseek-v4.1-flash` in `freebuff`: §10 `PASSTH` note added on the CH340 auto-reset when the port is opened and on the line-start requirement for the exit word.

Update by `opencode/glm-5.3-flash-free` in `OpenCode`: §10 gained the `SENS` real-sensor word (node 0 feed, 5 s apply timer, 90 s staleness gate, fake-feed retirement) and the file map rows for `link_serial` and `fake_data` were updated.

Update by `LLM` in `Freebuff`: the `/data/X` node page was rebuilt as a row of 180×180 temperature squares that copies the `/info` tile layout (value font `montserrat_28_compressed` on top, `Bottle n` caption in font 14), and the chamber title lost its `(WT32)`/`(no live data)`/`(fake)` source tag.

---
Content generated by `LLM` in `Freebuff`.
