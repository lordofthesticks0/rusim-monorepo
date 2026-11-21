# Graph Report - rusim-monorepo  (2026-11-21)

## Corpus Check
- 170 files · ~237,670 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 13 file(s) not represented in the graph (top: (none) 5, .toml 2, .example 1)

## Summary
- 2295 nodes · 4308 edges · 141 communities (133 shown, 8 thin omitted)
- Extraction: 83% EXTRACTED · 17% INFERRED · 0% AMBIGUOUS · INFERRED: 752 edges (avg confidence: 0.9)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `05855bb1`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Display Fake Data Harness
- Display Link Serial Protocol
- Flask Server Imports
- Dashboard Chart UI
- Hardware Reference Docs
- Legacy Pi Monitor CLI
- network-master-passthrough.py
- Display Link Stub
- network-master/main.cpp
- Dev Database Seeder
- domain.ts
- wt32-eth01_captive-recon/main.cpp
- ui_hold_button.cpp
- Legacy Master USB CLI
- main_native.cpp
- Display DB Fetch State Machine
- Dashboard Package Manifest
- Dashboard — full walkthrough
- read-legacy-slave.ts
- PWM Pin Readout Sketch
- Captive Portal Login Flow
- master.cpp
- Dashboard TypeScript Config
- 5. Checklist
- Pi Water Control API
- Firmware Test Runner
- render_current
- screen_data.cpp
- Slave Sensor Firmware
- ATmega2560 Mega Mini — Development & Reference Documentation
- 4. `methane-calibrate`
- data.ts
- Dashboard CSV Export
- Dashboard History Data
- Host Serial Buffer
- Agent Rules Document
- sim/Arduino.h
- Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)
- DB Contract Envelope
- Dashboard Compare View
- millis
- screen_control.cpp
- Captive Portal Login Script
- Factory Flash Tool
- Sensor Sketch Arduino Shims
- Documentation Index
- flask_cli.py
- Documentation review — all of `docs/`
- Memory Safety Skill
- Display DB Fault Codes
- Database schema catalog
- Dashboard public-deploy security report
- Temperature Probe Sketch
- pi-water-control-system-mirror.md
- data.ts
- Sample Schema
- Push Row Schema
- Memory-safety hardening (`display-v9`, `network-master`)
- DB Contract Host Tests
- Pi Clock Sync Worker
- Memory Safety Patterns
- screen_setup_poll
- DB Pass States
- CSV Download Handler
- Simulated ESP Heap
- ESP32-8048S050C · Board & Archive Guide
- Sensor calibration firmware
- Simulated Serial
- Legacy Master Firmware (`legacy/master/master.cpp`)
- Host ESP Heap Shim
- Display <-> WT32 link: `rusim-line` v1
- log_data_to_csv
- Endpoint details
- WiFi Provisioning Stages
- Sensor calibration firmware
- ui_shell.cpp
- Project Backlog
- WT32-ETH01 — LLM Documentation
- 4. `methane-calibrate`
- screen_home.cpp
- HTTP Client Type
- WiFi Event Type
- screen_home.cpp
- raspi-docs/README.md
- LVGL v8 to v9 migration report
- Chart
- authenticate
- PlatformIO build environments (`platformio.ini`)
- Network master ↔ database communication plan
- ESP32-8048S050 — LLM Generated Summary
- network-master (`firmware/network-master`) — production WT32 firmware
- delay
- index.md
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- Dashboard public-deploy security report
- 3. States
- 3. States
- 9. Network & services
- Stack
- CALIBRATION NOTES
- screen_control_poll
- 12. Copies & git state
- 3.2.1 Redesign plan — `/home`
- compile_commands.json and gen-compile-db.sh
- learned-mistakes.md
- status
- Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)
- Raspberry Pi `water-control-system` mirror
- run_shots
- Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)
- HoldBtnCtx
- sync_pi.sh
- 2. Boot & kiosk session model
- `leaf` terminal reader on the Pi
- 10. ESP-IDF Recommendations
- 6. Common Pin Map Reference
- Pi sync script — `sync_pi.sh`
- Display LVGL 9.3.0 -> 9.6.0 port
- screen_control_poll
- Captive portal login from Linux (bash)
- 6. Master firmware — `legacy/master/master.cpp`
- MosfetChannel
- 1. Hardware & OS
- digitalWrite
- 11. LVGL Recommendations
- 12. Arduino / PlatformIO Notes
- 14. Boot, Flashing, and Debugging
- 16. Performance and Stability Checklist
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants
- 3.2.1 Redesign plan — `/home`
- Route
- 3.6.1.3 Functions
- experimentReadings.test.ts
- Display `IS_DEBUG` flag
- Raspberry Pi `invitrocontroller` — System Documentation
- DbReply

## God Nodes (most connected - your core abstractions)
1. `millis()` - 59 edges
2. `Dashboard()` - 31 edges
3. `delay()` - 30 edges
4. `logf()` - 28 edges
5. `sens_apply_real()` - 22 edges
6. `render()` - 21 edges
7. `dbPassTick()` - 21 edges
8. `ESP32-8048S050 — LLM Generated Summary` - 21 edges
9. `Endpoint details` - 21 edges
10. `compilerOptions` - 20 edges

## Surprising Connections (you probably didn't know these)
- `5.2 Session expiry timer` --references--> `useAuth()`  [INFERRED]
  docs/dashboard.md → dashboard/src/auth.ts
- `2. High — `authenticate()` has no brute-force protection` --references--> `authenticate()`  [INFERRED]
  docs/plans-and-audits/security-report.md → dashboard/src/data.ts
- `5. Low — functional / ops` --references--> `fetchReadings()`  [INFERRED]
  docs/plans-and-audits/security-report.md → dashboard/src/data.ts
- `13.1 Sensors and units` --references--> `getParameterValue()`  [INFERRED]
  docs/dashboard.md → dashboard/src/data.ts
- `5.5 The Monitoring view` --references--> `generateSyntheticLiveReadings()`  [INFERRED]
  docs/dashboard.md → dashboard/src/monitoring.ts

## Import Cycles
- None detected.

## Communities (141 total, 8 thin omitted)

### Community 0 - "Display Fake Data Harness"
Cohesion: 0.05
Nodes (73): 10. WT32 link (`rusim-line` v1, production), 6. Bell / NULL semantics, 2.4 Real vs fake labelling, 1. Summary, 2.1 Re-login stops the running experiment, 2.2 Display reboot reports "stopped", 2.3 Full-screen rebuild on every frame, 2.5 Setpoint and stirrer controls do nothing (+65 more)

### Community 1 - "Display Link Serial Protocol"
Cohesion: 0.06
Nodes (75): 15.1 Render cost, 15.2 Grid, 15.3 Colour and text, 15.4 Refresh and input, 15. Layout, colour and refresh rules, 3. File map, 3.14 Backlight sleep, arg_lookup() (+67 more)

### Community 2 - "Flask Server Imports"
Cohesion: 0.09
Nodes (20): 14. `GET /read_log`, download_info(), export_data(), get_clock_payload(), get_drives(), get_interface_ip(), get_ip(), get_lan_ip() (+12 more)

### Community 3 - "Dashboard Chart UI"
Cohesion: 0.10
Nodes (34): BrandMark(), ChartInteraction, CompareActivePoint, CompareIcon(), CompareSeries, Dashboard(), DashboardView, HistoryIcon() (+26 more)

### Community 4 - "Hardware Reference Docs"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 5 - "Legacy Pi Monitor CLI"
Cohesion: 0.07
Nodes (45): 1. Usage, 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents (+37 more)

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 8 - "network-master/main.cpp"
Cohesion: 0.09
Nodes (32): 5.1 As built, 5. ESP implementation steps, Source references, arg_lookup(), BottleReading, copyFormArg(), dbApplyRunState(), DbMethod (+24 more)

### Community 9 - "Dev Database Seeder"
Cohesion: 0.10
Nodes (26): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+18 more)

### Community 10 - "domain.ts"
Cohesion: 0.09
Nodes (26): dependencies, react@19.1.1, react-dom@19.1.1, App(), AuthState, readStoredSession(), StoredSession, useAuth() (+18 more)

### Community 11 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.14
Nodes (20): 2. What it does, extractWindowLocation(), loop(), onEthEvent(), PageScan, cloudflareChallenge, formAction, hasForm (+12 more)

### Community 12 - "ui_hold_button.cpp"
Cohesion: 0.26
Nodes (7): hold_commit(), hold_tick(), on_delete(), on_pressed(), on_released(), ring_hide(), ring_show()

### Community 13 - "Legacy Master USB CLI"
Cohesion: 0.11
Nodes (25): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_WIDTHS, clock(), configurePort(), createLineGate(), Device (+17 more)

### Community 14 - "main_native.cpp"
Cohesion: 0.17
Nodes (5): DispBufDeleter, lvgl_tick_get(), fmt_age(), screen_info_hide(), set_tile()

### Community 15 - "Display DB Fetch State Machine"
Cohesion: 0.21
Nodes (24): 6.1 Database path (configuration, runs and uploads), 6. Memory and limits, 5. Content audit, 2026-11-18, dbArrayIsEmpty(), dbClassify(), dbFaultName(), DbFetch, DB_FETCH_AUTH (+16 more)

### Community 16 - "Dashboard Package Manifest"
Cohesion: 0.05
Nodes (40): @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13, @types/react-dom@19.1.9, typescript@5.9.2, vite@7.1.7 (+32 more)

### Community 17 - "Dashboard — full walkthrough"
Cohesion: 0.06
Nodes (31): summarizeParameter(), 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 11. The writer, 13.1 Sensors and units, 13.3 Migration timeline (+23 more)

### Community 18 - "read-legacy-slave.ts"
Cohesion: 0.14
Nodes (21): ALIGN, Bottle, configurePort(), createLineGate(), Device, formatCell(), formatInt(), formatNumber() (+13 more)

### Community 19 - "PWM Pin Readout Sketch"
Cohesion: 0.15
Nodes (22): digitalRead(), millis(), Channel, haveReading, haveRise, level, periodUs, pin (+14 more)

### Community 20 - "Captive Portal Login Flow"
Cohesion: 0.16
Nodes (24): 2. The steps, 3. Helper functions and their firmware counterparts, network-master: bounded buffers, 5. Portal login (`doLogin()`), doLogin(), extractWindowLocation(), fetchLoginPage(), fetchPortalUrl() (+16 more)

### Community 21 - "master.cpp"
Cohesion: 0.15
Nodes (19): 4.1 Trip thresholds, 4.2 Asymmetric Fast-Trip / Leaky-Bucket Recovery Debouncing, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing, 8. Slave UART Polling, Validation, and Watchdog (`Serial1`), computeWaterRingAverage(), isValidBottleObject(), loop() (+11 more)

### Community 22 - "Dashboard TypeScript Config"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 23 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 24 - "Pi Water Control API"
Cohesion: 0.09
Nodes (25): Runtime, 4. Backend — `web_control.py`, Background workers, Global state, `polling_worker` (daemon thread), `POST /control` Actions, Raw line buffer, REST routes (+17 more)

### Community 25 - "Firmware Test Runner"
Cohesion: 0.16
Nodes (6): main(), run(), braced(), declarations(), MethaneCalibrationRegression, native_source()

### Community 26 - "render_current"
Cohesion: 0.14
Nodes (28): 1.1 Router (`ui_shell.cpp`, `s_route`), 2.1 Styling, 2.4 Experiment number keeps resetting on `/control`, 5.3 Findings, mk_equipment(), mk_stepper_row(), screen_control_hide(), screen_control_show() (+20 more)

### Community 27 - "screen_data.cpp"
Cohesion: 0.17
Nodes (16): 3.5 Route `/data/X/bottle_Y` — chamber detail, fake_sensor_text(), fake_sensor_value_text(), bottle_refresh(), bottle_state(), chart_refresh(), cluster_refresh(), Page (+8 more)

### Community 28 - "Slave Sensor Firmware"
Cohesion: 0.13
Nodes (24): 4.1 pH, 4.4 Pressure (HX711 / HX710B), 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, 7. Verification (+16 more)

### Community 29 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 30 - "4. `methane-calibrate`"
Cohesion: 0.24
Nodes (8): 4.2 Methane (TGS2611), 6. `ph-calibrate`, Correctness limits, Sampling and output, Wiring, analogRead(), loop(), updateAllMethane()

### Community 31 - "data.ts"
Cohesion: 0.12
Nodes (22): FIRST_INTERNAL_CHAMBER_ID, CONCLUDED_AFTER_MS, fetchDevice(), fetchExperimentReadings(), fetchReadings(), isConcluded(), MAX_EXPERIMENT_ROWS, MAX_READING_ROWS (+14 more)

### Community 32 - "Dashboard CSV Export"
Cohesion: 0.16
Nodes (15): csvDisabledReason(), downloadExperimentCsv(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv() (+7 more)

### Community 33 - "Dashboard History Data"
Cohesion: 0.18
Nodes (18): ChevronIcon(), ExperimentHistoryView(), formatDate(), isTruncated(), ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber() (+10 more)

### Community 34 - "Host Serial Buffer"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 35 - "Agent Rules Document"
Cohesion: 0.13
Nodes (14): Backlog and Continuity, Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General (+6 more)

### Community 36 - "sim/Arduino.h"
Cohesion: 0.15
Nodes (3): What is shared vs stubbed, random(), randomSeed()

### Community 37 - "Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)"
Cohesion: 0.09
Nodes (21): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+13 more)

### Community 38 - "DB Contract Envelope"
Cohesion: 0.21
Nodes (16): civilFromDays(), daysFromCivil(), findField(), finiteValue(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject() (+8 more)

### Community 39 - "Dashboard Compare View"
Cohesion: 0.25
Nodes (15): CompareView(), ExperimentResultsView(), formatValue(), displayChamberNumber(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection (+7 more)

### Community 40 - "millis"
Cohesion: 0.19
Nodes (16): 7.1 `Serial` becomes the link, 7.2 Link plumbing, 7.3 State machine (semantics kept, shape changed), 7.4 Porting pitfalls to check at review, 7. WT32 end (`firmware/network-master/main.cpp`), clearCreds(), dbPassFinish(), link_emit_db() (+8 more)

### Community 41 - "screen_control.cpp"
Cohesion: 0.28
Nodes (10): on_dur(), on_exp(), on_int_test(), on_pump_commit(), on_run_commit(), on_stirrer_commit(), on_temp(), render() (+2 more)

### Community 42 - "Captive Portal Login Script"
Cohesion: 0.22
Nodes (17): attempt_login(), do_curl(), extract_window_location(), form_action(), log(), log_lines(), log_network(), on_interrupt() (+9 more)

### Community 43 - "Factory Flash Tool"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 44 - "Sensor Sketch Arduino Shims"
Cohesion: 0.14
Nodes (4): advance_ms(), delay(), loop(), setup()

### Community 45 - "Documentation Index"
Cohesion: 0.15
Nodes (13): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 3c. Plans and audits, 4. Hardware references, 5. Human documents (+5 more)

### Community 46 - "flask_cli.py"
Cohesion: 0.24
Nodes (5): get_json(), main(), _open(), post_download(), post_json()

### Community 47 - "Documentation review — all of `docs/`"
Cohesion: 0.40
Nodes (4): 2. Checked and left alone, 3. Open items, 4. What a reader should take from this, Documentation review — all of `docs/`

### Community 48 - "Memory Safety Skill"
Cohesion: 0.22
Nodes (7): 1. Files, 2. Run it, 3. How it works, 4. Stable windows and their averages, 5. What the current log shows, 6. Limits, Pressure calibration plot

### Community 49 - "Display DB Fault Codes"
Cohesion: 0.22
Nodes (9): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA (+1 more)

### Community 50 - "Database schema catalog"
Cohesion: 0.10
Nodes (20): Access control, Chamber numbering, Database schema catalog, Development tooling, `device_data`: active application schema, `device_data.device_config`, `device_data.devices`, `device_data.experiments` (+12 more)

### Community 51 - "Dashboard public-deploy security report"
Cohesion: 0.17
Nodes (11): 1. Blocker — legacy `public` schema is public-read (live, confirmed), 2. High — `authenticate()` has no brute-force protection, 3. High — Netlify bundle + env handling, 4. Medium — session hygiene and PII, 5. Low — functional / ops, Already fixed / verified good (keep these), Dashboard public-deploy security report, Findings (ordered by severity) (+3 more)

### Community 52 - "Temperature Probe Sketch"
Cohesion: 0.46
Nodes (7): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup()

### Community 53 - "pi-water-control-system-mirror.md"
Cohesion: 0.40
Nodes (5): 2026-10-20 change, Deploying a change to the kiosk, How it works, Kiosk text scale, Related documents

### Community 54 - "data.ts"
Cohesion: 0.25
Nodes (7): Data: 5 points per sensor, Formulas, How the two simulated points are made, Limits, Methane calibration report generator, Output files, Run

### Community 55 - "Sample Schema"
Cohesion: 0.25
Nodes (8): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC

### Community 56 - "Push Row Schema"
Cohesion: 0.13
Nodes (16): 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, bottleInRange(), purgeOutOfRangeRows(), PushRow, bottleId (+8 more)

### Community 57 - "Memory-safety hardening (`display-v9`, `network-master`)"
Cohesion: 0.25
Nodes (5): display-v9: DB fields from the `DB` line, display-v9: RAII ownership, Memory-safety hardening (`display-v9`, `network-master`), Verification, my_disp_flush()

### Community 58 - "DB Contract Host Tests"
Cohesion: 0.30
Nodes (10): contains(), main(), row(), test_ack_and_errors(), test_array_iteration(), test_bottle_range(), test_envelope(), test_field_parsing() (+2 more)

### Community 59 - "Pi Clock Sync Worker"
Cohesion: 0.20
Nodes (8): `clock_sync_worker` (daemon thread), clock_sync_worker(), _extract_portal_redirect_url(), _fetch_http_date(), _get_default_gateway_ip(), _NoRedirectHandler, _parse_http_date_header(), sync_clock_from_lan_http()

### Community 60 - "Memory Safety Patterns"
Cohesion: 0.25
Nodes (7): 1. Environment, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)

### Community 61 - "screen_setup_poll"
Cohesion: 0.07
Nodes (35): 11. Setup route, 2.2 Navigation, 2.3 Data refresh, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting, 3.15 SoftAP lifecycle (+27 more)

### Community 62 - "DB Pass States"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 64 - "Simulated ESP Heap"
Cohesion: 0.13
Nodes (7): Current state of the code, Experiments and results, How to monitor memory from here, /info Reboot: Heap-Walk Hang on `display-v9`, Next step (open), Symptom, SimESPClass

### Community 65 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 66 - "Sensor calibration firmware"
Cohesion: 0.29
Nodes (7): 1. Where the data comes from today, 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour, 6. Deferred, with the hook each needs, 8. Open risks, Migration plan: ESP pulls telemetry from the legacy Mega slave

### Community 68 - "Legacy Master Firmware (`legacy/master/master.cpp`)"
Cohesion: 0.17
Nodes (17): 10. Known Gaps, 1. Overview, 2. Pin Assignment, 3. Boot Defaults, 5. Automatic Pump Interlocks, 6. Mode and Safety Cutoffs, 7. Unified 3-Channel MOSFET PWM Ramping and `0–255` Debit Control, 9. Raspberry Pi USB Serial Protocol (+9 more)

### Community 70 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.11
Nodes (19): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display, 3.3 Gone from the Modbus design (+11 more)

### Community 71 - "log_data_to_csv"
Cohesion: 0.17
Nodes (13): 7. Data & logging, CSV format (current schema), Existing logs, Older schema (2025-12-12.csv), Storage, USB export, 7. `POST /toggle_logging`, fmt_val() (+5 more)

### Community 72 - "Endpoint details"
Cohesion: 0.12
Nodes (17): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. `GET /files`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip`, 18. `GET /get_timezones` (+9 more)

### Community 73 - "WiFi Provisioning Stages"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 75 - "Sensor calibration firmware"
Cohesion: 0.15
Nodes (15): 1. Common structure, 2. `co2-calibrate`, 3. `pressure-calibrate`, 5. `temp-probe`, 7. Related documents, Correctness limits, Field correction, How a reading is taken (+7 more)

### Community 76 - "ui_shell.cpp"
Cohesion: 0.11
Nodes (27): 4. Screen tree / state machine (as built), Widget lifetime rule, screen_info_refresh(), open_setup(), on_chip_clicked(), on_modal_ack(), on_stepper_pressed(), sev_color() (+19 more)

### Community 78 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 79 - "4. `methane-calibrate`"
Cohesion: 0.23
Nodes (11): 4. `methane-calibrate`, Local validation, Reference-data refit, Sampling, Two calibration layers, Wiring and constants, loop(), readRs() (+3 more)

### Community 80 - "screen_home.cpp"
Cohesion: 0.29
Nodes (10): elapsed_hours(), nodes_online(), on_duration(), on_run_commit(), render_run(), render_status(), screen_home_hide(), screen_home_poll() (+2 more)

### Community 81 - "HTTP Client Type"
Cohesion: 0.33
Nodes (6): DbExperiment, durationH, id, sampleIntervalS, setpoints, dbFindExperiment()

### Community 82 - "WiFi Event Type"
Cohesion: 0.33
Nodes (6): Stage, STAGE_DONE, STAGE_PROMPT_PASS, STAGE_PROMPT_USER, STAGE_RECON, STAGE_WAIT_IP

### Community 84 - "screen_home.cpp"
Cohesion: 0.33
Nodes (5): Configuration, Display V9 PC Simulator (`[env:display-sim]`), Run it, Scope note, Screenshots (headless)

### Community 85 - "raspi-docs/README.md"
Cohesion: 0.15
Nodes (7): Raspberry Pi kiosk UI CSS redesign, 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Three copies exist, What it does, 11. Key paths quick reference, Documentation destination

### Community 86 - "LVGL v8 to v9 migration report"
Cohesion: 0.05
Nodes (28): 12. WT32 login page QR, 13. Light theme and the /setup QR, 14. Debug flag `IS_DEBUG`, 1. Overview, 2. Hardware / platform notes, 5. USB control and diagnostic protocol, 7. Hold-to-confirm implementation, 8. Verify without hardware signals (+20 more)

### Community 87 - "Chart"
Cohesion: 0.16
Nodes (25): BottleChart(), Chart(), chartAreaPath(), chartGeometry(), chartPath(), CompareChart(), createChartPoints(), createTimeLabels() (+17 more)

### Community 88 - "authenticate"
Cohesion: 0.08
Nodes (30): authenticate(), Auth used, Build, DB experiment sync + NTP (change record), Link (`rusim-line` v1), Master (`firmware/network-master/main.cpp`), 1. Current state, 2. The database forbids writes (+22 more)

### Community 89 - "PlatformIO build environments (`platformio.ini`)"
Cohesion: 0.22
Nodes (8): 1. Layout, 2. Shared settings, 3. Environment table, 4. The ESP32-S3 display environments, 5. Commands, 6. History of this configuration, 7. Related documents, PlatformIO build environments (`platformio.ini`)

### Community 90 - "Network master ↔ database communication plan"
Cohesion: 0.12
Nodes (16): 1. Recommendation and evidence, 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions (+8 more)

### Community 91 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.14
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 92 - "network-master (`firmware/network-master`) — production WT32 firmware"
Cohesion: 0.22
Nodes (9): 1. Environment, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential handling, 3. Line protocol, 4. State machine, 7. Expected output, 8. Related documents (+1 more)

### Community 93 - "delay"
Cohesion: 0.38
Nodes (8): delay(), convertCountToPh(), loop(), readAverageCount(), setup(), initializeSensor(), loop(), setup()

### Community 94 - "index.md"
Cohesion: 0.14
Nodes (6): Retired: display-hosted Wi-Fi provisioning, Display <-> WT32 link (`display-wt32-link`) — replaced, 3D Models, PCB, gen-compile-db.sh script, install-leaf.sh script

### Community 95 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.20
Nodes (10): 1. What the program does, 2. Build environment, 3. Pin map, 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing, 6. Responsiveness, 7. Known gaps (+2 more)

### Community 96 - "Dashboard public-deploy security report"
Cohesion: 0.40
Nodes (4): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc

### Community 97 - "3. States"
Cohesion: 0.33
Nodes (6): 3.8.1.1 Removed content, 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative), 3.8.1.3 Clock, 3.8.1.5 Interaction notes, 3.8.1 Redesign plan — top bar, 3.8 Persistent status bar

### Community 98 - "3. States"
Cohesion: 0.10
Nodes (24): 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 3.10 Overheat warning, 41 °C, 3.11 Overheat breaker trip, 45 °C, 3.12 Bell and invalid-field list, 3.13 Phone-associated popup, 3.16 Debug-only UI (+16 more)

### Community 99 - "9. Network & services"
Cohesion: 0.15
Nodes (10): 9. Network & services, Enabled services (selected), Listening ports, Network Architecture (LAN Primary), Security note (see §10), 13. Flask REST API reference — `web_control.py`, Source-level notes, Summary (+2 more)

### Community 100 - "Stack"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 101 - "CALIBRATION NOTES"
Cohesion: 0.20
Nodes (10): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+2 more)

### Community 102 - "screen_control_poll"
Cohesion: 0.40
Nodes (4): 10. Security & caveats, Operational caveats, Recommendations, Security

### Community 103 - "12. Copies & git state"
Cohesion: 0.22
Nodes (9): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, UI, `web_control.py` (+1 more)

### Community 104 - "3.2.1 Redesign plan — `/home`"
Cohesion: 0.50
Nodes (4): 5. Frontend UI — `templates/index.html`, Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer, Tabs, Temporary MOSFET Speed Control (`0–255` Debit Testing)

### Community 105 - "compile_commands.json and gen-compile-db.sh"
Cohesion: 0.29
Nodes (6): 1. What the file is, 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh

### Community 106 - "learned-mistakes.md"
Cohesion: 0.22
Nodes (8): 2026-10-05, 2026-10-06, 2026-10-08, 2026-10-20, 2026-10-24, 2026-10-31, 2026-11-10, 2026-11-17

### Community 107 - "status"
Cohesion: 0.29
Nodes (6): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`), 8. `POST /download/info`, status()

### Community 108 - "Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)"
Cohesion: 0.25
Nodes (7): 1. Purpose, 2. Wire protocol, 3. How it opens the port, 4. REPL behaviour, 5. Usage, 6. Limits, Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)

### Community 109 - "Raspberry Pi `water-control-system` mirror"
Cohesion: 0.29
Nodes (7): Checksums, Layout, Raspberry Pi `water-control-system` mirror, Refreshing the mirror, Related documents, Relationship to the deleted `main_ui.html`, What is not included

### Community 110 - "run_shots"
Cohesion: 0.12
Nodes (28): Decoded backtrace, 3.3.1.1 Removed content, 3.3.1.2 Layout (content coordinates, panel 800x436), 3.3.1.3 Back button, 3.3.1.4 Behaviour kept, 3.3.1.5 Open item, 3.3.1 Redesign plan — `/data` node picker, 3.3 Route `/data` — node picker (+20 more)

### Community 111 - "Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)"
Cohesion: 0.18
Nodes (7): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)

### Community 112 - "HoldBtnCtx"
Cohesion: 0.17
Nodes (11): HoldBtnCtx, btn, completed, halo, label, onCommit, ring, start (+3 more)

### Community 113 - "sync_pi.sh"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 114 - "2. Boot & kiosk session model"
Cohesion: 0.29
Nodes (6): 2. Boot & kiosk session model, Boot sequence, Chromium kiosk, Notes / caveats, openbox autostart (`~/.config/openbox/autostart`), PM2 service (`/etc/systemd/system/pm2-invitro.service`)

### Community 115 - "`leaf` terminal reader on the Pi"
Cohesion: 0.29
Nodes (6): How the cross-compile works, `leaf` terminal reader on the Pi, Rebuilding and removing, Running it, Verification, Why it is not compiled on the Pi

### Community 116 - "10. ESP-IDF Recommendations"
Cohesion: 0.33
Nodes (6): 10. ESP-IDF Recommendations, ESP-IDF components you may use, Important menuconfig areas, Recommended minimum version, Target setup, Typical required features

### Community 117 - "6. Common Pin Map Reference"
Cohesion: 0.33
Nodes (6): 6.1 RGB display control pins, 6.2 RGB data pins, common capacitive variant, 6.3 GT911 capacitive touch pins, 6.4 Other common board pins, 6.5 Known documentation trap, 6. Common Pin Map Reference

### Community 118 - "Pi sync script — `sync_pi.sh`"
Cohesion: 0.20
Nodes (10): Failure behaviour, First run, Limitations, Master firmware flashing, Pi sync script — `sync_pi.sh`, Prerequisites, Rollback, Usage (+2 more)

### Community 119 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 120 - "screen_control_poll"
Cohesion: 0.20
Nodes (10): Display (`firmware/display-v9/`), 3.2.1.5 Polling, 3.6.1.1 Removed content, 3.6.1.2 Layout (content coordinates, panel 800x436), 3.6.1.4 Constants, 3.6.1.5 DB status text (shortened to fit 200 px at font 14), 3.6.1.6 Run button colours, 3.6.1 Redesign plan — `/control` (+2 more)

### Community 121 - "Captive portal login from Linux (bash)"
Cohesion: 0.25
Nodes (7): 1. Usage, 1a. Diagnostic output, 4. Differences from the firmware, 5. Testing, 6. Deployment on the legacy Pi, 7. Related documents, Captive portal login from Linux (bash)

### Community 122 - "6. Master firmware — `legacy/master/master.cpp`"
Cohesion: 0.22
Nodes (9): 6. Master firmware — `legacy/master/master.cpp`, Boot defaults, Commands, Control parameters, Deployment, Libraries, Pins, Stirrer modes (+1 more)

### Community 123 - "MosfetChannel"
Cohesion: 0.25
Nodes (8): MosfetChannel, currentPwm, maxPwm, pin, rampOriginMs, rampOriginPwm, state, targetPwm

### Community 124 - "1. Hardware & OS"
Cohesion: 0.29
Nodes (6): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account

### Community 125 - "digitalWrite"
Cohesion: 0.27
Nodes (11): setup(), digitalWrite(), pinMode(), loop(), rampUp(), setup(), stopMotor(), link_emit_hello() (+3 more)

### Community 126 - "11. LVGL Recommendations"
Cohesion: 0.50
Nodes (4): 11. LVGL Recommendations, Important LVGL settings, LVGL performance tips, Recommended LVGL version

### Community 127 - "12. Arduino / PlatformIO Notes"
Cohesion: 0.50
Nodes (4): 12. Arduino / PlatformIO Notes, Arduino IDE typical board settings, PlatformIO example concepts, Useful Arduino/PlatformIO libraries

### Community 128 - "14. Boot, Flashing, and Debugging"
Cohesion: 0.50
Nodes (4): 14. Boot, Flashing, and Debugging, Boot button, Native USB conflict, UART console

### Community 129 - "16. Performance and Stability Checklist"
Cohesion: 0.50
Nodes (4): 16. Performance and Stability Checklist, Display checklist, LVGL checklist, Touch checklist

### Community 130 - "13. ESPHome Notes"
Cohesion: 0.67
Nodes (3): 13. ESPHome Notes, ESPHome cautions, Relevant ESPHome components

### Community 131 - "15. Filesystem and Asset Storage"
Cohesion: 0.67
Nodes (3): 15. Filesystem and Asset Storage, Asset recommendations, Practical partition advice

### Community 132 - "4. Display Subsystem"
Cohesion: 0.67
Nodes (3): 4. Display Subsystem, Display interface, Typical timing parameters

### Community 133 - "5. Touch Variants"
Cohesion: 0.67
Nodes (3): 5. Touch Variants, Capacitive touch, GT911-class, Resistive touch, XPT2046-class

### Community 134 - "3.2.1 Redesign plan — `/home`"
Cohesion: 0.25
Nodes (8): 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.3 State and data model changes needed, 3.2.1.6 Integration with other screens, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels), 3.2.1.8 Out of scope for this screen (stays on /control), 3.2.1 Redesign plan — `/home`, 3.2 Route `/home`

### Community 135 - "Route"
Cohesion: 0.25
Nodes (8): Route, R_BOTTLE, R_CLUSTER, R_CONTROL, R_HOME, R_INFO, R_PICKER, R_SETUP

### Community 136 - "3.6.1.3 Functions"
Cohesion: 0.38
Nodes (6): 3.2.1.4 Functions to add / change, 3.6.1.3 Functions, bottles_online(), ui_hold_button_set_state(), ui_hold_button_set_symbol(), ui_hold_button_state()

### Community 138 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses

### Community 139 - "Raspberry Pi `invitrocontroller` — System Documentation"
Cohesion: 0.40
Nodes (5): Architecture at a glance, Current runtime state (2026-10-20 re-inspection), Quick facts, Raspberry Pi `invitrocontroller` — System Documentation, Table of contents

### Community 140 - "DbReply"
Cohesion: 0.50
Nodes (4): DbReply, code, len, truncated

## Knowledge Gaps
- **112 isolated node(s):** `RFC-4180`, `P_NONE`, `P_PICKER`, `P_CLUSTER`, `P_BOTTLE` (+107 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 970 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **8 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `Display Link Serial Protocol` to `Display Fake Data Harness`, `network-master/main.cpp`, `wt32-eth01_captive-recon/main.cpp`, `main_native.cpp`, `Display DB Fetch State Machine`, `master.cpp`, `render_current`, `Slave Sensor Firmware`, `sim/Arduino.h`, `millis`, `Display DB Fault Codes`, `Temperature Probe Sketch`, `screen_setup_poll`, `Sensor calibration firmware`, `ui_shell.cpp`, `4. `methane-calibrate``, `screen_home.cpp`, `delay`, `run_shots`, `digitalWrite`?**
  _High betweenness centrality (0.093) - this node is a cross-community bridge._
- **Are the 58 inferred relationships involving `millis()` (e.g. with `loop()` and `readCo2Uart()`) actually correct?**
  _`millis()` has 58 INFERRED edges - model-reasoned connections that need verification._
- **What connects `RFC-4180`, `P_NONE`, `P_PICKER` to the rest of the system?**
  _112 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Display Fake Data Harness` be split into smaller, more focused modules?**
  _Cohesion score 0.05337078651685393 - nodes in this community are weakly interconnected._
- **Why does `Factory flashing tool (`scripts-hardware/flash-factory.ts`)` connect `Factory Flash Tool` to `Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)`?**
  _High betweenness centrality (0.048) - this node is a cross-community bridge._
- **Are the 29 inferred relationships involving `delay()` (e.g. with `loop()` and `setup()`) actually correct?**
  _`delay()` has 29 INFERRED edges - model-reasoned connections that need verification._
- **Should `Display Link Serial Protocol` be split into smaller, more focused modules?**
  _Cohesion score 0.05553923009109609 - nodes in this community are weakly interconnected._