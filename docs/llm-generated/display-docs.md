# Display UI — LLM Documentation (`display-v9`, fake-data pass)

## 1. Overview

`firmware/display-v9/` is the Display MCU firmware: Sunton 800×480 ST7262
(RGB565) + GT911 touch, ESP32-S3, LVGL v9.3.0, Arduino_GFX flush path.

This pass implements the screen tree from the Display UI scribble notes
**with fake data only**: nothing is connected. The WT32 (Modbus master, 5-min
push) does not exist yet, so a local `AppState` pretends to be "the latest
cycle" and a USB-serial line protocol lets a human mutate it. All open
questions from the notes are parked; each stub carries a `TODO(WT32|HW)`.

Locked decisions honoured: no staleness UI, no PIN on Control,
hold-to-confirm 500 ms on binary toggles, no local log storage, 41 °C warn /
45 °C breaker two-tier overheat, 30 s backlight sleep, duration 1–99 h,
5 sensors + trend per bottle, NULL-field bell with live list and no history.

## 2. Hardware / platform notes

| Item | Detail |
|---|---|
| Panel | `Arduino_RPi_DPI_RGBPanel`, 800×480, 16 MHz PCLK, `auto_flush` (`main.cpp`) |
| Bus | `Arduino_ESP32RGBPanel`, DE 40 / VSYNC 41 / HSYNC 39 / PCLK 42, R 45/48/47/21/14, G 5/6/7/15/16/4, B 8/3/46/9/1 |
| Touch | GT911, SDA 19 / SCL 20 (`touch.h`), read via `my_touchpad_read()` |
| Backlight | `TFT_BL = 2`. Notes say GPIO45, but **GPIO45 is R0 of the RGB bus** — do not move without EE sign-off (`ui_config.h`, `screen_sleep.cpp`) |
| Draw buffer | 800·480/4·2 ≈ 187.5 KB `MALLOC_CAP_INTERNAL`, `RENDER_MODE_PARTIAL`, RGB565 |
| Env | `display-v9` in `platformio.ini`: esp32-s3-devkitc-1, qio_opi, 16 MB flash, opi PSRAM, lvgl 9.3.0 |

## 3. File map

| File | Role |
|---|---|
| `main.cpp` | HW init (gfx, touch, flush, draw buf, indev) + `fake_init()` + `link_modbus_init()` + `ui_init()`; `loop()` = USB serial poll → Modbus slave poll → boot-gate poll → sleep tick → `lv_timer_handler()` |
| `lv_conf.h` | Enables vs stock: `BUTTON, SPINBOX (+TEXTAREA, required by spinbox), SWITCH, CHART, BUTTONMATRIX, QRCODE (+CANVAS, required by qrcode)` = 1; fonts +12/+20/+28. `KEYBOARD` is now 0 (credential entry moved to QR, §12). **`SLIDER` deliberately left 0** (not needed). `MSGBOX/LIST/TABLE` left 0 — modals and lists are hand-built from `obj+label+button` |
| `link_modbus.h/.cpp` | Modbus RTU slave ID 1 on `Serial1` (RX 18 / TX 17, 9600 8N1). FC 0x03/0x06/0x10 + CRC16. Register map in §10. `Serial` stays USB debug only. Also owns the poll-request output (IO11 -> WT32 IO14): `link_modbus_set_credentials()` pulses it so the master re-reads a new pair at once |
| `screen_setup.h/.cpp` | `/setup` first-class route: 300 px QR (Wi-Fi join code, then the login URL) + SSID and passphrase readout + Start/Stop button. Shown first on boot and on WT32 `LOGIN_REQ`; routes home only on `RESULT=success`. No keyboard (§12); duration/exp number/run live on `/control`. It owns the SoftAP except while `/info` is up (§13). `RESULT=fail` puts the wrong-password text on the panel, and the phone sees the same verdict (§12) |
| `touch.h` | Unchanged GT911 glue |
| `ui_config.h` | Counts (6 clusters × 4 bottles, 2 online), ranges, 500 ms / 30 s / 5 min timings, thresholds, pins, light palette (§13), `FW_VERSION`, `IS_DEBUG` debug gate (§14) |
| `fake_data.h/.cpp` | `AppState g`: experiment, setpoint, stirrer, POST, chamber temp, 6×4 bottles × 5 sensors + NULL flags + temp trend ring; `fake_init()` seeds 2 online / 4 offline; `fake_wt32_push()` jitters online values every 5 min (lv_timer) |
| `serial_cmd.h/.cpp` | Human-typeable debug protocol over USB serial (see §5). Display-local only. Also `CREDS` and `AP` (§12) |
| `provision_ap.h/.cpp` | SoftAP + web credential form on the display, staging into the same Modbus registers as the `CREDS` serial command (§12). `/status` leads with a `waiting`/`success`/`fail` token and a reject gets the error page with a retry link |
| `ui_shell.h/.cpp` | Status bar, content router, single blocking-modal primitive, bell logic, overheat eval, `ui_mk_*` styled helpers (no LVGL theme is enabled, so all styling is explicit) |
| `screen_boot.h/.cpp` | `/boot` blocking gate: spinbox 1–99 h ±1 h + confirm; must confirm every boot |
| `screen_home.h/.cpp` | `/home`: DATA / CONTROL / INFO nav + run summary + serial hint |
| `screen_data.h/.cpp` | `/data` picker (6 clusters, offline greyed), `/data/X` 2×2 bottle grid ("node offline" inline), `/data/X/bottle_Y` 5 sensor rows + `lv_chart` temp trend (values ×10, range 300–450) |
| `screen_control.h/.cpp` | `/control`: setpoint −/+ 0.1 °C (35.0–40.0, logs `[FAKE->WT32]`), stirrer + POST switches with hold-to-confirm progress bars, plus a `PULSE INT` button that calls `link_notify_pulse()` so the poll-request line can be exercised by hand |
| `screen_info.h/.cpp` | `/info`: rebuilt on every open; fake storage/IP/HTTP/FW rows + real heap/PSRAM rows + fetch timestamp + a 180 px network QR with its own Start/Stop (§13). `screen_info_poll()` keeps the QR in step each loop |
| `screen_sleep.h/.cpp` | Idle timer on last-touch millis; backlight off after 30 s, on at next touch tick |

## 4. Screen tree / state machine (as built)

```
/boot (blocking gate, spinbox 1-99h, confirm) -> /home
/home -> /data (picker) -> /data/X (grid) -> /data/X/bottle_Y (5 sensors + chart)
/home -> /control (setpoint, stirrer hold-500ms, POST hold-500ms)
/home -> /info (fresh on open; network QR + Start/Stop, raises the same SoftAP as /setup)
[any, post-confirm]  chamber>=41 -> warn modal (ack; re-arms <41)
                     chamber>=45 -> trip modal (stirrer fake-OFF; re-arms <45)
[any]  30 s no touch -> backlight off -> touch -> back on
[status bar] bell red iff any NULL field -> tap -> live NULL list modal
5-min lv_timer -> fake_wt32_push() -> bell + content refresh
```

Boot gate is an overlay on the active screen (not in the content container),
so it blocks all routes until confirmed. Overheat modals reuse the same
single-modal primitive and can stack over any screen. `ui_refresh_current()`
re-runs the current route builder after pushes/serial edits; the modal
overlay is a sibling of the content container so refreshes never dismiss it.

## 5. Fake-data serial protocol

USB serial, 115200, newline-terminated, case-insensitive command word:

```
HELP | STATUS | PUSH | LINK
TEMP 38.3            chamber temp (drives 41/45 UI; TRY: TEMP 41.5, TEMP 45.2, TEMP 37.0)
SETPOINT 37.5        35.0-40.0, mirrors the /control stepper
DURATION 24          1-99 whole hours, quick-polled by the master
RUN ON|OFF           start/stop the experiment; each start increments EXP
EXP 3                set the experiment number
STIRRER ON|OFF       also logs [FAKE->WT32] like the on-screen toggle
POST ON|OFF
NODE <0-5> ON|OFF    OFF nulls the whole node; ON clears its NULLs
NULL <c> <b> <s>     set one field NULL   (s: 0 CO2,1 CH4,2 Press,3 pH,4 Temp)
UNNULL <c> <b> <s>   clear one field NULL
```

There are deliberately **no on-screen debug buttons/modals for faking** —
all injection goes through this port. `STATUS` prints run state + node list.
`LINK` prints the Modbus slave state (`req/ready/result/user/dur`).

> Display→WT32 login transport is now real Modbus RTU (see §10); the
> remaining fake part is the sensor-data push, still via `fake_wt32_push()`.

## 6. Bell / NULL semantics (fake)

`bellRed = fake_null_count() > 0`. At boot this is **red by design**: nodes
2–5 are "not connected", i.e. 80 NULL fields. Tapping the bell shows up to 10
live `C/B/sensor` lines plus a "+N more" tail. Offline clusters render inline
"node offline" labels — never blank, never stale badges.

## 7. Hold-to-confirm implementation

`screen_control.cpp` `HoldCtx` per toggle: `PRESSED` arms a 50 ms progress
timer + records `t0`; `VALUE_CHANGED` commits only if `now - t0 >= 500 ms`,
else reverts the switch to the committed `*flag` (guarded against recursive
events) and shows a hint. Early release with no value change just disarms.

## 8. Verify without hardware signals

```sh
pio run -e display-v9   # compile only; board need not be connected
```

On device (USB serial 115200): confirm the gate at 24 h → walk
Home → Data → Cluster 0 → Bottle → back; open Cluster 4 (offline); tap bell;
`/control` step setpoint, hold a toggle; `/info` twice (timestamps differ);
`TEMP 41.5` → warn modal → ack; `TEMP 45.2` → trip modal; `TEMP 37` → re-arm;
`NODE 2 ON` → bell count drops; wait 30 s → backlight off → touch → on.

## 9. Known gaps / next pass

- Real WT32 sensor-data push over the Modbus link (today only login
  transport is real; `fake_wt32_push()` still jitters local values).
- Real `→WT32` command transport (today: `Serial.printf("[FAKE->WT32] …")`).
- Backlight GPIO45 conflict resolution with EE.
- `LV_MEM_SIZE` is still 64 KB — bump if chart/table-heavy screens OOM.
- Duration sub-hour steps, wake-confirm UX, breaker power-domain scope: parked
  open questions, unchanged.

## 10. WT32 login link (Modbus RTU slave, production)

The display is the Modbus slave; the WT32 `network-master` env is the master.
The full wire spec — wiring, framing, register map, transaction patterns —
lives in `display-wt32-link.md`; this section is the display-end summary.
USB `Serial` is debug only (see §5 plus the `LINK` command). The link is
`Serial1`: RX IO18, TX IO17, 9600 8N1, common GND. `link_modbus_init()`
drains boot noise; `link_modbus_poll()` runs every `loop()` before
`screen_setup_poll()`.

Supported functions: `0x03` read holding, `0x06` write single, `0x10` write
multiple. Anything else returns exception `0x01`; unknown addresses return
`0x02`. Fixed-length requests parse eagerly at 8 bytes; variable-length ones
parse after 8 ms of line silence. CRC is standard Modbus CRC16.

| Address | Name | Write owner | Meaning |
|---|---|---|---|
| `0x0000` | LOGIN_REQ | master | `1` = show the login gate |
| `0x0001` | CREDS_READY | slave sets, master clears | `1` = staged creds ready |
| `0x0002` | RESULT | master | `0` none, `1` success, `2` fail |
| `0x0003` | EXP_RUNNING | read-only | `1` = experiment under way |
| `0x0004` | EXP_NUM | read-only | current experiment number |
| `0x0010-0x002F` | USERNAME | read-only | 64 B ASCII, big-endian, NUL-padded |
| `0x0030-0x004F` | PASSWORD | read-only | same encoding as username |
| `0x0060` | DURATION_H | read-only | 1-99, quick-polled by the master |
| `0x0070-0x0077` | IP_ADDR | master | 16 B ASCII, shown live in /info |
| `0x0078` | PING_MS | master | ms to 1.1.1.1, `0xFFFF` = none, shown live in /info |
| `0x0079` | NET_UP | master | `1` = WT32 holds IP |

`LOGIN_REQ=1` from the master routes to `/setup`. Staging a pair (phone form
or `CREDS`) sets `CREDS_READY=1`, shows "waiting for WT32", and pulses the
poll-request line on IO11 so the master reads it on its next loop pass instead
of up to 5 s later. `RESULT=1` routes home but does NOT start the run;
`RESULT=2` stays in setup with "wrong username or password" on the status line,
while the phone's own page shows the same verdict and a retry link (§12). The
master writes `CREDS_READY=0` after each read; `LOGIN_REQ=0` only on success.
The run starts from `/control` (hold-to-confirm), which flips `EXP_RUNNING` and
increments `EXP_NUM`.

The poll-request line is one wire, Display IO11 -> WT32 IO14, idle LOW, and it
carries no data: a 20 ms HIGH pulse means "poll now". The WT32 picks it up in an
interrupt and consumes it in its credential stage. A pulse that arrives while
the master is logging in is latched, so the retry after a failure is polled
twice as fast as the 5 s cycle allows. Full spec: `display-wt32-link.md` §4.3.

## 11. Setup route

`/setup` is a normal content route, not an overlay. `ui_refresh_current()`
skips route 6, so the 5-minute fake push never disturbs it.

The route was originally a two-textarea form with an `lv_keyboard`. That is
gone; §12 describes what replaced it. What survives: the route is raised by
`LOGIN_REQ` and lowered to `/home` on `RESULT=success`. The duration
spinbox moved to `/control`, so experiment duration and number can be
changed after login without re-authing.

Verify: `pio run -e display-v9`; on device send `LINK` over USB serial to
print `req/ready/result/user/dur`. Short RX to TX on the display alone to
loop back Modbus frames during development.

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
`/setup` status line carries the same wording. Retrying stages the pair and
pulses the poll-request line (§10), so the WT32 re-reads it on the next loop
pass instead of on its next 5 s cycle.

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
Content generated by `opencode/muse-spark-1.3-contributor-free` in `opencode`.
