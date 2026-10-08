# LVGL UI redesign (`display-v9`)

Working document for the UI redesign. Every state and tab the `display-v9` build
has today gets its own section. The **As built** block records what the code does
now, so a redesign can be argued against facts. The **Redesign** table under each
section is empty on purpose: nothing has been decided yet.

Source of truth for the as-built notes: `firmware/display-v9/` at the time of
writing. Screen geometry is in pixels; the panel is 800×480. Coordinates are
relative to the widget's parent (the 800×436 content container at y = 44, unless
stated otherwise).

`display-docs.md` covers the screen tree and the fake-data pass. This document
covers layout and interaction detail only, and goes stale faster.

---

## 1. Inventory

### 1.1 Router (`ui_shell.cpp`, `s_route`)

| Route id | Route | Builder | Back target |
|---|---|---|---|
| 6 | `/setup` | `screen_setup_show()` | none; exits to `/home` on `RESULT=success` |
| 0 | `/home` | `screen_home_show()` | none |
| 1 | `/data` | `screen_data_picker_show()` | `/home` |
| 2 | `/data/X` | `screen_cluster_show()` | `/data` |
| 3 | `/data/X/bottle_Y` | `screen_bottle_show()` | `/data/X` |
| 4 | `/control` | `screen_control_show()` | `/home` |
| 5 | `/info` | `screen_info_show()` | `/home` |

`render_current()` does `lv_obj_clean(s_content)` and rebuilds the whole route
from scratch. There is no widget state that survives a route change, and no
transition animation.

### 1.2 Persistent shell

| Element | Where |
|---|---|
| Status bar, 800×44 | `build_status_bar()` |
| Content container, 800×436 at y = 44 | `ui_init()` |
| Blocking modal primitive | `ui_modal_show()` / `ui_modal_hide()` |
| 5-minute fake push timer | `on_push_timer()` |

### 1.3 Non-route states

| State | Where |
|---|---|
| Blocking modal | `ui_shell.cpp` `ui_modal_show()` |
| Overheat warn, 41 °C | `ui_eval_overheat()` |
| Overheat trip, 45 °C | `ui_eval_overheat()` |
| Bell / invalid-field list | `on_bell_clicked()` |
| Phone-associated popup | `screen_setup.cpp` `qr_refresh()` |
| Backlight sleep | `screen_sleep.cpp` |
| SoftAP lifecycle | `provision_ap.cpp` |
| Debug-only UI | `#if IS_DEBUG` |

---

## 2. Cross-cutting

### 2.1 Styling

- `LV_USE_THEME_DEFAULT` is `0`. Every widget is styled inline through
  `ui_mk_label()`, `ui_mk_button()`, `ui_mk_card()`, plus local
  `lv_obj_set_style_*` calls where a screen needs something different.
- No LVGL style reuse: repeated properties (the 1 px `UI_COL_ACCENT` button
  border, the 10 px card radius) are re-set on every object.
- Palette in `ui_config.h`: `UI_COL_BG` 0xEEF2F7, `UI_COL_CARD` 0xFFFFFF,
  `UI_COL_BAR` 0xDCE3EC, `UI_COL_CARD_OFF` 0xD5DBE3, `UI_COL_ACCENT` 0x2C6E9B,
  `UI_COL_TEXT` 0x1B2330, `UI_COL_DIM` 0x5B6472, `UI_COL_OK` 0x1E8E4E,
  `UI_COL_WARN` 0xB87314, `UI_COL_ALARM` 0xC0392B.
- Fonts: `lv_font_montserrat_14` for body, `_20` for titles and the setpoint
  value. No small or large size, no monospace digit font for numbers.
- `screen_control.cpp` `mk_spin()` restyles the spinbox with literals
  (`lv_color_white()`, `lv_color_black()`) instead of palette macros.

### 2.2 Navigation

- Each screen draws its own back button. There is no shell-level back affordance,
  no breadcrumb, no title bar, no tab strip.
- `/data` is a three-level drill-down (nodes → chambers → sensors) with a
  `< Home` at the bottom of each level. Only `/home` is reachable from anywhere.
- The status bar has no navigation control, so the way back to `/home` from a
  deep `/data` level is three taps.

### 2.3 Data refresh

- `ui_refresh_current()` rebuilds the route. It returns early when
  `g.loggedIn` is false and for route 6.
- Sources of a rebuild: the 5-minute `on_push_timer()`, `ui_shell_on_data()`
  after any mutation, `db_refresh_warn()` on `/control` (label only, no rebuild),
  `info_qr_refresh()` on `/info` (label and QR only, no rebuild).
- `screen_setup_poll()` and `screen_info_poll()` run every `loop()`; both diff
  their own strings (`s_qrShown`, `s_stepShown`, `s_apBtnShown`) to avoid
  re-encoding the QR canvas or re-allocating a label buffer.
- `/info` is rebuilt on every open, so its "fetched at" stamp only changes on
  navigation, not on a timer.

### 2.4 Real vs fake labelling

- Roughly every value carries a literal `(fake)` or a `/path (fake data)` title.
  Real Modbus values are labelled `(real, WT32)`; heap and PSRAM are labelled
  `(real)`.
- The status bar reads `NET OK(fake)` as a fixed string and never updates.
- `main.cpp` serial banner says `fake-data pass`. `FW_VERSION` is
  `v9.6-fake-0.2.0`.

### 2.5 Write commands

- `/control` owns every command that changes state: setpoint, stirrer, POST, run,
  duration, experiment number.
- Binary toggles use the 500 ms hold-to-confirm in `screen_control.cpp`
  (`HoldCtx`): `PRESSED` records `t0` and starts a 50 ms progress timer;
  `VALUE_CHANGED` commits only if `millis() - t0 >= UI_HOLD_TO_CONFIRM_MS`,
  otherwise the switch is reverted and the hint reads `too short: hold 500ms.`
- The setpoint `-`/`+` buttons and the duration/exp `-`/`+` buttons have no
  confirmation at all; each press logs one `[FAKE->WT32]` line.
- `PULSE INT` (bench control for the Display IO11 → WT32 IO14 poll-request line)
  is in the production layout, not behind `IS_DEBUG`.

### 2.6 LVGL configuration

- `lv_conf.h` enables `BUTTON`, `SPINBOX`, `SWITCH`, `CHART`, `BUTTONMATRIX`,
  `QRCODE`, `CANVAS`. `KEYBOARD` is 0. `SLIDER`, `MSGBOX`, `LIST`, `TABLE`,
  `THEME_DEFAULT` are all 0.
- `LV_MEM_SIZE` is 64 KB. The 300 px QR canvas costs ~11.4 KB of it
  (`screen_setup.cpp` comment).
- Display: `LV_DISPLAY_RENDER_MODE_PARTIAL`, RGB565, one draw buffer of
  800·480/4·2 ≈ 187.5 KB from internal RAM.

---

## 3. States

### 3.1 Route `/setup`

Source: `screen_setup.cpp`.

As built:

- Title `Setup: network login` at (16, 6), font 20.
- Step label at (16, 34), width 760, font 14, `UI_COL_DIM`. Three strings:
  - AP down: `Tap Start to show the Wi-Fi code.`
  - AP up, no client: `Step 1: scan to join this device's Wi-Fi.`
  - AP up, client associated: `Step 2: scan again to open the login page.`
- QR canvas at (24, 64), 300×300, black on white, `lv_obj_set_hidden(true)`
  until the AP is up. It shows the `WIFI:` join payload, then the
  `http://192.168.4.1/` URL.
- Right column at x = 356: `Network:` label (356, 64); SSID value (356, 86),
  width 400, `UI_COL_ACCENT`, `(off)` / `SSID (hidden)` / `SSID`; `Password:`
  label (356, 116); passphrase value (436, 116), width 260.
- `Start phone login` / `Stop` button at (356, 236), 300×48.
- `IS_DEBUG` only: `DEBUG: bypass login` at (356, 292), 300×48, filled
  `UI_COL_WARN`.
- Status label at (24, 380), width 740, `UI_COL_WARN`, default
  `Credentials go to the WT32 over Modbus.`
- No back button and no Home access.
- Entry: `ui_init()` opens this route first; `link_modbus_take_login_request()`
  raises it later. Exit only via `RESULT=success` (`setup_close_success()`),
  which also stops the AP and hides any modal.
- `qr_refresh()` runs every loop while the route is up. On the first client
  association it shows the blocking `Phone connected` modal once
  (`s_phoneSeen` latch), covering the QR.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.2 Route `/home`

Source: `screen_home.cpp`.

As built:

- Title `Home (fake data)` at (16, 8), font 20.
- Summary line at (16, 40), width 760, `UI_COL_DIM`:
  `Run %dh (fake) | 2/6 nodes online | push every 5 min (fake)`.
- Three nav buttons, each 240×120 at y = 90: `DATA` (16), `CONTROL` (272),
  `INFO` (528). All filled `UI_COL_CARD`.
- Live line at (16, 230):
  `Chamber %.1fC (fake) | Setpoint %.1fC | Stirrer %s`.
- Two hint lines at y = 262 and y = 294: the USB serial tip and the
  "one bounded run, then shut down" session note.
- No run-state control, no experiment number, no per-node health summary, no
  quick jump to a specific chamber.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.3 Route `/data` — node picker

Source: `screen_data.cpp`, `screen_data_picker_show()`.

As built:

- Title `/data nodes (fake)` at (16, 8), font 20.
- Six buttons, 240×130, three per row: `x = 16 + col*256`, `y = 52 + row*150`,
  so rows at y = 52 and y = 202.
- Online node: `Node %d\nONLINE`, filled `UI_COL_CARD`. Offline node:
  `Node %d\nNOT CONNECTED`, filled `UI_COL_CARD_OFF`.
- `< Home` at (16, 360), 140×48.
- Node order is the array index, not health. No counts of invalid fields per
  node. An offline node is still tappable and enters the cluster view.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.4 Route `/data/X` — cluster

Source: `screen_data.cpp`, `screen_cluster_show()`.

As built:

- Title `/data/%d chambers (fake)` at (16, 8), width 500, font 20.
- Offline banner `node offline (fake NULL fields)` at (16, 40), width 500,
  `UI_COL_ALARM`.
- Four chamber buttons, 368×124, two per row: `x = 16 + (b%2)*384`,
  `y = 76 + (b/2)*140`, so rows at y = 76 and y = 216.
- Online: `Chamber %d\n<temp> (fake)` — temperature only, the other four
  sensors are not summarised. Offline: `Chamber %d\nnode offline`.
- `< Nodes` at (16, 360), 160×48.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.5 Route `/data/X/bottle_Y` — chamber detail

Source: `screen_data.cpp`, `screen_bottle_show()`.

As built:

- Title `/data/%d/chamber_%d (fake)` at (16, 8), width 500, font 20.
- Offline note `node offline: values are NULL this cycle, not stale.` at
  (16, 40), width 500, `UI_COL_ALARM`.
- Five sensor labels at x = 24, `y = 72 + s*30`, width 320, font 14, formatted
  `%-6s  %s`. Sensors are CO2, CH4, PRESS, pH, TEMP. Colour is
  `UI_COL_ALARM` when `sensorNull[s]`, otherwise `UI_COL_TEXT`.
  The unit is not appended here; `SENSOR_UNITS` is unused.
- `lv_chart` at (360, 72), 400×220, `LV_CHART_TYPE_LINE`, 24 points, Y axis
  300–450 (°C × 10), series colour `UI_COL_ACCENT`, background `UI_COL_CARD`.
  Values come from the in-RAM ring buffer `BottleFake::trend`.
- Caption `Temp trend x10C (fake ring)` at (360, 300).
- `< Chambers` at (16, 360), 160×48.
- No axis labels, ticks, units, gridlines, min/max markers, legend, or time
  axis. One of the five sensors is charted.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.6 Route `/control`

Source: `screen_control.cpp`.

As built, top to bottom:

- Title `/control (cmds -> WT32, fake)` at (16, 8), font 20. `< Home` at
  (648, 4), 140×36 — the only screen with the back control in the title row.
- Setpoint row: label at (16, 52) `Temp setpoint 35.0-40.0C, 0.1 steps:`;
  `-` at (16, 84) 64×48; value label at (96, 92) font 20 showing `%.1f C`;
  `+` at (300, 84) 64×48; note at (390, 96)
  `each step logs [FAKE->WT32] SETPOINT`.
- Three hold-to-confirm rows from `hold_attach()` at y = 160, 206, 252
  (`Stirrer`, `POST enable`, `Run experiment`). Each row: label (16, y) width
  200; `lv_switch` at (220, y-4) 72×36; `lv_bar` at (310, y+4) 160×16;
  hint label at (490, y) width 280.
- Duration row at y ≈ 310: label `Duration:` (16, 316); spinbox (120, 310)
  120×42, range 1–99, step 1; `-` at (250, 310) 48×42; `+` at (306, 310) 48×42;
  caption `hours 1-99` at (366, 318).
- Experiment number row at y ≈ 372: label `Exp number:` (16, 378); spinbox
  (120, 372) 120×42, range 0–9999, 4 digits; `-` / `+` at the same x as
  duration; DB status label at (366, 372) width 420.
- `INT test (IO11 -> WT32 IO14)` label at (16, 424); `PULSE INT` at (280, 418)
  140×42; hint at (430, 428) width 350.
- Layout fact: the last row ends at y = 460 in a 436-tall content container, so
  the `INT test` row overflows the container by 24 px. The container is still
  scrollable (default), so the overflow is reachable only by dragging.
- `screen_control_poll()` calls `db_refresh_warn()` every loop. It reads
  `link_modbus_get_db_latest()`, shows `DB: waiting for master sync...` while
  the value is -2, auto-suggests `exp = latest + 1` once per screen open, then
  renders either `WARN: exp %d already in DB (latest %d). Use %d.` in
  `UI_COL_WARN` or `DB latest %d, next %d OK.` in `UI_COL_OK`.
- No grouping between "safe to change" and "starts the run". The run switch sits
  between the stirrer and the duration row.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.7 Route `/info`

Source: `screen_info.cpp`.

As built:

- Title `/info (fresh on open)` at (16, 8), width 520, font 20.
- Diagnostic rows, all width 520, font 14, at fixed y:
  - y = 60 `Storage limits: %s`
  - y = 100 `IP address: %s (real, WT32)` or `waiting for WT32...`
  - y = 140 `Ping 1.1.1.1: %d ms (real, WT32)`, or `no reply yet`, or
    `no data yet (link down?)`
  - y = 180 `Latest HTTP reply: %s`
  - y = 200 `DB latest exp: %d (next %d)` plus ` NTP OK` / ` NTP no sync`
  - y = 220 `Firmware version: %s`
  - y = 260 `Heap free: %u B (real) | PSRAM: %u B (real)` in `UI_COL_DIM`
  - y = 300 `Fetched at %lu ms (fake stamp)` in `UI_COL_DIM`
  - The y = 180 and y = 200 rows are 20 px apart, so a wrapped HTTP reply
    collides with the DB row.
- Network QR 180×180 at (596, 52), black on white, hidden until the AP is up;
  step label at (596, 240) width 190; `Start phone login` / `Stop` at
  (596, 292), 190×48.
- `< Home` at (16, 360), 140×48.
- The route carries no rows for the run itself: no experiment number, duration,
  setpoint, chamber temperature, stirrer, or POST. It is diagnostics plus the
  network QR.
- `screen_info_poll()` refreshes only the QR, step label, and button label.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.8 Persistent status bar

Source: `ui_shell.cpp`, `build_status_bar()` / `ui_update_bell()`.

As built:

- 800×44 at the top, filled `UI_COL_BAR`, not scrollable.
- `NET OK(fake)` at (10, 12), width 150, font 14, `UI_COL_OK`. Static string;
  nothing updates it.
- POST label at (170, 12), width 110, font 14, `UI_COL_TEXT`. Text is
  `POST ON` / `POST OFF`, set in `ui_update_bell()`.
- Experiment label at (290, 12), width 220, font 14, `UI_COL_DIM`. Text is
  `exp #%d %dh (fake)` while running, otherwise `exp #%d --h (fake)`.
- Bell button at (690, 6), 100×32. Label `OK` on `UI_COL_OK` when
  `fake_null_count() == 0`, `ALERT(%d)` on `UI_COL_ALARM` otherwise. Initial
  fill is `UI_COL_DIM`.
- Not shown: chamber temperature, setpoint, stirrer state, WT32 link state,
  uptime, time of day, or a back control.
- The bar is 800 px wide; the region x = 480–690 is empty in every state.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.9 Blocking modal primitive

Source: `ui_shell.cpp`, `ui_modal_show()`.

As built:

- A plain `lv_obj_t` at (0, 0) 800×480, black at `LV_OPA_70`, added to
  `lv_screen_active()`, so it is a sibling of `s_content` and survives
  `lv_obj_clean(s_content)`.
- Card inside it: 500×340 at (150, 70) via `ui_mk_card()`. Title font 20 at
  (12, 8) width 460; body font 14 at (12, 48) width 460, `LV_LABEL_LONG_MODE_WRAP`;
  ack button 180×48 at (150, 250), filled `UI_COL_ACCENT` with a white label.
- One modal at a time: a second `ui_modal_show()` deletes the first.
- Fixed body area. No scroll; a body longer than the card is clipped.
- One action only. The ack callback is a `void (*)(void)`, so a caller cannot
  pass data.
- Callers: 41 °C warning, 45 °C trip, bell list, phone-associated popup.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.10 Overheat warning, 41 °C

Source: `ui_shell.cpp`, `ui_eval_overheat()`.

As built:

- Threshold `UI_OVERHEAT_WARN_C` 41.0. Evaluated against `g.chamberTempC`.
- One-shot per crossing: `s_warn41Shown` latches on, and re-arms only when the
  temperature falls below 41 °C. Re-entering the band shows the modal again.
- Modal: `41C WARNING (fake)` /
  `Chamber >= 41C.\nHeater keeps running (fake).\nSet TEMP < 41 via serial to
  re-arm.`, ack `Acknowledge`. The ack callback is empty.
- Gated on `g.loggedIn`.
- `ui_eval_overheat()` is called from exactly one place:
  `serial_cmd_poll()` after a mutating serial command. The 5-minute push and the
  Modbus slave do not call it, so a real overheat arriving by data push would
  not raise the modal.
- No persistent indicator after the modal is acknowledged: no status bar entry,
  no colour change on `/home` or `/control`.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.11 Overheat breaker trip, 45 °C

Source: `ui_shell.cpp`, `ui_eval_overheat()`.

As built:

- Threshold `UI_OVERHEAT_TRIP_C` 45.0. `s_trip45Shown` latch, re-armed below
  45 °C. Checked before the 41 °C branch, so a temperature at or above 45 °C
  never shows the warning.
- On first trigger: `g.stirrerOn = false` (a local fake; the comment notes the
  display stays powered for the demo), then `ui_modal_show()`, then
  `ui_refresh_current()`.
- Modal: `45C BREAKER TRIP (fake)` / `Chamber >= 45C.\nAll power cut (fake).\n
  Stirrer forced OFF.\nSet TEMP < 45 via serial to re-arm.`, ack
  `Acknowledge`.
- Same single call site as the warning: serial commands only.
- Because it forces `g.stirrerOn = false` but does not clear the switch's
  visual state through an event, the `/control` stirrer switch only updates on
  the rebuild that follows.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.12 Bell and invalid-field list

Source: `ui_shell.cpp`, `on_bell_clicked()`.

As built:

- Red (`ALERT(N)` on `UI_COL_ALARM`) whenever `fake_null_count() > 0`. At boot
  the 4 offline clusters contribute 80 NULL fields, so the bell starts red.
- Tap opens the blocking modal titled `Invalid fields (live, fake)` with ack
  `Close`.
- Body lists at most 10 entries as `N%d/C%d %s NULL`, ordered by cluster, then
  chamber, then sensor index, then a `... +%d more (fake)` tail when the total
  exceeds 10.
- Empty case: `All fields valid this cycle (fake).`
- Body is built in a `static char body[1024]`. No history is kept (locked
  decision). No clearing action, so the list cannot be dismissed as resolved
  from the panel.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.13 Phone-associated popup

Source: `screen_setup.cpp`, `qr_refresh()`.

As built:

- Raised the first time `provision_ap_client_count() > 0` after an AP start.
  Latched by `s_phoneSeen`, cleared when the count returns to 0 or the AP stops,
  so the next phone pops it again.
- Blocking modal, title `Phone connected`, ack `OK`, body: `Your phone joined
  this device's Wi-Fi.\n\nPress OK, then scan the code now on screen to open the
  login page and enter the portal credentials.`
- Intent: it covers the QR deliberately, because the QR changes to a different
  payload of the same size and colour and the user is looking at their phone.
- Dropped by `setup_close_success()` on login success.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.14 Backlight sleep

Source: `screen_sleep.cpp`.

As built:

- `sleep_note_input()` is called from `my_touchpad_read()` in `main.cpp` only
  when the touch state is `LV_INDEV_STATE_PRESSED`.
- `sleep_tick()` turns `UI_BL_PIN` (GPIO 2) LOW after `UI_SLEEP_MS` 60000 ms
  without a press, and HIGH on the next press.
- The backlight is only switched. The screen content is not redrawn, no dim
  state, no overlay, no wake screen.
- Suppressed entirely while `provision_ap_active()`, and the idle timer is
  refreshed in that state, so the screen stays lit for the whole AP lifetime.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.15 SoftAP lifecycle

Source: `provision_ap.cpp`, driven from `screen_setup_poll()` and
`screen_info_poll()`.

As built, states and who shows them:

| State | Detected by | Shown as |
|---|---|---|
| AP down | `provision_ap_active() == false` | Button reads `Start phone login`; QR hidden; SSID `(off)`, password `-`; step `Tap Start to show the Wi-Fi code.` |
| AP up, no client | `provision_ap_client_count() == 0` | QR shows the `WIFI:` join payload; step `Step 1: scan to join this device's Wi-Fi.` |
| AP up, client attached | count > 0 | QR shows `http://192.168.4.1/`; step `Step 2: scan again to open the login page.`; on `/setup` also the `Phone connected` modal |
| Credentials staged | `provision_ap_awaiting_verdict()` | Panel status stays on the WT32 wording; `/status` serves `waiting: …` and the phone polls every 1500 ms |
| Accepted | Modbus `RESULT=1` | `setup_close_success()` → `/home`, AP stopped |
| Rejected | Modbus `RESULT=2` | Panel status `Wrong username or password. Correct them on your phone and resend.` (or the short form when the AP is down); phone result page turns red with an `Enter them again` link |

- The AP is never started automatically. It stops when the route is neither
  `/setup` nor `/info`, and on a 10-minute HTTP idle timeout
  (`kIdleTimeoutMs`).
- `/setup` shows the AP failure to start (`Could not start the access point.`);
  `/info` shows nothing in that case.
- Staging a pair pulses the poll-request line (Display IO11 → WT32 IO14).

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

### 3.16 Debug-only UI

Source: `#if IS_DEBUG` blocks, gate documented in `display-debug-flag.md`.

As built:

- `IS_DEBUG` defaults to 0, set per build with `-DIS_DEBUG=1`.
- One gated widget: `DEBUG: bypass login` on `/setup` at (356, 292), filled
  `UI_COL_WARN`, routed through the normal success path.
- Ungated bench affordances that arguably belong in the same category:
  `PULSE INT` on `/control` (§3.6) and the `CREDS` / `AP` USB serial commands.

| Problem | Redesign idea | Priority |
|---|---|---|
|  |  |  |
|  |  |  |

---

## 4. Cross-cutting redesign table

| Area | Problem | Redesign idea | Priority |
|---|---|---|---|
| Styling |  |  |  |
| Navigation |  |  |  |
| Data refresh |  |  |  |
| Real vs fake labelling |  |  |  |
| Write commands |  |  |  |
| LVGL configuration |  |  |  |
| Layout and density |  |  |  |
| Typography |  |  |  |
| Accessibility and colour |  |  |  |
| Touch targets |  |  |  |

---

## 5. Discrepancies found while writing this

- `display-docs.md` §3 lists `screen_boot.h/.cpp` and a `/boot` blocking gate
  with a duration spinbox. Those files do not exist and there is no `/boot`
  route. The first route is `/setup`; the duration and experiment-number
  spinboxes are on `/control`. The same document's §11 describes this correctly,
  so §3 is the stale part.
- `ui_shell.h` and `link_modbus.h` comments still refer to the boot gate
  consuming credentials.

---

## 6. Verification

Facts in this document come from reading the source. The layout claims
(coordinate lists, overflow, row collisions) can be checked in
`[env:display-sim]` (`display-sim.md`) or on the panel.

```sh
pio run -e display-v9
```

---
Content generated by `opencode/space-bunny-free` in `opencode`.