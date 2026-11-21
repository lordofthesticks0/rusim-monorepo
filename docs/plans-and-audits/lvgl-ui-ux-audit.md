# LVGL UI — User Experience Audit (`display-v9`)

Scope: `firmware/display-v9/` screens, shell, hold button, sleep, and the
LVGL configuration that affects what the operator sees. Method: source review
only. Nothing was run on the panel or in `display-sim`; items marked
*unverified* need a look on hardware.

Panel: ESP32-8048S050 (5", 800×480, about 7.4 px/mm).

Severity:

- **High**: the operator can lose data, stop a run, or be misled about the
  state of the rig.
- **Medium**: an interaction regularly fails, or it takes clear extra effort
  or guesswork.
- **Low**: polish, consistency, or wording.

## 1. Summary

| # | Severity | Finding | Where |
|---|---|---|---|
| 1 | High | Re-login after a captive-portal expiry stops the running experiment | `screen_setup.cpp:104-109` |
| 2 | High | Display reboot tells the WT32 the run is stopped | `main.cpp:192`, `fake_data.cpp:91` |
| 3 | High | Every new sensor frame rebuilds the whole screen, cancelling 2 s holds | `ui_shell.cpp:414-424, 489-492` |
| 4 | High | `/control` resets the chosen experiment number on every rebuild | `screen_control.cpp:87-103, 142, 229` |
| 5 | High | Setpoint and stirrer look functional but send nothing | `screen_home.cpp:130-146`, `screen_control.cpp:145-169` |
| 6 | High | The tap that wakes the screen also presses whatever is under it | `main.cpp:92-107`, `screen_sleep.cpp` |
| 7 | Medium | Bell and "invalid" counter are permanently red | `fake_data.cpp:113-120`, `ui_shell.cpp:253-271` |
| 8 | Medium | Home progress uses the local duration, not the frozen run duration | `screen_home.cpp:93-127, 148-166` |
| 9 | Medium | Starting from `/home` skips the experiment-number warning | `screen_home.cpp:168-173, 231` |
| 10 | Medium | Back button on `/data/X` jumps to Home, not the node list | `screen_data.cpp:151-152` |
| 11 | Medium | Touch targets are small (status bar 4.3 mm) | `ui_shell.cpp:234-246`, steppers |
| 12 | Medium | Steppers have no hold-to-repeat | all `LV_EVENT_CLICKED` steppers |
| 13 | Medium | Buttons have no pressed feedback | `ui_shell.cpp:55-72` |
| 14 | Medium | Hold buttons show state by red/green colour only | `ui_hold_button.cpp:54, 173` |
| 15 | Medium (kept for now) | LVGL performance monitor overlay is compiled in | `lv_conf.h:1010-1023` |
| 16 | Medium (kept for now) | Debug bypass and link-test button ship in normal builds | `ui_config.h:65-67`, `screen_setup.cpp:89-93`, `screen_control.cpp:300-305` |
| 17 | Medium | `LOGINREQ` pulls the operator to `/setup` with no explanation | `screen_setup.cpp:136-143` |
| 18 | Medium | No status-bar indicator for WT32 link, internet, or upload | `ui_shell.cpp:225-251` |
| 19 | Low | Status-bar clock is a permanent `--:--` | `ui_shell.cpp:297-299` |
| 20 | Low | "Chamber temp" card always shows `-- C` | `screen_home.cpp:45-50` |
| 21 | Low | Overheat modal text tells the user to run a rejected command | `ui_shell.cpp:436-449` |
| 22 | Low | A new modal silently replaces the current one | `ui_shell.cpp:310-311` |
| 23 | Low | Trend chart: fixed scale, 12 min window, wiped by a 90 s outage | `screen_data.cpp:166-171`, `fake_data.cpp:32-39` |
| 24 | Low | Zero-based and mixed naming: `NODE 0`, `Bottle 0`, `N0/C2` | `screen_data.cpp`, `ui_shell.cpp:101` |
| 25 | Low | Icons without labels; unusual back symbol | `ui_shell.cpp:234-246`, `screen_data.cpp:75` |
| 26 | Low | `/info` jargon, dead Storage tile, likely IP wrap | `screen_info.cpp` |
| 27 | Low | No completion state when the run reaches 100 % | `screen_home.cpp:102-127` |
| 28 | Low | Mixed `C` and `°C` | several |

## 2. High

### 2.1 Re-login stops the running experiment

The WT32 falls back to `NEED_CREDS` when its 204 probe fails during a run and
sends `LOGINREQ` (`network-master/main.cpp` header comment). The display shows
`/setup`. When the login succeeds, `setup_close_success()` runs:

```cpp
g.experimentRunning = false;
g.experimentStartMs = 0;
link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
```

That sends `EXPERIMENT run=0` to the WT32, which pauses uploads for the active
run. A routine portal session expiry therefore stops the experiment, and the
operator is not told.

Fix direction: only reset run state on the first login since boot. On later
logins keep `g.experimentRunning` as it was.

### 2.2 Display reboot reports "stopped"

`fake_init()` sets `g.experimentRunning = false` and `setup()` immediately
calls `link_serial_set_exp_state(...)`. A brown-out or watchdog reset of the
display during a run sends `run=0`. The display has no way to learn the real
state: it never reads the run flag back from the WT32, even though
`link_serial_get_db_active_exp()` already reports the frozen active run.

Fix direction: treat the WT32 as the source of truth for "running" after a
reboot (adopt the reported active run), and do not emit `EXPERIMENT` until the
WT32 state has been received.

### 2.3 Full-screen rebuild on every frame

`on_sens_timer` runs every 1 s. When `sens_apply_real()` sees a new frame
(about every 5 s) it calls `ui_shell_on_data()` → `ui_refresh_current()` →
`render_current()`, which deletes and recreates every widget on the current
route.

Effects:

- A 2000 ms hold on the Run or Stirrer button is cancelled if a frame lands
  during the hold. With a 5 s cadence that is roughly 2 holds in 5. The ring
  disappears with no message, so the operator assumes they let go too early.
- Any press in progress (stepper, tile) is lost.
- The full 800×436 content area is redrawn every 5 s through a 1/4-screen
  buffer. Possible visible flicker or tearing (*unverified*).

Fix direction: keep widgets alive and update label text and styles in place.
`/home` and `/info` already have render functions that do this
(`home_render_status`, `info_render`); `/data` routes need equivalents. If a
rebuild is unavoidable, skip it while `lv_indev_get_active_obj()` is non-null.

### 2.4 Experiment number keeps resetting on `/control`

`screen_control_show()` and `screen_control_hide()` both clear
`s_dbSuggested`. `db_refresh_warn()` then overwrites `g.expNum` with
`max(latest, cfgMax) + 1` whenever `s_dbSuggested` is false. Because
`/control` is rebuilt on every frame (2.3), the operator's choice is
replaced within about 5 s. Selecting experiment `0` (the testing experiment)
or re-using a configured id is effectively impossible from the screen.

Fix direction: suggest once per boot or per DB change, and never after the
operator has touched the experiment stepper.

### 2.5 Setpoint and stirrer controls do nothing

The setpoint steppers on `/home` and `/control`, and the stirrer hold button,
change local state and print `[FAKE->WT32] ...` to USB serial. Nothing is sent
over the link. The UI gives no hint of this; the operator sees `37.5 C` and a
green stirrer button and reasonably believes the rig obeyed.

Fix direction: until transport exists, mark these controls as "not connected"
(disabled style plus a caption), or hide them.

### 2.6 Wake tap is delivered as a press

`my_touchpad_read()` reports the touch to LVGL whether or not the backlight is
off. `sleep_tick()` only turns the backlight back on. A tap on the dark screen
therefore also clicks the widget under the finger: it can step the setpoint,
change duration or experiment number, open a node, or switch tabs. Hold
buttons are safe only because they need 2 s.

The 60 s timeout makes this frequent: the operator cannot glance at run
progress without touching.

Fix direction: while asleep, swallow the first press (report `RELEASED` until
the finger lifts) and only wake. Consider a longer timeout, or dimming
instead of off, while a run is active.

## 3. Medium

### 3.1 Bell and "invalid" counter are always red

`clear_bottle()` sets all five `sensorNull` flags for absent bottles, and
`fake_null_count()` counts every slot. With one real node (4 bottles), the
other 20 bottles contribute 100 permanent NULL fields. The bell is always red
with a warning icon and `/home` always shows `⚠ 100+ invalid` in red. A
permanent alarm teaches operators to ignore it, so a real sensor failure on
node 0 is not noticed.

Fix direction: count NULL fields only for bottles that are expected (present
in the frame, or in the DB bottle count). Show absent bottles as "not
connected", not as an alarm.

### 3.2 Home progress uses the wrong duration

`home_elapsed_hours()` and the arc use `g.durationH`, the local stepper value.
The run actually uses the frozen snapshot, which may come from the DB
(`link_serial_get_db_active_dur()`). Also, the `/home` duration steppers stay
active during a run, so pressing them changes the progress arc and the
`x / y h` text immediately, while the real run is unchanged. Elapsed time is
measured from the display's `millis()`, so it resets to 0 after a display
reboot.

Fix direction: during a run, show the frozen duration from the WT32 and
disable the duration steppers (`/control` already says edits are pending).

### 3.3 Starting from `/home` skips the warnings

The `/home` run button calls the same `ui_experiment_toggle()`, but `/home`
does not show the `/control` messages: "exp N exists, use M" and
"DB 24h" vs "local 24h". An operator can start a run that re-uses an id with
readings, or uses a different duration than they expect, without seeing
either.

Fix direction: show the same description line on `/home`, or keep the
start control only on `/control`.

### 3.4 Back button on a node page goes to Home

`screen_cluster_show()` wires its back button to `on_back_picker`, which calls
`ui_show_home()`. `on_back_cluster` (which goes to the picker) exists but is
not used. From `/data/X` the operator expects the node list and lands on Home.
Also, the picker's own back button goes Home, which duplicates the Home tab.

### 3.5 Small touch targets

At about 7.4 px/mm:

| Control | Size (px) | Size (mm) |
|---|---|---|
| Status-bar tabs and bell | 32 × 32, 8 px gaps | 4.3 × 4.3 |
| `/home` setpoint steppers | 44 × 36 | 5.9 × 4.9 |
| `/control` duration/experiment steppers | 48 × 40 | 6.5 × 5.4 |

Common guidance for touch is at least 9 mm (about 64 px here), more with lab
gloves. The bell sits in the screen corner next to the Info tab.

### 3.6 Steppers have no repeat

All steppers react to `LV_EVENT_CLICKED` only. Worst cases: setpoint 35.0 to
40.0 is 50 taps; duration 1 to 99 h is 98 taps; experiment number 0 to 9999 is
one tap per id. `LV_EVENT_LONG_PRESSED_REPEAT` provides auto-repeat with no
extra state.

### 3.7 No pressed feedback

No LVGL theme is enabled, and `ui_mk_button()` sets styles only for
`LV_STATE_DEFAULT`. Buttons do not change when pressed. Only the bottle sensor
tiles set an `LV_STATE_PRESSED` style. The operator cannot tell whether a tap registered until the value changes
(and the nav tabs give no feedback at all until the route redraws).

Fix direction: add a pressed background (for example `UI_COL_BAR`) in
`ui_mk_button()`.

### 3.8 Hold buttons: colour-only state

Run and Stirrer show state only as green (on) or red (off). Problems:

- Red is read as "danger" or "stop", but here it means idle, shown with a
  Play icon.
- Red/green is the most common colour-vision deficiency pair.
- The stirrer icon is `LV_SYMBOL_POWER`, which reads as "power off the
  device".
- `/control` has no ON/OFF text for the stirrer. `/home` has no "hold 2 s"
  hint; a short tap flashes the ring and does nothing.

Fix direction: add a state label ("Stirrer: ON", "Run: idle"), use a neutral
colour for idle, and add the hold hint on `/home`.

### 3.9 Performance monitor overlay

Status: kept for now (decision 2026-11-21). The overlay stays while the
firmware is still being developed. Remove it before field use.

`lv_conf.h` sets `LV_USE_SYSMON 1` and `LV_USE_PERF_MONITOR 1` at
`LV_ALIGN_BOTTOM_RIGHT`. LVGL draws an FPS/CPU label over the bottom-right
corner of every screen, including the `/info` CPU tile. This is a developer
tool. The `/info` CPU gauge already reads `lv_timer_get_idle()` and does not
need the overlay.

### 3.10 Debug UI in normal builds

Status: kept for now (decision 2026-11-21). The bypass button and PUSH EXP
stay while the firmware is still being developed. Set `-DIS_DEBUG=0` and gate
PUSH EXP before field use.

`IS_DEBUG` defaults to 1 and `-DIS_DEBUG=0` is commented out in
`platformio.ini`. Every build shows an orange "DEBUG: bypass login" button on
`/setup`, which lets anyone skip the login. The "PUSH EXP" link-test button on
`/control` is not gated by `IS_DEBUG` at all, and its "sent EXPERIMENT now"
text never clears.

### 3.11 `LOGINREQ` routes away without explanation

When the WT32 asks for a new login, the display jumps to `/setup` from
whatever screen is open. No modal says why. Because `g.loggedIn` stays true,
the nav tabs remain enabled on `/setup`, so the operator can leave it and the
setup screen looks optional. After success it returns to Home, not to where
the operator was. Combined with 2.1, this is the path that stops runs.

### 3.12 No connection status in the status bar

The status bar shows tabs, a clock placeholder and the bell. Whether the WT32
link, the internet, and DB uploads are healthy is only on `/info`, and
"Network: Online" there only means an IP string is stored. After a link drop,
the first visible sign is node 0 going grey 90 s later. A small link/upload
icon in the bar would answer "is my data being saved?" at a glance. This does
not conflict with the locked "no staleness UI" decision, which concerns
per-value staleness.

## 4. Low

1. **Clock.** `ui_update_clock()` always writes `--:--`. It is in the most
   prominent place on the bar and looks broken. Either feed time from the WT32
   (it already reports `tok`) or show something useful, such as run elapsed
   time.
2. **Chamber temp card.** `g.chamberTempC` is never set from the link, so the
   top-left card on `/home` always shows `-- C`.
3. **Overheat text.** The 41 °C / 45 °C modals say "Set TEMP < 45 via
   serial", but the `TEMP` USB command is rejected (`display-docs.md` §5). The
   45 °C modal also claims "Stirrer switched OFF", which only flips a local
   flag.
4. **Modal replacement.** `ui_modal_show()` calls `ui_modal_hide()` first. If
   an overheat modal is open and another modal is shown (DB-block, bell), the
   alarm disappears and its latch prevents it from showing again. Latent while
   overheat has no data source. A new modal also does not wake a sleeping
   backlight.
5. **Trend chart.** Fixed scales (temperature 25–45 °C over about 112 px is
   0.18 °C per pixel, so normal drift is a flat line), no tick labels, no
   min/max, and a 12-minute window on runs that last 1–99 h. A 90 s outage
   calls `clear_bottle()`, which erases the trend. The selected tile shows a
   NULL value in white, hiding the red "failed" colour.
6. **Naming.** Screens say `NODE 0` and `Bottle 0`; the bell says `N0/C2`
   (C = bottle, from "chamber"). Code says "cluster". Operators usually count
   from 1 and use one word per thing.
7. **Icons.** Data is a list icon, Control a gear, Info a file icon, all
   without labels. Back buttons use `LV_SYMBOL_NEW_LINE` (↵), which reads as
   "enter", not "back"; `LV_SYMBOL_LEFT` is the usual choice. Swipe
   navigation exists but is not hinted anywhere, and swiping from a bottle
   page drops the drill-down.
8. **`/info`.** Values like `12/24h/db`, `0` / `12s/--`, `12 (24bt)` need the
   docs to read. Storage is always `--`. An IP such as `192.168.100.200` in
   the 28 px font is wider than the 164 px label and will likely wrap
   mid-number (*unverified*).
9. **Run completion.** At 100 % the arc stays full and the run keeps showing
   as running with no "complete" state or prompt.
10. **Units.** `/home`, `/control` and node tiles print `37.0 C`; bottle
    captions and the chart use `°C`. The built-in Montserrat fonts include
    `°`.
11. **Setup screen.** The typed-fallback URL is 14 px, small for reading
    across a bench. While waiting for an IP the screen gives no troubleshooting
    hint (cable, WT32 power) and no timeout.

## 5. Aesthetics

### 5.1 Hardware constraint

The ESP32-S3 renders every pixel in software through a 1/4-screen buffer.
Visual fixes must stay cheap to draw:

| Cheap (use) | Expensive (avoid) |
|---|---|
| Flat colour fills | Animations and screen transitions |
| 1–2 px borders, rounded corners | Blur and shadows (`shadow_width` is a blur kernel) |
| Labels, arcs, small circles | Gradients over large areas |
| Opaque objects | Partial opacity over large areas (every pixel is blended) |
| Larger fonts (cost is flash, not CPU) | Image scaling and rotation |

The largest rendering cost in the current UI is not a style choice: it is the
full-content rebuild every ~5 s (2.3). Fixing that saves more CPU than any
style change.

### 5.2 Summary

| # | Finding | Where |
|---|---|---|
| A1 | Accent-blue outlines on every card and button | `ui_shell.cpp:55-85` |
| A2 | Status bar blends into the page; tabs are unbalanced; tab borders stay grey after login | `ui_shell.cpp:190-251` |
| A3 | Red and orange appear in normal, healthy states | `ui_hold_button.cpp`, `screen_setup.cpp:95-98` |
| A4 | Warning and OK text fail contrast; offline cards are nearly invisible | `ui_config.h:69-82` |
| A5 | Weak type hierarchy and mixed capitalisation | all screens |
| A6 | Each screen uses a different page structure | all screens |
| A7 | Uneven spacing and empty areas on `/home` and `/control` | `screen_home.cpp:175-238`, `screen_control.cpp:228-305` |
| A8 | Node picker buttons carry only a digit | `screen_data.cpp:50-77` |
| A9 | Modal looks the same for every severity and has a fixed size | `ui_shell.cpp:310-331` |
| A10 | `/setup` is unbalanced and its key text is small | `screen_setup.cpp:64-102` |
| A11 | Trend chart is busy and has faint grid lines | `screen_data.cpp:276-306` |
| A12 | Mixed units and symbols | several |

### 5.3 Findings

**A1. Blue outlines everywhere.** `ui_mk_card()` draws a 2 px `UI_COL_ACCENT`
border and `ui_mk_button()` a 1 px one. Every card, tile and button has the
same blue outline, so the screen looks like a wireframe and blue no longer
means "you can tap this". Non-tappable `/info` tiles and tappable `/data`
tiles look identical. The outlines exist because white cards on `UI_COL_BG`
have a contrast of only 1.12:1.

Direction: give cards a neutral 1 px border (for example `0xC9D1DC`) and keep
the accent for tappable outlines and the selected/active fill. This keeps the
same draw cost.

**A2. Status bar.** `UI_COL_BAR` on `UI_COL_BG` is 1.15:1, so the bar barely
separates from the content, although the comment in `ui_config.h` says it
must. Home, Data and Control sit at the left; Info sits at the right next to
the bell, apart from the other tabs. The centre shows the `--:--` placeholder.

Bug: `set_tab_locked(true)` sets the tab border to `UI_COL_DIM`, and neither
`set_tab_locked(false)` nor `set_tab_state()` restores it. After login the
four tabs keep grey outlines while the bell keeps a blue one.

Direction: a solid darker bar (for example accent fill with white icons) or a
1 px divider line under it; the four nav tabs grouped together as wider
icon + label tabs; the bell alone at the right; the page title in the centre.

**A3. Alarm colours at rest.** The idle state, which is the normal state,
shows red: one 96 px red circle on `/home` and two on `/control`. The bell is
always red (3.1). `/setup` prints its normal instructions in `UI_COL_WARN`
orange. With red and orange on every screen, a real fault does not stand out.

Direction: neutral (grey or accent) for idle and off, green for running, red
only for faults, orange only for warnings.

**A4. Contrast.** Measured with the WCAG formula (4.5:1 is the minimum for
14 px text):

| Pair | Ratio | Used for |
|---|---|---|
| `UI_COL_WARN` on white | 3.82 | `/control` experiment line |
| `UI_COL_WARN` on `UI_COL_BG` | 3.40 | `/setup` status text |
| `UI_COL_OK` on white | 4.17 | `/control` experiment line, `/info` values |
| `UI_COL_CARD_OFF` vs `UI_COL_BG` | 1.24 | Offline node and bottle cards |
| `UI_COL_DIM` on `UI_COL_CARD_OFF` | 4.29 | Text on offline cards |

Darker candidates that pass on both white and `UI_COL_BG`: warn `0x8F5A0C`
(5.78 / 5.14), OK `0x17773F` (5.60 / 4.99). Offline cards need a second cue
besides fill, such as an "Offline" caption or a dashed/grey border.

**A5. Typography.** The most important numbers are not the largest:

- `/home` chamber temperature and bottle count use 20 px, the same as the
  setpoint and the clock.
- Run progress (`12.3 / 24 h`, `51 %`) is 14 px inside the arc.
- `lv_font_montserrat_12` is enabled in `lv_conf.h` but not used; it only
  costs flash.

Capitalisation is mixed: `CHAMBER TEMP`, `BOTTLES ONLINE`, `NODES`,
`BOTTLE 0` versus `Target setpoint`, `Experiment runtime`, `Duration`,
`Setup: network login`.

Montserrat has proportional digits, so a centred value such as the setpoint
shifts sideways when it changes (`37.1` → `37.0`).

Direction: a fixed scale of 14 px captions, 20 px section titles, 28 px
values and one hero size (36–48 px, digits-only subset generated with
`lv_font_conv` to keep flash small); one capitalisation rule; right- or
left-align changing numbers so only one edge moves.

**A6. Page structure.** Every screen is built differently:

| Screen | Title | Grouping | Back |
|---|---|---|---|
| `/home` | none | 2 cards + floating controls | — |
| `/data` picker, `/data/X` | centred caps | buttons/tiles | centred bottom |
| `/data/X/bottle_Y` | left, mixed size | tiles + card | centred bottom |
| `/control` | none | floating widgets, no cards; setpoint has no label | — |
| `/info` | none | tile grid | — |

Direction: one template. Title at top-left (or in the status bar centre),
content in cards on a 16 px grid, back button at top-left beside the title.
`/info` is the most consistent screen and is a good model.

**A7. Spacing on `/home` and `/control`.**

- `/home`: cards start at y = 8 while side margins are 16. The
  "Experiment runtime" caption is 4 px under the cards. The hold-button wrap
  (y 314–426) touches the arc's bottom edge (y 314). The lower-left and
  lower-right areas are empty while the centre column is crowded. Card B uses
  about 110 of its 160 px.
- `/control`: the duration and experiment rows end at y = 416 of 436, packed
  against the bottom edge, while the left column holds only the link-test
  button.

Direction: a two-column layout on `/home` (left: run card with arc, button
and duration; right: status card), and grouped cards on `/control`
("Temperature", "Stirrer", "Experiment").

**A8. Node picker.** Six 200 × 140 buttons each show one digit. There is no
"Node" word, no bottle count, no alert state, and offline is shown only by the
1.24:1 fill change.

Direction: `Node 1` title, a second line such as `4 bottles · online`, and a
small status dot (a filled circle is cheap to draw).

**A9. Modal.** The 45 °C trip, the DB-block notice and the bell list all use
the same white card with a blue border. The card is fixed at 500 × 340 with
the button fixed at y = 250, so a two-line message leaves a large gap. The
scrim is 70 % black over the full 800 × 480 area, and every redraw under it
(every ~5 s with 2.3) must blend all those pixels.

Direction: colour the title strip by severity (red, orange, neutral), size the
card to its content with a flex column, and use an opaque dark scrim, or skip
background refreshes while a modal is open.

**A10. `/setup`.** The right column has only "Login page:" and a 14 px URL.
The status paragraph sits at the bottom in orange. While waiting for an IP,
the QR is hidden and leaves an empty 300 × 300 hole. The orange debug button
is the most prominent element on the screen.

Direction: numbered 20 px steps (1. Scan, 2. Log in, 3. Wait), the URL at
28 px, and a bordered placeholder box with "Waiting for WT32…" in the QR
position.

**A11. Trend chart.** A 6 px dot on all 24 points plus a 3 px line looks busy.
The grid lines use `UI_COL_BAR` on white (about 1.3:1) and are hard to see.
There are no Y values on the chart itself.

Direction: hide point indicators except the latest one, darken the grid
lines, and place min/max labels at the left edge as plain labels.

**A12. Units and symbols.** `37.0 C` on `/home`, `/control` and node tiles,
`°C` on bottle captions; `0.1 C steps`; pressure prints a sign (`+0.12`)
while no other sensor does; `N0/C2` in the bell list; `↵` as the back icon.

## 6. Suggested order

1. 2.1 and 2.2 (run stopped by re-login or reboot). These are data-loss bugs.
2. 2.3 and 2.4 (in-place updates instead of rebuilds; stop overriding the
   experiment number).
3. 2.6 (swallow wake tap). 3.9 and 3.10 are kept for now.
4. 2.5 and 3.1 (honest controls and a meaningful bell).
5. Remaining Medium, then Low.
6. Aesthetics: A3 and A4 first (colour meaning and contrast are palette edits
   in `ui_config.h` and the hold button), then A1 and A2, then layout work
   (A6, A7) together with the in-place update refactor from 2.3, since both
   touch every screen builder.

---
Updated at 2026-11-21-13:58.
