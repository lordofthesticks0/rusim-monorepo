# Graph Report - rusim-monorepo  (2026-11-17)

## Corpus Check
- 171 files · ~476,866 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 12 file(s) not represented in the graph (top: (none) 4, .toml 2, .example 1)

## Summary
- 2167 nodes · 3948 edges · 140 communities (124 shown, 16 thin omitted)
- Extraction: 84% EXTRACTED · 16% INFERRED · 0% AMBIGUOUS · INFERRED: 617 edges (avg confidence: 0.91)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `5e2bb35b`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- getParameterValue
- display-v9/main.cpp
- String
- HoldBtnCtx
- web_control.py
- network-master-passthrough.py
- monitor-legacy-pi.ts
- millis
- millis
- wt32-eth01_captive-recon/main.cpp
- link_serial.cpp
- db_contract.h
- Channel
- compilerOptions
- Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)
- Pin Layout Overview
- Display UI — LLM Documentation (`display-v9`, fake-data pass)
- legacy-master-usb.ts
- touch.h
- package.json
- Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)
- flash-factory.ts
- monitoring.test.ts
- analogRead
- ESP to database sync — plan
- captive-portal-login.sh
- sync_pi.sh
- dev-db.ts
- 5. Checklist
- Network master ↔ database communication plan
- link_stub.cpp
- 3. States
- auth.ts
- ui_modal_show
- serial_worker
- Sensor calibration firmware
- render_current
- ui_update_bell
- ATmega2560 Mega Mini — Development & Reference Documentation
- Display <-> WT32 link: `rusim-line` v1
- DbFault
- screen_setup_poll
- data.ts
- Endpoint details
- HostSerial
- hardware.md
- Reference: `device_data` column detail
- index.md
- ESP32-8048S050C · Board & Archive Guide
- dbPassTick
- history.ts
- ui_mk_label
- render_current
- 3.7.1 Redesign plan — `/info`
- WT32-ETH01 — LLM Documentation
- csv.ts
- DbPass
- screen_control.cpp
- screen_info.cpp
- Dashboard — full walkthrough
- delay
- 13. Flask REST API reference — `web_control.py`
- provision_ap.cpp
- ESP32-8048S050 — LLM Generated Summary
- temp-probe/main.cpp
- display-sensors/Arduino.h
- LVGL v8 to v9 migration report
- log_data_to_csv
- network-master/main.cpp
- Memory Safety Patterns
- Repository Index
- dbGetJson
- Stack
- authenticate
- BottleSample
- compile_commands.json and gen-compile-db.sh
- ui_mk_label
- Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)
- Documentation review — all of `docs/`
- DbExperiment
- Rules
- Patterns by Language
- Kiosk text scale
- 12. Copies & git state
- CsvDownloadHandler
- PushRow
- db-contract/main.cpp
- Reference: `device_data` column detail
- Arduino.h
- compare.ts
- PlatformIO build environments (`platformio.ini`)
- Raspberry Pi `water-control-system` mirror
- 1. Hardware & OS
- raspi-docs/README.md
- SimSerial
- `leaf` terminal reader on the Pi
- master.cpp
- backlog.md
- 8. Auxiliary tools & scripts
- Display `IS_DEBUG` flag
- 3.2.1 Redesign plan — `/home`
- learned-mistakes.md
- Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)
- Dashboard public-deploy security report
- 10. ESP-IDF Recommendations
- 6. Common Pin Map Reference
- 9. Network & services
- 3.6.1 Redesign plan — `/control`
- Display LVGL 9.3.0 -> 9.6.0 port
- HostESP
- test_display_sensors.cpp
- network-master-database-plan.md
- Stage
- 5. Frontend UI — `templates/index.html`
- 11. LVGL Recommendations
- 12. Arduino / PlatformIO Notes
- 14. Boot, Flashing, and Debugging
- 16. Performance and Stability Checklist
- 10. Security & caveats
- llm-summary/learned-mistakes.md
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- setup
- SimESPClass
- Migration plan: ESP pulls telemetry from the legacy Mega slave
- 3.3.1 Redesign plan — `/data` node picker
- ui_hold_button_set_state
- motor-driver/main.cpp

## God Nodes (most connected - your core abstractions)
1. `millis()` - 54 edges
2. `delay()` - 30 edges
3. `logf()` - 28 edges
4. `Dashboard()` - 25 edges
5. `ui_update_bell()` - 23 edges
6. `render_current()` - 23 edges
7. `Endpoint details` - 21 edges
8. `ESP32-8048S050 — LLM Generated Summary` - 21 edges
9. `dbPassTick()` - 20 edges
10. `compilerOptions` - 20 edges

## Surprising Connections (you probably didn't know these)
- `3.1.1 `SENS` sensor values` --references--> `sens_apply_real()`  [INFERRED]
  docs/llm-generated/link-serial-protocol.md → firmware/display-v9/fake_data.cpp
- `6. Log forwarding` --references--> `logf()`  [INFERRED]
  docs/llm-generated/link-serial-protocol.md → firmware/network-master/main.cpp
- `9. Failure modes` --references--> `logf()`  [INFERRED]
  docs/llm-generated/link-serial-protocol.md → firmware/network-master/main.cpp
- `11. The writer` --references--> `pushEnqueueRound()`  [INFERRED]
  docs/llm-generated/dashboard.md → firmware/network-master/main.cpp
- `1. Where the data comes from today` --references--> `pushEnqueueRound()`  [INFERRED]
  docs/llm-generated/migration-plan.md → firmware/network-master/main.cpp

## Import Cycles
- None detected.

## Communities (140 total, 16 thin omitted)

### Community 0 - "getParameterValue"
Cohesion: 0.16
Nodes (16): createChartPoints(), getMetricValue(), getParameterValue(), ReadingWithStatus, Metadata, 12.1 The experiment list is bounded by the monitoring fetch's row cap, 12.3 Dead code, 12.5 Security posture (+8 more)

### Community 1 - "display-v9/main.cpp"
Cohesion: 0.14
Nodes (4): DispBufDeleter, lvgl_tick_get(), on_debug_bypass(), UiAckFn

### Community 2 - "String"
Cohesion: 0.10
Nodes (30): 1. Usage, 2. The five steps, 3. Helper functions and their firmware counterparts, 4. Differences from the firmware, 5. Testing, 6. Related documents, Captive portal login from Linux (bash), 7. Correctness rules for the push (decided) (+22 more)

### Community 3 - "HoldBtnCtx"
Cohesion: 0.12
Nodes (19): hold_commit(), hold_tick(), HoldBtnCtx, btn, completed, halo, label, onCommit (+11 more)

### Community 4 - "web_control.py"
Cohesion: 0.10
Nodes (18): download_info(), export_data(), get_clock_payload(), get_drives(), get_interface_ip(), get_ip(), get_lan_ip(), get_timezones() (+10 more)

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 7 - "monitor-legacy-pi.ts"
Cohesion: 0.07
Nodes (45): 1. Usage, 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents (+37 more)

### Community 8 - "millis"
Cohesion: 0.13
Nodes (19): 7.1 `Serial` becomes the link, 7.4 Porting pitfalls to check at review, 7. WT32 end (`firmware/network-master/main.cpp`), 1. Environment, 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring (+11 more)

### Community 9 - "millis"
Cohesion: 0.22
Nodes (18): 7.2 Link plumbing, 7.3 State machine (semantics kept, shape changed), sleep_tick(), digitalWrite(), millis(), clearCreds(), dbPassFinish(), dbPassForce() (+10 more)

### Community 10 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.08
Nodes (33): 1. Environment, 2. What it does, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`) (+25 more)

### Community 11 - "link_serial.cpp"
Cohesion: 0.08
Nodes (21): arg_lookup(), handle_db(), handle_hello(), handle_line(), handle_log(), handle_login_req(), handle_net(), handle_result() (+13 more)

### Community 12 - "db_contract.h"
Cohesion: 0.25
Nodes (14): civilFromDays(), daysFromCivil(), findField(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject(), parseErrorCode() (+6 more)

### Community 13 - "Channel"
Cohesion: 0.10
Nodes (30): How it measures, MH-Z19C PWM Pin Readout (D4-D7), Output, Range, Run, Safety, Troubleshooting, Wiring (+22 more)

### Community 14 - "compilerOptions"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 15 - "Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)"
Cohesion: 0.29
Nodes (7): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)

### Community 16 - "Pin Layout Overview"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 17 - "Display UI — LLM Documentation (`display-v9`, fake-data pass)"
Cohesion: 0.15
Nodes (19): 4.4 Pressure (HX711 / HX710B), 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, fitExponent(), handlePingStream() (+11 more)

### Community 18 - "legacy-master-usb.ts"
Cohesion: 0.12
Nodes (25): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_WIDTHS, clock(), configurePort(), createLineGate(), Device (+17 more)

### Community 19 - "touch.h"
Cohesion: 0.13
Nodes (8): 2. Hardware / platform notes, 3.14 Backlight sleep, my_touchpad_read(), sleep_note_input(), touch(), touch_has_signal(), touch_released(), touch_touched()

### Community 20 - "package.json"
Cohesion: 0.05
Nodes (40): @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13, @types/react-dom@19.1.9, typescript@5.9.2, vite@7.1.7 (+32 more)

### Community 21 - "Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)"
Cohesion: 0.09
Nodes (21): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+13 more)

### Community 22 - "flash-factory.ts"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 23 - "monitoring.test.ts"
Cohesion: 0.19
Nodes (12): ExperimentSummary, DashboardView, DEV_FORCE_LIVE_EXPERIMENT, generateSyntheticLiveReadings(), isMonitoringPollingEligible(), isSyntheticExperiment(), selectActiveExperiment(), selectMonitoringReadings() (+4 more)

### Community 24 - "analogRead"
Cohesion: 0.23
Nodes (10): 4.2 Methane (TGS2611), Sampling, analogRead(), loop(), readRs(), rsToPpm(), rsToPpmRaw(), setup() (+2 more)

### Community 25 - "ESP to database sync — plan"
Cohesion: 0.13
Nodes (14): 1. Current state, 2. The database forbids writes, 3. Nothing reads the sensors, 4. Why the device must not send `token_hash`, 5. Credential exposure on the WT32, 9. Related documents, `authenticate()` is unthrottled, ESP to database sync — plan (+6 more)

### Community 26 - "captive-portal-login.sh"
Cohesion: 0.32
Nodes (11): attempt_login(), extract_window_location(), form_action(), log(), probe_once(), prompt_credentials(), resolve_action(), captive-portal-login.sh script (+3 more)

### Community 27 - "sync_pi.sh"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 28 - "dev-db.ts"
Cohesion: 0.06
Nodes (47): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+39 more)

### Community 29 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 30 - "Network master ↔ database communication plan"
Cohesion: 0.12
Nodes (16): 1. Recommendation and evidence, 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions (+8 more)

### Community 32 - "3. States"
Cohesion: 0.15
Nodes (15): 3.11 Overheat breaker trip, 45 °C, 3.12 Bell and invalid-field list, 3.13 Phone-associated popup, 3.15 SoftAP lifecycle, 3.16 Debug-only UI, 3.1 Route `/setup`, 3.4.1.1 Removed content, 3.4.1.2 Layout (content coordinates, panel 800x436) (+7 more)

### Community 33 - "auth.ts"
Cohesion: 0.10
Nodes (23): dependencies, react@19.1.1, react-dom@19.1.1, App(), AuthState, readStoredSession(), StoredSession, useAuth() (+15 more)

### Community 34 - "ui_modal_show"
Cohesion: 0.18
Nodes (10): 2.2 Navigation, 2.3 Data refresh, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting, 4. Cross-cutting redesign table, 5. Discrepancies found while writing this (+2 more)

### Community 35 - "serial_worker"
Cohesion: 0.11
Nodes (22): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Runtime, Three copies exist, What it does, 4. Backend — `web_control.py`, Background workers, `POST /control` Actions (+14 more)

### Community 36 - "Sensor calibration firmware"
Cohesion: 0.14
Nodes (13): 1. Common structure, 2. `co2-calibrate`, 3. `pressure-calibrate`, 4. `methane-calibrate`, 5. `temp-probe`, 8. Related documents, Correctness limits, How a reading is taken (+5 more)

### Community 37 - "render_current"
Cohesion: 0.18
Nodes (22): 1.1 Router (`ui_shell.cpp`, `s_route`), 2.1 Styling, 3.5 Route `/data/X/bottle_Y` — chamber detail, axis_caption(), build_trend_card(), mk_sensor_tile(), mk_temp_tile(), on_back_bottle() (+14 more)

### Community 38 - "ui_update_bell"
Cohesion: 0.30
Nodes (15): 3.2.1.4 Functions to add / change, 3.2.1.5 Polling, bottles_online(), home_elapsed_hours(), home_render_run(), home_render_status(), nodes_online(), on_duration_down() (+7 more)

### Community 39 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 41 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.11
Nodes (19): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display, 3.3 Gone from the Modbus design (+11 more)

### Community 42 - "DbFault"
Cohesion: 0.20
Nodes (10): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA (+2 more)

### Community 43 - "screen_setup_poll"
Cohesion: 0.13
Nodes (21): 4. Screen tree / state machine (as built), 3.8.1.1 Removed content, 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative), 3.8.1.3 Clock, 3.8.1.5 Interaction notes, 3.8.1 Redesign plan — top bar, 3.8 Persistent status bar, link_serial_take_login_request() (+13 more)

### Community 44 - "data.ts"
Cohesion: 0.14
Nodes (26): BottleChart(), BrandMark(), Chart(), ChartInteraction, chartPath(), CompareActivePoint, CompareChart(), CompareIcon() (+18 more)

### Community 45 - "Endpoint details"
Cohesion: 0.11
Nodes (19): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. `GET /files`, 14. `GET /read_log`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip` (+11 more)

### Community 46 - "HostSerial"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 47 - "hardware.md"
Cohesion: 0.12
Nodes (12): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+4 more)

### Community 48 - "Reference: `device_data` column detail"
Cohesion: 0.18
Nodes (11): Database schema catalog — LLM Documentation, `device_data`: active application schema, Equipment hierarchy, Experiments and assignments, Exposure and scope, Integrity model, `public`: legacy application schema (no longer used), Relation inventories (+3 more)

### Community 49 - "index.md"
Cohesion: 0.10
Nodes (7): Retired: display-hosted Wi-Fi provisioning, Raspberry Pi kiosk UI CSS redesign, 11. Key paths quick reference, Documentation destination, Water level sensor test, gen-compile-db.sh script, install-leaf.sh script

### Community 50 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 51 - "dbPassTick"
Cohesion: 0.13
Nodes (24): Master (`firmware/network-master/main.cpp`), 8. Build plan, Deferred (2026-10-16): sensor chain, Done criteria, Pull: partially built, 6.1 Database path (configuration, runs and uploads), 6. Memory and limits, dbClassify() (+16 more)

### Community 52 - "history.ts"
Cohesion: 0.13
Nodes (21): ExperimentHistoryView(), formatDate(), isTruncated(), ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber(), HISTORY_PARAMETERS (+13 more)

### Community 53 - "ui_mk_label"
Cohesion: 0.20
Nodes (8): `clock_sync_worker` (daemon thread), clock_sync_worker(), _extract_portal_redirect_url(), _fetch_http_date(), _get_default_gateway_ip(), _NoRedirectHandler, _parse_http_date_header(), sync_clock_from_lan_http()

### Community 54 - "render_current"
Cohesion: 0.17
Nodes (21): 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 3.3.1.3 Back button, 3.8.1.4 Functions in `ui_shell.cpp` / `ui_shell.h`, on_back_cluster(), on_back_picker(), on_bell_clicked() (+13 more)

### Community 55 - "3.7.1 Redesign plan — `/info`"
Cohesion: 0.18
Nodes (11): 3.7.1.1 Removed content, 3.7.1.2 Layout, 3.7.1.3 Gauge colour, 3.7.1.4 Gauge fill rules, 3.7.1.5 Network state tile, 3.7.1.6 Font change, 3.7.1.8 Freshness, 3.7.1.9 Interaction notes (+3 more)

### Community 56 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 57 - "csv.ts"
Cohesion: 0.19
Nodes (14): csvDisabledReason(), downloadExperimentCsv(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv() (+6 more)

### Community 58 - "DbPass"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 59 - "screen_control.cpp"
Cohesion: 0.20
Nodes (20): Display (`firmware/display-v9/`), Widget lifetime rule, 3.6.1.3 Functions, 3.6 Route `/control`, db_refresh_warn(), dur_show(), exp_mirror_link(), exp_show() (+12 more)

### Community 60 - "screen_info.cpp"
Cohesion: 0.26
Nodes (12): Current state of the code, Decoded backtrace, Experiments and results, /info Reboot: Heap-Walk Hang on `display-v9`, Symptom, 3.7.1.7 Functions, gauge_set(), info_render() (+4 more)

### Community 61 - "Dashboard — full walkthrough"
Cohesion: 0.06
Nodes (36): summarizeParameter(), 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 11. The writer, 1.1 File map, 1.2 What it is not (+28 more)

### Community 62 - "delay"
Cohesion: 0.24
Nodes (11): loop(), readCo2Ppm(), readCo2Uart(), setup(), waitForLevel(), delay(), initializeSensor(), loop() (+3 more)

### Community 63 - "13. Flask REST API reference — `web_control.py`"
Cohesion: 0.24
Nodes (13): 3.10 Overheat warning, 41 °C, link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), loop(), handle_line(), parse_onoff(), print_help() (+5 more)

### Community 64 - "provision_ap.cpp"
Cohesion: 0.10
Nodes (11): main(), run(), braced(), declarations(), MethaneCalibrationRegression, native_source(), get_json(), main() (+3 more)

### Community 65 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.15
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 66 - "temp-probe/main.cpp"
Cohesion: 0.46
Nodes (7): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup()

### Community 68 - "LVGL v8 to v9 migration report"
Cohesion: 0.15
Nodes (12): 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, File-by-file summary, LVGL v8 to v9 migration report, Memory and runtime considerations (+4 more)

### Community 69 - "log_data_to_csv"
Cohesion: 0.17
Nodes (13): 7. Data & logging, CSV format (current schema), Existing logs, Older schema (2025-12-12.csv), Storage, USB export, 7. `POST /toggle_logging`, fmt_val() (+5 more)

### Community 70 - "network-master/main.cpp"
Cohesion: 0.11
Nodes (29): Verification, 5.1 As built, 5. ESP implementation steps, Source references, arg_lookup(), BottleReading, copyFormArg(), dbApplyRunState() (+21 more)

### Community 71 - "Memory Safety Patterns"
Cohesion: 0.18
Nodes (10): 1. Memory Bug Categories, 2. Safety Spectrum, Best Practices, Core Concepts, Debugging Tools, Detailed patterns and worked examples, Do's, Don'ts (+2 more)

### Community 72 - "Repository Index"
Cohesion: 0.17
Nodes (12): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 4. Hardware references, 5. Human documents, 5a. Agent lessons (+4 more)

### Community 73 - "dbGetJson"
Cohesion: 0.26
Nodes (13): dbArrayIsEmpty(), DbFetch, DB_FETCH_AUTH, DB_FETCH_FAIL, DB_FETCH_OK, DB_FETCH_PAYLOAD, dbGetJson(), dbIntField() (+5 more)

### Community 74 - "Stack"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 75 - "authenticate"
Cohesion: 0.21
Nodes (14): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), CONCLUDED_AFTER_MS, fetchDevice(), fetchExperimentReadings(), fetchReadings(), MAX_READING_ROWS (+6 more)

### Community 76 - "BottleSample"
Cohesion: 0.20
Nodes (10): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC (+2 more)

### Community 77 - "compile_commands.json and gen-compile-db.sh"
Cohesion: 0.29
Nodes (6): 1. What the file is, 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh

### Community 78 - "ui_mk_label"
Cohesion: 0.15
Nodes (12): Global state, `polling_worker` (daemon thread), control_api(), desired_state_commands(), polling_worker(), send_serial_cmd(), state_mismatch_commands(), stir_command() (+4 more)

### Community 79 - "Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)"
Cohesion: 0.29
Nodes (7): 1. Purpose, 2. Wire protocol, 3. How it opens the port, 4. REPL behaviour, 5. Usage, 6. Limits, Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)

### Community 80 - "Documentation review — all of `docs/`"
Cohesion: 0.40
Nodes (4): 2. Checked and left alone, 3. Open items, 4. What a reader should take from this, Documentation review — all of `docs/`

### Community 81 - "DbExperiment"
Cohesion: 0.33
Nodes (6): DbExperiment, durationH, id, sampleIntervalS, setpoints, dbFindExperiment()

### Community 82 - "Rules"
Cohesion: 0.13
Nodes (14): Backlog and Continuity, Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General (+6 more)

### Community 83 - "Patterns by Language"
Cohesion: 0.22
Nodes (8): memory-safety-patterns — detailed patterns and worked examples, Pattern 1: RAII in C++, Pattern 2: Smart Pointers in C++, Pattern 3: Ownership in Rust, Pattern 4: Safe Resource Management in C, Pattern 5: Bounds Checking, Pattern 6: Preventing Data Races, Patterns by Language

### Community 84 - "Kiosk text scale"
Cohesion: 0.40
Nodes (5): 2026-10-20 change, Deploying a change to the kiosk, How it works, Kiosk text scale, Related documents

### Community 85 - "12. Copies & git state"
Cohesion: 0.22
Nodes (9): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, UI, `web_control.py` (+1 more)

### Community 87 - "PushRow"
Cohesion: 0.20
Nodes (10): PushRow, bottleId, ch4, co2, experimentId, nullMask, ph, pressure (+2 more)

### Community 88 - "db-contract/main.cpp"
Cohesion: 0.36
Nodes (8): contains(), main(), test_ack_and_errors(), test_array_iteration(), test_envelope(), test_field_parsing(), test_sanitize(), test_timestamps()

### Community 89 - "Reference: `device_data` column detail"
Cohesion: 0.22
Nodes (9): Access control, Chamber numbering, Development tooling, `device_data.device_config`, `device_data.devices`, `device_data.experiments`, `device_data.readings`, `device_data.sessions` (+1 more)

### Community 90 - "Arduino.h"
Cohesion: 0.11
Nodes (7): Configuration, Display V9 PC Simulator (`[env:display-sim]`), Run it, Scope note, What is shared vs stubbed, random(), randomSeed()

### Community 91 - "compare.ts"
Cohesion: 0.27
Nodes (14): CompareView(), ExperimentResultsView(), displayChamberNumber(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor() (+6 more)

### Community 92 - "PlatformIO build environments (`platformio.ini`)"
Cohesion: 0.25
Nodes (8): 1. Layout, 2. Shared settings, 3. Environment table, 4. The ESP32-S3 display environments, 5. Commands, 6. History of this configuration, 7. Related documents, PlatformIO build environments (`platformio.ini`)

### Community 94 - "Raspberry Pi `water-control-system` mirror"
Cohesion: 0.29
Nodes (7): Checksums, Layout, Raspberry Pi `water-control-system` mirror, Refreshing the mirror, Related documents, Relationship to the deleted `main_ui.html`, What is not included

### Community 95 - "1. Hardware & OS"
Cohesion: 0.33
Nodes (6): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account

### Community 96 - "raspi-docs/README.md"
Cohesion: 0.33
Nodes (6): 2. Boot & kiosk session model, Boot sequence, Chromium kiosk, Notes / caveats, openbox autostart (`~/.config/openbox/autostart`), PM2 service (`/etc/systemd/system/pm2-invitro.service`)

### Community 97 - "SimSerial"
Cohesion: 0.20
Nodes (4): Next step (open), display-v9: RAII ownership, my_disp_flush(), SimSerial

### Community 98 - "`leaf` terminal reader on the Pi"
Cohesion: 0.33
Nodes (6): How the cross-compile works, `leaf` terminal reader on the Pi, Rebuilding and removing, Running it, Verification, Why it is not compiled on the Pi

### Community 100 - "master.cpp"
Cohesion: 0.05
Nodes (62): 10. Known Gaps, 1. Overview, 2. Pin Assignment, 3. Boot Defaults, 4.1 Trip thresholds, 4.2 Asymmetric Fast-Trip / Slow-Recovery Debouncing, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing (+54 more)

### Community 102 - "8. Auxiliary tools & scripts"
Cohesion: 0.50
Nodes (4): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc

### Community 103 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses

### Community 104 - "3.2.1 Redesign plan — `/home`"
Cohesion: 0.25
Nodes (8): 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.3 State and data model changes needed, 3.2.1.6 Integration with other screens, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels), 3.2.1.8 Out of scope for this screen (stays on /control), 3.2.1 Redesign plan — `/home`, 3.2 Route `/home`

### Community 105 - "learned-mistakes.md"
Cohesion: 0.29
Nodes (6): 2026-10-05, 2026-10-06, 2026-10-08, 2026-10-20, 2026-10-24, 2026-10-31

### Community 106 - "Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)"
Cohesion: 0.29
Nodes (6): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`), 8. `POST /download/info`, status()

### Community 108 - "Dashboard public-deploy security report"
Cohesion: 0.17
Nodes (11): 1. Blocker — legacy `public` schema is public-read (live, confirmed), 2. High — `authenticate()` has no brute-force protection, 3. High — Netlify bundle + env handling, 4. Medium — session hygiene and PII, 5. Low — functional / ops, Already fixed / verified good (keep these), Dashboard public-deploy security report, Findings (ordered by severity) (+3 more)

### Community 109 - "10. ESP-IDF Recommendations"
Cohesion: 0.33
Nodes (6): 10. ESP-IDF Recommendations, ESP-IDF components you may use, Important menuconfig areas, Recommended minimum version, Target setup, Typical required features

### Community 110 - "6. Common Pin Map Reference"
Cohesion: 0.33
Nodes (6): 6.1 RGB display control pins, 6.2 RGB data pins, common capacitive variant, 6.3 GT911 capacitive touch pins, 6.4 Other common board pins, 6.5 Known documentation trap, 6. Common Pin Map Reference

### Community 111 - "9. Network & services"
Cohesion: 0.15
Nodes (12): 9. Network & services, Enabled services (selected), Listening ports, Network Architecture (LAN Primary), Security note (see §10), 13. Flask REST API reference — `web_control.py`, Source-level notes, Stopping the Flask app does not stop the heater (+4 more)

### Community 112 - "3.6.1 Redesign plan — `/control`"
Cohesion: 0.33
Nodes (6): 3.6.1.1 Removed content, 3.6.1.2 Layout (content coordinates, panel 800x436), 3.6.1.4 Constants, 3.6.1.5 DB status text (shortened to fit 200 px at font 14), 3.6.1.6 Run button colours, 3.6.1 Redesign plan — `/control`

### Community 113 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 115 - "test_display_sensors.cpp"
Cohesion: 0.07
Nodes (57): 10. WT32 link (`rusim-line` v1, production), 11. Setup route, 12. WT32 login page QR, 13. Light theme and the /setup QR, 14. Debug flag `IS_DEBUG`, 1. Overview, 3. File map, 5. USB control and diagnostic protocol (+49 more)

### Community 116 - "network-master-database-plan.md"
Cohesion: 0.18
Nodes (5): Auth used, Build, DB experiment sync + NTP — LLM Documentation, Link (`rusim-line` v1), Display <-> WT32 link (`display-wt32-link`) — replaced

### Community 117 - "Stage"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 118 - "5. Frontend UI — `templates/index.html`"
Cohesion: 0.50
Nodes (4): 5. Frontend UI — `templates/index.html`, Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer, Tabs, Temporary MOSFET Speed Control (`0–255` Debit Testing)

### Community 119 - "11. LVGL Recommendations"
Cohesion: 0.50
Nodes (4): 11. LVGL Recommendations, Important LVGL settings, LVGL performance tips, Recommended LVGL version

### Community 120 - "12. Arduino / PlatformIO Notes"
Cohesion: 0.50
Nodes (4): 12. Arduino / PlatformIO Notes, Arduino IDE typical board settings, PlatformIO example concepts, Useful Arduino/PlatformIO libraries

### Community 121 - "14. Boot, Flashing, and Debugging"
Cohesion: 0.50
Nodes (4): 14. Boot, Flashing, and Debugging, Boot button, Native USB conflict, UART console

### Community 122 - "16. Performance and Stability Checklist"
Cohesion: 0.50
Nodes (4): 16. Performance and Stability Checklist, Display checklist, LVGL checklist, Touch checklist

### Community 123 - "10. Security & caveats"
Cohesion: 0.50
Nodes (4): 10. Security & caveats, Operational caveats, Recommendations, Security

### Community 125 - "13. ESPHome Notes"
Cohesion: 0.67
Nodes (3): 13. ESPHome Notes, ESPHome cautions, Relevant ESPHome components

### Community 126 - "15. Filesystem and Asset Storage"
Cohesion: 0.67
Nodes (3): 15. Filesystem and Asset Storage, Asset recommendations, Practical partition advice

### Community 127 - "4. Display Subsystem"
Cohesion: 0.67
Nodes (3): 4. Display Subsystem, Display interface, Typical timing parameters

### Community 128 - "5. Touch Variants"
Cohesion: 0.67
Nodes (3): 5. Touch Variants, Capacitive touch, GT911-class, Resistive touch, XPT2046-class

### Community 130 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.18
Nodes (11): 1. What the program does, 2. Build environment, 3. Pin map, 4.1 pH, 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing, 6. Responsiveness (+3 more)

### Community 131 - "setup"
Cohesion: 0.20
Nodes (11): emit_experiment(), emit_hello(), link_serial_db_ready(), link_serial_emit_experiment(), link_serial_init(), link_serial_send_raw(), link_serial_set_exp_state(), write_line() (+3 more)

### Community 136 - "Migration plan: ESP pulls telemetry from the legacy Mega slave"
Cohesion: 0.22
Nodes (9): 1. Where the data comes from today, 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour, 6. Deferred, with the hook each needs, 7. Verification, 8. Open risks, Migration plan: ESP pulls telemetry from the legacy Mega slave (+1 more)

### Community 137 - "3.3.1 Redesign plan — `/data` node picker"
Cohesion: 0.33
Nodes (6): 3.3.1.1 Removed content, 3.3.1.2 Layout (content coordinates, panel 800x436), 3.3.1.4 Behaviour kept, 3.3.1.5 Open item, 3.3.1 Redesign plan — `/data` node picker, 3.3 Route `/data` — node picker

### Community 138 - "ui_hold_button_set_state"
Cohesion: 0.40
Nodes (5): on_run_commit(), on_stirrer_commit(), ui_hold_button_set_state(), ui_hold_button_set_symbol(), ui_hold_button_state()

### Community 139 - "motor-driver/main.cpp"
Cohesion: 0.70
Nodes (4): loop(), rampUp(), setup(), stopMotor()

## Knowledge Gaps
- **98 isolated node(s):** `DB_FAULT_NONE`, `DB_FAULT_NET`, `DB_FAULT_AUTH`, `DB_FAULT_PAYLOAD`, `DB_FAULT_SCHEMA` (+93 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 936 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **16 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `millis` to `display-v9/main.cpp`, `temp-probe/main.cpp`, `setup`, `master.cpp`, `render_current`, `ui_update_bell`, `network-master/main.cpp`, `DbFault`, `link_serial.cpp`, `wt32-eth01_captive-recon/main.cpp`, `Display UI — LLM Documentation (`display-v9`, fake-data pass)`, `touch.h`, `dbPassTick`, `analogRead`, `Arduino.h`, `screen_info.cpp`, `delay`, `13. Flask REST API reference — `web_control.py``?**
  _High betweenness centrality (0.130) - this node is a cross-community bridge._
- **Are the 53 inferred relationships involving `millis()` (e.g. with `7.3 State machine (semantics kept, shape changed)` and `What is shared vs stubbed`) actually correct?**
  _`millis()` has 53 INFERRED edges - model-reasoned connections that need verification._
- **What connects `DB_FAULT_NONE`, `DB_FAULT_NET`, `DB_FAULT_AUTH` to the rest of the system?**
  _98 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `display-v9/main.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.14461538461538462 - nodes in this community are weakly interconnected._
- **Why does `LVGL UI redesign (`display-v9`)` connect `ui_modal_show` to `3. States`, `render_current`?**
  _High betweenness centrality (0.045) - this node is a cross-community bridge._
- **Are the 29 inferred relationships involving `delay()` (e.g. with `What is shared vs stubbed` and `loop()`) actually correct?**
  _`delay()` has 29 INFERRED edges - model-reasoned connections that need verification._
- **Should `String` be split into smaller, more focused modules?**
  _Cohesion score 0.09803921568627451 - nodes in this community are weakly interconnected._