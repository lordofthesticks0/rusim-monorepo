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

### 2.7 Shared hold-to-confirm round button

New files: `ui_hold_button.h`, `ui_hold_button.cpp`.

Purpose: one round button replaces the `lv_switch` + `lv_bar` hold pairs and
the `LV_EVENT_VALUE_CHANGED` dance in `screen_control.cpp`. While the button
is held, its border fills slowly; releasing before the threshold cancels;
holding past the threshold commits on release.

`UI_HOLD_TO_CONFIRM_MS` in `ui_config.h` changes from 500 to 2000.

Appearance:

- Body: `lv_button`, radius `LV_RADIUS_CIRCLE`, no border. Fill `UI_COL_ALARM`
  when off, `UI_COL_OK` when on.
- Centre: `lv_label` with a `LV_SYMBOL_*` glyph, font 20, white.
- Ring: `lv_arc` as a child of the button, size = button size + 16, centred,
  `lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE)`,
  `lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN)`,
  `lv_obj_remove_style(arc, NULL, LV_PART_KNOB)`, 8 px line width, indicator
  white. White reads on both the red and the green fill.

Behaviour:

- `LV_EVENT_PRESSED`: store `t0 = millis()`, start a 20 ms timer, set the ring
  to `min(100, (millis() - t0) * 100 / UI_HOLD_TO_CONFIRM_MS)`.
- `LV_EVENT_RELEASED` / `LV_EVENT_PRESS_LOST`: stop the timer. If
  `millis() - t0 < UI_HOLD_TO_CONFIRM_MS`, cancel: ring back to 0, state
  unchanged, callback not called. Otherwise commit: ring back to 0, state
  flipped, callback called.
- Commit happens on release, not when the ring reaches 100%. A press that
  runs past 2 s commits when the finger lifts; a press that lifts early does
  nothing. This avoids a toggle firing while the finger is still down.
- `LV_EVENT_DELETE`: delete the timer, clear the user data. Needed because the
  context is heap state that outlives the button otherwise.

API:

```c
typedef void (*UiHoldFn)(void *user);

lv_obj_t *ui_hold_button_create(lv_obj_t *parent, int x, int y, int size,
                                const char *symbol, bool initial,
                                UiHoldFn onCommit, void *user);
void ui_hold_button_set_state(lv_obj_t *btn, bool on);
void ui_hold_button_set_symbol(lv_obj_t *btn, const char *symbol);
bool ui_hold_button_state(lv_obj_t *btn);
```

Internals: `struct HoldBtnCtx { lv_obj_t *arc; lv_obj_t *label; lv_timer_t *timer; uint32_t t0; bool state; UiHoldFn onCommit; void *user; };` attached with `lv_obj_set_user_data()`.

Notes: `LV_USE_ARC` is already `1` in `lv_conf.h`, so no config change is
needed. The widget is created on the heap by `lv_button_create`, so the
context can be a plain `new`/`delete` pair, or a `static` block in
`ui_hold_button.cpp` if only a handful of buttons exist at once (planned use:
two, one on `/control` for stirrer, one shared for the experiment run toggle).

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
| Nav buttons are large and occupy the zone that should hold status data | Shrink to three small text-only buttons, centered near the top | High |
| No run start/stop or duration adjust on home | Add a big run toggle and ±1 h duration buttons under the progress bar | High |
| Chamber temp and setpoint are one text line with no direct control | Split into a card with large current temp and steppable target | High |
| No bottle/node health summary on the hub | Add a second card showing online bottles and nodes | Medium |
| No experiment progress visible | Add a large circular arc (lv_arc) for runtime and percent | High |
| Status text ("fake", hints) clutters the hub | Drop serial/file tips and inline fake labels from this screen | Low |

#### 3.2.1 Redesign plan — `/home`

Decision: the home screen becomes the **run console**. Navigation moves to the
persistent top bar (§3.8.1), so home keeps no navigation widgets. The three
oversized nav buttons are replaced by two data cards, a large progress arc,
and a run-control row. Home carries no header, no descriptive text, and no
note strip: it is cards, arc, controls, nothing else.

##### 3.2.1.1 Symbols

This screen uses the standard LVGL `LV_SYMBOL_*` glyphs (from the supplied 9.6
list) wherever a glyph reads faster than text. Each label is built as a
concatenated literal, e.g. `LV_SYMBOL_PLAY " Start"` or
`LV_SYMBOL_DOWN "1h"`. Plain words are kept only where no glyph in the list
fits (`CHAMBER TEMP`, `Target setpoint`, numeric readouts).

Assignments:

- Targets: decrease `LV_SYMBOL_DOWN`, increase `LV_SYMBOL_UP`.
- Durations: decrease `LV_SYMBOL_DOWN`, increase `LV_SYMBOL_UP`.
- Run: the hold button shows `LV_SYMBOL_PLAY` when stopped and
  `LV_SYMBOL_STOP` when running.
- States: stirrer ON `LV_SYMBOL_OK` + `ON`, OFF `LV_SYMBOL_CLOSE` + `OFF`;
  experiment Running `LV_SYMBOL_PLAY` + `Running`, Idle `LV_SYMBOL_STOP` + `Idle`;
  invalid fields `LV_SYMBOL_WARNING`; all valid `LV_SYMBOL_OK`.
- Navigation glyphs live in the top bar (§3.8.1), not on home.

##### 3.2.1.2 Layout (content coordinates, panel 800x436)

Zones, top to bottom:

| Zone | y range | Height | Content |
|---|---|---|---|
| Cards | 8..168 | 160 | two big cards side by side |
| Progress | 172..314 | 142 | title + arc + center readout |
| Controls | 322..418 | 96 | duration adjust + run hold button |
| bottom margin | 418..436 | 18 | (none) |

Detailed coordinates (all x/y are inside the 800x436 content container).
There is no nav row and no title: the top bar owns navigation and the active
route highlight.

Card A (current chamber temperature plus steppable target), 376x160 at (16,8):

| Widget (relative to card) | x | y | w | h | Text / value | Notes |
|---|---|---|---|---|---|---|
| title | 12 | 8 | 352 | 20 | `CHAMBER TEMP` | `UI_COL_DIM`, font 14 |
| `s_curTemp` | 12 | 28 | 352 | 26 | `%.1f C` from `g.chamberTempC` | `UI_COL_TEXT`, font 20 |
| target label | 12 | 58 | 352 | 18 | `Target setpoint` | `UI_COL_DIM`, font 14 |
| `s_btnTargetMinus` | 12 | 80 | 44 | 36 | `LV_SYMBOL_DOWN` | `UI_COL_CARD` |
| `s_target` | 68 | 84 | 100 | 26 | `%.1f C` from `g.tempSetpointC` | `UI_COL_TEXT`, font 20, centered |
| `s_btnTargetPlus` | 180 | 80 | 44 | 36 | `LV_SYMBOL_UP` | `UI_COL_CARD` |
| hint | 236 | 88 | 128 | 16 | `0.1 C steps` | `UI_COL_DIM`, font 14 |
| stirrer readout | 12 | 124 | 352 | 18 | `Stirrer ` + (`g.stirrerOn` ? `LV_SYMBOL_OK " ON"` : `LV_SYMBOL_CLOSE " OFF"`) | `UI_COL_TEXT`, font 14 |

Card B (bottles online), 376x160 at (408,8):

| Widget (relative to card) | x | y | w | h | Text / value | Notes |
|---|---|---|---|---|---|---|
| title | 12 | 8 | 352 | 18 | `BOTTLES ONLINE` | `UI_COL_DIM`, font 14 |
| `s_bottleCount` | 12 | 28 | 352 | 32 | `%d / %d` (online bottles, total bottles) | `UI_COL_TEXT`, font 20 |
| `s_nodeCount` | 12 | 64 | 352 | 18 | `%d / %d nodes online` | `UI_COL_TEXT`, font 14 |
| `s_expNum` | 12 | 86 | 352 | 18 | `Experiment #%d` | `UI_COL_DIM`, font 14 |
| `s_runState` | 12 | 108 | 352 | 18 | running `LV_SYMBOL_PLAY " Running"` / idle `LV_SYMBOL_STOP " Idle"` | colour `UI_COL_OK` when running else `UI_COL_DIM` |
| `s_invalidCount` | 12 | 130 | 352 | 18 | if >0: `LV_SYMBOL_WARNING " %d invalid"` else `LV_SYMBOL_OK " All valid"` | `UI_COL_ALARM` if > 0 else `UI_COL_DIM` |

Progress zone, arc centered at (400,254) inside content:

| Widget | x | y | w | h | Text / value | Notes |
|---|---|---|---|---|---|---|
| `s_progTitle` | 240 | 172 | 320 | 18 | `Experiment runtime` | `UI_COL_DIM`, font 14, centered |
| `s_arc` | 340 | 194 | 120 | 120 | arc, range 0..100, value percent | foreground `UI_COL_ACCENT`, background `UI_COL_BAR`; non-interactive |
| `s_arcReadout` | 340 | 238 | 120 | 36 | running `%.1f / %d h` + newline + `%d%%`; idle `Idle` + newline + `%d h set` | `UI_COL_TEXT`, font 14 |

Controls row, centred on x = 400, run hold button is the tall centre piece:

| Widget | x | y | w | h | Text | Notes |
|---|---|---|---|---|---|---|
| `s_btnDurMinus` | 176 | 350 | 104 | 44 | `LV_SYMBOL_DOWN "1h"` | immediate, clamped |
| `s_btnRun` | 352 | 322 | 96 | 96 | `LV_SYMBOL_PLAY` idle, `LV_SYMBOL_STOP` running | §2.7 hold button, state = `g.experimentRunning`, ring spans 346..454 |
| `s_btnDurPlus` | 520 | 350 | 104 | 44 | `LV_SYMBOL_UP "1h"` | immediate, clamped |

##### 3.2.1.3 State and data model changes needed

1. Add `uint32_t experimentStartMs` to `AppState` in `fake_data.h`, default
   `0` (not running). Set it on run start from any screen and clear on stop.
   `fake_init()` initialises it to 0.
2. `g.experimentRunning` stays the single source of run state; `expNum`,
   `durationH`, `tempSetpointC` unchanged.
3. `screen_home.cpp` handles the case `g.experimentRunning == true` and
   `g.experimentStartMs == 0` on show: set `g.experimentStartMs = millis()`.
   This covers runs started on `/control` before home was opened.
4. `home_elapsed_hours()` returns `(millis() - g.experimentStartMs) / 3600000.0f`
   clamped to `0 .. g.durationH`, or `0` when not running.

##### 3.2.1.4 Functions to add / change

Header `screen_home.h`:

- `void screen_home_show(lv_obj_t *parent)` (reworked).
- `void screen_home_poll()` (new): refreshes arc, readouts, card values, button
  colour. No-op when `s_arc == nullptr`.

Implementation statics in `screen_home.cpp`:

- Static object pointers named as in the layout tables above, plus
  `static uint32_t s_lastPollMs = 0`.

Callback functions:

- `on_target_minus`: decrements `g.tempSetpointC` by `UI_TEMP_STEP_C`, clamps to
  `UI_TEMP_MIN_C`, logs `[FAKE->WT32] SETPOINT %.1f`, refreshes `s_target`,
  calls `ui_update_bell()`.
- `on_target_plus`: increments, clamps to `UI_TEMP_MAX_C`, same handling.
- `bottles_online()`: returns sum of `UI_BOTTLES_PER_CLUSTER` for clusters where
  `cluster[c].online` is true.
- `home_render_status()`: writes text and colours into `s_curTemp`, `s_target`,
  `s_bottleCount`, `s_nodeCount`, `s_expNum`, `s_runState`, `s_invalidCount`
  from current `g`. Called from show and poll.
- `home_render_run()`: sets the hold button symbol and state from
  `g.experimentRunning` via `ui_hold_button_set_symbol()` and
  `ui_hold_button_set_state()`, updates `s_arc` value and `s_arcReadout`.
  Called from show and poll.
- `on_duration_down`: `g.durationH = max(UI_DURATION_MIN_H, g.durationH - 1)`,
  log `[HOME] duration %dh`, `link_modbus_set_exp_state(...)`,
  `ui_update_bell()`, `home_render_run()`.
- `on_duration_up`: same with +1 clamped to `UI_DURATION_MAX_H`.
- `on_run_commit(void *user)`: the shared hold button callback (§2.7), same rule
  as `/control`'s `on_run_commit()`: stop sets `g.experimentRunning = false`
  and `g.experimentStartMs = 0`; start sets `g.experimentRunning = true`,
  `g.experimentStartMs = millis()`, `g.expNum++`; both mirror to the link and
  log. Finish with `ui_update_bell()` and `home_render_run()`. The rule lives in
  one shared helper (`ui_experiment_toggle()` in `ui_shell.cpp`) so home and
  `/control` cannot drift apart.
- Show order: card A children, card B children, progress children, control
  buttons, then `home_render_status()` and `home_render_run()`. No navigation
  widgets and no title: the top bar owns both (§3.8.1).
- Arc setup: `lv_arc_create(parent)`, `lv_arc_set_range(s_arc, 0, 100)`,
  `lv_arc_set_value(s_arc, 0)`. `lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE)`.
  Remove the knob part style so the arc reads as an indicator. No slider.

##### 3.2.1.5 Polling

Add `screen_home_poll()` to the `loop()` in `main.cpp` next to
`screen_control_poll()`. Body:
```
if (s_arc == nullptr) return;
uint32_t now = millis();
if (now - s_lastPollMs < 1000) return;
s_lastPollMs = now;
home_render_status();
home_render_run();
```
`home_render_run()` recomputes progress from `millis()`, so the arc advances
once per second without a full route rebuild.

##### 3.2.1.6 Integration with other screens

- `/control` loses its `HoldCtx` run switch (§3.6.1) and uses the same hold
  button, so the run toggle lives in one helper: `ui_experiment_toggle()` in
  `ui_shell.cpp` flips `g.experimentRunning`, sets or clears
  `g.experimentStartMs`, increments `g.expNum` on a start, mirrors the link, and
  logs. Both screens call it, so the two cannot drift apart.
- `on_push_timer()` rebuilds the route every 5 min; static start time survives
  the rebuild, so the arc does not reset on a push.
- The status bar is reworked separately (§3.8.1); it owns navigation, the
  warning bell, and the clock.

##### 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels)

Card A: `CHAMBER TEMP`, `%.1f C`, `Target setpoint`,
  `LV_SYMBOL_DOWN`, `LV_SYMBOL_UP`, `0.1 C steps`,
  `Stirrer ` + (`ON` -> `LV_SYMBOL_OK " ON"`, `OFF` -> `LV_SYMBOL_CLOSE " OFF"`).
Card B: `BOTTLES ONLINE`, `%d / %d`, `%d / %d nodes online`,
  `Experiment #%d`,
  running `LV_SYMBOL_PLAY " Running"` / idle `LV_SYMBOL_STOP " Idle"`,
  invalid `LV_SYMBOL_WARNING " %d invalid"` or `LV_SYMBOL_OK " All valid"`.
Progress: `Experiment runtime`,
  running `%.1f / %d h` then `%d%%`,
  idle `Idle` then `%d h set`.
Controls: `LV_SYMBOL_DOWN "1h"`, `LV_SYMBOL_UP "1h"`,
  run hold button glyph `LV_SYMBOL_PLAY` / `LV_SYMBOL_STOP`.

##### 3.2.1.8 Out of scope for this screen (stays on /control)

Setpoint range notes, POST enable, stirrer toggle, duration spinbox, exp
number, DB warn, and INT test pin remain on `/control`. Home mirrors only
start/stop and duration ±1 h.

Removed from home in this redesign: the `Home (fake data)` header, the
`Run %dh | nodes online | push every 5 min` summary line, the three nav
buttons (moved to the top bar), the `Chamber / Setpoint / Stirrer` text line
(replaced by card A), the USB serial tip, the session note, and every `(fake)`
literal.

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
| Header carries the route and a `(fake)` tag | Header becomes `NODES` | High |
| Each node button reads `Node N` plus an `ONLINE` / `NOT CONNECTED` sentence | Show the number only, in a larger font | High |
| Offline state needs a second line to be readable | Reuse the greyed fill that already exists; no status word | Medium |
| `< Home` is a wide text button | Icon-only button with `LV_SYMBOL_NEW_LINE` | Medium |
| Route is not visible in the shell | Data tab stays highlighted across routes 1, 2 and 3 (§3.8.1) | High |

#### 3.3.1 Redesign plan — `/data` node picker

Decision: the picker is a 3×2 grid of numbered tiles under a `NODES` header.
Nothing else.

##### 3.3.1.1 Removed content

- The `/data nodes (fake)` header text.
- The `Node %d` prefix and the `ONLINE` / `NOT CONNECTED` lines inside every
  button.
- The `< Home` label (the back button becomes an icon, see below).

##### 3.3.1.2 Layout (content coordinates, panel 800x436)

| Widget | x | y | w | h | Content | Notes |
|---|---|---|---|---|---|---|
| `s_nodesTitle` | 16 | 8 | 400 | 28 | `NODES` | font 20, `UI_COL_TEXT` |
| `s_nodeBtn[0..5]` | 68, 300, 532 | 56, 216 | 200 | 140 | `%d` (node index) | `ui_mk_button()`; row = `c / 3`, col = `c % 3` |
| `s_backBtn` | 364 | 372 | 72 | 48 | `LV_SYMBOL_NEW_LINE` | icon only |

Grid maths: 3·200 + 2·32 = 664, so the block starts at (800 − 664) / 2 = 68.
Rows are 140 tall with a 20 px gap: y = 56 and y = 216, ending at 356. The back
button sits centred at y = 372, 48 tall, so the block ends at 420.

Node tile styling:

- Fill `UI_COL_CARD` when `g.cluster[c].online`, `UI_COL_CARD_OFF` when not.
  This grey fill is the only offline signal.
- Single label, child index 0, restyled after `ui_mk_button()`:
  `lv_obj_set_style_text_font(label, &lv_font_montserrat_28, LV_PART_MAIN)`
  and `lv_obj_set_style_text_color(label, lv_color_hex(g.cluster[c].online ?
  UI_COL_TEXT : UI_COL_DIM), LV_PART_MAIN)`. `ui_mk_button()` centres the
  child, so no positioning is needed.
- `LV_FONT_MONTSERRAT_28` is already enabled in `lv_conf.h`.

##### 3.3.1.3 Back button

Every back button inside `/data` (this route, the cluster route, and the
chamber-detail route in §3.5) becomes the same widget: 72×48 at x = 364,
`LV_SYMBOL_NEW_LINE` and nothing else, no text label. Callbacks are unchanged:
picker back goes to `ui_show_home()`, cluster back goes to
`ui_show_data_picker()`, chamber-detail back goes to `ui_show_cluster(c)`.

##### 3.3.1.4 Behaviour kept

- Node order is still the array index, `0` to `5`. Flagged in §3.3.1.5.
- An offline node is still tappable and still opens the cluster view.
- `lv_obj_add_event_cb()` still carries the cluster index as user data.

##### 3.3.1.5 Open item

Nodes are numbered from 0, matching the array index and the `N%d/C%d` strings in
the bell modal. If the dashboard numbers nodes 1 to 6, the picker should print
`c + 1` while the rest of the code keeps the index. Not decided; worth checking
against the database schema before implementation.

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
| Header carries the route and a `(fake)` tag | Header becomes `NODE %d` | High |
| Offline banner explains a condition the tiles can show themselves | Remove the banner; grey the tiles instead | Medium |
| Each tile reads `Chamber N` plus the temperature inline | Big number, temperature as small flavour text below it | High |
| Temperature line is the only summary; no unit is visible | Flavour text is the formatted value, `%.1f C` | Medium |
| `< Nodes` is a wide text button | Icon-only button with `LV_SYMBOL_NEW_LINE` (§3.3.1.3) | Medium |

#### 3.4.1 Redesign plan — `/data/X` cluster

Decision: the cluster route is a 2×2 grid of numbered chamber tiles under a
`NODE %d` header, each tile carrying its temperature as a small line below the
number.

##### 3.4.1.1 Removed content

- The `/data/%d chambers (fake)` header text.
- The `node offline (fake NULL fields)` banner row at (16, 40).
- The `Chamber %d` prefix and the `node offline` sentence inside the tiles.
- The `< Nodes` label (the back button becomes an icon).

##### 3.4.1.2 Layout (content coordinates, panel 800x436)

| Widget | x | y | w | h | Content | Notes |
|---|---|---|---|---|---|---|
| `s_nodeTitle` | 16 | 8 | 400 | 28 | `NODE %d` (cluster index) | font 20, `UI_COL_TEXT` |
| `s_chamberBtn[0..3]` | 60, 420 | 56, 216 | 320 | 140 | number + flavour | `ui_mk_button()`; row = `b / 2`, col = `b % 2` |
| `s_backBtn` | 364 | 372 | 72 | 48 | `LV_SYMBOL_NEW_LINE` | icon only, same as §3.3.1.3 |

Grid maths: 2·320 + 40 = 680, so the block starts at (800 − 680) / 2 = 60, the
columns are at x = 60 and x = 420, the rows at y = 56 and y = 216 (140 tall,
20 px gap), ending at 356.

##### 3.4.1.3 Tile contents

`ui_mk_button()` centres one child label, so a second label is added and
positioned with alignment instead of coordinates:

- `s_chamberNum[b]`: `lv_label_create(btn)`, text `%d` (chamber index),
  `lv_obj_align(num, LV_ALIGN_TOP_MID, 0, 22)`, font `lv_font_montserrat_28`,
  colour `UI_COL_TEXT` (or `UI_COL_DIM` when the node is offline),
  `lv_obj_set_width(num, 300)` with centred text.
- `s_chamberTemp[b]`: `lv_label_create(btn)`, `lv_obj_align(temp,
  LV_ALIGN_BOTTOM_MID, 0, -26)`, font `lv_font_montserrat_14`, colour
  `UI_COL_DIM`, `lv_obj_set_width(temp, 300)` with centred text.
- Temperature text: online `%.1f C` from `fake_sensor_text(c, b, S_TEMP, ...)`,
  for example `37.4 C`. Offline node: `--`.
- Tile fill: `UI_COL_CARD` when `g.cluster[c].online`, `UI_COL_CARD_OFF` when
  not, the same rule the node picker uses.

##### 3.4.1.4 Behaviour kept

- Tapping a tile still opens the chamber detail with `(c << 8) | b` as user
  data.
- The back button still goes to the picker.
- Only the temperature is summarised; the other four sensors still appear only
  in the chamber detail (§3.5), which is unchanged.

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
| `< Home` button and the `/control` header duplicate the top bar | Remove both | High |
| Setpoint hint text and the "each step logs" note add noise | Remove both; the stepper stands alone, centred | Medium |
| Hold-to-confirm switches do not work reliably (switch state, bar, timer) | Replace with the shared round hold button (§2.7) | High |
| Hold is 500 ms, long enough to trigger by accident | Raise to 2000 ms | High |
| POST enable is on the panel and does nothing useful | Remove it entirely | High |
| Run experiment is a switch labelled "hold 500ms" | Make it a round hold button that starts and stops, same widget as the stirrer | High |
| Duration and experiment number use spinboxes styled with literals | Restyle as the same stepper row as the setpoint, with the description below the label | Medium |
| `+` / `-` glyphs are typographic, thin, easy to miss | Use `LV_SYMBOL_UP` / `LV_SYMBOL_DOWN` everywhere | Medium |
| INT test row overflows the container by 24 px | Move it into the free left column; keep the button and behaviour unchanged | Medium |

#### 3.6.1 Redesign plan — `/control`

Decision: `/control` becomes a vertical console. Header and back button go
away, the setpoint stepper is centred at the top, the stirrer and the
experiment run toggle become two round hold buttons stacked in the middle with
their descriptions underneath, and duration plus experiment number sit side by
side at the bottom in the same stepper style as the setpoint. The INT test
stays, parked in the free left column.

##### 3.6.1.1 Removed content

- `< Home` button (top bar owns navigation, §3.8.1).
- `/control (cmds -> WT32, fake)` header.
- `Temp setpoint 35.0-40.0C, 0.1 steps:` label.
- `each step logs [FAKE->WT32] SETPOINT` note.
- POST enable row, entirely.
- `HoldCtx`, `hold_attach()`, `on_hold_event()`, `hold_progress()`,
  `hold_reset()` and the three `lv_switch` + `lv_bar` pairs in
  `screen_control.cpp`, replaced by the shared widget in §2.7.
- `mk_spin()` and both `lv_spinbox` instances, replaced by stepper rows.

##### 3.6.1.2 Layout (content coordinates, panel 800x436)

Setpoint row, group width 252, centred (left 274), 64×52 buttons:

| Widget | x | y | w | h | Label | Notes |
|---|---|---|---|---|---|---|
| `s_btnTempDown` | 274 | 16 | 64 | 52 | `LV_SYMBOL_DOWN` | `UI_COL_CARD`, centred glyph `UI_COL_ACCENT` |
| `s_setpoint` | 350 | 32 | 100 | 28 | `%.1f C` from `g.tempSetpointC` | font 20, `UI_COL_TEXT`, centred text |
| `s_btnTempUp` | 462 | 16 | 64 | 52 | `LV_SYMBOL_UP` | `UI_COL_CARD` |

Stirrer hold button and its description:

| Widget | x | y | w | h | Label | Notes |
|---|---|---|---|---|---|---|
| `s_btnStirrer` | 352 | 76 | 96 | 96 | `LV_SYMBOL_POWER` | §2.7, state = `g.stirrerOn`, ring spans 346..454 |
| stirrer desc | 200 | 182 | 400 | 16 | `Hold 2s to turn the stirrer on or off` | font 14, `UI_COL_DIM`, centred |

Run hold button and its description:

| Widget | x | y | w | h | Label | Notes |
|---|---|---|---|---|---|---|
| `s_btnRun` | 352 | 206 | 96 | 96 | `LV_SYMBOL_PLAY` idle, `LV_SYMBOL_STOP` running | §2.7, state = `g.experimentRunning` |
| run desc | 200 | 312 | 400 | 16 | `Hold 2s to start or stop the experiment` | font 14, `UI_COL_DIM`, centred |

Duration block, centred on x = 200:

| Widget | x | y | w | h | Label | Notes |
|---|---|---|---|---|---|---|
| label | 100 | 340 | 200 | 18 | `Duration` | font 14, `UI_COL_TEXT`, centred |
| `s_durDesc` | 100 | 358 | 200 | 16 | `hours 1-99` | font 14, `UI_COL_DIM`, centred |
| `s_btnDurDown` | 116 | 376 | 48 | 40 | `LV_SYMBOL_DOWN` | `UI_COL_CARD` |
| `s_durValue` | 172 | 384 | 56 | 24 | `%d` from `g.durationH` | font 20, centred |
| `s_btnDurUp` | 236 | 376 | 48 | 40 | `LV_SYMBOL_UP` | `UI_COL_CARD` |

Experiment number block, centred on x = 600:

| Widget | x | y | w | h | Label | Notes |
|---|---|---|---|---|---|---|
| label | 500 | 340 | 200 | 18 | `Experiment number` | font 14, `UI_COL_TEXT`, centred |
| `s_expDesc` | 500 | 358 | 200 | 16 | DB status, see §3.6.1.5 | font 14, centred, colour `UI_COL_OK` / `UI_COL_WARN` / `UI_COL_DIM` |
| `s_btnExpDown` | 516 | 376 | 48 | 40 | `LV_SYMBOL_DOWN` | `UI_COL_CARD` |
| `s_expValue` | 572 | 384 | 56 | 24 | `%d` from `g.expNum` | font 20, centred |
| `s_btnExpUp` | 636 | 376 | 48 | 40 | `LV_SYMBOL_UP` | `UI_COL_CARD` |

INT test, in the free left column beside the hold buttons:

| Widget | x | y | w | h | Label | Notes |
|---|---|---|---|---|---|---|
| label | 16 | 96 | 180 | 32 | `INT test (IO11 -> WT32 IO14)` | font 14, wraps to two lines |
| `s_intBtn` | 16 | 136 | 160 | 42 | `PULSE INT` | `UI_COL_CARD`, unchanged behaviour |
| `s_intHint` | 16 | 184 | 180 | 32 | `asked WT32 to poll now` | font 14, `UI_COL_DIM`, wraps |

The right column beside the hold buttons (x 600..790, y 76..330) stays empty in
this pass.

##### 3.6.1.3 Functions

Header: `screen_control.h` keeps `screen_control_show()` and
`screen_control_poll()`, and drops nothing else; `HoldCtx` was file-local.

`screen_control.cpp` statics: `s_btnTempDown`, `s_btnTempUp`, `s_setpoint`,
`s_btnStirrer`, `s_btnRun`, `s_btnDurDown`, `s_btnDurUp`, `s_durValue`,
`s_btnExpDown`, `s_btnExpUp`, `s_expValue`, `s_expDesc`, `s_intBtn`,
`s_intHint`.

Callbacks:

- `on_temp_down`: `g.tempSetpointC -= UI_TEMP_STEP_C`, clamp to
  `UI_TEMP_MIN_C`, update `s_setpoint`, log `[FAKE->WT32] SETPOINT %.1f`.
  (Same behaviour as the old minus button, new glyph.)
- `on_temp_up`: increments, clamps to `UI_TEMP_MAX_C`, same handling.
- `on_stirrer_commit(void *user)`: `g.stirrerOn = !g.stirrerOn`, log
  `[FAKE->WT32] STIRRER %s`, call `ui_hold_button_set_state(s_btnStirrer,
  g.stirrerOn)`.
- `on_run_commit(void *user)`: calls the shared `ui_experiment_toggle()` in
  `ui_shell.cpp`, which flips `g.experimentRunning`, sets or clears
  `g.experimentStartMs = millis()`, increments `g.expNum` on a start edge (same
  rule as the old `on_run_toggled()`), mirrors `link_modbus_set_exp_state()`,
  and logs `[FAKE->WT32] RUN ON/OFF`. Then set the button with
  `ui_hold_button_set_symbol()` (PLAY or STOP) and
  `ui_hold_button_set_state()`, and call `ui_update_bell()`. The home run
  button calls the same helper.
- `on_dur_down` / `on_dur_up`: `g.durationH` clamped to
  `UI_DURATION_MIN_H .. UI_DURATION_MAX_H`, update `s_durValue`, mirror to the
  link, `ui_update_bell()`.
- `on_exp_down` / `on_exp_up`: `g.expNum` clamped to
  `UI_EXP_NUM_MIN .. UI_EXP_NUM_MAX`, update `s_expValue`, mirror to the link,
  `ui_update_bell()`, `db_refresh_warn()`.
- `on_int_test`: unchanged. Calls `link_notify_pulse()`, prints the same
  serial line, writes `s_intHint`.
- `db_refresh_warn()`: same behaviour as today, but it writes into
  `s_expDesc` instead of a wide label, and the auto-suggest latch
  `s_dbSuggested` is reset in `screen_control_show()` on every open.
- `screen_control_show()` order: setpoint row, stirrer hold button + desc, run
  hold button + desc, duration block, experiment block, INT test, then
  `db_refresh_warn()` and the two `ui_hold_button_set_*()` calls so the buttons
  start out matching `g`.

##### 3.6.1.4 Constants

- `UI_HOLD_TO_CONFIRM_MS` 500 -> 2000 in `ui_config.h`. Nothing else uses the
  old value once `HoldCtx` is deleted.
- Duration still steps by whole hours, `UI_DURATION_MIN_H` 1 to
  `UI_DURATION_MAX_H` 99.
- Experiment number still `UI_EXP_NUM_MIN` 0 to `UI_EXP_NUM_MAX` 9999.

##### 3.6.1.5 DB status text (shortened to fit 200 px at font 14)

| State | Text | Colour |
|---|---|---|
| no sync yet | `DB: waiting for sync` | `UI_COL_DIM` |
| current number is the one to use | `DB latest %d, use %d` | `UI_COL_OK` |
| number already in the DB | `exp %d exists, use %d` | `UI_COL_WARN` |

The auto-suggest still raises `g.expNum` to `latest + 1` once per screen open,
which normally makes the mismatch row unreachable after the first push.

##### 3.6.1.6 Run button colours

Decided: the run button follows the stirrer rule. Red (`UI_COL_ALARM`) while
the experiment is stopped, green (`UI_COL_OK`) while it runs, on both `/control`
and home. The glyph still switches `LV_SYMBOL_PLAY` / `LV_SYMBOL_STOP` so the
state does not depend on colour alone.

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
| Header and `< Home` duplicate the top bar | Remove both | High |
| Rows sit at hand-picked y values, 20 px apart in places, so wrapped text collides | Lay the screen out with flex so every item is its own widget with one uniform gap | High |
| Storage limit is a sentence, no number | Replace with an arc gauge: ring fill = percent, centre = percent, colour green to red | High |
| IP address is a text row with flavour text | Square tile, large centred value, caption `IP Address` | High |
| HTTP reply, firmware version, DB experiment are three different row styles | Reuse the same square tile | High |
| HTTP reply text is a raw log line | Reduce it to a network state: `Blocked`, `Offline`, `Online` | Medium |
| Heap and PSRAM share one sentence | Two arc gauges, fill = used percent, centre = free bytes | Medium |
| Wi-Fi QR and its button take the right third of the screen | Remove them; `/setup` keeps the QR | High |
| `Fetched at <ms>` stamp only changes on navigation | Remove it; values refresh in place | Medium |
| Values are painted once when the route is built | `screen_info_poll()` repaints the tiles every second from live getters | Medium |

#### 3.7.1 Redesign plan — `/info`

Decision: `/info` becomes a uniform grid of seven tiles. Four square value
tiles on the first row, three arc gauges on the second row. One gap everywhere
(16 px), one caption style, one value style. Nothing else on the screen.

##### 3.7.1.1 Removed content

- `/info (fresh on open)` header.
- `< Home` button (top bar owns navigation, §3.8.1).
- Wi-Fi QR canvas, its step label, and the `Start phone login` / `Stop` button.
  With them: `info_qr_refresh()`, `qr_show()`, `step_show()`, `on_ap_toggle()`,
  the `s_qr` / `s_qrShown` / `s_step` / `s_stepShown` / `s_apBtn` / `s_apBtnShown`
  statics, and the `ui_is_info()` guard in `screen_setup_poll()` that kept the
  SoftAP alive while this route was up. `LV_USE_QRCODE` stays 1 for `/setup`.
- `Ping 1.1.1.1` row and `link_modbus_get_ping_ms()`.
- `Latest HTTP reply: <raw line>` row (replaced by the network-state tile).
- `Storage limits: <sentence>` row and `g.storageNote` as a UI source.
- `Fetched at <ms> (fake stamp)` row.
- The `NTP OK` / `NTP no sync` suffix on the DB row; NTP state has no tile.

##### 3.7.1.2 Layout

The shell content container stays untouched (the router still clears it). The
screen creates one child container with the flex layout and puts seven tiles in
it, so the spacing is decided by the layout, not by hand-picked coordinates.

Flex container: full 800×436, `LV_FLEX_FLOW_ROW_WRAP`,
`lv_obj_set_style_pad_all(cont, 16, 0)`, `lv_obj_set_style_pad_row(cont, 16, 0)`,
`lv_obj_set_style_pad_column(cont, 16, 0)`, `lv_obj_set_style_border_width(cont, 0, 0)`,
`lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0)`, not scrollable.

Tiles: 180×180 each. Four fit per row: 4·180 + 3·16 = 768, leaving 16 px on each
side inside the 16 px padding. Two rows: 2·180 + 16 = 376, leaving 30 px above
and below.

| Row | Order | Tile | Caption | Value |
|---|---|---|---|---|
| 1 | 1 | `s_tileIp` | `IP Address` | `link_modbus_get_ip()` string, `--` when empty |
| 1 | 2 | `s_tileNet` | `Network` | `Online` / `Offline` / `Blocked` |
| 1 | 3 | `s_tileFw` | `Firmware` | `FW_VERSION` |
| 1 | 4 | `s_tileDb` | `DB experiment` | `link_modbus_get_db_latest()` or `--` |
| 2 | 1 | `s_gaugeStorage` | `Storage` | percent filled, centre text |
| 2 | 2 | `s_gaugeHeap` | `Heap free` | used percent, centre text `123 KB` |
| 2 | 3 | `s_gaugePsram` | `PSRAM free` | used percent, centre text `4.1 MB` |

Value tile internals (180×180 cell):

- Cell: `ui_mk_card()` sized 180×180, then `lv_obj_set_flex_grow(cell, 0)`.
- Caption: `lv_label` at (8, 140), width 164, font 14, `UI_COL_DIM`, centred
  text (`lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0)`).
- Value: `lv_label` centred with `lv_obj_center()` and
  `lv_obj_set_width(value, 164)`, font `LV_FONT_MONTSERRAT_28_COMPRESSED`,
  `LV_LABEL_LONG_MODE_WRAP`, centred text, colour set per value.

Gauge tile internals (180×180 cell):

- Cell: `ui_mk_card()` sized 180×180.
- Arc: `lv_arc` at (24, 8), 132×132, range 0..100, `lv_obj_remove_style(arc,
  NULL, LV_PART_KNOB)`, `lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE)`,
  background arc `UI_COL_BAR` at 40% opacity, indicator arc coloured per
  §3.7.1.3, 10 px line width.
- Centre text: `lv_label` at (44, 62), width 92, font 20, centred, two lines
  allowed (`%d%%` and the short byte string for the memory gauges).
- Caption: same as the value tile, at (8, 140).

##### 3.7.1.3 Gauge colour

Green at 0 percent, red at 100 percent, linear in between:

```
lv_color_t lo = lv_color_hex(UI_COL_OK);      // 0x1E8E4E
lv_color_t hi = lv_color_hex(UI_COL_ALARM);   // 0xC0392B
lv_color_t c  = lv_color_mix(lo, hi, pct);    // pct is 0..100, mix wants 0..255
```

`lv_color_mix(c1, c2, mix)` is the v9 helper; confirm the exact signature in
the installed `lvgl/src/misc/lv_color.h` before writing, and scale `pct` to
0..255 (`pct * 255 / 100`) if it takes a `uint32_t` ratio.

Applied to `LV_PART_INDICATOR` of the arc with `lv_obj_set_style_arc_color()`.

##### 3.7.1.4 Gauge fill rules

| Gauge | Fill percent | Centre text | Notes |
|---|---|---|---|
| Storage | 100 while no real value exists | `100%` | decided: unknown reads as full, so it shows red until the WT32 sends a real limit. `info_storage_pct()` is the single place to change when that data arrives. |
| Heap free | used percent | free bytes, e.g. `132 KB` | free = `ESP.getFreeHeap()`, total = `ESP.getHeapSize()` if the core exposes it, otherwise `heap_caps_get_total_size(MALLOC_CAP_INTERNAL)` |
| PSRAM free | used percent | free bytes, e.g. `4.1 MB` | free = `ESP.getFreePsram()`, total = `ESP.getPsramSize()` (already used by the current row, which reports the total, not the free) |

The as-built row reads `ESP.getPsramSize()` and labels it `PSRAM`, so it shows
the total size today. That is the bug this replaces.

##### 3.7.1.5 Network state tile

Precedence, first match wins:

| Condition | Text | Colour |
|---|---|---|
| `!g.loggedIn` | `Blocked` | `UI_COL_WARN` |
| `link_modbus_get_ip()` is empty | `Offline` | `UI_COL_ALARM` |
| otherwise | `Online` | `UI_COL_OK` |

The raw `g.lastHttp` string stops being displayed. `link_modbus_get_ping_ms()`
is no longer called from this route.

##### 3.7.1.6 Font change

`lv_conf.h` currently sets `LV_FONT_MONTSERRAT_28_COMPRESSED 0`. Change it to
`1` and use `&lv_font_montserrat_28_compressed` for the tile values. Per the
LVGL built-in font docs this is Montserrat at 3 bpp: it is the largest built-in
font and the cheapest in flash, but it is still proportional, so the digits are
not monospaced. `FW_VERSION` currently reads `v9.6-fake-0.2.0` (16 characters)
and wraps inside the 164 px value area; drop the `-fake-` segment to
`v9.6-0.2.0` so it fits on one line.

##### 3.7.1.7 Functions

Header: `screen_info_show(lv_obj_t *parent)` unchanged,
`screen_info_poll()` reworked.

New statics: `s_tileIp`, `s_tileNet`, `s_tileFw`, `s_tileDb`,
`s_gaugeStorage`, `s_gaugeHeap`, `s_gaugePsram`, plus one label pointer per tile
for value / centre / caption, and `static uint32_t s_lastPollMs = 0`.

New helpers, all file-local:

- `static lv_obj_t *mk_tile(lv_obj_t *parent, const char *caption, lv_obj_t **valueOut)`:
  makes the 180×180 card, the caption, and the centred value label, returns the
  card.
- `static lv_obj_t *mk_gauge(lv_obj_t *parent, const char *caption, lv_obj_t **centreOut)`:
  makes the card, the arc, the centre label, the caption, returns the card.
- `static void gauge_set(lv_obj_t *arc, lv_obj_t *centre, int pct, const char *centreText)`:
  `lv_arc_set_value`, `lv_obj_set_style_arc_color(... LV_PART_INDICATOR)` from
  the mix in §3.7.1.3, `lv_label_set_text(centre, centreText)`.
- `static int pct_used(uint32_t freeBytes, uint32_t totalBytes)`:
  `100 - freeBytes * 100 / totalBytes`, guarded against `totalBytes == 0`.
- `static void fmt_bytes(char *out, size_t n, uint32_t bytes)`: `0 B`,
  `132 KB` under 1 MB, `%.1f MB` above.
- `static int info_storage_pct()`: returns 100 today; the future WT32 value
  goes here.
- `static void info_render()`: writes all seven tiles from live getters.

`screen_info_poll()`:

```
if (s_gaugeStorage == nullptr) return;
uint32_t now = millis();
if (now - s_lastPollMs < 1000) return;
s_lastPollMs = now;
info_render();
```

`screen_info_show()`: create the flex container, build the seven tiles in the
order of the layout table, then call `info_render()` once.

##### 3.7.1.8 Freshness

The tiles read their getters on every poll, so nothing depends on a rebuild:
the IP updates as soon as `link_modbus` has it, the DB number as soon as the
master syncs, and the memory gauges every second. That is the limit of the
current architecture: values that only arrive on the 5-minute push (chamber
readings, storage limit) are as fresh as the push, and no new transport is
introduced here.

##### 3.7.1.9 Interaction notes

- No tile is clickable; the screen is read-only.
- The screen no longer raises the SoftAP, so `/info` becomes a safe place to
  leave the device while a run is going.
- Values that do not fit are wrapped inside their tile, not clipped, because
  `LV_LABEL_LONG_MODE_WRAP` is set.

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
| No way back to `/home` except by drilling up | Add a permanent Home tab at the far left | High |
| Navigation buttons live inside each screen and duplicate per screen | Move all four tabs (Home, Data, Control, Info) into the top bar | High |
| Active location is only visible on `/control` | Highlight the tab matching the current route | High |
| `NET OK(fake)` is a fixed string that never changes | Remove it | Low |
| `POST ON` / `POST OFF` is not actionable from the bar | Remove it; POST stays on `/control` | Low |
| `exp #%d %dh` duplicates home card B | Remove it from the bar | Low |
| Bell shows a count (`ALERT(%d)`) that is also in the modal body | Show a glyph only: `LV_SYMBOL_BELL`, switching to `LV_SYMBOL_WARNING` on alerts | High |
| Bell fill (`UI_COL_DIM` / `UI_COL_OK` / `UI_COL_ALARM`) carries the meaning | Bell is white by default, red when alerts exist | Medium |
| No clock anywhere | Put a centred clock in the bar | Medium |
| x = 480–690 is empty in every state | Fill with the clock and the Info tab | Medium |

#### 3.8.1 Redesign plan — top bar

Decision: the bar becomes the navigation bar. It holds, from left to right:
Home tab, Data tab, Control tab, a centred clock, then Info tab and the
warning bell on the right. Nothing else.

##### 3.8.1.1 Removed content

`NET OK(fake)`, the POST label, and the `exp #%d %dh` label are deleted. The
bar no longer carries data; POST stays on `/control`, the experiment number
and duration live in home card B.

##### 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative)

| Widget | x | y | w | h | Label | Style |
|---|---|---|---|---|---|---|
| `s_tabHome` | 6 | 6 | 90 | 32 | `LV_SYMBOL_HOME` | tab style, see below |
| `s_tabData` | 104 | 6 | 90 | 32 | `LV_SYMBOL_LIST` | tab style |
| `s_tabControl` | 202 | 6 | 110 | 32 | `LV_SYMBOL_SETTINGS` | tab style |
| `s_clock` | 300 | 10 | 200 | 24 | `--:--` (placeholder) | `UI_COL_TEXT`, font 20, centred text |
| `s_tabInfo` | 560 | 6 | 100 | 32 | `LV_SYMBOL_FILE` | tab style |
| `s_bellBtn` | 672 | 6 | 100 | 32 | `LV_SYMBOL_BELL` / `LV_SYMBOL_WARNING` | bell style |

Tab style (this is the bordered look the current bell button already has, so
`ui_mk_button()` is reused unchanged):

- Fill `UI_COL_CARD` (white), 1 px `UI_COL_ACCENT` border, radius 8, centred
  glyph in `UI_COL_ACCENT`.
- Active tab: fill `UI_COL_ACCENT`, glyph in white. Active means the current
  route: Home for route 0, Data for routes 1, 2 and 3 (the `/data` drill-down
  keeps Data lit), Control for route 4, Info for route 5. Route 6 (`/setup`)
  lights nothing.

Bell style:

- No alerts (`fake_null_count() == 0`): fill `UI_COL_CARD`, 1 px accent border,
  glyph `LV_SYMBOL_BELL` in `UI_COL_ACCENT`.
- Alerts present: fill `UI_COL_ALARM`, no border, glyph `LV_SYMBOL_WARNING` in
  white. No count, no text.

##### 3.8.1.3 Clock

The display has no RTC and no route to the internet (it only runs a SoftAP),
so no real time source exists today. The plan ships a placeholder: the label
reads `--:--` and is set once at build. Two replacement paths, both documented
here so the decision is not lost:

- Uptime: format `millis()` as `HH:MM:SS`, refresh once per second from a new
  `ui_shell_poll()` called in `loop()`. Label would read as uptime, not clock
  time.
- Wall clock: add a `TIME_S` holding register on the WT32 master (it already
  reports NTP sync through `link_modbus_time_ok()`), read it over Modbus, and
  format with `localtime()`. This needs a change on the WT32 firmware.

Whichever path is chosen later only touches `ui_update_clock()`.

##### 3.8.1.4 Functions in `ui_shell.cpp` / `ui_shell.h`

- `build_status_bar()` (reworked): creates the four tab buttons, the clock
  label, and the bell button; nothing else. The bar object keeps its current
  800×44 `UI_COL_BAR` fill and `lv_obj_set_scrollable(bar, false)`.
- `static void on_tab_clicked(lv_event_t *e)` (new): reads `intptr_t` user data
  `0/1/2/3` -> `ui_show_home()` / `ui_show_data_picker()` / `ui_show_control()`
  / `ui_show_info()`.
- `static void set_tab_state(lv_obj_t *tab, lv_obj_t *label, bool active)` (new):
  applies the active/inactive fills from §3.8.1.2.
- `void ui_update_nav()` (new, public): maps `s_route` to the active tab and
  calls `set_tab_state()` on all four. Called at the end of `render_current()`,
  so every route change and every rebuild refreshes the highlight.
- `void ui_update_bell()` (reworked): same trigger (`fake_null_count() == 0`),
  but it sets only the glyph and the fill, no count and no `OK` text.
- `void ui_update_clock()` (new, public): placeholder implementation sets
  `--:--`; the real implementation later formats the chosen time source.
- Statics: `s_tabHome`, `s_tabData`, `s_tabControl`, `s_tabInfo`,
  `s_bellBtn`, `s_bellLabel`, `s_clock`. `s_postLabel` and `s_expLabel` are
  deleted.
- `on_bell_clicked()` is unchanged: it still opens the invalid-field modal.

##### 3.8.1.5 Interaction notes

- Tab buttons use `LV_EVENT_CLICKED`; they are plain buttons, no hold.
- The bell and the tabs sit outside `s_content`, so `lv_obj_clean(s_content)`
  on a route change never touches them.
- The back buttons inside `/data` and `/info` stay as they are for now; once
  the top bar carries all navigation, those rows become redundant and can be
  removed in a later pass.
- The 41 °C and 45 °C modals still cover the bar, since they are siblings of
  `s_content` on the active screen.

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
Content generated by `opencode/cat-jam-free` in `opencode`.