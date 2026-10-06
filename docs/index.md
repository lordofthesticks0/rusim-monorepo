# Repository Index

Scope: every part of the repository, and the document that describes it. Use this
to find the right file before reading anything else.

Documents in `docs/` are written and maintained by humans. Documents in
`docs/llm-generated/` are written by AI agents. Where the two overlap, the human
document is the source of truth for anything measured on real hardware.

## 1. Repository layout

| Path | Contents |
|---|---|
| `firmware/` | One directory per PlatformIO program. All microcontroller code. |
| `dashboard/` | React + TypeScript + Vite dashboard, its Supabase project, and the dev database script. |
| `scripts-hardware/` | Bun scripts that talk to physical boards. |
| `platformio.ini` | Every firmware build environment. |
| `docs/` | Human documentation. |
| `docs/llm-generated/` | Agent-written documentation. |
| `.backup/` | Local full-flash images and digests. Git-ignored, not committed. |
| `.pio/` | PlatformIO build output. Git-ignored. |
| `compile_commands.json` | Editor tooling. Git-ignored. |

## 2. Firmware

| Directory | Board | Document |
|---|---|---|
| `firmware/display-v9/` | ESP32-S3 DevKitC-1 | `display-docs.md`, `migration-notes.md`, `display-wt32-link.md`, `display-provisioning-ap.md`, `display-sim.md`, `display-debug-flag.md` |
| `firmware/wt32-eth01_ping-test/` | WT32-ETH01 | `wt32-eth01-ping-test.md`, `wt32-eth01.md` |
| `firmware/wt32-eth01_captive-recon/` | WT32-ETH01 | `wt32-eth01-captive-recon.md` |
| `firmware/network-master/` | WT32-ETH01 | `network-master.md` (production), `display-wt32-link.md` |
| `firmware/methane-calibrate/` | ATmega2560 Mega Mini | `sensor-calibration-firmware.md` §6 |
| `firmware/pressure-calibrate/` | ATmega2560 Mega Mini | `sensor-calibration-firmware.md` §5 |
| `firmware/co2-calibrate/` | ATmega328 Nano | `sensor-calibration-firmware.md` §4 |
| `firmware/loadcell-test/` | ATmega2560 Mega Mini | `sensor-calibration-firmware.md` §3 |
| `firmware/blink-test/` | ATmega2560 Mega Mini | `sensor-calibration-firmware.md` §2 |
| `firmware/max485-test/` | ATmega2560 Mega Mini | `rs485-loopback-test.md` |

### Firmware documents

| Document | Covers |
|---|---|
| `platformio-environments.md` | `platformio.ini`: environment table, source filters, display build flags, commands, fixed defects |
| `display-docs.md` | The `display-v9` UI: screen tree, file map, fake-data serial protocol, hold-to-confirm |
| `migration-notes.md` | LVGL v8 to v9 changes and the required code edits |
| `sensor-calibration-firmware.md` | The five sensor test and calibration programs |
| `rs485-loopback-test.md` | The dual-MAX485 loopback test and its direction control |
| `wt32-eth01-ping-test.md` | The Ethernet and ping demo |
| `wt32-eth01-captive-recon.md` | Captive portal recon firmware |
| `network-master.md` | Production WT32 firmware: portal login via display Modbus |
| `display-wt32-link.md` | The single Modbus RTU line: wiring, framing, register map, flows |
| `db-sync-experiment-ntp.md` | Supabase latest-experiment sync, mismatch warn, NTP (backlog #1/#9/#12) |
| `display-provisioning-ap.md` | SoftAP credential form on the display: phone login instead of on-screen typing |
| `display-sim.md` | `[env:display-sim]`: native PC/SDL build of the display-v9 UI |
| `display-debug-flag.md` | `IS_DEBUG`: compile-time debug gate (login bypass on `/setup`) |
| `flash-factory-tool.md` | `scripts-hardware/flash-factory.ts` and the factory image |
| `captive-portal-workaround.md` | Plan for headless captive portal authentication on the WT32-ETH01 |

## 3. Dashboard

| Path | Role |
|---|---|
| `dashboard/src/App.tsx` | Root component, routing, panels |
| `dashboard/src/data.ts` | All Supabase reads, the session lifecycle, parameter metadata |
| `dashboard/src/auth.ts` | `useAuth` hook, `localStorage` session, client-side backoff |
| `dashboard/src/supabase.ts` | Typed client factory, `device_data` schema default, session header |
| `dashboard/src/csv.ts` | RFC 4180 export of an experiment's readings |
| `dashboard/src/types/domain.ts` | Domain types shared across the app |
| `dashboard/src/types/database.ts` | Generated from the live database |
| `dashboard/src/styles.css` | All styling |
| `dashboard/scripts/dev-db.ts` | Local Postgres bootstrap, migrations, seeding, token application |
| `dashboard/supabase/migrations/` | Applied in filename order |
| `dashboard/supabase/seed.sql` | Development seed data |
| `dashboard/supabase/config.toml` | Supabase CLI project config |
| `dashboard/netlify.toml` | Build, publish directory, security headers, SPA redirect |

### Dashboard documents

| Document | Covers |
|---|---|
| `dashboard.md` | Full walkthrough: running it, auth end to end, from a row to a chart point, statistics, CSV export, `dev-db`, deployment |
| `database.md` | Schema catalog for `device_data` and the unused `public` schema, relation inventories, exposure |
| `security-report.md` | Public deployment findings, ordered by severity, with verification queries |

## 4. Hardware references

These describe boards rather than this project's code.

| Document | Board |
|---|---|
| `atmega2560-mega-mini.md` | ATmega2560 Mega Mini: pinout, bootloader, differences from the Arduino Mega 2560 |
| `SUNTON-luna.md` | ESP32-8048S050C: signal map, bring-up, factory firmware, archive map |
| `SUNTON-qwen.md` | ESP32-8048S050: memory, display, touch, LVGL and ESP-IDF recommendations |
| `wt32-eth01.md` | WT32-ETH01: specifications, pinout, variants, Ethernet configuration, gotchas |

## 5. Human documents

| Path | Covers |
|---|---|
| `docs/hardware.md` | PCB documentation: module pinouts, the full Mega pin map, sensors, MAX485 |
| `docs/calibration.md` | Recorded calibration values and dates, per sensor |
| `docs/images/` | Images referenced by the human documentation |

## 6. Where new work belongs

- Firmware change: read `platformio-environments.md` for the build, the
  subsystem document above for the code, then update that subsystem document.
- Dashboard change: read `dashboard.md` for the app and `database.md` for the
  schema. `dashboard/src/supabase.ts` carries an instruction to update
  `database.md` on every schema change.
- Hardware change: read `docs/hardware.md` first and do not edit it. Report
  discrepancies to the user instead.
- Any new documentation goes in `docs/llm-generated/` with the generation footer.
