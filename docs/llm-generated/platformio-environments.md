# PlatformIO build environments (`platformio.ini`)

Scope: the build configuration for every firmware directory in this repo, how one
source directory is mapped to one environment, and the gaps in the current
configuration.

This document does not describe what any sketch does. The sketches are described
in their own documents; see `../index.md` for the map.

## 1. Layout

| Path | Meaning |
|---|---|
| `platformio.ini` | All environment definitions. The only build configuration in the repo. |
| `firmware/` | `src_dir`. Every firmware program is one subdirectory here. |
| `.pio/build/<env>/` | Compiled output per environment. Git-ignored. |
| `.pio/libdeps/<env>/` | Libraries downloaded per environment. Git-ignored. |
| `compile_commands.json` | Generated at the repo root by `gen-compile-db.sh` for clangd / editors. Git-ignored. |

`[platformio] src_dir = firmware` means PlatformIO treats `firmware/` as the source
root. Each program is therefore addressed by its path relative to `firmware/`,
which is why an Arduino sketch directory maps onto a `firmware/` subdirectory
without any renaming.

## 2. Shared settings

Every environment inherits from `[env]`:

```
platform = atmelavr
framework = arduino
```

The two AVR program families (ATmega2560 Mega Mini, ATmega328 Nano) inherit
`platform = atmelavr`. The ESP32 environments each override `platform`,
`framework`, and `monitor_speed`.

Every environment also pins the esptool package:

```
platform_packages = platformio/tool-esptoolpy@^2.41100.260830
```

On the AVR environments this package is not part of the build. It is pinned so
that the same esptool version is available to `pio pkg exec`, which the factory
flashing script relies on (see `flash-factory-tool.md`).

## 3. Environment table

| Environment | Board | Platform | `monitor_speed` | Source dir | Libraries |
|---|---|---|---|---|---|
| `co2-calibrate` | `megaatmega2560` | atmelavr | 9600 | `co2-calibrate/` | none |
| `methane-calibrate` | `megaatmega2560` | atmelavr | 115200 | `methane-calibrate/` | none |
| `pressure-calibrate` | `uno` | atmelavr | 115200 | `pressure-calibrate/` | `robtillaart/HX711@^0.6.5` |
| `temp-probe` | `megaatmega2560` | atmelavr | 115200 | `temp-probe/` | `paulstoffregen/OneWire@^2.3.8`, `milesburton/DallasTemperature@^4.0.6` |
| `display-v9` | `esp32-s3-devkitc-1` | espressif32 | 115200 | `display-v9/` | LVGL 9.6.0, GFX 1.2.8, TAMC_GT911 1.0.2 |
| `display-sim` | native | (none) | (n/a) | `display-v9/` minus `main.cpp`, `link_serial.cpp`, `provision_ap.cpp` | LVGL 9.6.0, SDL2 |
| `wt32-eth01_captive-recon` | `wt32-eth01` | espressif32 | 115200 | `wt32-eth01_captive-recon/` | (core only) |
| `motor-driver` | `nanoatmega328new` | atmelavr | 115200 | `motor-driver/` | none |
| `pwm-50-hold` | `megaatmega2560` | atmelavr | 115200 | `tests/pwm-50-hold/` | none |
| `network-master` | `wt32-eth01` | espressif32 | 115200 | `network-master/` | `dvarrel/ESPping@^1.0.5` |
| `legacy-slave` | `megaatmega2560` | atmelavr | 9600 | `../legacy/slave/` | `paulstoffregen/OneWire@^2.3.8`, `milesburton/DallasTemperature@^4.0.6`, `robtillaart/HX711@^0.6.5`, `bblanchon/ArduinoJson@^6.21.0` |
| `legacy-master` | `megaatmega2560` | atmelavr | 115200 | `../legacy/master/` | `paulstoffregen/OneWire@^2.3.8`, `milesburton/DallasTemperature@^4.0.6`, `bblanchon/ArduinoJson@^6.21.0` |

`build_src_filter = +<name>/` is what selects one directory. The `+` prefix adds
that subtree to the (empty) default filter, so only the named directory compiles
and its siblings are ignored. `firmware/tests/` holds throwaway test sketches,
one subdirectory per environment; `pwm-50-hold` is the first one and its filter
is `+<tests/pwm-50-hold/>`.

`legacy-slave` is the one exception to the `firmware/` layout: its source sits in
`legacy/slave/`, outside `src_dir`, and the filter reaches it with `+<../legacy/slave/>`.
See `legacy-slave.md`.

## 4. The ESP32-S3 display environments

[env:display-v9] sets:

```
board_build.arduino.memory_type = qio_opi
board_build.flash_size = 16MB
board_build.psram_type = opi
-D BOARD_HAS_PSRAM
-D LV_LVGL_H_INCLUDE_SIMPLE
-D LV_CONF_INCLUDE_SIMPLE
-I firmware/display-v9
```

- `qio_opi` / `opi` select quad-SPI flash with octal PSRAM. The board's flash and
  PSRAM are OPI parts, so the wrong setting here produces a board that boots and
  then crashes on the first PSRAM access.
- `BOARD_HAS_PSRAM` tells the Arduino ESP32 core to enable the PSRAM allocator.
- `LV_CONF_INCLUDE_SIMPLE` makes `#include "lv_conf.h"` resolve to the `lv_conf.h`
  sitting in the source directory, instead of the library's own copy. Without it,
  the local configuration is ignored and the build silently uses LVGL defaults.
- `LV_LVGL_H_INCLUDE_SIMPLE` does the same for `lvgl.h`.
- `-I firmware/display-v9` points the compiler at the source directory. LVGL 9
  resolves `lv_conf.h` through `LV_CONF_INCLUDE_SIMPLE` alone, so this flag is
  redundant here. It is kept because the earlier LVGL 8 build relied on it.
- A commented-out `-DIS_DEBUG=1` line sits in the same `build_flags` block.
  Uncommenting it enables the `IS_DEBUG` debug gate (currently a login
  bypass button on `/setup`). See `display-debug-flag.md`. `[env:display-sim]`
  inherits these flags, so the sim gets it too.

There was a second environment, `[env:display]`, pinned to LVGL 8.3.6 and pointed
at a `firmware/display/` directory that was never committed. It failed to build
with "Nothing to build" and has been removed. `firmware/display-v9/` is the only
display source in the repo. See `migration-notes.md` for the v8 to v9 change list
and `display-docs.md` for the UI itself.

## 5. Commands

Run from the repo root.

```bash
pio pkg install                       # fetch libraries for all environments
pio run -e network-master             # build one environment
pio run                               # build every environment
pio run -e display-v9 -t upload       # build and flash
pio run -e display-v9 -t upload --target uploadfs   # flash a LittleFS image
pio device monitor -e display-v9      # serial monitor at the env's monitor_speed
pio device list --json-output         # enumerate serial ports
```

`--target upload` uses the board's upload protocol. The ESP32 environments upload
over USB CDC or UART depending on what the board exposes; if autodetection picks
the wrong port, pass `--upload-port`.

## 6. History of this configuration

Two problems were found in this file and fixed. They are recorded here because
each one produced a build failure rather than a warning.

1. **`extra_scripts` pointed at a missing file.** `[env]` declared
   `extra_scripts = pre:scripts/compilation_db.py`, but the only version of that
   script ever committed was a one-line no-op (`Import("env")`), and it was
   deleted when the directory was renamed to `scripts-hardware/`. PlatformIO
   treats a missing pre-script as fatal, so *every* build failed at startup with
   `*** missing SConscript file 'scripts/compilation_db.py'`. The line has been
   removed. The compilation database is produced separately by
   `gen-compile-db.sh`, which merges one `compiledb` snapshot per environment;
   `compile_commands.json` is git-ignored. See `compile-commands.md`.
2. **`[env:display]` had no source.** It filtered `+<display/>` and passed
   `-I firmware/display`, but `firmware/display/` was never committed. The build
   failed with "Nothing to build". The environment has been removed;
   `[env:display-v9]` supersedes it.
The configuration has 12 `[env:*]` sections after the standalone test
environments were removed.

`network-master` is the production WT32 environment. Credentials, network
status, DB state and experiment state travel over `rusim-line` v1 on UART0
(Debug-header `IO1`/`IO3`, 115200 8N1). See `network-master.md` and
`link-serial-protocol.md`. `wt32-eth01_captive-recon` remains available for
portal reconnaissance.

## 7. Related documents

- `sensor-calibration-firmware.md` — sensor calibration programs.
- `wt32-eth01-captive-recon.md` — the `wt32-eth01_captive-recon` environment.
- `network-master.md` — the production `network-master` environment.
- `legacy-slave.md` — the `legacy-slave` environment and its source.
- `legacy-master.md` — the `legacy-master` environment and its source.
- `display-docs.md`, `migration-notes.md` — the display environments.
- `flash-factory-tool.md` — `scripts-hardware/flash-factory.ts`.
- `../hardware.md` — pinouts for the boards referenced here.
- `../calibration.md` — human-recorded calibration results.

---
Content generated by `openai/gpt-6-codex` in `harness`.

Update by `opencode/glm-5.3-flash-free` in `opencode`: display-v9 environment table
says LVGL 9.6.0; `-DIS_DEBUG=1` in `[env:display-v9]` is now commented out as this
document already recommended, so `IS_DEBUG` is off in production builds again.

Update by `opencode/deepseek-v4.1-flash-free` in `opencode`: `network-master` now uses `rusim-line` v1 on UART0 `IO1`/`IO3` instead of Modbus RTU on `Serial1`.

Update by `openai/gpt-6-codex` in `harness`: re-counted 11 `[env:*]` sections
after removing the standalone test environments.

Update by `opencode/deepseek-v4.1-flash-free` in `opencode`: added `pwm-50-hold`
(`megaatmega2560`, source `tests/pwm-50-hold/`), the first sketch in
`firmware/tests/`. The environment count in §6 is now 12.
