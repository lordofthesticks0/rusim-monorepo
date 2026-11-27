# Graph Report - rusim-monorepo  (2026-11-27)

## Corpus Check
- 224 files · ~312,324 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 19 file(s) not represented in the graph (top: (none) 8, .lock 3, .toml 3)

## Summary
- 3044 nodes · 5575 edges · 180 communities (161 shown, 19 thin omitted)
- Extraction: 84% EXTRACTED · 16% INFERRED · 0% AMBIGUOUS · INFERRED: 879 edges (avg confidence: 0.9)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `782d117b`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- test_display_sensors.cpp
- link_serial.cpp
- web_control.py
- App.tsx
- Pin Layout Overview
- monitor-legacy-pi.ts
- network-master-passthrough.py
- link_stub.cpp
- auth.ts
- db-check.ts
- lib.rs
- wt32-eth01_captive-recon/main.cpp
- 3. States
- dev-db.ts
- display-v9/main.cpp
- millis
- dashboard/package.json
- Dashboard — full walkthrough
- read-legacy-slave.ts
- Channel
- network-master: bounded buffers
- master.cpp
- compilerOptions
- 5. Checklist
- serial_worker
- test_methane_calibration.py
- screen_data.cpp
- screen_data.cpp
- handle_line
- ATmega2560 Mega Mini — Development & Reference Documentation
- hyperframes/capture/capture.ts
- run_shots
- experimentReadings.test.ts
- history.ts
- HostSerial
- Rules
- sim/Arduino.h
- Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)
- db_contract.h
- data.ts
- readingCache.ts
- ui_shell.cpp
- captive-portal-login.sh
- flash-factory.ts
- Repository Index
- flask_cli.py
- Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)
- hyperframes-mobile/capture/capture.ts
- probeOnline
- Reference: `device_data` column detail
- analyze.ts
- compare.ts
- boot-loop.ts
- Methane calibration report generator
- BottleSample
- PushRow
- LVGL v8 to v9 migration report
- db-contract/main.cpp
- viaDrawer
- polling_worker
- screen_setup_poll
- DbPass
- CsvDownloadHandler
- SimESPClass
- ESP32-8048S050C · Board & Archive Guide
- verification/capture.ts
- Verification toolkit (report data collection)
- Chart
- HostESP
- Display <-> WT32 link: `rusim-line` v1
- metrics.cpp
- Endpoint details
- _close_tunnel
- brandTransition.ts
- 2. Figures
- VideoDemoVideo
- WT32-ETH01 — LLM Documentation
- WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)
- main
- db-replay.ts
- Stage
- co2-calibrate/main.cpp
- raspi-docs/README.md
- my_touchpad_read
- sens_report_round
- authenticate
- network-master/main.cpp
- Network master ↔ database communication plan
- ESP32-8048S050 — LLM Generated Summary
- doLogin
- main.rs
- network-master-database-plan.md
- Pressure calibration plot
- hyperframes-mobile/hyperframes.json
- fake-slave.ts
- hyperframes/capture/package.json
- 9. Network & services
- Stack
- CALIBRATION NOTES
- hyperframes-mobile/capture/package.json
- Display V9 PC Simulator (`[env:display-sim]`)
- 12. Copies & git state
- dashboard-load.ts
- fidelity.ts
- Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)
- Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)
- _fetch_http_date
- 1. Hardware & OS
- PlatformIO build environments (`platformio.ini`)
- ui_hold_button.cpp
- sync_pi.sh
- 2. Boot & kiosk session model
- `leaf` terminal reader on the Pi
- 10. ESP-IDF Recommendations
- 6. Common Pin Map Reference
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- Display LVGL 9.3.0 -> 9.6.0 port
- BeatScene
- hyperframes/hyperframes.json
- delay
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
- millis
- Display `IS_DEBUG` flag
- .new
- compile_commands.json and gen-compile-db.sh
- Review: RuSim dashboard demo, mobile UI
- MetBoot
- temp-probe/main.cpp
- Dashboard public-deploy security report
- TouchPending
- Display Demo Video (`video-demo/`)
- Sensor calibration firmware
- TouchResult
- ui_exp_status_text
- analogRead
- Kiosk text scale
- Raspberry Pi `water-control-system` mirror
- Review: RuSim dashboard demo
- SensPending
- screen_home.cpp
- SensResult
- _Relay
- DbExperiment
- Documentation review — all of `docs/`
- 5. Frontend UI — `templates/index.html`
- DbFault
- Easing
- Storylines: RuSim dashboard demo
- delay
- Dashboard Demo Videos (`video-demo/hyperframes/`, `video-demo/hyperframes-mobile/`)
- SimSerial
- Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)
- video-demo
- Migration plan: ESP pulls telemetry from the legacy Mega slave
- hyperframes/capture/build-sfx.sh
- hyperframes/SPEC.md
- Stage
- 10. Security & caveats
- hyperframes-mobile/capture/build-sfx.sh
- 11. Key paths quick reference
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
9. `CompareChart()` - 21 edges
10. `render()` - 21 edges

## Surprising Connections (you probably didn't know these)
- `5.5 The Monitoring view` --references--> `generateSyntheticLiveReadings()`  [INFERRED]
  docs/dashboard.md → dashboard/src/monitoring.ts
- `11. The writer` --references--> `pushEnqueueRound()`  [INFERRED]
  docs/dashboard.md → firmware/network-master/main.cpp
- `13.1 Sensors and units` --references--> `getParameterValue()`  [INFERRED]
  docs/dashboard.md → dashboard/src/data.ts
- `3.1.1 `SENS` sensor values` --references--> `sens_apply_real()`  [INFERRED]
  docs/link-serial-protocol.md → firmware/display-v9/fake_data.cpp
- `6. Responsiveness` --references--> `serviceMasterPing()`  [INFERRED]
  docs/legacy-slave.md → legacy/slave/slave.cpp

## Import Cycles
- None detected.

## Communities (180 total, 19 thin omitted)

### Community 0 - "test_display_sensors.cpp"
Cohesion: 0.05
Nodes (77): 10. WT32 link (`rusim-line` v1, production), 6. Bell / NULL semantics, 2.4 Real vs fake labelling, 3.2.1.3 State and data model changes needed, 3.5 Route `/data/X/bottle_Y` — chamber detail, 1. Summary, 2.1 Re-login stops the running experiment, 2.2 Display reboot reports "stopped" (+69 more)

### Community 1 - "link_serial.cpp"
Cohesion: 0.09
Nodes (38): arg_lookup(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line(), handle_log(), handle_login_req() (+30 more)

### Community 2 - "web_control.py"
Cohesion: 0.08
Nodes (24): 14. `GET /read_log`, 8. `POST /download/info`, Summary, command(), download_info(), export_data(), get_clock_payload(), get_drives() (+16 more)

### Community 3 - "App.tsx"
Cohesion: 0.08
Nodes (41): ArrowIcon(), BottleChart(), BrandHooks, ChartInteraction, ChevronIcon(), CompareActivePoint, CompareIcon(), CompareSeries (+33 more)

### Community 4 - "Pin Layout Overview"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 5 - "monitor-legacy-pi.ts"
Cohesion: 0.07
Nodes (45): 1. Usage, 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents (+37 more)

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.16
Nodes (8): _after_upload(), _before_upload(), _cleanup_after_failed_upload(), _configured_port(), _display_ports(), _find_port(), _open_display(), _shutdown()

### Community 8 - "auth.ts"
Cohesion: 0.09
Nodes (29): dependencies, react@19.1.1, react-dom@19.1.1, App(), BrandMark(), BrandStage(), shake(), AuthState (+21 more)

### Community 9 - "db-check.ts"
Cohesion: 0.14
Nodes (7): Block, { flags }, outDir, REPO, sql, stamp, vars

### Community 10 - "lib.rs"
Cohesion: 0.04
Nodes (50): ACCENT_SOFT, BASE_SCALE, BEZEL, BOTTLE, CLOSE, CONTROL, CROSSFADE, CROSSFADE_RUNTIME (+42 more)

### Community 11 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.14
Nodes (20): 2. What it does, extractWindowLocation(), loop(), onEthEvent(), PageScan, cloudflareChallenge, formAction, hasForm (+12 more)

### Community 12 - "3. States"
Cohesion: 0.07
Nodes (34): 1.3 Non-route states, 1. Inventory, 2.2 Navigation, 2.3 Data refresh, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting (+26 more)

### Community 13 - "dev-db.ts"
Cohesion: 0.05
Nodes (51): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+43 more)

### Community 14 - "display-v9/main.cpp"
Cohesion: 0.11
Nodes (15): DispBufDeleter, lvgl_tick_get(), on_dur(), on_exp(), on_int_test(), on_pump_commit(), on_run_commit(), on_stirrer_commit() (+7 more)

### Community 15 - "millis"
Cohesion: 0.14
Nodes (25): 6.2 Metrics build (`network-master-metrics`), 6. Memory and limits, 5. Content audit, 2026-11-18, dbArrayIsEmpty(), dbClassify(), DbFetch, DB_FETCH_AUTH, DB_FETCH_FAIL (+17 more)

### Community 16 - "dashboard/package.json"
Cohesion: 0.05
Nodes (40): @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13, @types/react-dom@19.1.9, typescript@5.9.2, vite@7.1.7 (+32 more)

### Community 17 - "Dashboard — full walkthrough"
Cohesion: 0.07
Nodes (28): summarizeParameter(), 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 11. The writer, 13.1 Sensors and units, 13.3 Migration timeline (+20 more)

### Community 18 - "read-legacy-slave.ts"
Cohesion: 0.14
Nodes (21): ALIGN, Bottle, configurePort(), createLineGate(), Device, formatCell(), formatInt(), formatNumber() (+13 more)

### Community 19 - "Channel"
Cohesion: 0.15
Nodes (22): digitalRead(), millis(), Channel, haveReading, haveRise, level, periodUs, pin (+14 more)

### Community 20 - "network-master: bounded buffers"
Cohesion: 0.11
Nodes (29): 1. Usage, 1a. Diagnostic output, 2. The steps, 3. Helper functions and their firmware counterparts, 4. Differences from the firmware, 5. Testing, 6. Deployment on the legacy Pi, 7. Related documents (+21 more)

### Community 21 - "master.cpp"
Cohesion: 0.05
Nodes (64): 10. Known Gaps, 1. Overview, 2. Pin Assignment, 3. Boot Defaults, 4.1 Trip thresholds, 4.2 Asymmetric Fast-Trip / Leaky-Bucket Recovery Debouncing, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing (+56 more)

### Community 22 - "compilerOptions"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 23 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 24 - "serial_worker"
Cohesion: 0.10
Nodes (24): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Runtime, Three copies exist, What it does, 4. Backend — `web_control.py`, Background workers, `POST /control` Actions (+16 more)

### Community 25 - "test_methane_calibration.py"
Cohesion: 0.23
Nodes (4): braced(), declarations(), MethaneCalibrationRegression, native_source()

### Community 26 - "screen_data.cpp"
Cohesion: 0.15
Nodes (24): 1.1 Router (`ui_shell.cpp`, `s_route`), 2.1 Styling, 2.4 Experiment number keeps resetting on `/control`, mk_equipment(), mk_stepper_row(), screen_control_hide(), screen_control_show(), make_tappable() (+16 more)

### Community 27 - "screen_data.cpp"
Cohesion: 0.23
Nodes (13): bottle_refresh(), bottle_state(), chart_refresh(), cluster_refresh(), Page, P_BOTTLE, P_CLUSTER, P_NONE (+5 more)

### Community 28 - "handle_line"
Cohesion: 0.25
Nodes (13): link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), loop(), met_tick(), handle_line(), parse_onoff(), print_help() (+5 more)

### Community 29 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 30 - "hyperframes/capture/capture.ts"
Cohesion: 0.10
Nodes (23): addSeries(), begin(), ch4Points, chartHeads, CLIPS, csvLines, csvName, DATA (+15 more)

### Community 31 - "run_shots"
Cohesion: 0.20
Nodes (20): Decoded backtrace, 3.3.1.3 Back button, 3.8.1.4 Functions in `ui_shell.cpp` / `ui_shell.h`, 3.4 Back button on a node page goes to Home, on_back_cluster(), on_back_picker(), on_pick_bottle(), on_pick_cluster() (+12 more)

### Community 32 - "experimentReadings.test.ts"
Cohesion: 0.22
Nodes (6): fakeFetch(), parsed, requests, RESERVED, table, value()

### Community 33 - "history.ts"
Cohesion: 0.21
Nodes (13): ParameterStats, ChamberSummary, getExperimentChambers(), groupReadingsByChamber(), HISTORY_PARAMETERS, HistoryPointDetails, parameterValue(), selectChamberReadings() (+5 more)

### Community 34 - "HostSerial"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 35 - "Rules"
Cohesion: 0.14
Nodes (13): Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General, General Requirements (+5 more)

### Community 36 - "sim/Arduino.h"
Cohesion: 0.14
Nodes (3): What is shared vs stubbed, random(), randomSeed()

### Community 37 - "Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)"
Cohesion: 0.09
Nodes (21): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+13 more)

### Community 38 - "db_contract.h"
Cohesion: 0.21
Nodes (16): bottleInRange(), civilFromDays(), daysFromCivil(), findField(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject() (+8 more)

### Community 39 - "data.ts"
Cohesion: 0.08
Nodes (38): csvDisabledReason(), downloadExperimentCsv(), FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), COLUMNS, csvFileName(), downloadCsv() (+30 more)

### Community 40 - "readingCache.ts"
Cohesion: 0.10
Nodes (33): ExperimentSummary, fetchDevice(), fetchExperimentBounds(), fetchExperimentIds(), fetchExperimentList(), fetchExperimentReadings(), fetchLatestReading(), isConcluded() (+25 more)

### Community 41 - "ui_shell.cpp"
Cohesion: 0.09
Nodes (33): 4. Screen tree / state machine (as built), 1.2 Persistent shell, 3.8.1.1 Removed content, 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative), 3.8.1.3 Clock, 3.8.1.5 Interaction notes, 3.8.1 Redesign plan — top bar, 3.8 Persistent status bar (+25 more)

### Community 42 - "captive-portal-login.sh"
Cohesion: 0.22
Nodes (17): attempt_login(), do_curl(), extract_window_location(), form_action(), log(), log_lines(), log_network(), on_interrupt() (+9 more)

### Community 43 - "flash-factory.ts"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 45 - "Repository Index"
Cohesion: 0.17
Nodes (12): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 3c. Plans and audits, 4. Hardware references, 5. Human documents (+4 more)

### Community 46 - "flask_cli.py"
Cohesion: 0.13
Nodes (7): main(), run(), get_json(), main(), _open(), post_download(), post_json()

### Community 47 - "Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)"
Cohesion: 0.11
Nodes (23): 12. WT32 login page QR, 13. Light theme and the /setup QR, 14. Debug flag `IS_DEBUG`, 15.1 Render cost, 15.2 Grid, 15.3 Colour and text, 15.4 Refresh and input, 15. Layout, colour and refresh rules (+15 more)

### Community 48 - "hyperframes-mobile/capture/capture.ts"
Cohesion: 0.10
Nodes (18): ch4, ch4Points, chartHeads, CLIPS, csvLines, csvName, DATA, FRAMES (+10 more)

### Community 49 - "probeOnline"
Cohesion: 0.15
Nodes (15): The Backlog, 7. Correctness rules for the push (decided), 2. Startup Path (Boot to `ready=1`), 3.1 30-second cooldown on every startup failure, 3.2 Late, blocking NTP sync that resets SNTP on every retry, 3.3 Five all-or-nothing cold-TLS HTTPS calls before `ready=1`, 3.4 Pre-pass delays and redundant portal probes, 3.5 Potential socket-timeout stall on chunked responses (+7 more)

### Community 50 - "Reference: `device_data` column detail"
Cohesion: 0.10
Nodes (20): Access control, Chamber numbering, Database schema catalog, Development tooling, `device_data`: active application schema, `device_data.device_config`, `device_data.devices`, `device_data.experiments` (+12 more)

### Community 51 - "analyze.ts"
Cohesion: 0.11
Nodes (22): csv(), { flags, positional }, json, LIMIT, linkIn, md, met, metas (+14 more)

### Community 52 - "compare.ts"
Cohesion: 0.25
Nodes (17): CompareView(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor(), compareSeriesLabel(), distinctExperimentIds() (+9 more)

### Community 53 - "boot-loop.ts"
Cohesion: 0.07
Nodes (26): board, { flags }, header, md, method, onLine(), open(), outDir (+18 more)

### Community 54 - "Methane calibration report generator"
Cohesion: 0.25
Nodes (7): Data: 5 points per sensor, Formulas, How the two simulated points are made, Limits, Methane calibration report generator, Output files, Run

### Community 55 - "BottleSample"
Cohesion: 0.20
Nodes (10): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC (+2 more)

### Community 56 - "PushRow"
Cohesion: 0.20
Nodes (10): PushRow, bottleId, ch4, co2, experimentId, nullMask, ph, pressure (+2 more)

### Community 57 - "LVGL v8 to v9 migration report"
Cohesion: 0.15
Nodes (12): 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, File-by-file summary, LVGL v8 to v9 migration report, Memory and runtime considerations (+4 more)

### Community 58 - "db-contract/main.cpp"
Cohesion: 0.30
Nodes (10): contains(), main(), row(), test_ack_and_errors(), test_array_iteration(), test_bottle_range(), test_envelope(), test_field_parsing() (+2 more)

### Community 59 - "viaDrawer"
Cohesion: 0.20
Nodes (15): addSeries(), begin(), freeze(), log(), navItem(), pick(), rectOf(), rects() (+7 more)

### Community 60 - "polling_worker"
Cohesion: 0.17
Nodes (10): Global state, `polling_worker` (daemon thread), control_api(), desired_state_commands(), polling_worker(), state_mismatch_commands(), stir_command(), threshold_command() (+2 more)

### Community 61 - "screen_setup_poll"
Cohesion: 0.09
Nodes (27): 11. Setup route, Widget lifetime rule, 3.13 Phone-associated popup, 3.15 SoftAP lifecycle, 3.1 Route `/setup`, 3.7.1.1 Removed content, 3.7.1.2 Layout, 3.7.1.3 Gauge colour (+19 more)

### Community 62 - "DbPass"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 64 - "SimESPClass"
Cohesion: 0.13
Nodes (7): Current state of the code, Experiments and results, How to monitor memory from here, /info Reboot: Heap-Walk Hang on `display-v9`, Next step (open), Symptom, SimESPClass

### Community 65 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 66 - "verification/capture.ts"
Cohesion: 0.13
Nodes (17): display-v9: RAII ownership, baud, { flags }, label, meta, onLine(), open(), outDir (+9 more)

### Community 67 - "Verification toolkit (report data collection)"
Cohesion: 0.10
Nodes (20): 1. Overview, 2. Metrics builds, 3.1 Display, 3.2 WT32, 3. `MET` line reference, 4. Commands, 5. Host tools, 6.1 Display (checklist §3) (+12 more)

### Community 68 - "Chart"
Cohesion: 0.15
Nodes (30): Chart(), chartAreaPath(), chartGeometry(), chartPath(), CompareChart(), createChartPoints(), createTimeLabels(), createYTicks() (+22 more)

### Community 70 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.11
Nodes (19): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display, 3.3 Gone from the Modbus design (+11 more)

### Community 71 - "metrics.cpp"
Cohesion: 0.10
Nodes (30): 16. Verification metrics build (`display-v9-metrics`), handle_sens(), my_disp_flush(), setup(), BootMark, ms, stage, finish_touch() (+22 more)

### Community 72 - "Endpoint details"
Cohesion: 0.10
Nodes (21): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. Flask REST API reference — `web_control.py`, 13. `GET /files`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip` (+13 more)

### Community 73 - "_close_tunnel"
Cohesion: 0.27
Nodes (5): _arm_tunnel(), _close_tunnel(), _read_until(), _saw(), _write_all()

### Community 75 - "brandTransition.ts"
Cohesion: 0.14
Nodes (28): Box, boxOnto(), BrandPhase, camera(), CameraFrame, cameraShift(), centreOf(), css() (+20 more)

### Community 76 - "2. Figures"
Cohesion: 0.09
Nodes (30): 4.1 pH, 4.4 Pressure (HX711 / HX710B), 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, 1. How to read the diagrams (+22 more)

### Community 77 - "VideoDemoVideo"
Cohesion: 0.12
Nodes (6): Touch, VideoDemoVideo, .BACKGROUND_COLOR, .FPS, .HEIGHT, .WIDTH

### Community 78 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 79 - "WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)"
Cohesion: 0.25
Nodes (7): 1. Environment, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)

### Community 80 - "main"
Cohesion: 0.10
Nodes (21): Display (`firmware/display-v9/`), 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.5 Polling, 3.2.1.6 Integration with other screens, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels), 3.2.1.8 Out of scope for this screen (stays on /control), 3.2.1 Redesign plan — `/home` (+13 more)

### Community 81 - "db-replay.ts"
Cohesion: 0.13
Nodes (15): bottles, call(), exp, { flags }, iso(), lat, latencyN, md (+7 more)

### Community 82 - "Stage"
Cohesion: 0.33
Nodes (6): Stage, STAGE_DONE, STAGE_PROMPT_PASS, STAGE_PROMPT_USER, STAGE_RECON, STAGE_WAIT_IP

### Community 84 - "co2-calibrate/main.cpp"
Cohesion: 0.24
Nodes (10): 2. `co2-calibrate`, Correctness limits, Field correction, How a reading is taken, Plotting a serial capture, loop(), readCo2Ppm(), readCo2Uart() (+2 more)

### Community 85 - "raspi-docs/README.md"
Cohesion: 0.12
Nodes (3): Raspberry Pi kiosk UI CSS redesign, gen-compile-db.sh script, install-leaf.sh script

### Community 86 - "my_touchpad_read"
Cohesion: 0.14
Nodes (6): 2. Hardware / platform notes, my_touchpad_read(), touch(), touch_has_signal(), touch_released(), touch_touched()

### Community 87 - "sens_report_round"
Cohesion: 0.12
Nodes (21): display-v9: DB fields from the `DB` line, Memory-safety hardening (`display-v9`, `network-master`), Verification, 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, 5.1 As built (+13 more)

### Community 88 - "authenticate"
Cohesion: 0.07
Nodes (31): authenticate(), 12.1 The experiment list is bounded by the monitoring fetch's row cap (fixed), 12.2 The chart heading says "Across experiments" (fixed), 12.4 Stale comments and docs, 12.5 Security posture, 12.6 Functional gaps, 12. Findings, Link (`rusim-line` v1) (+23 more)

### Community 89 - "network-master/main.cpp"
Cohesion: 0.10
Nodes (40): 7.2 Link plumbing, arg_lookup(), clearCreds(), copyFormArg(), dbApplyRunState(), handle_experiment(), handle_line(), handleLogin() (+32 more)

### Community 90 - "Network master ↔ database communication plan"
Cohesion: 0.12
Nodes (16): 1. Recommendation and evidence, 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions (+8 more)

### Community 91 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.14
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 92 - "doLogin"
Cohesion: 0.17
Nodes (16): 1. Environment, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential handling, 3. Line protocol, 4.1 Wi-Fi fallback, 4. State machine, 5. Portal login (`doLogin()`) (+8 more)

### Community 95 - "Pressure calibration plot"
Cohesion: 0.22
Nodes (7): 1. Files, 2. Run it, 3. How it works, 4. Stable windows and their averages, 5. What the current log shows, 6. Limits, Pressure calibration plot

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
Cohesion: 0.29
Nodes (6): 9. Network & services, Enabled services (selected), Listening ports, Network Architecture (LAN Primary), Security note (see §10), disable_wifi_client_and_stop_hotspot()

### Community 100 - "Stack"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 101 - "CALIBRATION NOTES"
Cohesion: 0.14
Nodes (12): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+4 more)

### Community 102 - "hyperframes-mobile/capture/package.json"
Cohesion: 0.25
Nodes (8): dependencies, playwright-core@1.65.1, playwright-core, name, private, scripts, capture, type

### Community 103 - "Display V9 PC Simulator (`[env:display-sim]`)"
Cohesion: 0.40
Nodes (5): Configuration, Display V9 PC Simulator (`[env:display-sim]`), Run it, Scope note, Screenshots (headless)

### Community 104 - "12. Copies & git state"
Cohesion: 0.22
Nodes (9): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, UI, `web_control.py` (+1 more)

### Community 105 - "dashboard-load.ts"
Cohesion: 0.12
Nodes (15): durationMs, exp, { flags }, get(), kinds, md, outDir, REPO (+7 more)

### Community 106 - "fidelity.ts"
Cohesion: 0.09
Nodes (17): Cmp, detail, Exp, expected, FIELDS, { flags }, md, outDir (+9 more)

### Community 107 - "Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)"
Cohesion: 0.40
Nodes (4): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)

### Community 108 - "Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)"
Cohesion: 0.29
Nodes (7): 1. Purpose, 2. Wire protocol, 3. How it opens the port, 4. REPL behaviour, 5. Usage, 6. Limits, Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)

### Community 109 - "_fetch_http_date"
Cohesion: 0.20
Nodes (8): `clock_sync_worker` (daemon thread), clock_sync_worker(), _extract_portal_redirect_url(), _fetch_http_date(), _get_default_gateway_ip(), _NoRedirectHandler, _parse_http_date_header(), sync_clock_from_lan_http()

### Community 110 - "1. Hardware & OS"
Cohesion: 0.33
Nodes (6): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account

### Community 111 - "PlatformIO build environments (`platformio.ini`)"
Cohesion: 0.22
Nodes (8): 1. Layout, 2. Shared settings, 3. Environment table, 4. The ESP32-S3 display environments, 5. Commands, 6. History of this configuration, 7. Related documents, PlatformIO build environments (`platformio.ini`)

### Community 112 - "ui_hold_button.cpp"
Cohesion: 0.11
Nodes (23): 3.2.1.4 Functions to add / change, 3.6.1.3 Functions, hold_commit(), hold_tick(), HoldBtnCtx, btn, completed, halo (+15 more)

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

### Community 118 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.18
Nodes (11): 1. What the program does, 2. Build environment, 3. Pin map, 4.2 Methane (TGS2611), 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing, 6. Responsiveness (+3 more)

### Community 119 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 120 - "BeatScene"
Cohesion: 0.13
Nodes (12): Beat, BEATS, BeatScene, Gesture, Hold, None, Tap, Kind (+4 more)

### Community 121 - "hyperframes/hyperframes.json"
Cohesion: 0.20
Nodes (9): authoringSkill, media, autoProxy, paths, assets, blocks, components, registry (+1 more)

### Community 122 - "delay"
Cohesion: 0.20
Nodes (14): Sampling, delay(), loop(), readRs(), rsToPpm(), rsToPpmRaw(), setup(), loop() (+6 more)

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

### Community 137 - "millis"
Cohesion: 0.20
Nodes (19): Auth used, Build, DB experiment sync + NTP (change record), Master (`firmware/network-master/main.cpp`), 6.1 Database path (configuration, runs and uploads), 1. Symptom and Display Gate, millis(), dbFaultName() (+11 more)

### Community 138 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses

### Community 139 - ".new"
Cohesion: 0.21
Nodes (8): camera_axis(), camera_for(), Cue, cues(), kf(), pan_for(), spot_for(), touch_times()

### Community 140 - "compile_commands.json and gen-compile-db.sh"
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
Cohesion: 0.22
Nodes (9): Build, Commands, Display Demo Video (`video-demo/`), How the video works, Layout, Limitations, Refreshing the snapshots, Sound (+1 more)

### Community 148 - "Sensor calibration firmware"
Cohesion: 0.20
Nodes (10): 1. Common structure, 3. `pressure-calibrate`, 4. `methane-calibrate`, 5. `temp-probe`, 7. Related documents, Local validation, Reference-data refit, Sensor calibration firmware (+2 more)

### Community 149 - "TouchResult"
Cohesion: 0.17
Nodes (12): TouchResult, down, from, invUs, latUs, nf, renderUs, stuck (+4 more)

### Community 150 - "ui_exp_status_text"
Cohesion: 0.16
Nodes (18): link_serial_db_ready(), link_serial_get_db_active_dur(), link_serial_get_db_active_exp(), link_serial_get_db_active_src(), link_serial_get_db_cfg_max(), link_serial_get_db_latest(), link_serial_get_db_pending_dur(), link_serial_get_db_pending_exp() (+10 more)

### Community 151 - "analogRead"
Cohesion: 0.25
Nodes (10): 6. `ph-calibrate`, Correctness limits, Linear conversion (temporary), Sampling and output, Wiring, analogRead(), convertCountToPh(), loop() (+2 more)

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

### Community 156 - "screen_home.cpp"
Cohesion: 0.33
Nodes (10): bottles_online(), elapsed_hours(), nodes_online(), on_run_commit(), render_run(), render_status(), screen_home_hide(), screen_home_poll() (+2 more)

### Community 157 - "SensResult"
Cohesion: 0.33
Nodes (6): SensResult, applyUs, localSeq, noframe, renderUs, wseq

### Community 159 - "DbExperiment"
Cohesion: 0.33
Nodes (6): DbExperiment, durationH, id, sampleIntervalS, setpoints, dbFindExperiment()

### Community 160 - "Documentation review — all of `docs/`"
Cohesion: 0.40
Nodes (4): 2. Checked and left alone, 3. Open items, 4. What a reader should take from this, Documentation review — all of `docs/`

### Community 161 - "5. Frontend UI — `templates/index.html`"
Cohesion: 0.40
Nodes (5): 5. Frontend UI — `templates/index.html`, Dormant Clean User Template (`templates/Clean User Version.html`), Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer, Tabs, Temporary MOSFET Speed Control (`0–255` Debit Testing)

### Community 164 - "DbFault"
Cohesion: 0.25
Nodes (8): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA

### Community 165 - "Easing"
Cohesion: 0.40
Nodes (4): EASE_IN, EASE_IN_OUT, EASE_OUT, LINEAR

### Community 166 - "Storylines: RuSim dashboard demo"
Cohesion: 0.40
Nodes (4): A. One chamber, start to finish (recommended), B. Four questions at the bench, C. The silent chamber (the bold one), Storylines: RuSim dashboard demo

### Community 167 - "delay"
Cohesion: 0.29
Nodes (5): advance_ms(), delay(), loop(), setup(), loop()

### Community 168 - "Dashboard Demo Videos (`video-demo/hyperframes/`, `video-demo/hyperframes-mobile/`)"
Cohesion: 0.13
Nodes (9): Dashboard Demo Videos (`video-demo/hyperframes/`, `video-demo/hyperframes-mobile/`), Files and Git, How the film is built, How the screens are recorded, Layout, Mobile version (`video-demo/hyperframes-mobile/`), Running it, What is on screen (+1 more)

### Community 170 - "Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)"
Cohesion: 0.25
Nodes (8): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Keep-alive, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)

### Community 172 - "Migration plan: ESP pulls telemetry from the legacy Mega slave"
Cohesion: 0.25
Nodes (8): 1. Where the data comes from today, 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour, 6. Deferred, with the hook each needs, 7. Verification, 8. Open risks, Migration plan: ESP pulls telemetry from the legacy Mega slave

### Community 175 - "Stage"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 176 - "10. Security & caveats"
Cohesion: 0.50
Nodes (4): 10. Security & caveats, Operational caveats, Recommendations, Security

## Knowledge Gaps
- **170 isolated node(s):** `name`, `cloudflareChallenge`, `formAction`, `hasForm`, `hasUsernameField` (+165 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1400 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **19 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `millis` to `test_display_sensors.cpp`, `link_serial.cpp`, `wt32-eth01_captive-recon/main.cpp`, `display-v9/main.cpp`, `millis`, `temp-probe/main.cpp`, `master.cpp`, `ui_exp_status_text`, `analogRead`, `screen_data.cpp`, `screen_home.cpp`, `handle_line`, `run_shots`, `sim/Arduino.h`, `Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)`, `screen_setup_poll`, `metrics.cpp`, `2. Figures`, `co2-calibrate/main.cpp`, `sens_report_round`, `network-master/main.cpp`, `delay`?**
  _High betweenness centrality (0.107) - this node is a cross-community bridge._
- **Are the 72 inferred relationships involving `millis()` (e.g. with `loop()` and `readCo2Uart()`) actually correct?**
  _`millis()` has 72 INFERRED edges - model-reasoned connections that need verification._
- **What connects `name`, `cloudflareChallenge`, `formAction` to the rest of the system?**
  _170 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `test_display_sensors.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.05049088359046283 - nodes in this community are weakly interconnected._
- **Why does `ESP32-8048S050 — LLM Generated Summary` connect `ESP32-8048S050 — LLM Generated Summary` to `14. Boot, Flashing, and Debugging`, `16. Performance and Stability Checklist`, `13. ESPHome Notes`, `15. Filesystem and Asset Storage`, `4. Display Subsystem`, `5. Touch Variants`, `10. ESP-IDF Recommendations`, `6. Common Pin Map Reference`, `11. LVGL Recommendations`, `12. Arduino / PlatformIO Notes`?**
  _High betweenness centrality (0.033) - this node is a cross-community bridge._
- **Are the 31 inferred relationships involving `delay()` (e.g. with `loop()` and `setup()`) actually correct?**
  _`delay()` has 31 INFERRED edges - model-reasoned connections that need verification._
- **Should `link_serial.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.08748615725359911 - nodes in this community are weakly interconnected._