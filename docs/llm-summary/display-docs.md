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
| `main.cpp` | HW init (gfx, touch, flush, draw buf, indev) + `fake_init()` + `ui_init()`; `loop()` = serial poll → sleep tick → `lv_timer_handler()` |
| `lv_conf.h` | Enables vs stock: `BUTTON, SPINBOX (+TEXTAREA, required by spinbox), SWITCH, CHART` = 1; fonts +12/+20/+28. **`SLIDER` deliberately left 0** (not needed). `MSGBOX/LIST/TABLE` left 0 — modals and lists are hand-built from `obj+label+button` |
| `touch.h` | Unchanged GT911 glue |
| `ui_config.h` | Counts (6 clusters × 4 bottles, 2 online), ranges, 500 ms / 30 s / 5 min timings, thresholds, pins, palette, `FW_VERSION` |
| `fake_data.h/.cpp` | `AppState g`: experiment, setpoint, stirrer, POST, chamber temp, 6×4 bottles × 5 sensors + NULL flags + temp trend ring; `fake_init()` seeds 2 online / 4 offline; `fake_wt32_push()` jitters online values every 5 min (lv_timer) |
| `serial_cmd.h/.cpp` | Human-typeable debug protocol over USB serial (see §5). Display-local only |
| `ui_shell.h/.cpp` | Status bar, content router, single blocking-modal primitive, bell logic, overheat eval, `ui_mk_*` styled helpers (no LVGL theme is enabled, so all styling is explicit) |
| `screen_boot.h/.cpp` | `/boot` blocking gate: spinbox 1–99 h ±1 h + confirm; must confirm every boot |
| `screen_home.h/.cpp` | `/home`: DATA / CONTROL / INFO nav + run summary + serial hint |
| `screen_data.h/.cpp` | `/data` picker (6 clusters, offline greyed), `/data/X` 2×2 bottle grid ("node offline" inline), `/data/X/bottle_Y` 5 sensor rows + `lv_chart` temp trend (values ×10, range 300–450) |
| `screen_control.h/.cpp` | `/control`: setpoint −/+ 0.1 °C (35.0–40.0, logs `[FAKE->WT32]`), stirrer + POST switches with hold-to-confirm progress bars |
| `screen_info.h/.cpp` | `/info`: rebuilt on every open; fake storage/IP/HTTP/FW rows + real heap/PSRAM rows + fetch timestamp |
| `screen_sleep.h/.cpp` | Idle timer on last-touch millis; backlight off after 30 s, on at next touch tick |

## 4. Screen tree / state machine (as built)

```
/boot (blocking gate, spinbox 1-99h, confirm) -> /home
/home -> /data (picker) -> /data/X (grid) -> /data/X/bottle_Y (5 sensors + chart)
/home -> /control (setpoint, stirrer hold-500ms, POST hold-500ms)
/home -> /info (fresh on open)
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
HELP | STATUS | PUSH
TEMP 38.3            chamber temp (drives 41/45 UI; TRY: TEMP 41.5, TEMP 45.2, TEMP 37.0)
SETPOINT 37.5        35.0-40.0, mirrors the /control stepper
DURATION 24          1-99 whole hours, mirrors the boot spinbox
STIRRER ON|OFF       also logs [FAKE->WT32] like the on-screen toggle
POST ON|OFF
NODE <0-5> ON|OFF    OFF nulls the whole node; ON clears its NULLs
NULL <c> <b> <s>     set one field NULL   (s: 0 CO2,1 CH4,2 Press,3 pH,4 Temp)
UNNULL <c> <b> <s>   clear one field NULL
```

There are deliberately **no on-screen debug buttons/modals for faking** —
all injection goes through this port. `STATUS` prints run state + node list.

> Future: Central→Display will be framed **JSON over UART** pushed every
> 5 min and parsed into `AppState` (replacing `fake_wt32_push` internals and
> this text protocol). On-screen code reads only `AppState`, so the swap is
> contained in `fake_data.cpp` + a new UART parser (`TODO(WT32)`).

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

- Real WT32 UART JSON push parser + `Serial1` wiring + PSRAM single-cycle buffer.
- Real `→WT32` command transport (today: `Serial.printf("[FAKE->WT32] …")`).
- Backlight GPIO45 conflict resolution with EE.
- `LV_MEM_SIZE` is still 64 KB — bump if chart/table-heavy screens OOM.
- Duration sub-hour steps, wake-confirm UX, breaker power-domain scope: parked
  open questions, unchanged.
