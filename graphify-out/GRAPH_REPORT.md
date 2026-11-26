# Graph Report - rusim-monorepo  (2026-11-26)

## Corpus Check
- 219 files · ~304,955 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 25 file(s) not represented in the graph (top: (none) 8, .woff2 4, .lock 3)

## Summary
- 2967 nodes · 5356 edges · 181 communities (161 shown, 20 thin omitted)
- Extraction: 84% EXTRACTED · 16% INFERRED · 0% AMBIGUOUS · INFERRED: 852 edges (avg confidence: 0.9)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `368e4e6d`
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
- logf
- db-check.ts
- lib.rs
- wt32-eth01_captive-recon/main.cpp
- sens_report_round
- dev-db.ts
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
- serial_worker
- Firmware Test Runner
- render_current
- screen_data.cpp
- Migration plan: ESP pulls telemetry from the legacy Mega slave
- ATmega2560 Mega Mini — Development & Reference Documentation
- capture/capture.ts
- data.ts
- Dashboard CSV Export
- Dashboard History Data
- Host Serial Buffer
- Agent Rules Document
- sim/Arduino.h
- Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)
- DB Contract Envelope
- Dashboard Compare View
- 3. States
- screen_control.cpp
- Captive Portal Login Script
- Factory Flash Tool
- Documentation Index
- flask_cli.py
- ui_shell.cpp
- hyperframes-mobile/capture/capture.ts
- Display DB Fault Codes
- Database schema catalog
- analyze.ts
- Temperature Probe Sketch
- boot-loop.ts
- data.ts
- Sample Schema
- Push Row Schema
- 3. Root Causes
- DB Contract Host Tests
- viaDrawer
- polling_worker
- screen_setup_poll
- DB Pass States
- CSV Download Handler
- Simulated ESP Heap
- ESP32-8048S050C · Board & Archive Guide
- capture.ts
- Verification toolkit (report data collection)
- Legacy Master Firmware (`legacy/master/master.cpp`)
- Host ESP Heap Shim
- Display <-> WT32 link: `rusim-line` v1
- metrics.cpp
- Endpoint details
- WiFi Provisioning Stages
- delay
- slave.cpp
- VideoDemoVideo
- WT32-ETH01 — LLM Documentation
- Sensor calibration firmware
- screen_home.cpp
- db-replay.ts
- LVGL v8 to v9 migration report
- analogRead
- pi-water-control-system-mirror.md
- LVGL v8 to v9 migration report
- Chart
- authenticate
- network-master/main.cpp
- Network master ↔ database communication plan
- ESP32-8048S050 — LLM Generated Summary
- doLogin
- main.rs
- network-master-database-plan.md
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- hyperframes-mobile/hyperframes.json
- fake-slave.ts
- hyperframes/capture/package.json
- 9. Network & services
- Stack
- CALIBRATION NOTES
- hyperframes-mobile/capture/package.json
- MosfetChannel
- 3.2.1 Redesign plan — `/home`
- dashboard-load.ts
- fidelity.ts
- status
- Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)
- Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)
- 1. Hardware & OS
- Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)
- HoldBtnCtx
- sync_pi.sh
- 2. Boot & kiosk session model
- `leaf` terminal reader on the Pi
- 10. ESP-IDF Recommendations
- 6. Common Pin Map Reference
- co2-calibrate/main.cpp
- Display LVGL 9.3.0 -> 9.6.0 port
- BeatScene
- hyperframes.json
- 12. Findings
- compilerOptions
- 8. Auxiliary tools & scripts
- scripts
- 11. LVGL Recommendations
- 12. Arduino / PlatformIO Notes
- 14. Boot, Flashing, and Debugging
- 16. Performance and Stability Checklist
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants
- scripts
- Route
- log_data_to_csv
- Legacy Master Firmware (`legacy/master/master.cpp`)
- Display `IS_DEBUG` flag
- .new
- DbReply
- Review: RuSim dashboard demo, mobile UI
- MetBoot
- temp-probe/main.cpp
- Dashboard public-deploy security report
- TouchPending
- Display Demo Video (`video-demo/`)
- pinMode
- TouchResult
- 2. Figures
- metrics.ts
- Kiosk text scale
- Raspberry Pi `water-control-system` mirror
- Review: RuSim dashboard demo
- SensPending
- 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing
- SensResult
- Pi sync script — `sync_pi.sh`
- 6. Master firmware — `legacy/master/master.cpp`
- Documentation review — all of `docs/`
- 5. Frontend UI — `templates/index.html`
- met_tick
- Easing
- Storylines: RuSim dashboard demo
- delay
- forceAllOff
- motor-driver/main.cpp
- indev_timer_wrapper
- video-demo
- BootMark
- build-sfx.sh
- SPEC.md
- SystemMode
- 10. Security & caveats
- hyperframes-mobile/capture/build-sfx.sh
- 11. Key paths quick reference
- gen-compile-db.sh
- hyperframes-mobile/SPEC.md

## God Nodes (most connected - your core abstractions)
1. `millis()` - 73 edges
2. `Dashboard()` - 34 edges
3. `delay()` - 32 edges
4. `logf()` - 31 edges
5. `loop()` - 25 edges
6. `2. Figures` - 25 edges
7. `sens_apply_real()` - 24 edges
8. `dbPassTick()` - 24 edges
9. `render()` - 21 edges
10. `ESP32-8048S050 — LLM Generated Summary` - 21 edges

## Surprising Connections (you probably didn't know these)
- `2. High — `authenticate()` has no brute-force protection` --references--> `authenticate()`  [INFERRED]
  docs/plans-and-audits/security-report.md → dashboard/src/data.ts
- `5. Low — functional / ops` --references--> `fetchReadings()`  [INFERRED]
  docs/plans-and-audits/security-report.md → dashboard/src/data.ts
- `13.1 Sensors and units` --references--> `getParameterValue()`  [INFERRED]
  docs/dashboard.md → dashboard/src/data.ts
- `5.5 The Monitoring view` --references--> `generateSyntheticLiveReadings()`  [INFERRED]
  docs/dashboard.md → dashboard/src/monitoring.ts
- `5.4 Polling` --references--> `isMonitoringPollingEligible()`  [INFERRED]
  docs/dashboard.md → dashboard/src/monitoring.ts

## Import Cycles
- None detected.

## Communities (181 total, 20 thin omitted)

### Community 0 - "Display Fake Data Harness"
Cohesion: 0.05
Nodes (73): 10. WT32 link (`rusim-line` v1, production), 2.4 Real vs fake labelling, 1. Summary, 2.1 Re-login stops the running experiment, 2.2 Display reboot reports "stopped", 2.3 Full-screen rebuild on every frame, 2.5 Setpoint and stirrer controls do nothing, 2.6 Wake tap is delivered as a press (+65 more)

### Community 1 - "Display Link Serial Protocol"
Cohesion: 0.08
Nodes (49): arg_lookup(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line(), handle_log(), handle_login_req() (+41 more)

### Community 2 - "Flask Server Imports"
Cohesion: 0.07
Nodes (26): `clock_sync_worker` (daemon thread), clock_sync_worker(), download_info(), export_data(), _extract_portal_redirect_url(), _fetch_http_date(), get_clock_payload(), _get_default_gateway_ip() (+18 more)

### Community 3 - "Dashboard Chart UI"
Cohesion: 0.09
Nodes (36): App(), ArrowIcon(), BrandMark(), ChartInteraction, CompareActivePoint, CompareIcon(), CompareSeries, Dashboard() (+28 more)

### Community 4 - "Hardware Reference Docs"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 5 - "Legacy Pi Monitor CLI"
Cohesion: 0.07
Nodes (45): 1. Usage, 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents (+37 more)

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 8 - "logf"
Cohesion: 0.17
Nodes (18): The Backlog, link_emit_net(), link_log_dump(), log_eth_event(), log_uplink_change(), log_wifi_event(), logf(), loop() (+10 more)

### Community 9 - "db-check.ts"
Cohesion: 0.15
Nodes (7): Block, { flags }, outDir, REPO, sql, stamp, vars

### Community 10 - "lib.rs"
Cohesion: 0.04
Nodes (50): ACCENT_SOFT, BASE_SCALE, BEZEL, BOTTLE, CLOSE, CONTROL, CROSSFADE, CROSSFADE_RUNTIME (+42 more)

### Community 11 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.08
Nodes (33): 1. Environment, 2. What it does, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`) (+25 more)

### Community 12 - "sens_report_round"
Cohesion: 0.09
Nodes (28): display-v9: DB fields from the `DB` line, Memory-safety hardening (`display-v9`, `network-master`), Verification, 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, 5.1 As built (+20 more)

### Community 13 - "dev-db.ts"
Cohesion: 0.05
Nodes (51): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+43 more)

### Community 14 - "main_native.cpp"
Cohesion: 0.17
Nodes (5): DispBufDeleter, lvgl_tick_get(), screen_info_hide(), settle(), shot()

### Community 15 - "Display DB Fetch State Machine"
Cohesion: 0.10
Nodes (45): Auth used, Build, DB experiment sync + NTP (change record), Master (`firmware/network-master/main.cpp`), 6.1 Database path (configuration, runs and uploads), 6.2 Metrics build (`network-master-metrics`), 6. Memory and limits, 5. Content audit, 2026-11-18 (+37 more)

### Community 16 - "Dashboard Package Manifest"
Cohesion: 0.05
Nodes (44): dependencies, react@19.1.1, react-dom@19.1.1, @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13 (+36 more)

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
Cohesion: 0.10
Nodes (31): 1. Usage, 1a. Diagnostic output, 2. The steps, 3. Helper functions and their firmware counterparts, 4. Differences from the firmware, 5. Testing, 6. Deployment on the legacy Pi, 7. Related documents (+23 more)

### Community 21 - "master.cpp"
Cohesion: 0.24
Nodes (12): 4.2 Asymmetric Fast-Trip / Leaky-Bucket Recovery Debouncing, 7. Unified 3-Channel MOSFET PWM Ramping and `0–255` Debit Control, driveMosfetPwm(), loop(), readAdcSample(), resetMosfetOff(), sendSlavePing(), setup() (+4 more)

### Community 22 - "Dashboard TypeScript Config"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 23 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 24 - "serial_worker"
Cohesion: 0.10
Nodes (24): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Runtime, Three copies exist, What it does, 4. Backend — `web_control.py`, Background workers, `POST /control` Actions (+16 more)

### Community 25 - "Firmware Test Runner"
Cohesion: 0.23
Nodes (4): braced(), declarations(), MethaneCalibrationRegression, native_source()

### Community 26 - "render_current"
Cohesion: 0.10
Nodes (37): Decoded backtrace, 1.1 Router (`ui_shell.cpp`, `s_route`), 2.1 Styling, 3.4.1.1 Removed content, 3.4.1.2 Layout (content coordinates, panel 800x436), 3.4.1.3 Tile contents, 3.4.1.4 Behaviour kept, 3.4.1 Redesign plan — `/data/X` cluster (+29 more)

### Community 27 - "screen_data.cpp"
Cohesion: 0.15
Nodes (19): 3.4 Back button on a node page goes to Home, bottle_refresh(), bottle_state(), chart_refresh(), cluster_refresh(), on_back_cluster(), on_back_picker(), on_pick_bottle() (+11 more)

### Community 28 - "Migration plan: ESP pulls telemetry from the legacy Mega slave"
Cohesion: 0.25
Nodes (8): 1. Where the data comes from today, 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour, 6. Deferred, with the hook each needs, 7. Verification, 8. Open risks, Migration plan: ESP pulls telemetry from the legacy Mega slave

### Community 29 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 30 - "capture/capture.ts"
Cohesion: 0.10
Nodes (23): addSeries(), begin(), ch4Points, chartHeads, CLIPS, csvLines, csvName, DATA (+15 more)

### Community 31 - "data.ts"
Cohesion: 0.09
Nodes (32): AuthState, readStoredSession(), StoredSession, useAuth(), writeStoredSession(), CONCLUDED_AFTER_MS, fetchDevice(), fetchExperimentReadings() (+24 more)

### Community 32 - "Dashboard CSV Export"
Cohesion: 0.13
Nodes (15): csvDisabledReason(), downloadExperimentCsv(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv() (+7 more)

### Community 33 - "Dashboard History Data"
Cohesion: 0.18
Nodes (18): ChevronIcon(), ExperimentHistoryView(), formatDate(), isTruncated(), ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber() (+10 more)

### Community 34 - "Host Serial Buffer"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 35 - "Agent Rules Document"
Cohesion: 0.14
Nodes (13): Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General, General Requirements (+5 more)

### Community 36 - "sim/Arduino.h"
Cohesion: 0.10
Nodes (4): What is shared vs stubbed, random(), randomSeed(), SimSerial

### Community 37 - "Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)"
Cohesion: 0.09
Nodes (21): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+13 more)

### Community 38 - "DB Contract Envelope"
Cohesion: 0.21
Nodes (16): bottleInRange(), civilFromDays(), daysFromCivil(), findField(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject() (+8 more)

### Community 39 - "Dashboard Compare View"
Cohesion: 0.16
Nodes (15): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), ExperimentSummary, DashboardView, DEV_FORCE_LIVE_EXPERIMENT, generateSyntheticLiveReadings(), isMonitoringPollingEligible() (+7 more)

### Community 40 - "3. States"
Cohesion: 0.09
Nodes (22): 3.10 Overheat warning, 41 °C, 3.12 Bell and invalid-field list, 3.13 Phone-associated popup, 3.16 Debug-only UI, 3.3.1.1 Removed content, 3.3.1.2 Layout (content coordinates, panel 800x436), 3.3.1.4 Behaviour kept, 3.3.1.5 Open item (+14 more)

### Community 41 - "screen_control.cpp"
Cohesion: 0.36
Nodes (9): on_dur(), on_exp(), on_int_test(), on_pump_commit(), on_run_commit(), on_stirrer_commit(), on_temp(), render() (+1 more)

### Community 42 - "Captive Portal Login Script"
Cohesion: 0.22
Nodes (17): attempt_login(), do_curl(), extract_window_location(), form_action(), log(), log_lines(), log_network(), on_interrupt() (+9 more)

### Community 43 - "Factory Flash Tool"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 45 - "Documentation Index"
Cohesion: 0.17
Nodes (12): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 3c. Plans and audits, 4. Hardware references, 5. Human documents (+4 more)

### Community 46 - "flask_cli.py"
Cohesion: 0.13
Nodes (7): main(), run(), get_json(), main(), _open(), post_download(), post_json()

### Community 47 - "ui_shell.cpp"
Cohesion: 0.09
Nodes (45): 11. Setup route, 4. Screen tree / state machine (as built), 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 3.11 Overheat breaker trip, 45 °C, 3.1 Route `/setup`, 3.3.1.3 Back button (+37 more)

### Community 48 - "hyperframes-mobile/capture/capture.ts"
Cohesion: 0.10
Nodes (18): ch4, ch4Points, chartHeads, CLIPS, csvLines, csvName, DATA, FRAMES (+10 more)

### Community 49 - "Display DB Fault Codes"
Cohesion: 0.25
Nodes (8): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA

### Community 50 - "Database schema catalog"
Cohesion: 0.10
Nodes (20): Access control, Chamber numbering, Database schema catalog, Development tooling, `device_data`: active application schema, `device_data.device_config`, `device_data.devices`, `device_data.experiments` (+12 more)

### Community 51 - "analyze.ts"
Cohesion: 0.11
Nodes (19): csv(), { flags, positional }, json, LIMIT, linkIn, md, met, metas (+11 more)

### Community 52 - "Temperature Probe Sketch"
Cohesion: 0.27
Nodes (14): CompareView(), ExperimentResultsView(), displayChamberNumber(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor() (+6 more)

### Community 53 - "boot-loop.ts"
Cohesion: 0.08
Nodes (26): board, { flags }, header, md, method, onLine(), open(), outDir (+18 more)

### Community 54 - "data.ts"
Cohesion: 0.25
Nodes (7): Data: 5 points per sensor, Formulas, How the two simulated points are made, Limits, Methane calibration report generator, Output files, Run

### Community 55 - "Sample Schema"
Cohesion: 0.20
Nodes (10): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC (+2 more)

### Community 56 - "Push Row Schema"
Cohesion: 0.20
Nodes (10): PushRow, bottleId, ch4, co2, experimentId, nullMask, ph, pressure (+2 more)

### Community 57 - "3. Root Causes"
Cohesion: 0.29
Nodes (7): 3.1 30-second cooldown on every startup failure, 3.3 Five all-or-nothing cold-TLS HTTPS calls before `ready=1`, 3.4 Pre-pass delays and redundant portal probes, 3. Root Causes, 4. Verification on Hardware, 5. Recommended Fixes, WT32 Database Authentication & Startup Latency Audit

### Community 58 - "DB Contract Host Tests"
Cohesion: 0.30
Nodes (10): contains(), main(), row(), test_ack_and_errors(), test_array_iteration(), test_bottle_range(), test_envelope(), test_field_parsing() (+2 more)

### Community 59 - "viaDrawer"
Cohesion: 0.20
Nodes (15): addSeries(), begin(), freeze(), log(), navItem(), pick(), rectOf(), rects() (+7 more)

### Community 60 - "polling_worker"
Cohesion: 0.17
Nodes (10): Global state, `polling_worker` (daemon thread), control_api(), desired_state_commands(), polling_worker(), state_mismatch_commands(), stir_command(), threshold_command() (+2 more)

### Community 61 - "screen_setup_poll"
Cohesion: 0.08
Nodes (30): 2.2 Navigation, 2.3 Data refresh, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting, 3.15 SoftAP lifecycle, 3.7.1.1 Removed content (+22 more)

### Community 62 - "DB Pass States"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 64 - "Simulated ESP Heap"
Cohesion: 0.13
Nodes (7): Current state of the code, Experiments and results, How to monitor memory from here, /info Reboot: Heap-Walk Hang on `display-v9`, Next step (open), Symptom, SimESPClass

### Community 65 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 66 - "capture.ts"
Cohesion: 0.13
Nodes (17): display-v9: RAII ownership, baud, { flags }, label, meta, onLine(), open(), outDir (+9 more)

### Community 67 - "Verification toolkit (report data collection)"
Cohesion: 0.10
Nodes (20): 1. Overview, 2. Metrics builds, 3.1 Display, 3.2 WT32, 3. `MET` line reference, 4. Commands, 5. Host tools, 6.1 Display (checklist §3) (+12 more)

### Community 68 - "Legacy Master Firmware (`legacy/master/master.cpp`)"
Cohesion: 0.10
Nodes (32): Display (`firmware/display-v9/`), 15.4 Refresh and input, 3. File map, Widget lifetime rule, 3.14 Backlight sleep, 3.2.1.5 Polling, link_serial_passthrough_active(), link_serial_passthrough_set() (+24 more)

### Community 70 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.11
Nodes (19): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display, 3.3 Gone from the Modbus design (+11 more)

### Community 71 - "metrics.cpp"
Cohesion: 0.18
Nodes (17): 16. Verification metrics build (`display-v9-metrics`), handle_sens(), my_disp_flush(), gap_track(), met_after_timer_handler(), met_boot(), met_console(), met_flush() (+9 more)

### Community 72 - "Endpoint details"
Cohesion: 0.11
Nodes (19): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. `GET /files`, 14. `GET /read_log`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip` (+11 more)

### Community 73 - "WiFi Provisioning Stages"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 75 - "delay"
Cohesion: 0.28
Nodes (10): Sampling, delay(), loop(), readRs(), rsToPpm(), rsToPpmRaw(), setup(), initializeSensor() (+2 more)

### Community 76 - "slave.cpp"
Cohesion: 0.27
Nodes (15): 4.1 pH, 4.4 Pressure (HX711 / HX710B), convertPhCount(), fitExponent(), initPressureSensor(), loop(), readCo2Ppm(), readPhRaw() (+7 more)

### Community 77 - "VideoDemoVideo"
Cohesion: 0.12
Nodes (6): Touch, VideoDemoVideo, .BACKGROUND_COLOR, .FPS, .HEIGHT, .WIDTH

### Community 78 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 79 - "Sensor calibration firmware"
Cohesion: 0.11
Nodes (17): 1. Files, 2. Run it, 3. How it works, 4. Stable windows and their averages, 5. What the current log shows, 6. Limits, Pressure calibration plot, 1. Common structure (+9 more)

### Community 80 - "screen_home.cpp"
Cohesion: 0.22
Nodes (13): link_serial_set_exp_state(), bottles_online(), elapsed_hours(), nodes_online(), on_duration(), on_run_commit(), render_run(), render_status() (+5 more)

### Community 81 - "db-replay.ts"
Cohesion: 0.12
Nodes (16): bottles, call(), exp, { flags }, iso(), lat, latencyN, md (+8 more)

### Community 82 - "LVGL v8 to v9 migration report"
Cohesion: 0.15
Nodes (12): 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, File-by-file summary, LVGL v8 to v9 migration report, Memory and runtime considerations (+4 more)

### Community 84 - "analogRead"
Cohesion: 0.25
Nodes (10): 6. `ph-calibrate`, Correctness limits, Linear conversion (temporary), Sampling and output, Wiring, analogRead(), convertCountToPh(), loop() (+2 more)

### Community 86 - "LVGL v8 to v9 migration report"
Cohesion: 0.14
Nodes (6): 2. Hardware / platform notes, my_touchpad_read(), touch(), touch_has_signal(), touch_released(), touch_touched()

### Community 87 - "Chart"
Cohesion: 0.17
Nodes (24): BottleChart(), Chart(), chartAreaPath(), chartGeometry(), chartPath(), CompareChart(), createChartPoints(), createTimeLabels() (+16 more)

### Community 88 - "authenticate"
Cohesion: 0.11
Nodes (23): authenticate(), Link (`rusim-line` v1), 1. Current state, 2. The database forbids writes, 3. Nothing reads the sensors, 4. Why the device must not send `token_hash`, 5. Credential exposure on the WT32, 6.1 Write grant: `SECURITY DEFINER` RPC (+15 more)

### Community 89 - "network-master/main.cpp"
Cohesion: 0.09
Nodes (25): arg_lookup(), copyFormArg(), DbMethod, DB_GET, DB_POST, handle_experiment(), handle_line(), handleLogin() (+17 more)

### Community 90 - "Network master ↔ database communication plan"
Cohesion: 0.12
Nodes (16): 1. Recommendation and evidence, 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions (+8 more)

### Community 91 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.14
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 92 - "doLogin"
Cohesion: 0.14
Nodes (19): 1. Environment, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential handling, 3. Line protocol, 4.1 Wi-Fi fallback, 4. State machine, 5. Portal login (`doLogin()`) (+11 more)

### Community 95 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.18
Nodes (11): 1. What the program does, 2. Build environment, 3. Pin map, 4.2 Methane (TGS2611), 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing, 6. Responsiveness (+3 more)

### Community 96 - "hyperframes-mobile/hyperframes.json"
Cohesion: 0.20
Nodes (9): authoringSkill, media, autoProxy, paths, assets, blocks, components, registry (+1 more)

### Community 97 - "fake-slave.ts"
Cohesion: 0.12
Nodes (16): Bottle, build(), expectedPath, { flags }, HEADER, MIXED, outDir, port (+8 more)

### Community 98 - "hyperframes/capture/package.json"
Cohesion: 0.25
Nodes (8): dependencies, playwright-core@1.65.1, playwright-core, name, private, scripts, capture, type

### Community 99 - "9. Network & services"
Cohesion: 0.18
Nodes (10): 9. Network & services, Enabled services (selected), Listening ports, Network Architecture (LAN Primary), Security note (see §10), 13. Flask REST API reference — `web_control.py`, Source-level notes, Summary (+2 more)

### Community 100 - "Stack"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 101 - "CALIBRATION NOTES"
Cohesion: 0.14
Nodes (12): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+4 more)

### Community 102 - "hyperframes-mobile/capture/package.json"
Cohesion: 0.25
Nodes (8): dependencies, playwright-core@1.65.1, playwright-core, name, private, scripts, capture, type

### Community 103 - "MosfetChannel"
Cohesion: 0.25
Nodes (8): MosfetChannel, currentPwm, maxPwm, pin, rampOriginMs, rampOriginPwm, state, targetPwm

### Community 104 - "3.2.1 Redesign plan — `/home`"
Cohesion: 0.22
Nodes (9): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, UI, `web_control.py` (+1 more)

### Community 105 - "dashboard-load.ts"
Cohesion: 0.09
Nodes (24): table(), durationMs, exp, { flags }, get(), kinds, md, outDir (+16 more)

### Community 106 - "fidelity.ts"
Cohesion: 0.12
Nodes (10): Cmp, detail, Exp, expected, FIELDS, { flags }, md, outDir (+2 more)

### Community 107 - "status"
Cohesion: 0.29
Nodes (6): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`), 8. `POST /download/info`, status()

### Community 108 - "Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)"
Cohesion: 0.29
Nodes (7): 1. Purpose, 2. Wire protocol, 3. How it opens the port, 4. REPL behaviour, 5. Usage, 6. Limits, Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)

### Community 109 - "Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)"
Cohesion: 0.14
Nodes (14): 12. WT32 login page QR, 13. Light theme and the /setup QR, 14. Debug flag `IS_DEBUG`, 15.1 Render cost, 15.2 Grid, 15.3 Colour and text, 15. Layout, colour and refresh rules, 1. Overview (+6 more)

### Community 110 - "1. Hardware & OS"
Cohesion: 0.33
Nodes (6): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account

### Community 111 - "Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)"
Cohesion: 0.12
Nodes (16): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Keep-alive, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`) (+8 more)

### Community 112 - "HoldBtnCtx"
Cohesion: 0.06
Nodes (39): 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.3 State and data model changes needed, 3.2.1.4 Functions to add / change, 3.2.1.6 Integration with other screens, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels), 3.2.1.8 Out of scope for this screen (stays on /control), 3.2.1 Redesign plan — `/home` (+31 more)

### Community 113 - "sync_pi.sh"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 114 - "2. Boot & kiosk session model"
Cohesion: 0.33
Nodes (6): 2. Boot & kiosk session model, Boot sequence, Chromium kiosk, Notes / caveats, openbox autostart (`~/.config/openbox/autostart`), PM2 service (`/etc/systemd/system/pm2-invitro.service`)

### Community 115 - "`leaf` terminal reader on the Pi"
Cohesion: 0.33
Nodes (6): How the cross-compile works, `leaf` terminal reader on the Pi, Rebuilding and removing, Running it, Verification, Why it is not compiled on the Pi

### Community 116 - "10. ESP-IDF Recommendations"
Cohesion: 0.33
Nodes (6): 10. ESP-IDF Recommendations, ESP-IDF components you may use, Important menuconfig areas, Recommended minimum version, Target setup, Typical required features

### Community 117 - "6. Common Pin Map Reference"
Cohesion: 0.33
Nodes (6): 6.1 RGB display control pins, 6.2 RGB data pins, common capacitive variant, 6.3 GT911 capacitive touch pins, 6.4 Other common board pins, 6.5 Known documentation trap, 6. Common Pin Map Reference

### Community 118 - "co2-calibrate/main.cpp"
Cohesion: 0.24
Nodes (10): 2. `co2-calibrate`, Correctness limits, Field correction, How a reading is taken, Plotting a serial capture, loop(), readCo2Ppm(), readCo2Uart() (+2 more)

### Community 119 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 120 - "BeatScene"
Cohesion: 0.13
Nodes (12): Beat, BEATS, BeatScene, Gesture, Hold, None, Tap, Kind (+4 more)

### Community 121 - "hyperframes.json"
Cohesion: 0.20
Nodes (9): authoringSkill, media, autoProxy, paths, assets, blocks, components, registry (+1 more)

### Community 122 - "12. Findings"
Cohesion: 0.25
Nodes (8): ReadingWithStatus, Metadata, 12.1 The experiment list is bounded by the monitoring fetch's row cap, 12.2 The chart heading says "Across experiments" (fixed), 12.3 Dead code, 12.5 Security posture, 12.6 Functional gaps, 12. Findings

### Community 123 - "compilerOptions"
Cohesion: 0.13
Nodes (14): compilerOptions, allowImportingTsExtensions, lib, module, moduleDetection, moduleResolution, noEmit, skipLibCheck (+6 more)

### Community 124 - "8. Auxiliary tools & scripts"
Cohesion: 0.50
Nodes (4): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc

### Community 125 - "scripts"
Cohesion: 0.22
Nodes (8): name, private, scripts, check, dev, publish, render, type

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

### Community 134 - "scripts"
Cohesion: 0.22
Nodes (8): name, private, scripts, check, dev, publish, render, type

### Community 135 - "Route"
Cohesion: 0.25
Nodes (8): Route, R_BOTTLE, R_CLUSTER, R_CONTROL, R_HOME, R_INFO, R_PICKER, R_SETUP

### Community 136 - "log_data_to_csv"
Cohesion: 0.17
Nodes (13): 7. Data & logging, CSV format (current schema), Existing logs, Older schema (2025-12-12.csv), Storage, USB export, 7. `POST /toggle_logging`, fmt_val() (+5 more)

### Community 137 - "Legacy Master Firmware (`legacy/master/master.cpp`)"
Cohesion: 0.29
Nodes (7): 10. Known Gaps, 2. Pin Assignment, 3. Boot Defaults, 5. Automatic Pump Interlocks, 6. Mode and Safety Cutoffs, 9. Raspberry Pi USB Serial Protocol, Legacy Master Firmware (`legacy/master/master.cpp`)

### Community 138 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses

### Community 139 - ".new"
Cohesion: 0.21
Nodes (8): camera_axis(), camera_for(), Cue, cues(), kf(), pan_for(), spot_for(), touch_times()

### Community 140 - "DbReply"
Cohesion: 0.29
Nodes (6): 1. What the file is, 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh

### Community 141 - "Review: RuSim dashboard demo, mobile UI"
Cohesion: 0.25
Nodes (7): Creative calls, Facts to keep in mind, Measured, and only looked at, Review: RuSim dashboard demo, mobile UI, Warnings kept on purpose, Weakest shots, and what I would change, What the person asked for (2026-11-26)

### Community 142 - "MetBoot"
Cohesion: 0.15
Nodes (13): MetBoot, MB_ACK_FIRST, MB_AUTH, MB_COUNT, MB_ENQ_FIRST, MB_ETH_LINK, MB_ETH_START, MB_GOT_IP (+5 more)

### Community 143 - "temp-probe/main.cpp"
Cohesion: 0.22
Nodes (9): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup(), printAddress() (+1 more)

### Community 144 - "Dashboard public-deploy security report"
Cohesion: 0.17
Nodes (11): 1. Blocker — legacy `public` schema is public-read (live, confirmed), 2. High — `authenticate()` has no brute-force protection, 3. High — Netlify bundle + env handling, 4. Medium — session hygiene and PII, 5. Low — functional / ops, Already fixed / verified good (keep these), Dashboard public-deploy security report, Findings (ordered by severity) (+3 more)

### Community 145 - "TouchPending"
Cohesion: 0.17
Nodes (12): TouchPending, armed, down, from, inv, invUs, renderSeen, renderStartUs (+4 more)

### Community 146 - "Display Demo Video (`video-demo/`)"
Cohesion: 0.07
Nodes (23): Dashboard Demo Videos (`video-demo/hyperframes/`, `video-demo/hyperframes-mobile/`), Files and Git, How the film is built, How the screens are recorded, Layout, Mobile version (`video-demo/hyperframes-mobile/`), Running it, What is on screen (+15 more)

### Community 148 - "pinMode"
Cohesion: 0.40
Nodes (4): setup(), met_attach(), pinMode(), touch_init()

### Community 149 - "TouchResult"
Cohesion: 0.15
Nodes (13): push_result(), TouchResult, down, from, invUs, latUs, nf, renderUs (+5 more)

### Community 150 - "2. Figures"
Cohesion: 0.09
Nodes (15): 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, 1. How to read the diagrams, 2. Figures, 3. Regenerating the images (+7 more)

### Community 151 - "metrics.ts"
Cohesion: 0.29
Nodes (9): heapBytes(), MetricRecord, metricsEnabled, newestAgeSeconds(), recordMetric(), sendToSink(), sinkUrl(), startFetchTimer() (+1 more)

### Community 152 - "Kiosk text scale"
Cohesion: 0.40
Nodes (5): 2026-10-20 change, Deploying a change to the kiosk, How it works, Kiosk text scale, Related documents

### Community 153 - "Raspberry Pi `water-control-system` mirror"
Cohesion: 0.29
Nodes (7): Checksums, Layout, Raspberry Pi `water-control-system` mirror, Refreshing the mirror, Related documents, Relationship to the deleted `main_ui.html`, What is not included

### Community 154 - "Review: RuSim dashboard demo"
Cohesion: 0.25
Nodes (7): Creative calls, Facts to keep in mind, Measured, and only looked at, Review: RuSim dashboard demo, Second cut (2026-11-26), Warnings kept on purpose, Weakest shot, and what I would change

### Community 155 - "SensPending"
Cohesion: 0.29
Nodes (7): SensPending, applyArmed, applyUs, localSeq, rxArmed, rxUs, wseq

### Community 156 - "4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing"
Cohesion: 0.50
Nodes (5): 4.1 Trip thresholds, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing, computeWaterRingAverage(), sampleWaterSensor()

### Community 157 - "SensResult"
Cohesion: 0.33
Nodes (6): SensResult, applyUs, localSeq, noframe, renderUs, wseq

### Community 158 - "Pi sync script — `sync_pi.sh`"
Cohesion: 0.20
Nodes (10): Failure behaviour, First run, Limitations, Master firmware flashing, Pi sync script — `sync_pi.sh`, Prerequisites, Rollback, Usage (+2 more)

### Community 159 - "6. Master firmware — `legacy/master/master.cpp`"
Cohesion: 0.22
Nodes (9): 6. Master firmware — `legacy/master/master.cpp`, Boot defaults, Commands, Control parameters, Deployment, Libraries, Pins, Stirrer modes (+1 more)

### Community 160 - "Documentation review — all of `docs/`"
Cohesion: 0.40
Nodes (4): 2. Checked and left alone, 3. Open items, 4. What a reader should take from this, Documentation review — all of `docs/`

### Community 161 - "5. Frontend UI — `templates/index.html`"
Cohesion: 0.40
Nodes (5): 5. Frontend UI — `templates/index.html`, Dormant Clean User Template (`templates/Clean User Version.html`), Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer, Tabs, Temporary MOSFET Speed Control (`0–255` Debit Testing)

### Community 164 - "met_tick"
Cohesion: 0.39
Nodes (7): finish_touch(), met_tick(), met_touch(), on_display_event(), probe(), push_sens(), ui_route_name()

### Community 165 - "Easing"
Cohesion: 0.40
Nodes (4): EASE_IN, EASE_IN_OUT, EASE_OUT, LINEAR

### Community 166 - "Storylines: RuSim dashboard demo"
Cohesion: 0.40
Nodes (4): A. One chamber, start to finish (recommended), B. Four questions at the bench, C. The silent chamber (the bold one), Storylines: RuSim dashboard demo

### Community 167 - "delay"
Cohesion: 0.29
Nodes (5): advance_ms(), delay(), loop(), setup(), loop()

### Community 168 - "forceAllOff"
Cohesion: 0.24
Nodes (11): 1. Overview, 8. Slave UART Polling, Validation, and Watchdog (`Serial1`), Main loop, Stopping the Flask app does not stop the heater, Turning the heater off, driveHeater(), forceAllOff(), handleUsbCommand() (+3 more)

### Community 169 - "motor-driver/main.cpp"
Cohesion: 0.70
Nodes (4): loop(), rampUp(), setup(), stopMotor()

### Community 172 - "BootMark"
Cohesion: 0.67
Nodes (3): BootMark, ms, stage

### Community 175 - "SystemMode"
Cohesion: 0.67
Nodes (3): SystemMode, MODE_AUTO, MODE_OFF

### Community 176 - "10. Security & caveats"
Cohesion: 0.50
Nodes (4): 10. Security & caveats, Operational caveats, Recommendations, Security

## Knowledge Gaps
- **170 isolated node(s):** `RFC-4180`, `stage`, `ms`, `armed`, `down` (+165 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1376 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **20 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `Display DB Fetch State Machine` to `Display Fake Data Harness`, `Display Link Serial Protocol`, `logf`, `wt32-eth01_captive-recon/main.cpp`, `sens_report_round`, `main_native.cpp`, `temp-probe/main.cpp`, `master.cpp`, `2. Figures`, `render_current`, `met_tick`, `sim/Arduino.h`, `ui_shell.cpp`, `screen_setup_poll`, `Legacy Master Firmware (`legacy/master/master.cpp`)`, `metrics.cpp`, `delay`, `slave.cpp`, `screen_home.cpp`, `analogRead`, `network-master/main.cpp`, `doLogin`, `co2-calibrate/main.cpp`?**
  _High betweenness centrality (0.092) - this node is a cross-community bridge._
- **Are the 72 inferred relationships involving `millis()` (e.g. with `loop()` and `readCo2Uart()`) actually correct?**
  _`millis()` has 72 INFERRED edges - model-reasoned connections that need verification._
- **What connects `RFC-4180`, `stage`, `ms` to the rest of the system?**
  _170 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Display Fake Data Harness` be split into smaller, more focused modules?**
  _Cohesion score 0.05362614913176711 - nodes in this community are weakly interconnected._
- **Why does `ESP32-8048S050 — LLM Generated Summary` connect `ESP32-8048S050 — LLM Generated Summary` to `14. Boot, Flashing, and Debugging`, `16. Performance and Stability Checklist`, `13. ESPHome Notes`, `15. Filesystem and Asset Storage`, `4. Display Subsystem`, `5. Touch Variants`, `10. ESP-IDF Recommendations`, `6. Common Pin Map Reference`, `11. LVGL Recommendations`, `12. Arduino / PlatformIO Notes`?**
  _High betweenness centrality (0.035) - this node is a cross-community bridge._
- **Are the 31 inferred relationships involving `delay()` (e.g. with `loop()` and `setup()`) actually correct?**
  _`delay()` has 31 INFERRED edges - model-reasoned connections that need verification._
- **Should `Display Link Serial Protocol` be split into smaller, more focused modules?**
  _Cohesion score 0.07617051013277429 - nodes in this community are weakly interconnected._