# Repository Index

Scope: every part of the repository, and the document that describes it. Use this
to find the right file before reading anything else.

Documents in `docs/` are written and maintained by humans. Documents in
`docs/llm-generated/` are written by AI agents. Where the two overlap, the human
document is the source of truth for anything measured on real hardware.

## 1. Repository layout

| Path | Contents |
|---|---|
| [`firmware/`](../firmware/) | One directory per PlatformIO program. All microcontroller code. |
| [`dashboard/`](../dashboard/) | React + TypeScript + Vite dashboard, its Supabase project, and the dev database script. |
| [`scripts-hardware/`](../scripts-hardware/) | Bun hardware tools and the PlatformIO WT32 passthrough upload hook. |
| [`platformio.ini`](../platformio.ini) | Every firmware build environment. |
| [`docs/`](../docs/) | Human documentation. |
| [`docs/llm-generated/`](../docs/llm-generated/) | Agent-written documentation. |
| [`docs/llm-generated/raspi-docs/`](../docs/llm-generated/raspi-docs/) | Agent-written documentation of the live Raspberry Pi `invitrocontroller` system: board, OS, kiosk, backend, firmware, logging, network. |
| [`.backup/`](../.backup/) | Local full-flash images and digests. Git-ignored, not committed. |
| [`.pio/`](../.pio/) | PlatformIO build output. Git-ignored. |
| [`compile_commands.json`](../compile_commands.json) | Editor tooling: the clangd compilation database, merged from every PlatformIO environment by [`gen-compile-db.sh`](../gen-compile-db.sh) |

## 2. Firmware

| Directory | Board | Document |
|---|---|---|
| [`firmware/display-v9/`](../firmware/display-v9/) | ESP32-S3 DevKitC-1 | [display-docs.md](llm-generated/display-docs.md), [migration-notes.md](llm-generated/migration-notes.md), [link-serial-protocol.md](llm-generated/link-serial-protocol.md), [display-provisioning-ap.md](llm-generated/display-provisioning-ap.md), [display-sim.md](llm-generated/display-sim.md), [display-debug-flag.md](llm-generated/display-debug-flag.md), [lvgl-redesign.md](llm-generated/lvgl-redesign.md), [display-info-reboot.md](llm-generated/display-info-reboot.md) |
| [`firmware/wt32-eth01_ping-test/`](../firmware/wt32-eth01_ping-test/) | WT32-ETH01 | [wt32-eth01-ping-test.md](llm-generated/wt32-eth01-ping-test.md), [wt32-eth01.md](llm-generated/wt32-eth01.md) |
| [`firmware/wt32-eth01_captive-recon/`](../firmware/wt32-eth01_captive-recon/) | WT32-ETH01 | [wt32-eth01-captive-recon.md](llm-generated/wt32-eth01-captive-recon.md) |
| [`firmware/network-master/`](../firmware/network-master/) | WT32-ETH01 | [network-master.md](llm-generated/network-master.md) (production), [link-serial-protocol.md](llm-generated/link-serial-protocol.md) |
| [`firmware/methane-calibrate/`](../firmware/methane-calibrate/) | ATmega2560 Mega Mini | [sensor-calibration-firmware.md](llm-generated/sensor-calibration-firmware.md) §6 |
| [`firmware/pressure-calibrate/`](../firmware/pressure-calibrate/) | ATmega2560 Mega Mini | [sensor-calibration-firmware.md](llm-generated/sensor-calibration-firmware.md) §5 |
| [`firmware/temp-probe/`](../firmware/temp-probe/) | ATmega2560 Mega Mini | [sensor-calibration-firmware.md](llm-generated/sensor-calibration-firmware.md) §7 |
| [`firmware/co2-calibrate/`](../firmware/co2-calibrate/) | ATmega328 Nano | [sensor-calibration-firmware.md](llm-generated/sensor-calibration-firmware.md) §4 |
| [`firmware/loadcell-test/`](../firmware/loadcell-test/) | ATmega2560 Mega Mini | [sensor-calibration-firmware.md](llm-generated/sensor-calibration-firmware.md) §3 |
| [`firmware/blink-test/`](../firmware/blink-test/) | ATmega2560 Mega Mini | [sensor-calibration-firmware.md](llm-generated/sensor-calibration-firmware.md) §2 |
| [`firmware/max485-test/`](../firmware/max485-test/) | ATmega2560 Mega Mini | [rs485-loopback-test.md](llm-generated/rs485-loopback-test.md) |

### Firmware documents

| Document | Covers |
|---|---|
| [platformio-environments.md](llm-generated/platformio-environments.md) | `platformio.ini`: environment table, source filters, display build flags, commands, fixed defects |
| [compile-commands.md](llm-generated/compile-commands.md) | `compile_commands.json` and `gen-compile-db.sh`: the clangd compilation database, regeneration, and which errors it cannot remove |
| [display-docs.md](llm-generated/display-docs.md) | The `display-v9` UI: screen tree, file map, fake-data serial protocol, hold-to-confirm |
| [lvgl-redesign.md](llm-generated/lvgl-redesign.md) | `display-v9` UI redesign working document: as-built layout and behaviour per state and tab, empty redesign tables |
| [migration-notes.md](llm-generated/migration-notes.md) | LVGL v8 to v9 changes and the required code edits |
| [sensor-calibration-firmware.md](llm-generated/sensor-calibration-firmware.md) | The six sensor test and calibration programs |
| [rs485-loopback-test.md](llm-generated/rs485-loopback-test.md) | The dual-MAX485 loopback test and its direction control |
| [wt32-eth01-ping-test.md](llm-generated/wt32-eth01-ping-test.md) | The Ethernet and ping demo |
| [wt32-eth01-captive-recon.md](llm-generated/wt32-eth01-captive-recon.md) | Captive portal recon firmware |
| [network-master.md](llm-generated/network-master.md) | Production WT32 firmware: portal login via display `rusim-line` link |
| [link-serial-protocol.md](llm-generated/link-serial-protocol.md) | `rusim-line` v1 wire spec: wiring, line rules, command set, log forwarding, `PASSTH`, RS485 follow-on |
| [display-wt32-link.md](llm-generated/display-wt32-link.md) | Retired: replaced by `link-serial-protocol.md` |
| [comms-rework-plan.md](llm-generated/comms-rework-plan.md) | Implemented plan that produced `rusim-line` v1 (blueprint; the wire spec is `link-serial-protocol.md`) |
| [db-sync-experiment-ntp.md](llm-generated/db-sync-experiment-ntp.md) | Supabase latest-experiment sync, mismatch warn, NTP (backlog #1/#9/#12) |
| [esp-db-sync.md](llm-generated/esp-db-sync.md) | ESP to database round trip: the missing push path, write grants, sensor chain, token exposure |
| [display-provisioning-ap.md](llm-generated/display-provisioning-ap.md) | SoftAP credential form on the display: phone login instead of on-screen typing |
| [display-sim.md](llm-generated/display-sim.md) | `[env:display-sim]`: native PC/SDL build of the display-v9 UI |
| [display-debug-flag.md](llm-generated/display-debug-flag.md) | `IS_DEBUG`: compile-time debug gate (login bypass on `/setup`) |
| [display-info-reboot.md](llm-generated/display-info-reboot.md) | `/info` reboot triage: heap-walk hang, backtrace decode, CPU gauge + `MEM` command, open corruption hunt |
| [display-lvgl-9-6-port.md](llm-generated/display-lvgl-9-6-port.md) | LVGL 9.3.0 -> 9.6.0 port: config + deprecated-API changes |
| [memory-safety-hardening.md](llm-generated/memory-safety-hardening.md) | RAII ownership on `display-v9`, bounded buffers + atomic ISR flag on `network-master` |
| [flash-factory-tool.md](llm-generated/flash-factory-tool.md) | `scripts-hardware/flash-factory.ts` and the factory image |
| [captive-portal-workaround.md](llm-generated/captive-portal-workaround.md) | Plan for headless captive portal authentication on the WT32-ETH01 |
| [captive-portal-login-script.md](llm-generated/captive-portal-login-script.md) | `scripts-hardware/captive-portal-login.sh`: the firmware login sequence as a one-shot bash script |
| [legacy-slave.md](llm-generated/legacy-slave.md) | `legacy/slave/new-slave.cpp`: 4-bottle sensor slave, JSON protocol, USB stream, HX711 init trap, known gaps |
| [migration-plan.md](llm-generated/migration-plan.md) | Plan: ESP pulls telemetry from the legacy Mega slave over UART1, log-only first step, pH conversion deferred to the slave |
| [legacy-master.md](llm-generated/legacy-master.md) | `legacy/master/master.cpp`: water-bath master firmware, MOSFET PWM pump and stirrer ramping, hysteresis, Pi USB serial commands |

## 3. Dashboard

| Path | Role |
|---|---|
| [`dashboard/src/App.tsx`](../dashboard/src/App.tsx) | Root component, routing, panels |
| [`dashboard/src/data.ts`](../dashboard/src/data.ts) | All Supabase reads, the session lifecycle, parameter metadata |
| [`dashboard/src/auth.ts`](../dashboard/src/auth.ts) | `useAuth` hook, `localStorage` session, client-side backoff |
| [`dashboard/src/supabase.ts`](../dashboard/src/supabase.ts) | Typed client factory, `device_data` schema default, session header |
| [`dashboard/src/csv.ts`](../dashboard/src/csv.ts) | RFC 4180 export of an experiment's readings |
| [`dashboard/src/types/domain.ts`](../dashboard/src/types/domain.ts) | Domain types shared across the app |
| [`dashboard/src/types/database.ts`](../dashboard/src/types/database.ts) | Generated from the live database |
| [`dashboard/src/styles.css`](../dashboard/src/styles.css) | All styling |
| [`dashboard/scripts/dev-db.ts`](../dashboard/scripts/dev-db.ts) | Local Postgres bootstrap, migrations, seeding, token application |
| [`dashboard/supabase/migrations/`](../dashboard/supabase/migrations/) | Applied in filename order |
| [`dashboard/supabase/seed.sql`](../dashboard/supabase/seed.sql) | Development seed data |
| [`dashboard/supabase/config.toml`](../dashboard/supabase/config.toml) | Supabase CLI project config |
| [`dashboard/netlify.toml`](../dashboard/netlify.toml) | Build, publish directory, security headers, SPA redirect |

### Dashboard documents

| Document | Covers |
|---|---|
| [dashboard.md](llm-generated/dashboard.md) | Full walkthrough: running it, auth end to end, from a row to a chart point, statistics, CSV export, `dev-db`, deployment |
| [database.md](llm-generated/database.md) | Schema catalog for `device_data` and the unused `public` schema, relation inventories, exposure |
| [security-report.md](llm-generated/security-report.md) | Public deployment findings, ordered by severity, with verification queries |

## 3a. Legacy / Raspberry Pi

| Path | Role |
|---|---|
| [`legacy/pi/water-control-system/`](../legacy/pi/water-control-system/) | Mirror of the live Pi project: `web_control.py`, `templates/index.html`, `water_control_arduino/`, `logs/`. Edit here, then copy to the Pi. [pi-water-control-system-mirror.md](llm-generated/pi-water-control-system-mirror.md). The kiosk text is scaled by 20 %; see [pi-kiosk-font-scale.md](llm-generated/pi-kiosk-font-scale.md). Manual deploy plus automatic master-firmware flash: [`sync_pi.sh`](../sync_pi.sh), also installed at `/home/invitro/sync_pi.sh` on the Pi — [pi-sync-script.md](llm-generated/pi-sync-script.md). |
| [`legacy/pi/captive-portal-login.sh`](../legacy/pi/captive-portal-login.sh) | Bash reimplementation of the master firmware's captive-portal login sequence. [captive-portal-login-script.md](llm-generated/captive-portal-login-script.md). |
| [`legacy/slave/new-slave.cpp`](../legacy/slave/new-slave.cpp) | ATmega2560 Mega Mini sensor slave: 4 bottles of pH, CH4, temperature, CO2 and pressure, published as JSON over RS485 and USB. [legacy-slave.md](llm-generated/legacy-slave.md). |
| [`legacy/master/master.cpp`](../legacy/master/master.cpp) | ATmega2560 Mega Mini water-bath master controller: bath DS18B20, Slave UART poller, heater/pump overrides, and 3-second MOSFET PWM ramping on D3 for the pump and D4 for the stirrer. [legacy-master.md](llm-generated/legacy-master.md). |
| [`scripts-hardware/read-legacy-slave.ts`](../scripts-hardware/read-legacy-slave.ts) | Bun wrapper that reads the slave's JSON stream and prints a per-bottle table. [legacy-slave-json-wrapper.md](llm-generated/legacy-slave-json-wrapper.md). |
| [`scripts-hardware/monitor-legacy-pi.ts`](../scripts-hardware/monitor-legacy-pi.ts) | Bun monitor for the master on the Pi over SSH: `/status` polling by default, `--raw` follows the Flask app's `/raw` buffer without stopping the service. [legacy-pi-remote-monitor.md](llm-generated/legacy-pi-remote-monitor.md). |

## 3b. Raspberry Pi system documentation (`raspi-docs/`)

Agent-written documentation of the live `invitrocontroller` Raspberry Pi 3B system, generated from SSH inspection. Index: [raspi-docs/README.md](llm-generated/raspi-docs/README.md).

| Document | Covers |
|---|---|
| [raspi-docs/README.md](llm-generated/raspi-docs/README.md) | Overview, quick facts, architecture at a glance, current runtime state |
| [raspi-docs/01-hardware-and-os.md](llm-generated/raspi-docs/01-hardware-and-os.md) | Board, OS, toolchain, packages |
| [raspi-docs/02-boot-and-kiosk.md](llm-generated/raspi-docs/02-boot-and-kiosk.md) | Autologin, LightDM, openbox, PM2, Chromium kiosk |
| [raspi-docs/03-project-overview.md](llm-generated/raspi-docs/03-project-overview.md) | `water-control-system` and its three copies |
| [raspi-docs/04-backend-web-control.md](llm-generated/raspi-docs/04-backend-web-control.md) | Flask + SocketIO backend, routes, workers, CSV logging |
| [raspi-docs/05-frontend-ui.md](llm-generated/raspi-docs/05-frontend-ui.md) | Kiosk dashboard: tabs, indicators, bottle grid |
| [raspi-docs/06-master-firmware.md](llm-generated/raspi-docs/06-master-firmware.md) | Arduino master sketch `water_control_arduino.ino` |
| [raspi-docs/07-data-and-logging.md](llm-generated/raspi-docs/07-data-and-logging.md) | CSV schema, logs, USB export |
| [raspi-docs/08-auxiliary-tools.md](llm-generated/raspi-docs/08-auxiliary-tools.md) | Helper scripts, analog tests, rpi-clone |
| [raspi-docs/09-network-and-services.md](llm-generated/raspi-docs/09-network-and-services.md) | Network, enabled services, cloudflared |
| [raspi-docs/10-security-and-caveats.md](llm-generated/raspi-docs/10-security-and-caveats.md) | Risks, offline state, observations |
| [raspi-docs/11-key-paths.md](llm-generated/raspi-docs/11-key-paths.md) | Quick path reference |
| [raspi-docs/12-copies-and-git-state.md](llm-generated/raspi-docs/12-copies-and-git-state.md) | The three project copies and their uncommitted changes |
| [raspi-docs/13-rest-api-reference.md](llm-generated/raspi-docs/13-rest-api-reference.md) | Every Flask route with request/response shapes, heater-off procedure, stopping the service safely |
| [raspi-docs/14-leaf-reader.md](llm-generated/raspi-docs/14-leaf-reader.md) | `leaf` 0.2.0 terminal reader: aarch64 cross-compile on the dev machine, verification, rebuild script |

## 4. Hardware references

These describe boards rather than this project's code.

| Document | Board |
|---|---|
| [atmega2560-mega-mini.md](llm-generated/atmega2560-mega-mini.md) | ATmega2560 Mega Mini: pinout, bootloader, differences from the Arduino Mega 2560 |
| [SUNTON-luna.md](llm-generated/SUNTON-luna.md) | ESP32-8048S050C: signal map, bring-up, factory firmware, archive map |
| [SUNTON-qwen.md](llm-generated/SUNTON-qwen.md) | ESP32-8048S050: memory, display, touch, LVGL and ESP-IDF recommendations |
| [wt32-eth01.md](llm-generated/wt32-eth01.md) | WT32-ETH01: specifications, pinout, variants, Ethernet configuration, gotchas |

## 5. Human documents

| Path | Covers |
|---|---|
| [`docs/hardware.md`](../docs/hardware.md) | PCB documentation: module pinouts, the full Mega pin map, sensors, MAX485 |
| [`docs/calibration.md`](../docs/calibration.md) | Recorded calibration values and dates, per sensor |
| [`docs/images/`](../docs/images/) | Images referenced by the human documentation |

## 6. Where new work belongs

- Firmware change: read [platformio-environments.md](llm-generated/platformio-environments.md)
  for the build, the subsystem document above for the code, then update that
  subsystem document.
- Dashboard change: read [dashboard.md](llm-generated/dashboard.md) for the app
  and [database.md](llm-generated/database.md) for the schema.
  [`dashboard/src/supabase.ts`](../dashboard/src/supabase.ts) carries an
  instruction to update `database.md` on every schema change.
- Hardware change: read [docs/hardware.md](hardware.md) first and do not edit
  it. Report discrepancies to the user instead.
- Any new documentation goes in `docs/llm-generated/` with the generation footer.
