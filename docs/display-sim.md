# Display V9 PC Simulator (`[env:display-sim]`)

Runs the real `display-v9` LVGL UI on a desktop window instead of the
Sunton 800x480 panel, so screens can be iterated on without flashing.

## Run it

```bash
pio run -e display-sim -t exec
```

The window is 800x480 and mouse input works as touch. Typed lines on the
sim's stdin are fed through the same serial command protocol used over USB.
`HELP` lists the commands: `SETPOINT`, `DURATION`, `STIRRER`, `POST`, `RUN`,
`EXP`, `PUSH`, `STATUS`, `LINK`, `MEM` and `PASSTH`. `TEMP`, `NODE`, `NULL` and
`UNNULL` are recognised only to print a rejection message, because measured
values come from the WT32.

## What is shared vs stubbed

- Shared, compiled unchanged: `ui_shell.cpp`, all `screen_*.cpp`,
  `fake_data.cpp`, `serial_cmd.cpp`.
- Shim: `firmware/display-v9/sim/Arduino.h` provides `millis()`, `delay()`,
  `Serial` (printf + line-buffered stdin), `pinMode`/`digitalWrite` no-ops,
  `random`/`randomSeed`, `map`, and a stub `ESP.*`.
- Stub: `firmware/display-v9/sim/link_stub.cpp` implements every
  `link_serial_*` entry point in RAM. `LINK` prints the link state; the WT32
  never answers, so no `NET`, `DB` or `RESULT` arrives and `/setup` stays up
  unless the `IS_DEBUG` bypass is used.
- Not present: `link_serial.cpp` and `main.cpp` (the ESP32/GFX/touch driver),
  both excluded via `build_src_filter`; `sim/main_native.cpp` supplies `main()`.

## Configuration

`firmware/display-v9/sim/lv_conf.h` is a copy of the firmware one with
`LV_COLOR_DEPTH 32` and `LV_USE_SDL 1` / `LV_USE_OS LV_OS_SDL2`. It lives in
the `-I firmware/display-v9/sim` path that is searched before the firmware
directory, so the firmware `lv_conf.h` is untouched and still used by
`[env:display-v9]`.

## Scope note

There is no theme and no GFX framebuffer path in the sim; fonts, widget
styling and screen transitions are identical because the same draw code
runs. Sensor timing and WT32 link behavior are faked. Windows: the same
`display-sim` env is intended to work (native platform + SDL2), but only
Linux was verified.

## Widget lifetime rule

`ui_shell.cpp` `render_current()` frees the route with `lv_obj_clean()`.
Any screen that keeps static widget pointers or `lv_timer`s across routes
must tear them down before the clean: `screen_control_hide()` deletes the
hold-to-confirm timers and nulls the statics, and `screen_control_poll()`
returns early unless `ui_is_control()`. Without this the poll/timer
callbacks touch deleted objects and LVGL logs `No screen found`
(`lv_obj_tree.c`) every loop. `screen_info`/`screen_setup` polls are
guarded by `ui_is_info()`/`ui_is_setup()` for the same reason.

---
Updated at 2026-11-18-00:12.
