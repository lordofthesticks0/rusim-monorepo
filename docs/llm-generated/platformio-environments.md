# PlatformIO build environments (`platformio.ini`)

Scope: the build configuration for every firmware directory in this repo, how one
source directory is mapped to one environment, and the gaps in the current
configuration.

This document does not describe what any sketch does. The sketches are described
in their own documents; see `subsystem-index.md` for the map.

## 1. Layout

| Path | Meaning |
|---|---|
| `platformio.ini` | All environment definitions. The only build configuration in the repo. |
| `firmware/` | `src_dir`. Every firmware program is one subdirectory here. |
| `.pio/build/<env>/` | Compiled output per environment. Git-ignored. |
| `.pio/libdeps/<env>/` | Libraries downloaded per environment. Git-ignored. |
| `compile_commands.json` | Generated at the repo root for clangd / editors. Git-ignored. |

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
| `blink-test` | `megaatmega2560` | atmelavr | 9600 | `blink-test/` | none |
| `loadcell-test` | `megaatmega2560` | atmelavr | 115200 | `loadcell-test/` | `robtillaart/HX711@^0.6.5` |
| `co2-calibrate` | `nanoatmega328` | atmelavr | 9600 | `co2-calibrate/` | none |
| `methane-calibrate` | `megaatmega2560` | atmelavr | 115200 | `methane-calibrate/` | none |
| `pressure-calibrate` | `megaatmega2560` | atmelavr | 115200 | `pressure-calibrate/` | `robtillaart/HX711@^0.6.5` |
| `max485-test1` | `megaatmega2560` | atmelavr | 9600 | `max485-test/` | none |
| `max485-test2` | `nanoatmega328` | atmelavr | 9600 | `max485-test/` | none |
| `display-v9` | `esp32-s3-devkitc-1` | espressif32 | 115200 | `display-v9/` | LVGL 9.3.0, GFX 1.2.8, TAMC_GT911 1.0.2 |
| `wt32-eth01_ping-test` | `wt32-eth01` | espressif32 | 115200 | `wt32-eth01_ping-test/` | `dvarrel/ESPping@^1.0.5` |
| `wt32-eth01_captive-recon` | `wt32-eth01` | espressif32 | 115200 | `wt32-eth01_captive-recon/` | (core only) |
| `network-master` | `wt32-eth01` | espressif32 | 115200 | `network-master/` | `dvarrel/ESPping@^1.0.5` |

`build_src_filter = +<name>/` is what selects one directory. The `+` prefix adds
that subtree to the (empty) default filter, so only the named directory compiles
and its siblings are ignored. Two environments share one source directory on
purpose: `max485-test1` and `max485-test2` build the same code for two boards, and
the source guards on `ARDUINO_AVR_MEGA2560` so the Nano build still compiles
(see `rs485-loopback-test.md`).

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

There was a second environment, `[env:display]`, pinned to LVGL 8.3.6 and pointed
at a `firmware/display/` directory that was never committed. It failed to build
with "Nothing to build" and has been removed. `firmware/display-v9/` is the only
display source in the repo. See `migration-notes.md` for the v8 to v9 change list
and `display-docs.md` for the UI itself.

## 5. Commands

Run from the repo root.

```bash
pio pkg install                       # fetch libraries for all environments
pio run -e blink-test                 # build one environment
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

Four problems were found in this file and fixed. They are recorded here because
each one produced a build failure rather than a warning.

1. **`extra_scripts` pointed at a missing file.** `[env]` declared
   `extra_scripts = pre:scripts/compilation_db.py`, but the only version of that
   script ever committed was a one-line no-op (`Import("env")`), and it was
   deleted when the directory was renamed to `scripts-hardware/`. PlatformIO
   treats a missing pre-script as fatal, so *every* build failed at startup with
   `*** missing SConscript file 'scripts/compilation_db.py'`. The line has been
   removed. `compile_commands.json` is no longer generated by anything; the copy
   at the repo root is a stale leftover and is git-ignored.
2. **`[env:display]` had no source.** It filtered `+<display/>` and passed
   `-I firmware/display`, but `firmware/display/` was never committed. The build
   failed with "Nothing to build". The environment has been removed;
   `[env:display-v9]` supersedes it.
3. **`firmware/loadcell-test/` had no environment.** An `[env:loadcell-test]` has
   been added, targeting `megaatmega2560` at 115200 baud with
   `robtillaart/HX711@^0.6.5`, matching the D12 / D13 pins and the 115200 baud
   rate the source uses.
4. **The `max485-test` environments declared a library the source must not use.**
   Both declared `circuitstate/CSE_ArduinoRS485@^1.0.14`, while
   `firmware/max485-test/main.cpp` includes only `<Arduino.h>` and drives the
   direction pins by hand. That library asserts DE and RE oppositely, which
   shorts the tied pins on this hardware (`../hardware.md` §5). Both declarations
   have been removed.

All nine environments now build from a clean checkout with `pio run`.

`network-master` is the production WT32 env: same board and libraries as
`wt32-eth01_ping-test`, but credentials come from the display over Modbus RTU
(master, slave ID 1, 9600 8N1 on Serial1 RX=IO5/TX=IO17) instead of the
serial console. See `network-master.md`. The two prototype envs
(`wt32-eth01_ping-test`, `wt32-eth01_captive-recon`) stay for bench testing.

## 7. Related documents

- `rs485-loopback-test.md` — the shared `max485-test` source.
- `sensor-calibration-firmware.md` — `blink-test`, `co2-calibrate`,
  `methane-calibrate`, `pressure-calibrate`, `loadcell-test`.
- `wt32-eth01-ping-test.md` — the `wt32-eth01_ping-test` environment.
- `wt32-eth01-captive-recon.md` — the `wt32-eth01_captive-recon` environment.
- `network-master.md` — the production `network-master` environment.
- `display-docs.md`, `migration-notes.md` — the display environments.
- `flash-factory-tool.md` — `scripts-hardware/flash-factory.ts`.
- `../hardware.md` — pinouts for the boards referenced here.
- `../calibration.md` — human-recorded calibration results.

---
Content generated by `stealth/space-bunny-alpha` in `opencode`.
