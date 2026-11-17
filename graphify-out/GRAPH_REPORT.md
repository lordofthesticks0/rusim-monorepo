# Graph Report - rusim-monorepo  (2026-11-18)

## Corpus Check
- 80 files · ~475,642 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1366 nodes · 2648 edges · 86 communities (68 shown, 18 thin omitted)
- Extraction: 92% EXTRACTED · 8% INFERRED · 0% AMBIGUOUS · INFERRED: 221 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Display Fake Data Harness
- Display Link Serial Protocol
- Flask Server Imports
- Dashboard Chart UI
- Hardware Reference Docs
- Legacy Pi Monitor CLI
- Network Master Passthrough
- Display Link Stub
- Network Master Config Portal
- Dev Database Seeder
- Dashboard Auth Session
- WT32 Captive Portal Recon
- Display Hold Button
- Legacy Master USB CLI
- Display V9 Screen Headers
- Display DB Fetch State Machine
- Dashboard Package Manifest
- Legacy Slave Reader CLI
- Display UI Shell
- PWM Pin Readout Sketch
- Captive Portal Login Flow
- Master Heater and Pump Control
- Dashboard TypeScript Config
- Display Control Screen
- Pi Water Control API
- Firmware Test Runner
- Display Data Screen
- Display V9 Main Loop
- Slave Sensor Firmware
- Supabase Client Setup
- Methane Calibration Sketch
- Dashboard Chamber Monitoring
- Dashboard CSV Export
- Dashboard History Data
- Host Serial Buffer
- Agent Rules Document
- Arduino Sim Layer
- Display Home Screen
- DB Contract Envelope
- Dashboard Compare View
- Display Serial Passthrough
- Display Touch Driver
- Captive Portal Login Script
- Factory Flash Tool
- Sensor Sketch Arduino Shims
- Documentation Index
- Display Navigation and Modals
- Legacy Flask CSV CLI
- Memory Safety Skill
- Display DB Fault Codes
- Repository README
- Pi Sync Script
- Temperature Probe Sketch
- Human-Owned Doc Links
- Calibration Notes Document
- Sample Schema
- Push Row Schema
- Display Sensor Reporting
- DB Contract Host Tests
- Pi Clock Sync Worker
- Memory Safety Patterns
- Display Info Screen
- DB Pass States
- CSV Download Handler
- Simulated ESP Heap
- MOSFET PWM Channel
- CO2 Calibration Sketch
- Simulated Serial
- DB Experiment Lookup
- Host ESP Heap Shim
- Motor Driver Sketch
- Pi CSV Logging
- React Dependencies
- WiFi Provisioning Stages
- DB Method Enum
- System Mode Enum
- Project Backlog
- ETH Event Callback

## God Nodes (most connected - your core abstractions)
1. `millis()` - 37 edges
2. `logf()` - 26 edges
3. `delay()` - 24 edges
4. `ui_update_bell()` - 20 edges
5. `compilerOptions` - 20 edges
6. `render_current()` - 19 edges
7. `HoldBtnCtx` - 17 edges
8. `dbPassTick()` - 17 edges
9. `HostSerial` - 16 edges
10. `loop()` - 16 edges

## Surprising Connections (you probably didn't know these)
- `3. Dashboard` --references--> `useAuth()`  [INFERRED]
  docs/index.md → dashboard/src/auth.ts
- `driveHeater()` --calls--> `pinMode()`  [INFERRED]
  legacy/master/master.cpp → firmware/display-v9/sim/Arduino.h
- `driveMosfetPwm()` --calls--> `pinMode()`  [INFERRED]
  legacy/master/master.cpp → firmware/display-v9/sim/Arduino.h
- `setup()` --calls--> `pinMode()`  [INFERRED]
  legacy/master/master.cpp → firmware/display-v9/sim/Arduino.h
- `setup()` --calls--> `pinMode()`  [INFERRED]
  legacy/slave/new-slave.cpp → firmware/display-v9/sim/Arduino.h

## Import Cycles
- None detected.

## Communities (86 total, 18 thin omitted)

### Community 0 - "Display Fake Data Harness"
Cohesion: 0.11
Nodes (41): clear_bottle(), fake_any_null(), fake_init(), fake_null_count(), fake_sensor_text(), fake_sensor_value_text(), sens_apply_real(), sens_real_active() (+33 more)

### Community 1 - "Display Link Serial Protocol"
Cohesion: 0.08
Nodes (29): arg_lookup(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line(), handle_log(), handle_login_req() (+21 more)

### Community 2 - "Flask Server Imports"
Cohesion: 0.08
Nodes (23): command(), disable_wifi_client_and_stop_hotspot(), download_info(), export_data(), get_clock_payload(), get_drives(), get_interface_ip(), get_ip() (+15 more)

### Community 3 - "Dashboard Chart UI"
Cohesion: 0.11
Nodes (33): App(), BottleChart(), BrandMark(), Chart(), ChartInteraction, chartPath(), CompareActivePoint, CompareChart() (+25 more)

### Community 4 - "Hardware Reference Docs"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 5 - "Legacy Pi Monitor CLI"
Cohesion: 0.10
Nodes (36): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_IDS, BOTTLE_WIDTHS, cell(), clock(), fetchRaw() (+28 more)

### Community 6 - "Network Master Passthrough"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 8 - "Network Master Config Portal"
Cohesion: 0.12
Nodes (26): arg_lookup(), clearCreds(), copyFormArg(), dbPassFinish(), dbPassForce(), handle_experiment(), handle_line(), handleLogin() (+18 more)

### Community 9 - "Dev Database Seeder"
Cohesion: 0.10
Nodes (26): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+18 more)

### Community 10 - "Dashboard Auth Session"
Cohesion: 0.11
Nodes (26): AuthState, readStoredSession(), StoredSession, useAuth(), writeStoredSession(), authenticate(), CONCLUDED_AFTER_MS, fetchDevice() (+18 more)

### Community 11 - "WT32 Captive Portal Recon"
Cohesion: 0.11
Nodes (25): extractWindowLocation(), loop(), onEthEvent(), PageScan, cloudflareChallenge, formAction, hasForm, hasUsernameField (+17 more)

### Community 12 - "Display Hold Button"
Cohesion: 0.10
Nodes (23): on_run_commit(), hold_commit(), hold_tick(), HoldBtnCtx, btn, completed, halo, label (+15 more)

### Community 13 - "Legacy Master USB CLI"
Cohesion: 0.11
Nodes (25): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_WIDTHS, clock(), configurePort(), createLineGate(), Device (+17 more)

### Community 14 - "Display V9 Screen Headers"
Cohesion: 0.11
Nodes (12): link_serial_get_ip(), link_serial_take_login_request(), link_serial_take_result(), on_debug_bypass(), paint_address(), screen_setup_poll(), screen_setup_show(), setup_close_success() (+4 more)

### Community 15 - "Display DB Fetch State Machine"
Cohesion: 0.17
Nodes (27): dbArrayIsEmpty(), dbClassify(), DbFetch, DB_FETCH_AUTH, DB_FETCH_FAIL, DB_FETCH_OK, DB_FETCH_PAYLOAD, dbGetJson() (+19 more)

### Community 16 - "Dashboard Package Manifest"
Cohesion: 0.10
Nodes (24): devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13, @types/react-dom@19.1.9, typescript@5.9.2, vite@7.1.7, @vitejs/plugin-react@5.0.4 (+16 more)

### Community 17 - "Legacy Slave Reader CLI"
Cohesion: 0.14
Nodes (21): ALIGN, Bottle, configurePort(), createLineGate(), Device, formatCell(), formatInt(), formatNumber() (+13 more)

### Community 18 - "Display UI Shell"
Cohesion: 0.16
Nodes (18): main(), build_status_bar(), on_bell_clicked(), on_modal_ack(), on_sens_timer(), set_tab_locked(), set_tab_state(), style_plain() (+10 more)

### Community 19 - "PWM Pin Readout Sketch"
Cohesion: 0.15
Nodes (22): digitalRead(), millis(), Channel, haveReading, haveRise, level, periodUs, pin (+14 more)

### Community 20 - "Captive Portal Login Flow"
Cohesion: 0.13
Nodes (21): doLogin(), extractWindowLocation(), fetchLoginPage(), fetchPortalUrl(), PageScan, formAction, hasForm, hasUsernameField (+13 more)

### Community 21 - "Master Heater and Pump Control"
Cohesion: 0.21
Nodes (16): driveHeater(), driveMosfetPwm(), forceAllOff(), handleUsbCommand(), isValidBottleObject(), loop(), pollSlaveSerial(), readAdcAveraged() (+8 more)

### Community 22 - "Dashboard TypeScript Config"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 23 - "Display Control Screen"
Cohesion: 0.25
Nodes (17): db_refresh_warn(), dur_show(), exp_mirror_link(), exp_show(), on_dur_down(), on_dur_up(), on_exp_down(), on_exp_up() (+9 more)

### Community 24 - "Pi Water Control API"
Cohesion: 0.11
Nodes (13): close_serial_conn(), control_api(), desired_state_commands(), polling_worker(), send_serial_cmd(), serial_worker(), set_mode(), state_mismatch_commands() (+5 more)

### Community 25 - "Firmware Test Runner"
Cohesion: 0.15
Nodes (6): main(), run(), braced(), declarations(), MethaneCalibrationRegression, native_source()

### Community 26 - "Display Data Screen"
Cohesion: 0.21
Nodes (16): axis_caption(), build_trend_card(), mk_sensor_tile(), mk_temp_tile(), on_back_bottle(), on_pick_bottle(), on_pick_cluster(), on_pick_sensor() (+8 more)

### Community 27 - "Display V9 Main Loop"
Cohesion: 0.11
Nodes (3): DispBufDeleter, my_disp_flush(), my_touchpad_read()

### Community 28 - "Slave Sensor Firmware"
Cohesion: 0.23
Nodes (16): analogRead(), convertPhVoltage(), fitExponent(), handlePingStream(), initPressureSensor(), loop(), readCo2Ppm(), sendTelemetryJson() (+8 more)

### Community 29 - "Supabase Client Setup"
Cohesion: 0.12
Nodes (16): @supabase/supabase-js@2.57.4, buildClient(), DeviceDataClient, readConfig(), SESSION_HEADER, CompositeTypes, Constants, Database (+8 more)

### Community 30 - "Methane Calibration Sketch"
Cohesion: 0.18
Nodes (11): delay(), loop(), readRs(), rsToPpm(), rsToPpmRaw(), setup(), initializeSensor(), loop() (+3 more)

### Community 31 - "Dashboard Chamber Monitoring"
Cohesion: 0.20
Nodes (13): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), ExperimentSummary, DashboardView, DEV_FORCE_LIVE_EXPERIMENT, generateSyntheticLiveReadings(), isMonitoringPollingEligible() (+5 more)

### Community 32 - "Dashboard CSV Export"
Cohesion: 0.17
Nodes (12): COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv(), withUnit(), getParameterValue() (+4 more)

### Community 33 - "Dashboard History Data"
Cohesion: 0.23
Nodes (13): ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber(), HISTORY_PARAMETERS, HistoryPointDetails, parameterValue(), selectChamberReadings() (+5 more)

### Community 34 - "Host Serial Buffer"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 35 - "Agent Rules Document"
Cohesion: 0.13
Nodes (14): Backlog and Continuity, Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General (+6 more)

### Community 37 - "Display Home Screen"
Cohesion: 0.29
Nodes (12): bottles_online(), home_elapsed_hours(), home_render_run(), home_render_status(), nodes_online(), on_duration_down(), on_duration_up(), on_run_commit() (+4 more)

### Community 38 - "DB Contract Envelope"
Cohesion: 0.25
Nodes (14): civilFromDays(), daysFromCivil(), findField(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject(), parseErrorCode() (+6 more)

### Community 39 - "Dashboard Compare View"
Cohesion: 0.27
Nodes (11): displayChamberNumber(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor(), compareSeriesLabel(), distinctExperimentIds() (+3 more)

### Community 40 - "Display Serial Passthrough"
Cohesion: 0.28
Nodes (11): link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), handle_line(), parse_onoff(), print_help(), print_mem(), print_status() (+3 more)

### Community 42 - "Captive Portal Login Script"
Cohesion: 0.32
Nodes (11): attempt_login(), extract_window_location(), form_action(), log(), probe_once(), prompt_credentials(), resolve_action(), captive-portal-login.sh script (+3 more)

### Community 43 - "Factory Flash Tool"
Cohesion: 0.32
Nodes (12): autodetectPort(), candidatePorts(), Device, flash(), getArg(), hasFlag(), listDevices(), main() (+4 more)

### Community 45 - "Documentation Index"
Cohesion: 0.17
Nodes (12): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 4. Hardware references, 5. Human documents, 5a. Agent lessons (+4 more)

### Community 46 - "Display Navigation and Modals"
Cohesion: 0.26
Nodes (12): screen_control_hide(), on_back_cluster(), on_back_picker(), screen_home_hide(), on_swipe_gesture(), on_tab_clicked(), render_current(), ui_modal_visible() (+4 more)

### Community 47 - "Legacy Flask CSV CLI"
Cohesion: 0.27
Nodes (5): get_json(), main(), _open(), post_download(), post_json()

### Community 48 - "Memory Safety Skill"
Cohesion: 0.18
Nodes (10): 1. Memory Bug Categories, 2. Safety Spectrum, Best Practices, Core Concepts, Debugging Tools, Detailed patterns and worked examples, Do's, Don'ts (+2 more)

### Community 49 - "Display DB Fault Codes"
Cohesion: 0.18
Nodes (11): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA (+3 more)

### Community 50 - "Repository README"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 51 - "Pi Sync Script"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 52 - "Temperature Probe Sketch"
Cohesion: 0.33
Nodes (7): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup()

### Community 53 - "Human-Owned Doc Links"
Cohesion: 0.27
Nodes (4): 3D Models, PCB, gen-compile-db.sh script, install-leaf.sh script

### Community 54 - "Calibration Notes Document"
Cohesion: 0.20
Nodes (10): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+2 more)

### Community 55 - "Sample Schema"
Cohesion: 0.20
Nodes (10): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC (+2 more)

### Community 56 - "Push Row Schema"
Cohesion: 0.20
Nodes (10): PushRow, bottleId, ch4, co2, experimentId, nullMask, ph, pressure (+2 more)

### Community 57 - "Display Sensor Reporting"
Cohesion: 0.24
Nodes (9): BottleReading, link_emit_sens(), pushEnqueueRound(), sens_float_field(), sens_format_value(), sens_log_failure(), sens_poll(), sens_report_round() (+1 more)

### Community 58 - "DB Contract Host Tests"
Cohesion: 0.36
Nodes (8): contains(), main(), test_ack_and_errors(), test_array_iteration(), test_envelope(), test_field_parsing(), test_sanitize(), test_timestamps()

### Community 59 - "Pi Clock Sync Worker"
Cohesion: 0.20
Nodes (7): clock_sync_worker(), _extract_portal_redirect_url(), _fetch_http_date(), _get_default_gateway_ip(), _NoRedirectHandler, _parse_http_date_header(), sync_clock_from_lan_http()

### Community 60 - "Memory Safety Patterns"
Cohesion: 0.22
Nodes (8): memory-safety-patterns — detailed patterns and worked examples, Pattern 1: RAII in C++, Pattern 2: Smart Pointers in C++, Pattern 3: Ownership in Rust, Pattern 4: Safe Resource Management in C, Pattern 5: Bounds Checking, Pattern 6: Preventing Data Races, Patterns by Language

### Community 61 - "Display Info Screen"
Cohesion: 0.44
Nodes (7): gauge_set(), info_render(), mk_gauge(), mk_tile(), screen_info_poll(), screen_info_show(), ui_is_info()

### Community 62 - "DB Pass States"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 65 - "MOSFET PWM Channel"
Cohesion: 0.25
Nodes (8): MosfetChannel, currentPwm, maxPwm, pin, rampOriginMs, rampOriginPwm, state, targetPwm

### Community 66 - "CO2 Calibration Sketch"
Cohesion: 0.43
Nodes (6): loop(), readCo2Ppm(), readCo2Uart(), setup(), waitForLevel(), pinMode()

### Community 68 - "DB Experiment Lookup"
Cohesion: 0.29
Nodes (7): dbApplyRunState(), DbExperiment, durationH, id, sampleIntervalS, setpoints, dbFindExperiment()

### Community 70 - "Motor Driver Sketch"
Cohesion: 0.60
Nodes (5): digitalWrite(), loop(), rampUp(), setup(), stopMotor()

### Community 71 - "Pi CSV Logging"
Cohesion: 0.40
Nodes (6): fmt_val(), format_aligned_row(), get_todays_filename(), log_data_to_csv(), toggle_logging(), write_headers_if_new()

### Community 72 - "React Dependencies"
Cohesion: 0.40
Nodes (5): dependencies, react@19.1.1, react-dom@19.1.1, react, react-dom

### Community 73 - "WiFi Provisioning Stages"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 75 - "DB Method Enum"
Cohesion: 0.67
Nodes (3): DbMethod, DB_GET, DB_POST

### Community 76 - "System Mode Enum"
Cohesion: 0.67
Nodes (3): SystemMode, MODE_AUTO, MODE_OFF

## Knowledge Gaps
- **98 isolated node(s):** `cloudflareChallenge`, `formAction`, `hasForm`, `hasUsernameField`, `magic` (+93 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 495 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **18 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `Display Link Serial Protocol` to `CO2 Calibration Sketch`, `Arduino Sim Layer`, `Display Home Screen`, `Display Serial Passthrough`, `WT32 Captive Portal Recon`, `Display V9 Screen Headers`, `Temperature Probe Sketch`, `Master Heater and Pump Control`, `Slave Sensor Firmware`, `Display Info Screen`, `Methane Calibration Sketch`?**
  _High betweenness centrality (0.094) - this node is a cross-community bridge._
- **Are the 36 inferred relationships involving `millis()` (e.g. with `loop()` and `readCo2Uart()`) actually correct?**
  _`millis()` has 36 INFERRED edges - model-reasoned connections that need verification._
- **What connects `cloudflareChallenge`, `formAction`, `hasForm` to the rest of the system?**
  _98 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Display Fake Data Harness` be split into smaller, more focused modules?**
  _Cohesion score 0.10844155844155844 - nodes in this community are weakly interconnected._
- **Why does `Electronics` connect `Hardware Reference Docs` to `Human-Owned Doc Links`?**
  _High betweenness centrality (0.064) - this node is a cross-community bridge._
- **Are the 23 inferred relationships involving `delay()` (e.g. with `loop()` and `setup()`) actually correct?**
  _`delay()` has 23 INFERRED edges - model-reasoned connections that need verification._
- **Should `Display Link Serial Protocol` be split into smaller, more focused modules?**
  _Cohesion score 0.08350951374207188 - nodes in this community are weakly interconnected._