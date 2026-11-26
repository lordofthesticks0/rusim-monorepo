# Graph Report - rusim-monorepo  (2026-11-26)

## Corpus Check
- 196 files · ~274,075 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 15 file(s) not represented in the graph (top: (none) 6, .toml 3, .example 1)

## Summary
- 2772 nodes · 5090 edges · 172 communities (157 shown, 15 thin omitted)
- Extraction: 84% EXTRACTED · 16% INFERRED · 0% AMBIGUOUS · INFERRED: 829 edges (avg confidence: 0.9)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `294f0c9b`
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
- db-check.ts
- lib.rs
- wt32-eth01_captive-recon/main.cpp
- screen_control.cpp
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
- polling_worker
- Firmware Test Runner
- render_current
- screen_data.cpp
- sens_poll
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
- ui_mk_button
- 3.6.1.3 Functions
- Captive Portal Login Script
- Factory Flash Tool
- Sensor Sketch Arduino Shims
- Documentation Index
- flask_cli.py
- run_shots
- Memory Safety Skill
- Display DB Fault Codes
- Database schema catalog
- analyze.ts
- Temperature Probe Sketch
- boot-loop.ts
- data.ts
- Sample Schema
- Push Row Schema
- dbRequest
- DB Contract Host Tests
- sync_clock_from_lan_http
- logf
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
- Sensor calibration firmware
- slave.cpp
- VideoDemoVideo
- WT32-ETH01 — LLM Documentation
- 4. `methane-calibrate`
- screen_home.cpp
- db-replay.ts
- ui_modal_show_sev
- Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)
- pi-water-control-system-mirror.md
- LVGL v8 to v9 migration report
- Chart
- authenticate
- PlatformIO build environments (`platformio.ini`)
- Network master ↔ database communication plan
- ESP32-8048S050 — LLM Generated Summary
- network-master (`firmware/network-master`) — production WT32 firmware
- main.rs
- index.md
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- auth.ts
- fake-slave.ts
- main
- 9. Network & services
- Stack
- CALIBRATION NOTES
- screen_control_poll
- 03-project-overview.md
- 3.2.1 Redesign plan — `/home`
- dashboard-load.ts
- fidelity.ts
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
- forceAllOff
- Display LVGL 9.3.0 -> 9.6.0 port
- BeatScene
- Pi sync script — `sync_pi.sh`
- 6. Master firmware — `legacy/master/master.cpp`
- compilerOptions
- raspi-docs/README.md
- 11. LVGL Recommendations
- 12. Arduino / PlatformIO Notes
- 14. Boot, Flashing, and Debugging
- 16. Performance and Stability Checklist
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants
- LVGL v8 to v9 migration report
- Route
- 3.6.1.3 Functions
- Sensor calibration firmware
- Display `IS_DEBUG` flag
- .new
- DbReply
- 6. Master firmware — `legacy/master/master.cpp`
- MetBoot
- temp-probe/main.cpp
- Dashboard public-deploy security report
- TouchPending
- Display Demo Video (`video-demo/`)
- MosfetChannel
- TouchResult
- handlePingStream
- metrics.ts
- my_disp_flush
- 8. Auxiliary tools & scripts
- Legacy Master Firmware (`legacy/master/master.cpp`)
- SensPending
- 3.2.1 Redesign plan — `/home`
- SensResult
- pinMode
- 2. High
- BootMark
- Display V9 PC Simulator (`[env:display-sim]`)
- 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing
- Easing
- Kind
- SystemMode
- video-demo

## God Nodes (most connected - your core abstractions)
1. `millis()` - 73 edges
2. `Dashboard()` - 33 edges
3. `delay()` - 32 edges
4. `logf()` - 31 edges
5. `loop()` - 25 edges
6. `dbPassTick()` - 24 edges
7. `sens_apply_real()` - 24 edges
8. `render()` - 21 edges
9. `Endpoint details` - 21 edges
10. `ESP32-8048S050 — LLM Generated Summary` - 21 edges

## Surprising Connections (you probably didn't know these)
- `6. Log forwarding` --references--> `logf()`  [INFERRED]
  docs/link-serial-protocol.md → firmware/network-master/main.cpp
- `9. Failure modes` --references--> `logf()`  [INFERRED]
  docs/link-serial-protocol.md → firmware/network-master/main.cpp
- `5. Recommended Fixes` --references--> `probeOnline()`  [INFERRED]
  docs/plans-and-audits/WT32-audit.md → firmware/network-master/main.cpp
- `11. The writer` --references--> `pushEnqueueRound()`  [INFERRED]
  docs/dashboard.md → firmware/network-master/main.cpp
- `Failure mode if the pin does break` --references--> `sbAuthenticate()`  [INFERRED]
  docs/plans-and-audits/esp-db-sync.md → firmware/network-master/main.cpp

## Import Cycles
- None detected.

## Communities (172 total, 15 thin omitted)

### Community 0 - "Display Fake Data Harness"
Cohesion: 0.06
Nodes (67): 6. Bell / NULL semantics, 2.4 Real vs fake labelling, 3.2.1.3 State and data model changes needed, 1. Summary, 2.2 Display reboot reports "stopped", 3.10 Debug UI in normal builds, 3.11 `LOGINREQ` routes away without explanation, 3.12 No connection status in the status bar (+59 more)

### Community 1 - "Display Link Serial Protocol"
Cohesion: 0.08
Nodes (47): arg_lookup(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line(), handle_log(), handle_login_req() (+39 more)

### Community 2 - "Flask Server Imports"
Cohesion: 0.08
Nodes (24): 8. `POST /download/info`, download_info(), export_data(), _extract_portal_redirect_url(), _fetch_http_date(), get_clock_payload(), get_drives(), get_interface_ip() (+16 more)

### Community 3 - "Dashboard Chart UI"
Cohesion: 0.10
Nodes (34): BottleChart(), BrandMark(), ChartInteraction, ChevronIcon(), CompareActivePoint, CompareIcon(), CompareSeries, Dashboard() (+26 more)

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
Cohesion: 0.08
Nodes (33): arg_lookup(), BottleReading, copyFormArg(), dbApplyRunState(), DbExperiment, durationH, id, sampleIntervalS (+25 more)

### Community 9 - "db-check.ts"
Cohesion: 0.14
Nodes (7): Block, { flags }, outDir, REPO, sql, stamp, vars

### Community 10 - "lib.rs"
Cohesion: 0.04
Nodes (43): ACCENT_SOFT, BASE_SCALE, BEZEL, BOTTLE, CLOSE, CONTROL, CROSSFADE, CROSSFADE_RUNTIME (+35 more)

### Community 11 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.08
Nodes (33): 1. Environment, 2. What it does, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`) (+25 more)

### Community 12 - "screen_control.cpp"
Cohesion: 0.29
Nodes (11): link_serial_set_exp_state(), on_dur(), on_exp(), on_int_test(), on_pump_commit(), on_run_commit(), on_stirrer_commit(), on_temp() (+3 more)

### Community 13 - "Legacy Master USB CLI"
Cohesion: 0.05
Nodes (51): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+43 more)

### Community 14 - "main_native.cpp"
Cohesion: 0.14
Nodes (8): DispBufDeleter, lvgl_tick_get(), fmt_age(), screen_info_hide(), screen_info_refresh(), set_tile(), settle(), shot()

### Community 15 - "Display DB Fetch State Machine"
Cohesion: 0.12
Nodes (38): Auth used, Build, DB experiment sync + NTP (change record), Link (`rusim-line` v1), Master (`firmware/network-master/main.cpp`), 6.1 Database path (configuration, runs and uploads), 6.2 Metrics build (`network-master-metrics`), 6. Memory and limits (+30 more)

### Community 16 - "Dashboard Package Manifest"
Cohesion: 0.05
Nodes (44): dependencies, react@19.1.1, react-dom@19.1.1, @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13 (+36 more)

### Community 17 - "Dashboard — full walkthrough"
Cohesion: 0.06
Nodes (33): summarizeParameter(), 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 11. The writer, 12.1 The experiment list is bounded by the monitoring fetch's row cap, 12.2 The chart heading says "Across experiments" (fixed) (+25 more)

### Community 18 - "read-legacy-slave.ts"
Cohesion: 0.14
Nodes (21): ALIGN, Bottle, configurePort(), createLineGate(), Device, formatCell(), formatInt(), formatNumber() (+13 more)

### Community 19 - "PWM Pin Readout Sketch"
Cohesion: 0.15
Nodes (22): digitalRead(), millis(), Channel, haveReading, haveRise, level, periodUs, pin (+14 more)

### Community 20 - "Captive Portal Login Flow"
Cohesion: 0.14
Nodes (26): The Backlog, 2. The steps, 3. Helper functions and their firmware counterparts, network-master: bounded buffers, 5. Portal login (`doLogin()`), 3.4 Pre-pass delays and redundant portal probes, doLogin(), extractWindowLocation() (+18 more)

### Community 21 - "master.cpp"
Cohesion: 0.24
Nodes (12): 4.2 Asymmetric Fast-Trip / Leaky-Bucket Recovery Debouncing, 7. Unified 3-Channel MOSFET PWM Ramping and `0–255` Debit Control, driveMosfetPwm(), loop(), readAdcSample(), resetMosfetOff(), sendSlavePing(), setup() (+4 more)

### Community 22 - "Dashboard TypeScript Config"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 23 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 24 - "polling_worker"
Cohesion: 0.08
Nodes (31): 1. Fixes applied, 2. Checked and left alone, 3. Open items, 4. What a reader should take from this, Documentation review — all of `docs/`, Runtime, 4. Backend — `web_control.py`, Background workers (+23 more)

### Community 25 - "Firmware Test Runner"
Cohesion: 0.16
Nodes (6): main(), run(), braced(), declarations(), MethaneCalibrationRegression, native_source()

### Community 26 - "render_current"
Cohesion: 0.16
Nodes (22): 1.1 Router (`ui_shell.cpp`, `s_route`), 2.1 Styling, mk_equipment(), mk_stepper_row(), screen_control_show(), make_tappable(), screen_bottle_show(), screen_cluster_show() (+14 more)

### Community 27 - "screen_data.cpp"
Cohesion: 0.17
Nodes (16): 3.5 Route `/data/X/bottle_Y` — chamber detail, fake_sensor_text(), fake_sensor_value_text(), bottle_refresh(), bottle_state(), chart_refresh(), cluster_refresh(), Page (+8 more)

### Community 28 - "sens_poll"
Cohesion: 0.13
Nodes (17): 7.1 `Serial` becomes the link, 7.2 Link plumbing, 7.3 State machine (semantics kept, shape changed), 7.4 Porting pitfalls to check at review, 7. WT32 end (`firmware/network-master/main.cpp`), 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour (+9 more)

### Community 29 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 30 - "4. `methane-calibrate`"
Cohesion: 0.17
Nodes (13): 4.2 Methane (TGS2611), 6. `ph-calibrate`, Correctness limits, Linear conversion (temporary), Sampling and output, Wiring, analogRead(), convertCountToPh() (+5 more)

### Community 31 - "data.ts"
Cohesion: 0.14
Nodes (15): CONCLUDED_AFTER_MS, fetchDevice(), fetchExperimentReadings(), fetchReadings(), MAX_EXPERIMENT_ROWS, MAX_READING_ROWS, POLL_INTERVAL_MS, ReadingRow (+7 more)

### Community 32 - "Dashboard CSV Export"
Cohesion: 0.14
Nodes (19): csvDisabledReason(), downloadExperimentCsv(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv() (+11 more)

### Community 33 - "Dashboard History Data"
Cohesion: 0.18
Nodes (15): ParameterStats, ChamberSummary, getExperimentChambers(), groupReadingsByChamber(), HISTORY_PARAMETERS, HistoryPointDetails, parameterValue(), selectChamberReadings() (+7 more)

### Community 34 - "Host Serial Buffer"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 35 - "Agent Rules Document"
Cohesion: 0.14
Nodes (13): Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General, General Requirements (+5 more)

### Community 36 - "sim/Arduino.h"
Cohesion: 0.15
Nodes (3): What is shared vs stubbed, random(), randomSeed()

### Community 37 - "Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)"
Cohesion: 0.09
Nodes (21): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+13 more)

### Community 38 - "DB Contract Envelope"
Cohesion: 0.21
Nodes (16): bottleInRange(), civilFromDays(), daysFromCivil(), findField(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject() (+8 more)

### Community 39 - "Dashboard Compare View"
Cohesion: 0.13
Nodes (17): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), ExperimentSummary, DashboardView, DEV_FORCE_LIVE_EXPERIMENT, generateSyntheticLiveReadings(), isMonitoringPollingEligible() (+9 more)

### Community 40 - "ui_mk_button"
Cohesion: 0.09
Nodes (27): 3.10 Overheat warning, 41 °C, 3.16 Debug-only UI, 3.3.1.1 Removed content, 3.3.1.2 Layout (content coordinates, panel 800x436), 3.3.1.4 Behaviour kept, 3.3.1.5 Open item, 3.3.1 Redesign plan — `/data` node picker, 3.3 Route `/data` — node picker (+19 more)

### Community 41 - "3.6.1.3 Functions"
Cohesion: 0.25
Nodes (8): 3.6.1.1 Removed content, 3.6.1.2 Layout (content coordinates, panel 800x436), 3.6.1.3 Functions, 3.6.1.4 Constants, 3.6.1.5 DB status text (shortened to fit 200 px at font 14), 3.6.1.6 Run button colours, 3.6.1 Redesign plan — `/control`, 3.6 Route `/control`

### Community 42 - "Captive Portal Login Script"
Cohesion: 0.22
Nodes (17): attempt_login(), do_curl(), extract_window_location(), form_action(), log(), log_lines(), log_network(), on_interrupt() (+9 more)

### Community 43 - "Factory Flash Tool"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 44 - "Sensor Sketch Arduino Shims"
Cohesion: 0.13
Nodes (4): advance_ms(), delay(), loop(), setup()

### Community 45 - "Documentation Index"
Cohesion: 0.17
Nodes (12): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 3c. Plans and audits, 4. Hardware references, 5. Human documents (+4 more)

### Community 46 - "flask_cli.py"
Cohesion: 0.23
Nodes (5): get_json(), main(), _open(), post_download(), post_json()

### Community 47 - "run_shots"
Cohesion: 0.17
Nodes (22): Decoded backtrace, 3.12 Bell and invalid-field list, 3.3.1.3 Back button, 3.8.1.4 Functions in `ui_shell.cpp` / `ui_shell.h`, 3.4 Back button on a node page goes to Home, on_back_cluster(), on_back_picker(), on_pick_bottle() (+14 more)

### Community 48 - "Memory Safety Skill"
Cohesion: 0.22
Nodes (7): 1. Files, 2. Run it, 3. How it works, 4. Stable windows and their averages, 5. What the current log shows, 6. Limits, Pressure calibration plot

### Community 49 - "Display DB Fault Codes"
Cohesion: 0.25
Nodes (8): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA

### Community 50 - "Database schema catalog"
Cohesion: 0.10
Nodes (20): Access control, Chamber numbering, Database schema catalog, Development tooling, `device_data`: active application schema, `device_data.device_config`, `device_data.devices`, `device_data.experiments` (+12 more)

### Community 51 - "analyze.ts"
Cohesion: 0.12
Nodes (19): csv(), { flags, positional }, json, LIMIT, linkIn, md, met, metas (+11 more)

### Community 52 - "Temperature Probe Sketch"
Cohesion: 0.29
Nodes (13): CompareView(), displayChamberNumber(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor(), compareSeriesLabel() (+5 more)

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

### Community 57 - "dbRequest"
Cohesion: 0.12
Nodes (20): 3.1 30-second cooldown on every startup failure, 3.2 Late, blocking NTP sync that resets SNTP on every retry, 3.3 Five all-or-nothing cold-TLS HTTPS calls before `ready=1`, 3.5 Potential socket-timeout stall on chunked responses, 3. Root Causes, 4. Verification on Hardware, 5. Recommended Fixes, WT32 Database Authentication & Startup Latency Audit (+12 more)

### Community 58 - "DB Contract Host Tests"
Cohesion: 0.30
Nodes (10): contains(), main(), row(), test_ack_and_errors(), test_array_iteration(), test_bottle_range(), test_envelope(), test_field_parsing() (+2 more)

### Community 59 - "sync_clock_from_lan_http"
Cohesion: 0.50
Nodes (4): `clock_sync_worker` (daemon thread), clock_sync_worker(), _get_default_gateway_ip(), sync_clock_from_lan_http()

### Community 60 - "logf"
Cohesion: 0.16
Nodes (21): 4.1 Wi-Fi fallback, 4. State machine, link_emit_loginreq(), link_emit_net(), link_emit_truth(), link_heartbeat_tick(), link_log_dump(), log_eth_event() (+13 more)

### Community 61 - "screen_setup_poll"
Cohesion: 0.08
Nodes (33): 11. Setup route, 2.2 Navigation, 2.3 Data refresh, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting, 3.13 Phone-associated popup (+25 more)

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
Cohesion: 0.28
Nodes (11): link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), handle_line(), parse_onoff(), print_help(), print_mem(), print_status() (+3 more)

### Community 70 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.11
Nodes (19): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display, 3.3 Gone from the Modbus design (+11 more)

### Community 71 - "metrics.cpp"
Cohesion: 0.14
Nodes (21): 16. Verification metrics build (`display-v9-metrics`), handle_sens(), finish_touch(), gap_track(), indev_timer_wrapper(), met_after_timer_handler(), met_boot(), met_console() (+13 more)

### Community 72 - "Endpoint details"
Cohesion: 0.11
Nodes (19): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. `GET /files`, 14. `GET /read_log`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip` (+11 more)

### Community 73 - "WiFi Provisioning Stages"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 75 - "Sensor calibration firmware"
Cohesion: 0.24
Nodes (10): 2. `co2-calibrate`, Correctness limits, Field correction, How a reading is taken, Plotting a serial capture, loop(), readCo2Ppm(), readCo2Uart() (+2 more)

### Community 76 - "slave.cpp"
Cohesion: 0.24
Nodes (14): 4.1 pH, 4.4 Pressure (HX711 / HX710B), convertPhCount(), fitExponent(), initPressureSensor(), loop(), readCo2Ppm(), readPhRaw() (+6 more)

### Community 77 - "VideoDemoVideo"
Cohesion: 0.12
Nodes (6): Touch, VideoDemoVideo, .BACKGROUND_COLOR, .FPS, .HEIGHT, .WIDTH

### Community 78 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 79 - "4. `methane-calibrate`"
Cohesion: 0.28
Nodes (10): Sampling, delay(), loop(), readRs(), rsToPpm(), rsToPpmRaw(), setup(), initializeSensor() (+2 more)

### Community 80 - "screen_home.cpp"
Cohesion: 0.24
Nodes (11): bottles_online(), elapsed_hours(), nodes_online(), on_duration(), on_run_commit(), render_run(), render_status(), screen_home_hide() (+3 more)

### Community 81 - "db-replay.ts"
Cohesion: 0.13
Nodes (15): bottles, call(), exp, { flags }, iso(), lat, latencyN, md (+7 more)

### Community 82 - "ui_modal_show_sev"
Cohesion: 0.15
Nodes (15): 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 4. Cross-cutting redesign table, 5. Discrepancies found while writing this, 6. Verification, LVGL UI redesign (`display-v9`), sev_color() (+7 more)

### Community 84 - "Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)"
Cohesion: 0.10
Nodes (29): 10. WT32 link (`rusim-line` v1, production), 12. WT32 login page QR, 13. Light theme and the /setup QR, 14. Debug flag `IS_DEBUG`, 15.1 Render cost, 15.2 Grid, 15.3 Colour and text, 15.4 Refresh and input (+21 more)

### Community 85 - "pi-water-control-system-mirror.md"
Cohesion: 0.14
Nodes (11): 2026-10-20 change, Deploying a change to the kiosk, How it works, Kiosk text scale, Related documents, Raspberry Pi kiosk UI CSS redesign, 5. Frontend UI — `templates/index.html`, Dormant Clean User Template (`templates/Clean User Version.html`) (+3 more)

### Community 86 - "LVGL v8 to v9 migration report"
Cohesion: 0.14
Nodes (6): 2. Hardware / platform notes, my_touchpad_read(), touch(), touch_has_signal(), touch_released(), touch_touched()

### Community 87 - "Chart"
Cohesion: 0.15
Nodes (26): Chart(), chartAreaPath(), chartGeometry(), chartPath(), CompareChart(), createChartPoints(), createTimeLabels(), createYTicks() (+18 more)

### Community 88 - "authenticate"
Cohesion: 0.08
Nodes (29): authenticate(), display-v9: DB fields from the `DB` line, Memory-safety hardening (`display-v9`, `network-master`), Verification, 1. Current state, 2. The database forbids writes, 3. Nothing reads the sensors, 4. Why the device must not send `token_hash` (+21 more)

### Community 89 - "PlatformIO build environments (`platformio.ini`)"
Cohesion: 0.25
Nodes (8): 1. Layout, 2. Shared settings, 3. Environment table, 4. The ESP32-S3 display environments, 5. Commands, 6. History of this configuration, 7. Related documents, PlatformIO build environments (`platformio.ini`)

### Community 90 - "Network master ↔ database communication plan"
Cohesion: 0.12
Nodes (16): 1. Recommendation and evidence, 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions (+8 more)

### Community 91 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.14
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 92 - "network-master (`firmware/network-master`) — production WT32 firmware"
Cohesion: 0.15
Nodes (13): 1. Environment, 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential handling (+5 more)

### Community 94 - "index.md"
Cohesion: 0.12
Nodes (6): Retired: display-hosted Wi-Fi provisioning, Display <-> WT32 link (`display-wt32-link`) — replaced, 3D Models, PCB, gen-compile-db.sh script, install-leaf.sh script

### Community 95 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.20
Nodes (10): 1. What the program does, 2. Build environment, 3. Pin map, 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing, 6. Responsiveness, 7. Known gaps (+2 more)

### Community 96 - "auth.ts"
Cohesion: 0.17
Nodes (13): App(), AuthState, readStoredSession(), StoredSession, useAuth(), writeStoredSession(), signOut(), 5.1 Page load (+5 more)

### Community 97 - "fake-slave.ts"
Cohesion: 0.12
Nodes (16): Bottle, build(), expectedPath, { flags }, HEADER, MIXED, outDir, port (+8 more)

### Community 98 - "main"
Cohesion: 0.16
Nodes (8): Display (`firmware/display-v9/`), 3.2.1.5 Polling, screen_control_poll(), SimSerial, link_serial_init(), link_serial_poll(), link_serial_set_exp_state(), main()

### Community 99 - "9. Network & services"
Cohesion: 0.18
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

### Community 103 - "03-project-overview.md"
Cohesion: 0.40
Nodes (4): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Three copies exist, What it does

### Community 104 - "3.2.1 Redesign plan — `/home`"
Cohesion: 0.22
Nodes (8): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, UI, `web_control.py`

### Community 105 - "dashboard-load.ts"
Cohesion: 0.09
Nodes (25): table(), durationMs, exp, { flags }, get(), kinds, md, outDir (+17 more)

### Community 106 - "fidelity.ts"
Cohesion: 0.12
Nodes (10): Cmp, detail, Exp, expected, FIELDS, { flags }, md, outDir (+2 more)

### Community 107 - "status"
Cohesion: 0.40
Nodes (4): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)

### Community 108 - "Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)"
Cohesion: 0.25
Nodes (7): 1. Purpose, 2. Wire protocol, 3. How it opens the port, 4. REPL behaviour, 5. Usage, 6. Limits, Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)

### Community 109 - "Raspberry Pi `water-control-system` mirror"
Cohesion: 0.29
Nodes (7): Checksums, Layout, Raspberry Pi `water-control-system` mirror, Refreshing the mirror, Related documents, Relationship to the deleted `main_ui.html`, What is not included

### Community 110 - "run_shots"
Cohesion: 0.13
Nodes (22): 4. Screen tree / state machine (as built), Widget lifetime rule, 3.11 Overheat breaker trip, 45 °C, 2.3 Full-screen rebuild on every frame, on_chip_clicked(), on_modal_ack(), on_sens_timer(), on_stepper_pressed() (+14 more)

### Community 111 - "Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)"
Cohesion: 0.15
Nodes (8): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Keep-alive, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)

### Community 112 - "HoldBtnCtx"
Cohesion: 0.11
Nodes (23): 3.2.1.4 Functions to add / change, met_hold(), hold_commit(), hold_tick(), HoldBtnCtx, btn, completed, halo (+15 more)

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

### Community 118 - "forceAllOff"
Cohesion: 0.24
Nodes (11): 1. Overview, 8. Slave UART Polling, Validation, and Watchdog (`Serial1`), Main loop, Stopping the Flask app does not stop the heater, Turning the heater off, driveHeater(), forceAllOff(), handleUsbCommand() (+3 more)

### Community 119 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 120 - "BeatScene"
Cohesion: 0.20
Nodes (7): Beat, BEATS, BeatScene, Gesture, Hold, None, Tap

### Community 121 - "Pi sync script — `sync_pi.sh`"
Cohesion: 0.20
Nodes (10): Failure behaviour, First run, Limitations, Master firmware flashing, Pi sync script — `sync_pi.sh`, Prerequisites, Rollback, Usage (+2 more)

### Community 122 - "6. Master firmware — `legacy/master/master.cpp`"
Cohesion: 0.25
Nodes (7): 1. Usage, 1a. Diagnostic output, 4. Differences from the firmware, 5. Testing, 6. Deployment on the legacy Pi, 7. Related documents, Captive portal login from Linux (bash)

### Community 123 - "compilerOptions"
Cohesion: 0.13
Nodes (14): compilerOptions, allowImportingTsExtensions, lib, module, moduleDetection, moduleResolution, noEmit, skipLibCheck (+6 more)

### Community 124 - "raspi-docs/README.md"
Cohesion: 0.08
Nodes (19): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account, 7. Data & logging, CSV format (current schema) (+11 more)

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

### Community 134 - "LVGL v8 to v9 migration report"
Cohesion: 0.15
Nodes (12): 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, File-by-file summary, LVGL v8 to v9 migration report, Memory and runtime considerations (+4 more)

### Community 135 - "Route"
Cohesion: 0.25
Nodes (8): Route, R_BOTTLE, R_CLUSTER, R_CONTROL, R_HOME, R_INFO, R_PICKER, R_SETUP

### Community 136 - "3.6.1.3 Functions"
Cohesion: 0.33
Nodes (7): 7. `POST /toggle_logging`, fmt_val(), format_aligned_row(), get_todays_filename(), log_data_to_csv(), toggle_logging(), write_headers_if_new()

### Community 137 - "Sensor calibration firmware"
Cohesion: 0.20
Nodes (10): 1. Common structure, 3. `pressure-calibrate`, 4. `methane-calibrate`, 5. `temp-probe`, 7. Related documents, Local validation, Reference-data refit, Sensor calibration firmware (+2 more)

### Community 138 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses

### Community 139 - ".new"
Cohesion: 0.24
Nodes (5): camera_axis(), camera_for(), kf(), spot_for(), touch_times()

### Community 140 - "DbReply"
Cohesion: 0.29
Nodes (6): 1. What the file is, 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh

### Community 141 - "6. Master firmware — `legacy/master/master.cpp`"
Cohesion: 0.22
Nodes (9): 6. Master firmware — `legacy/master/master.cpp`, Boot defaults, Commands, Control parameters, Deployment, Libraries, Pins, Stirrer modes (+1 more)

### Community 142 - "MetBoot"
Cohesion: 0.15
Nodes (13): MetBoot, MB_ACK_FIRST, MB_AUTH, MB_COUNT, MB_ENQ_FIRST, MB_ETH_LINK, MB_ETH_START, MB_GOT_IP (+5 more)

### Community 143 - "temp-probe/main.cpp"
Cohesion: 0.24
Nodes (9): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup(), printAddress() (+1 more)

### Community 144 - "Dashboard public-deploy security report"
Cohesion: 0.17
Nodes (11): 1. Blocker — legacy `public` schema is public-read (live, confirmed), 2. High — `authenticate()` has no brute-force protection, 3. High — Netlify bundle + env handling, 4. Medium — session hygiene and PII, 5. Low — functional / ops, Already fixed / verified good (keep these), Dashboard public-deploy security report, Findings (ordered by severity) (+3 more)

### Community 145 - "TouchPending"
Cohesion: 0.17
Nodes (12): TouchPending, armed, down, from, inv, invUs, renderSeen, renderStartUs (+4 more)

### Community 146 - "Display Demo Video (`video-demo/`)"
Cohesion: 0.25
Nodes (8): Build, Commands, Display Demo Video (`video-demo/`), How the video works, Layout, Limitations, Refreshing the snapshots, Storyboard

### Community 148 - "MosfetChannel"
Cohesion: 0.25
Nodes (8): MosfetChannel, currentPwm, maxPwm, pin, rampOriginMs, rampOriginPwm, state, targetPwm

### Community 149 - "TouchResult"
Cohesion: 0.15
Nodes (13): push_result(), TouchResult, down, from, invUs, latUs, nf, renderUs (+5 more)

### Community 150 - "handlePingStream"
Cohesion: 0.24
Nodes (10): 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, freeRam(), handlePingStream(), masterPollImminent() (+2 more)

### Community 151 - "metrics.ts"
Cohesion: 0.29
Nodes (9): heapBytes(), MetricRecord, metricsEnabled, newestAgeSeconds(), recordMetric(), sendToSink(), sinkUrl(), startFetchTimer() (+1 more)

### Community 153 - "8. Auxiliary tools & scripts"
Cohesion: 0.40
Nodes (4): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc

### Community 154 - "Legacy Master Firmware (`legacy/master/master.cpp`)"
Cohesion: 0.29
Nodes (7): 10. Known Gaps, 2. Pin Assignment, 3. Boot Defaults, 5. Automatic Pump Interlocks, 6. Mode and Safety Cutoffs, 9. Raspberry Pi USB Serial Protocol, Legacy Master Firmware (`legacy/master/master.cpp`)

### Community 155 - "SensPending"
Cohesion: 0.29
Nodes (7): SensPending, applyArmed, applyUs, localSeq, rxArmed, rxUs, wseq

### Community 156 - "3.2.1 Redesign plan — `/home`"
Cohesion: 0.29
Nodes (7): 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.6 Integration with other screens, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels), 3.2.1.8 Out of scope for this screen (stays on /control), 3.2.1 Redesign plan — `/home`, 3.2 Route `/home`

### Community 157 - "SensResult"
Cohesion: 0.33
Nodes (6): SensResult, applyUs, localSeq, noframe, renderUs, wseq

### Community 158 - "pinMode"
Cohesion: 0.33
Nodes (5): setup(), met_attach(), met_init_early(), pinMode(), touch_init()

### Community 159 - "2. High"
Cohesion: 0.33
Nodes (6): 2.1 Re-login stops the running experiment, 2.4 Experiment number keeps resetting on `/control`, 2.5 Setpoint and stirrer controls do nothing, 2.6 Wake tap is delivered as a press, 2. High, screen_control_hide()

### Community 160 - "BootMark"
Cohesion: 0.67
Nodes (3): BootMark, ms, stage

### Community 161 - "Display V9 PC Simulator (`[env:display-sim]`)"
Cohesion: 0.40
Nodes (5): Configuration, Display V9 PC Simulator (`[env:display-sim]`), Run it, Scope note, Screenshots (headless)

### Community 164 - "4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing"
Cohesion: 0.50
Nodes (5): 4.1 Trip thresholds, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing, computeWaterRingAverage(), sampleWaterSensor()

### Community 165 - "Easing"
Cohesion: 0.40
Nodes (4): EASE_IN, EASE_IN_OUT, EASE_OUT, LINEAR

### Community 166 - "Kind"
Cohesion: 0.40
Nodes (5): Kind, End, Sleep, Step, Title

### Community 167 - "SystemMode"
Cohesion: 0.67
Nodes (3): SystemMode, MODE_AUTO, MODE_OFF

## Knowledge Gaps
- **170 isolated node(s):** `stage`, `ms`, `armed`, `down`, `wake` (+165 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1259 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **15 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `Display DB Fetch State Machine` to `Display Fake Data Harness`, `Display Link Serial Protocol`, `network-master/main.cpp`, `wt32-eth01_captive-recon/main.cpp`, `main_native.cpp`, `temp-probe/main.cpp`, `master.cpp`, `handlePingStream`, `render_current`, `sens_poll`, `4. `methane-calibrate``, `sim/Arduino.h`, `run_shots`, `dbRequest`, `logf`, `screen_setup_poll`, `Legacy Master Firmware (`legacy/master/master.cpp`)`, `metrics.cpp`, `Sensor calibration firmware`, `slave.cpp`, `4. `methane-calibrate``, `screen_home.cpp`, `Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)`, `authenticate`, `run_shots`?**
  _High betweenness centrality (0.079) - this node is a cross-community bridge._
- **Are the 72 inferred relationships involving `millis()` (e.g. with `loop()` and `readCo2Uart()`) actually correct?**
  _`millis()` has 72 INFERRED edges - model-reasoned connections that need verification._
- **What connects `stage`, `ms`, `armed` to the rest of the system?**
  _170 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Display Fake Data Harness` be split into smaller, more focused modules?**
  _Cohesion score 0.059319482083709726 - nodes in this community are weakly interconnected._
- **Why does `ESP32-8048S050 — LLM Generated Summary` connect `ESP32-8048S050 — LLM Generated Summary` to `14. Boot, Flashing, and Debugging`, `16. Performance and Stability Checklist`, `13. ESPHome Notes`, `15. Filesystem and Asset Storage`, `4. Display Subsystem`, `5. Touch Variants`, `10. ESP-IDF Recommendations`, `6. Common Pin Map Reference`, `11. LVGL Recommendations`, `12. Arduino / PlatformIO Notes`?**
  _High betweenness centrality (0.035) - this node is a cross-community bridge._
- **Are the 31 inferred relationships involving `delay()` (e.g. with `loop()` and `setup()`) actually correct?**
  _`delay()` has 31 INFERRED edges - model-reasoned connections that need verification._
- **Should `Display Link Serial Protocol` be split into smaller, more focused modules?**
  _Cohesion score 0.08069381598793364 - nodes in this community are weakly interconnected._