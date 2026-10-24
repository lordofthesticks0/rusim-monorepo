# Display UI — LLM Documentation (`display-v9`, fake-data pass)

## 1. Overview

`firmware/display-v9/` is the Display MCU firmware: Sunton 800×480 ST7262
(RGB565) + GT911 touch, ESP32-S3, LVGL v9.6.0, Arduino_GFX flush path.

This pass implements the screen tree from the Display UI scribble notes
**with fake sensor data**: no sensor chain is attached. The WT32 is real for
login, network status, DB state and experiment state over the serial link
(`link-serial-protocol.md`); a local `AppState` still supplies the bottle
readings, and a USB-serial line protocol lets a human mutate it. All open
questions from the notes are parked; each stub carries a `TODO(WT32|HW)`.

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
| `link_serial.h/.cpp` | `rusim-line` v1 peer on `Serial1` (RX 18 / TX 17, 115200 8N1). Line parser, 5 s heartbeats, all inbound/outbound words, log forwarding, `PASSTH` tunnel. `Serial` stays USB debug only. Full wire spec in §10 and `link-serial-protocol.md` |
| `screen_setup.h/.cpp` | `/setup` first-class route (route 6): 300 px QR (Wi-Fi join code, then the login URL) + SSID and passphrase readout + Start/Stop button. `ui_init()` opens it first; WT32 `LOGINREQ` re-raises it; `RESULT OK` routes home. No keyboard (§12); duration/exp number/run live on `/control`. The SoftAP stays up while either `/setup` or `/info` is showing (§13). `RESULT FAIL` puts the wrong-password text on the panel, and the phone sees the same verdict (§12) |
| `touch.h` | Unchanged GT911 glue |
| `ui_config.h` | Counts (6 clusters × 4 bottles, 2 online), ranges, 2000 ms / 60 s / 5 min timings, thresholds, pins, light palette (§13), `FW_VERSION`, `IS_DEBUG` debug gate (§14) |
| `fake_data.h/.cpp` | `AppState g`: experiment, setpoint, stirrer, POST, chamber temp, 6×4 bottles × 5 sensors + NULL flags + temp trend ring; `fake_init()` seeds 2 online / 4 offline; `fake_wt32_push()` jitters online values every 5 min (lv_timer) |
| `serial_cmd.h/.cpp` | Human-typeable debug protocol over USB serial (see §5). Display-local only. Also `CREDS` and `AP` (§12) |
| `provision_ap.h/.cpp` | SoftAP + web credential form on the display, staging into the same link pair as the `CREDS` serial command (§12). `/status` leads with a `waiting`/`success`/`fail` token and a reject gets the error page with a retry link |
| `ui_shell.h/.cpp` | Status bar, content router, single blocking-modal primitive, bell logic, overheat eval, `ui_mk_*` styled helpers (no LVGL theme is enabled, so all styling is explicit) |
| `screen_home.h/.cpp` | `/home`: DATA / CONTROL / INFO nav + run summary + serial hint |
| `screen_data.h/.cpp` | `/data` picker (6 clusters, offline greyed), `/data/X` 2×2 bottle grid ("node offline" inline), `/data/X/bottle_Y` 5 sensor rows + `lv_chart` temp trend (values ×10, range 300–450) |
| `screen_control.h/.cpp` | `/control`: setpoint −/+ 0.1 °C (35.0–40.0, logs `[FAKE->WT32]`), duration/experiment-number steppers, and stirrer + run switches built from `ui_hold_button` (2000 ms hold, progress ring), plus a button that sends one immediate `EXPERIMENT` line for bench testing |
| `screen_info.h/.cpp` | `/info`: rebuilt on every open; fake storage/IP/HTTP/FW rows + real heap/PSRAM rows + fetch timestamp + a 180 px network QR with its own Start/Stop (§13). `screen_info_poll()` keeps the QR in step each loop |
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
5-min lv_timer -> fake_wt32_push() -> bell + content refresh
```

There is no boot gate. `ui_init()` calls `ui_show_setup()` and that route
remains up until credentials are accepted or `IS_DEBUG` bypasses it.
Overheat modals reuse the single-modal primitive and can stack over any
screen. `ui_refresh_current()` re-runs the current route builder after
pushes/serial edits; the modal overlay is a sibling of the content container
so refreshes never dismiss it.

## 5. Fake-data serial protocol

USB serial, 115200, newline-terminated, case-insensitive command word:

```
HELP | STATUS | PUSH | LINK
TEMP 38.3            chamber temp (drives 41/45 UI; TRY: TEMP 41.5, TEMP 45.2, TEMP 37.0)
SETPOINT 37.5        35.0-40.0, mirrors the /control stepper
DURATION 24          1-99 whole hours; pushes EXPERIMENT to the WT32
RUN ON|OFF           start/stop the experiment; each start increments EXP
EXP 3                set the experiment number
STIRRER ON|OFF       also logs [FAKE->WT32] like the on-screen toggle
POST ON|OFF
NODE <0-5> ON|OFF    OFF nulls the whole node; ON clears its NULLs
NULL <c> <b> <s>     set one field NULL   (s: 0 CO2,1 CH4,2 Press,3 pH,4 Temp)
UNNULL <c> <b> <s>   clear one field NULL
CREDS <user> <pass>  stage a portal pair (bench path; echoed scrollback)
AP                   toggle the credential SoftAP, print both QR payloads
MEM                  heap/PSRAM free, low-water and largest block
PASSTH ON|OFF        raw USB <-> IO17/IO18 tunnel (§10)
W32CMD <line>        send one arbitrary line to the WT32 (IS_DEBUG builds)
```

There are deliberately **no on-screen debug buttons/modals for faking** —
all injection goes through this port. `STATUS` prints run state + node list.
`LINK` prints the link state (`req/pair/result/user/exp`, `ip/ping/up/age`,
`db`, passthrough). `PASSTH ON|OFF` opens the raw USB <-> IO17/IO18 tunnel
used to flash or watch the WT32 (§10). `W32CMD <line>` forwards one arbitrary
line and exists only in `IS_DEBUG=1` builds.

> Display→WT32 login, network status, DB state and experiment state are real
> over the serial link (see §10); the remaining fake part is the sensor-data
> push, still via `fake_wt32_push()`.

## 6. Bell / NULL semantics (fake)

`bellRed = fake_null_count() > 0`. At boot this is **red by design**: nodes
2–5 are "not connected", i.e. 80 NULL fields. Tapping the bell shows up to 10
live `C/B/sensor` lines plus a "+N more" tail. Offline clusters render inline
"node offline" labels — never blank, never stale badges.

## 7. Hold-to-confirm implementation

`ui_hold_button.cpp` owns every binary toggle. `PRESSED` starts a 20 ms
progress timer and records the start tick; the timer fills the arc and calls
the commit callback once `lv_tick_elaps(start) >= UI_HOLD_TO_CONFIRM_MS`
(2000 ms). `RELEASED` before that deletes the timer and leaves the state
unchanged. The widget keeps its own `state`, so the caller only reacts to the
commit callback.

## 8. Verify without hardware signals

```sh
pio run -e display-v9   # compile only; board need not be connected
```

On device (USB serial 115200): confirm the gate at 24 h → walk
Home → Data → Cluster 0 → Bottle → back; open Cluster 4 (offline); tap bell;
`/control` step setpoint, hold a toggle; `/info` twice (timestamps differ);
`TEMP 41.5` → warn modal → ack; `TEMP 45.2` → trip modal; `TEMP 37` → re-arm;
`NODE 2 ON` → bell count drops; wait 60 s → backlight off → touch → on.

## 9. Known gaps / next pass

- Real WT32 sensor-data push over the serial link (login, network, DB and
  experiment state are already real; `fake_wt32_push()` still jitters local
  values).
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
| `DB latest= time= bottles= drops=` | `/info` DB store; `-` = unknown, `latest=0` = no rows |
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
page (§12). The run starts from `/control` (hold-to-confirm), which flips
`run=1` and increments `num`.

Every outbound command is echoed locally as one `[LINK>] <word> [args]` line.
`CREDS` echoes the word only, never the payload. A WT32 `HELLO` clears the
local verdict and re-pushes the kept pair, so the rebooted master can log in
without retyping. A display `HELLO` makes the WT32 re-send `NET`/`DB` and
`RESULT OK` if it is running.

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
skips route 6, so the 5-minute fake push never disturbs it.

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
the Home button stops the AP. Under `-DIS_DEBUG=1`, reach `/info` via the bypass
and check the same.

## 14. Debug flag `IS_DEBUG`

`IS_DEBUG` is the compile-time gate for debug-only shortcuts in this UI.
Defined in `ui_config.h`, default `0`, overridable per build with
`-DIS_DEBUG=1` (a commented-out line sits in `[env:display-v9]` in
`platformio.ini`; `[env:display-sim]` inherits it). Full detail lives in
`display-debug-flag.md`.

Current effect: when `1`, `/setup` gains a `DEBUG: bypass login` button
(`screen_setup.cpp`) that runs the normal success path — `g.loggedIn`,
SoftAP stopped, route to `/home` — without the WT32 `RESULT=success`.
Boot still starts at the login screen either way. Guarded by
`#if IS_DEBUG`, so release builds contain none of the debug code.

Note for future agents: use `IS_DEBUG` for anything intentionally gated,
whether for safety (skipping auth, forcing outputs) or any other reason.
Branch on the existing flag instead of adding a new one, mark debug UI
with a `DEBUG:` prefix, and never enable it in a production flash.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.

Update by `opencode/glm-5.3-flash-free` in `opencode`: LVGL version references corrected
v9.3.0 -> v9.6.0, matching `platformio.ini` (`lvgl/lvgl@9.6.0`) and the 9.6 port.

Update by `opencode/deepseek-v4.1-flash-free` in `opencode`: Modbus RTU slave section replaced by the `rusim-line` v1 peer (`link-serial-protocol.md`); file map, serial commands (`LINK`, `PASSTH`, `W32CMD`) and known gaps updated.

Update by `codebuff/deepseek-v4.1-flash` in `freebuff`: §10 `PASSTH` note added on the CH340 auto-reset when the port is opened and on the line-start requirement for the exit word.
