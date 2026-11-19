# Graph Report - rusim-monorepo  (2026-11-19)

## Corpus Check
- 168 files · ~477,086 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 13 file(s) not represented in the graph (top: (none) 5, .toml 2, .example 1)

## Summary
- 2168 nodes · 3872 edges · 135 communities (117 shown, 18 thin omitted)
- Extraction: 85% EXTRACTED · 15% INFERRED · 0% AMBIGUOUS · INFERRED: 588 edges (avg confidence: 0.91)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `197ef52a`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

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
- wt32-eth01_captive-recon/main.cpp
- screen_control.cpp
- Legacy Master USB CLI
- Display V9 Screen Headers
- Display DB Fetch State Machine
- Dashboard Package Manifest
- Dashboard — full walkthrough
- ui_shell.cpp
- PWM Pin Readout Sketch
- Captive Portal Login Flow
- master.cpp
- Dashboard TypeScript Config
- 5. Checklist
- Pi Water Control API
- Firmware Test Runner
- ui_mk_button
- display-v9/main.cpp
- Slave Sensor Firmware
- ATmega2560 Mega Mini — Development & Reference Documentation
- 4. `methane-calibrate`
- Dashboard Chamber Monitoring
- Dashboard CSV Export
- Dashboard History Data
- Host Serial Buffer
- Agent Rules Document
- sim/Arduino.h
- Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)
- DB Contract Envelope
- Dashboard Compare View
- Display Serial Passthrough
- Display Touch Driver
- Captive Portal Login Script
- Factory Flash Tool
- Sensor Sketch Arduino Shims
- Documentation Index
- screen_data.cpp
- serial_worker
- Memory Safety Skill
- Display DB Fault Codes
- Repository README
- Pi Sync Script
- Temperature Probe Sketch
- index.md
- Calibration Notes Document
- Sample Schema
- Push Row Schema
- sens_report_round
- DB Contract Host Tests
- Pi Clock Sync Worker
- Memory Safety Patterns
- 3.7.1 Redesign plan — `/info`
- DB Pass States
- CSV Download Handler
- Simulated ESP Heap
- ESP32-8048S050C · Board & Archive Guide
- Sensor calibration firmware
- Simulated Serial
- DB Experiment Lookup
- Host ESP Heap Shim
- Display <-> WT32 link: `rusim-line` v1
- log_data_to_csv
- Endpoint details
- WiFi Provisioning Stages
- DB Method Enum
- Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)
- Project Backlog
- WT32-ETH01 — LLM Documentation
- LVGL v8 to v9 migration report
- createChartPoints
- ESP to database sync — plan
- PlatformIO build environments (`platformio.ini`)
- Network master ↔ database communication plan
- ESP32-8048S050 — LLM Generated Summary
- network-master (`firmware/network-master`) — production WT32 firmware
- analogRead
- network-master-database-plan.md
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- Dashboard public-deploy security report
- Database schema catalog
- 2. Cross-cutting
- 9. Network & services
- WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)
- Migration plan: ESP pulls telemetry from the legacy Mega slave
- Reference: `device_data` column detail
- 12. Copies & git state
- PageScan
- compile_commands.json and gen-compile-db.sh
- learned-mistakes.md
- status
- Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)
- Raspberry Pi `water-control-system` mirror
- Display `IS_DEBUG` flag
- /info Reboot: Heap-Walk Hang on `display-v9`
- 3.3.1 Redesign plan — `/data` node picker
- 1. Hardware & OS
- 2. Boot & kiosk session model
- `leaf` terminal reader on the Pi
- 10. ESP-IDF Recommendations
- 6. Common Pin Map Reference
- Stage
- Display LVGL 9.3.0 -> 9.6.0 port
- Kiosk text scale
- Documentation review — all of `docs/`
- display-provisioning-ap.md
- 5. Frontend UI — `templates/index.html`
- 8. Auxiliary tools & scripts
- 10. Security & caveats
- 11. LVGL Recommendations
- 12. Arduino / PlatformIO Notes
- 14. Boot, Flashing, and Debugging
- 16. Performance and Stability Checklist
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants
- onEthEvent

## God Nodes (most connected - your core abstractions)
1. `millis()` - 38 edges
2. `delay()` - 27 edges
3. `logf()` - 26 edges
4. `ui_update_bell()` - 23 edges
5. `render_current()` - 23 edges
6. `ESP32-8048S050 — LLM Generated Summary` - 21 edges
7. `Endpoint details` - 21 edges
8. `dbPassTick()` - 21 edges
9. `compilerOptions` - 20 edges
10. `sens_apply_real()` - 19 edges

## Surprising Connections (you probably didn't know these)
- `6.1 Scaling` --references--> `chartPath()`  [INFERRED]
  docs/dashboard.md → dashboard/src/App.tsx
- `13.1 Sensors and units` --references--> `getParameterValue()`  [INFERRED]
  docs/dashboard.md → dashboard/src/data.ts
- `Link (`rusim-line` v1)` --references--> `authenticate()`  [INFERRED]
  docs/db-sync-experiment-ntp.md → dashboard/src/data.ts
- `Current effects` --references--> `setup_close_success()`  [INFERRED]
  docs/display-debug-flag.md → firmware/display-v9/screen_setup.cpp
- `13. Light theme and the /setup QR` --references--> `ui_mk_button()`  [INFERRED]
  docs/display-docs.md → firmware/display-v9/ui_shell.cpp

## Import Cycles
- None detected.

## Communities (135 total, 18 thin omitted)

### Community 0 - "Display Fake Data Harness"
Cohesion: 0.11
Nodes (43): 3. File map, 2.4 Real vs fake labelling, clear_bottle(), fake_any_null(), fake_init(), fake_null_count(), fake_sensor_text(), fake_sensor_value_text() (+35 more)

### Community 1 - "Display Link Serial Protocol"
Cohesion: 0.08
Nodes (29): arg_lookup(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line(), handle_log(), handle_login_req() (+21 more)

### Community 2 - "Flask Server Imports"
Cohesion: 0.10
Nodes (18): download_info(), export_data(), get_clock_payload(), get_drives(), get_interface_ip(), get_ip(), get_lan_ip(), get_timezones() (+10 more)

### Community 3 - "Dashboard Chart UI"
Cohesion: 0.10
Nodes (35): dependencies, react@19.1.1, react-dom@19.1.1, App(), BottleChart(), BrandMark(), Chart(), ChartInteraction (+27 more)

### Community 4 - "Hardware Reference Docs"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 5 - "Legacy Pi Monitor CLI"
Cohesion: 0.07
Nodes (45): 1. Usage, 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents (+37 more)

### Community 6 - "Network Master Passthrough"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 8 - "Network Master Config Portal"
Cohesion: 0.11
Nodes (30): 7.2 Link plumbing, arg_lookup(), clearCreds(), copyFormArg(), dbApplyRunState(), dbFaultName(), dbFindExperiment(), dbPassFinish() (+22 more)

### Community 9 - "Dev Database Seeder"
Cohesion: 0.06
Nodes (47): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+39 more)

### Community 10 - "Dashboard Auth Session"
Cohesion: 0.10
Nodes (30): AuthState, readStoredSession(), StoredSession, useAuth(), writeStoredSession(), authenticate(), CONCLUDED_AFTER_MS, fetchDevice() (+22 more)

### Community 11 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.33
Nodes (8): extractWindowLocation(), loop(), probeVerify(), readBody(), readLine(), runRecon(), scanPage(), valueAfter()

### Community 12 - "screen_control.cpp"
Cohesion: 0.05
Nodes (73): Display (`firmware/display-v9/`), Widget lifetime rule, 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.3 State and data model changes needed, 3.2.1.4 Functions to add / change, 3.2.1.5 Polling, 3.2.1.6 Integration with other screens (+65 more)

### Community 13 - "Legacy Master USB CLI"
Cohesion: 0.11
Nodes (25): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_WIDTHS, clock(), configurePort(), createLineGate(), Device (+17 more)

### Community 14 - "Display V9 Screen Headers"
Cohesion: 0.10
Nodes (25): 3.13 Phone-associated popup, 3.14 Backlight sleep, 3.15 SoftAP lifecycle, 3.16 Debug-only UI, 3.1 Route `/setup`, 3.8.1.1 Removed content, 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative), 3.8.1.3 Clock (+17 more)

### Community 15 - "Display DB Fetch State Machine"
Cohesion: 0.11
Nodes (40): Auth used, Build, DB experiment sync + NTP (change record), Link (`rusim-line` v1), Master (`firmware/network-master/main.cpp`), 6.1 Database path (configuration, runs and uploads), 6. Memory and limits, 5. Content audit, 2026-11-18 (+32 more)

### Community 16 - "Dashboard Package Manifest"
Cohesion: 0.05
Nodes (40): @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13, @types/react-dom@19.1.9, typescript@5.9.2, vite@7.1.7 (+32 more)

### Community 17 - "Dashboard — full walkthrough"
Cohesion: 0.06
Nodes (35): summarizeParameter(), 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 1.1 File map, 1.2 What it is not, 1. What it is (+27 more)

### Community 18 - "ui_shell.cpp"
Cohesion: 0.14
Nodes (30): 4. Screen tree / state machine (as built), Decoded backtrace, 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 3.12 Bell and invalid-field list, 3.3.1.3 Back button, 3.8.1.4 Functions in `ui_shell.cpp` / `ui_shell.h` (+22 more)

### Community 19 - "PWM Pin Readout Sketch"
Cohesion: 0.16
Nodes (21): millis(), Channel, haveReading, haveRise, level, periodUs, pin, readHighUs (+13 more)

### Community 20 - "Captive Portal Login Flow"
Cohesion: 0.10
Nodes (35): 1. Usage, 2. The five steps, 3. Helper functions and their firmware counterparts, 4. Differences from the firmware, 5. Testing, 6. Related documents, Captive portal login from Linux (bash), network-master: bounded buffers (+27 more)

### Community 21 - "master.cpp"
Cohesion: 0.05
Nodes (64): 10. Known Gaps, 1. Overview, 2. Pin Assignment, 3. Boot Defaults, 4.1 Trip thresholds, 4.2 Asymmetric Fast-Trip / Slow-Recovery Debouncing, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing (+56 more)

### Community 22 - "Dashboard TypeScript Config"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 23 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 24 - "Pi Water Control API"
Cohesion: 0.15
Nodes (12): Global state, `polling_worker` (daemon thread), control_api(), desired_state_commands(), polling_worker(), send_serial_cmd(), state_mismatch_commands(), stir_command() (+4 more)

### Community 25 - "Firmware Test Runner"
Cohesion: 0.10
Nodes (11): main(), run(), braced(), declarations(), MethaneCalibrationRegression, native_source(), get_json(), main() (+3 more)

### Community 26 - "ui_mk_button"
Cohesion: 0.13
Nodes (27): 1.1 Router (`ui_shell.cpp`, `s_route`), 2.1 Styling, 3.4.1.1 Removed content, 3.4.1.2 Layout (content coordinates, panel 800x436), 3.4.1.3 Tile contents, 3.4.1.4 Behaviour kept, 3.4.1 Redesign plan — `/data/X` cluster, 3.4 Route `/data/X` — cluster (+19 more)

### Community 27 - "display-v9/main.cpp"
Cohesion: 0.09
Nodes (3): display-v9: RAII ownership, DispBufDeleter, my_disp_flush()

### Community 28 - "Slave Sensor Firmware"
Cohesion: 0.18
Nodes (20): 4.4 Pressure (HX711 / HX710B), 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, fitExponent(), handlePingStream() (+12 more)

### Community 29 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 30 - "4. `methane-calibrate`"
Cohesion: 0.23
Nodes (11): 4. `methane-calibrate`, Local validation, Reference-data refit, Sampling, Two calibration layers, Wiring and constants, loop(), readRs() (+3 more)

### Community 31 - "Dashboard Chamber Monitoring"
Cohesion: 0.20
Nodes (13): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), ExperimentSummary, DashboardView, DEV_FORCE_LIVE_EXPERIMENT, generateSyntheticLiveReadings(), isMonitoringPollingEligible() (+5 more)

### Community 32 - "Dashboard CSV Export"
Cohesion: 0.19
Nodes (12): csvDisabledReason(), displayChamberNumber(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv() (+4 more)

### Community 33 - "Dashboard History Data"
Cohesion: 0.17
Nodes (17): ParameterName, ParameterStats, ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber(), HISTORY_PARAMETERS, HistoryPointDetails (+9 more)

### Community 34 - "Host Serial Buffer"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 35 - "Agent Rules Document"
Cohesion: 0.13
Nodes (14): Backlog and Continuity, Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General (+6 more)

### Community 36 - "sim/Arduino.h"
Cohesion: 0.11
Nodes (12): Configuration, Display V9 PC Simulator (`[env:display-sim]`), Run it, Scope note, What is shared vs stubbed, digitalWrite(), random(), randomSeed() (+4 more)

### Community 37 - "Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)"
Cohesion: 0.09
Nodes (21): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+13 more)

### Community 38 - "DB Contract Envelope"
Cohesion: 0.25
Nodes (14): civilFromDays(), daysFromCivil(), findField(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject(), parseErrorCode() (+6 more)

### Community 39 - "Dashboard Compare View"
Cohesion: 0.20
Nodes (14): addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor(), compareSeriesLabel(), distinctExperimentIds(), isDuplicateSelection() (+6 more)

### Community 40 - "Display Serial Passthrough"
Cohesion: 0.23
Nodes (12): 3.10 Overheat warning, 41 °C, link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), handle_line(), parse_onoff(), print_help(), print_mem() (+4 more)

### Community 42 - "Captive Portal Login Script"
Cohesion: 0.32
Nodes (11): attempt_login(), extract_window_location(), form_action(), log(), probe_once(), prompt_credentials(), resolve_action(), captive-portal-login.sh script (+3 more)

### Community 43 - "Factory Flash Tool"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 45 - "Documentation Index"
Cohesion: 0.15
Nodes (13): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 3c. Plans and audits, 4. Hardware references, 5. Human documents (+5 more)

### Community 46 - "screen_data.cpp"
Cohesion: 0.36
Nodes (8): on_back_bottle(), on_back_cluster(), on_back_picker(), on_pick_bottle(), on_pick_cluster(), on_pick_sensor(), ui_show_bottle(), ui_show_cluster()

### Community 47 - "serial_worker"
Cohesion: 0.11
Nodes (22): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Runtime, Three copies exist, What it does, 4. Backend — `web_control.py`, Background workers, `POST /control` Actions (+14 more)

### Community 48 - "Memory Safety Skill"
Cohesion: 0.18
Nodes (10): 1. Memory Bug Categories, 2. Safety Spectrum, Best Practices, Core Concepts, Debugging Tools, Detailed patterns and worked examples, Do's, Don'ts (+2 more)

### Community 49 - "Display DB Fault Codes"
Cohesion: 0.22
Nodes (9): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA (+1 more)

### Community 50 - "Repository README"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 51 - "Pi Sync Script"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 52 - "Temperature Probe Sketch"
Cohesion: 0.18
Nodes (13): delay(), initializeSensor(), loop(), setup(), checkIdleLine(), loop(), printAddress(), printBusDiagnostics() (+5 more)

### Community 53 - "index.md"
Cohesion: 0.12
Nodes (5): Raspberry Pi kiosk UI CSS redesign, 11. Key paths quick reference, Documentation destination, gen-compile-db.sh script, install-leaf.sh script

### Community 54 - "Calibration Notes Document"
Cohesion: 0.20
Nodes (10): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+2 more)

### Community 55 - "Sample Schema"
Cohesion: 0.20
Nodes (10): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC (+2 more)

### Community 56 - "Push Row Schema"
Cohesion: 0.20
Nodes (10): PushRow, bottleId, ch4, co2, experimentId, nullMask, ph, pressure (+2 more)

### Community 57 - "sens_report_round"
Cohesion: 0.12
Nodes (20): 11. The writer, display-v9: DB fields from the `DB` line, Memory-safety hardening (`display-v9`, `network-master`), Verification, 6.1 Write grant: `SECURITY DEFINER` RPC, 1. Where the data comes from today, 5.1 As built, 5. ESP implementation steps (+12 more)

### Community 58 - "DB Contract Host Tests"
Cohesion: 0.36
Nodes (8): contains(), main(), test_ack_and_errors(), test_array_iteration(), test_envelope(), test_field_parsing(), test_sanitize(), test_timestamps()

### Community 59 - "Pi Clock Sync Worker"
Cohesion: 0.20
Nodes (8): `clock_sync_worker` (daemon thread), clock_sync_worker(), _extract_portal_redirect_url(), _fetch_http_date(), _get_default_gateway_ip(), _NoRedirectHandler, _parse_http_date_header(), sync_clock_from_lan_http()

### Community 60 - "Memory Safety Patterns"
Cohesion: 0.22
Nodes (8): memory-safety-patterns — detailed patterns and worked examples, Pattern 1: RAII in C++, Pattern 2: Smart Pointers in C++, Pattern 3: Ownership in Rust, Pattern 4: Safe Resource Management in C, Pattern 5: Bounds Checking, Pattern 6: Preventing Data Races, Patterns by Language

### Community 61 - "3.7.1 Redesign plan — `/info`"
Cohesion: 0.18
Nodes (11): 3.7.1.1 Removed content, 3.7.1.2 Layout, 3.7.1.3 Gauge colour, 3.7.1.4 Gauge fill rules, 3.7.1.5 Network state tile, 3.7.1.6 Font change, 3.7.1.8 Freshness, 3.7.1.9 Interaction notes (+3 more)

### Community 62 - "DB Pass States"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 65 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 66 - "Sensor calibration firmware"
Cohesion: 0.14
Nodes (15): 1. Common structure, 2. `co2-calibrate`, 3. `pressure-calibrate`, 5. `temp-probe`, 7. Related documents, Correctness limits, Field correction, How a reading is taken (+7 more)

### Community 68 - "DB Experiment Lookup"
Cohesion: 0.40
Nodes (5): DbExperiment, durationH, id, sampleIntervalS, setpoints

### Community 70 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.11
Nodes (19): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display, 3.3 Gone from the Modbus design (+11 more)

### Community 71 - "log_data_to_csv"
Cohesion: 0.17
Nodes (13): 7. Data & logging, CSV format (current schema), Existing logs, Older schema (2025-12-12.csv), Storage, USB export, 7. `POST /toggle_logging`, fmt_val() (+5 more)

### Community 72 - "Endpoint details"
Cohesion: 0.11
Nodes (19): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. `GET /files`, 14. `GET /read_log`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip` (+11 more)

### Community 73 - "WiFi Provisioning Stages"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 75 - "DB Method Enum"
Cohesion: 0.67
Nodes (3): DbMethod, DB_GET, DB_POST

### Community 76 - "Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)"
Cohesion: 0.12
Nodes (17): 10. WT32 link (`rusim-line` v1, production), 11. Setup route, 12. WT32 login page QR, 13. Light theme and the /setup QR, 14. Debug flag `IS_DEBUG`, 1. Overview, 5. USB control and diagnostic protocol, 6. Bell / NULL semantics (+9 more)

### Community 78 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 86 - "LVGL v8 to v9 migration report"
Cohesion: 0.12
Nodes (14): 2. Hardware / platform notes, 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, File-by-file summary, LVGL v8 to v9 migration report (+6 more)

### Community 87 - "createChartPoints"
Cohesion: 0.14
Nodes (16): createChartPoints(), getMetricValue(), ReadingWithStatus, Metadata, 12.1 The experiment list is bounded by the monitoring fetch's row cap, 12.2 The chart heading says "Across experiments" (fixed), 12.3 Dead code, 12.5 Security posture (+8 more)

### Community 88 - "ESP to database sync — plan"
Cohesion: 0.12
Nodes (16): 1. Current state, 2. The database forbids writes, 3. Nothing reads the sensors, 4. Why the device must not send `token_hash`, 5. Credential exposure on the WT32, 6.2 Pull scope: full `experiments` plus one config row, scoped to same `device_id`, 6. Decisions (resolved 2026-10-16), 9. Related documents (+8 more)

### Community 89 - "PlatformIO build environments (`platformio.ini`)"
Cohesion: 0.12
Nodes (15): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`), 1. Layout (+7 more)

### Community 90 - "Network master ↔ database communication plan"
Cohesion: 0.13
Nodes (15): 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions, 5. Implementation order (+7 more)

### Community 91 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.14
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 92 - "network-master (`firmware/network-master`) — production WT32 firmware"
Cohesion: 0.15
Nodes (13): 1. Environment, 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential handling (+5 more)

### Community 93 - "analogRead"
Cohesion: 0.21
Nodes (10): 6. `ph-calibrate`, Correctness limits, Sampling and output, Wiring, analogRead(), convertCountToPh(), loop(), readAverageCount() (+2 more)

### Community 94 - "network-master-database-plan.md"
Cohesion: 0.18
Nodes (3): Display <-> WT32 link (`display-wt32-link`) — replaced, 3D Models, PCB

### Community 95 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.17
Nodes (12): 1. What the program does, 2. Build environment, 3. Pin map, 4.1 pH, 4.2 Methane (TGS2611), 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing (+4 more)

### Community 96 - "Dashboard public-deploy security report"
Cohesion: 0.17
Nodes (11): 1. Blocker — legacy `public` schema is public-read (live, confirmed), 2. High — `authenticate()` has no brute-force protection, 3. High — Netlify bundle + env handling, 4. Medium — session hygiene and PII, 5. Low — functional / ops, Already fixed / verified good (keep these), Dashboard public-deploy security report, Findings (ordered by severity) (+3 more)

### Community 97 - "Database schema catalog"
Cohesion: 0.18
Nodes (11): Database schema catalog, `device_data`: active application schema, Equipment hierarchy, Experiments and assignments, Exposure and scope, Integrity model, `public`: legacy application schema (no longer used), Relation inventories (+3 more)

### Community 98 - "2. Cross-cutting"
Cohesion: 0.18
Nodes (10): 2.2 Navigation, 2.3 Data refresh, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting, 4. Cross-cutting redesign table, 5. Discrepancies found while writing this (+2 more)

### Community 99 - "9. Network & services"
Cohesion: 0.18
Nodes (10): 9. Network & services, Enabled services (selected), Listening ports, Network Architecture (LAN Primary), Security note (see §10), 13. Flask REST API reference — `web_control.py`, Source-level notes, Summary (+2 more)

### Community 100 - "WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)"
Cohesion: 0.18
Nodes (10): 1. Environment, 2. What it does, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`) (+2 more)

### Community 101 - "Migration plan: ESP pulls telemetry from the legacy Mega slave"
Cohesion: 0.20
Nodes (9): 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour, 6. Deferred, with the hook each needs, 7. Verification, 8. Open risks, Migration plan: ESP pulls telemetry from the legacy Mega slave, Linear conversion (temporary) (+1 more)

### Community 102 - "Reference: `device_data` column detail"
Cohesion: 0.22
Nodes (9): Access control, Chamber numbering, Development tooling, `device_data.device_config`, `device_data.devices`, `device_data.experiments`, `device_data.readings`, `device_data.sessions` (+1 more)

### Community 103 - "12. Copies & git state"
Cohesion: 0.22
Nodes (9): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, UI, `web_control.py` (+1 more)

### Community 104 - "PageScan"
Cohesion: 0.25
Nodes (8): PageScan, cloudflareChallenge, formAction, hasForm, hasUsernameField, magic, redir, totalBytes

### Community 105 - "compile_commands.json and gen-compile-db.sh"
Cohesion: 0.29
Nodes (6): 1. What the file is, 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh

### Community 106 - "learned-mistakes.md"
Cohesion: 0.29
Nodes (6): 2026-10-05, 2026-10-06, 2026-10-08, 2026-10-20, 2026-10-24, 2026-10-31

### Community 107 - "status"
Cohesion: 0.29
Nodes (6): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`), 8. `POST /download/info`, status()

### Community 108 - "Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)"
Cohesion: 0.29
Nodes (7): 1. Purpose, 2. Wire protocol, 3. How it opens the port, 4. REPL behaviour, 5. Usage, 6. Limits, Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)

### Community 109 - "Raspberry Pi `water-control-system` mirror"
Cohesion: 0.29
Nodes (7): Checksums, Layout, Raspberry Pi `water-control-system` mirror, Refreshing the mirror, Related documents, Relationship to the deleted `main_ui.html`, What is not included

### Community 110 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses

### Community 111 - "/info Reboot: Heap-Walk Hang on `display-v9`"
Cohesion: 0.33
Nodes (5): Current state of the code, Experiments and results, /info Reboot: Heap-Walk Hang on `display-v9`, Next step (open), Symptom

### Community 112 - "3.3.1 Redesign plan — `/data` node picker"
Cohesion: 0.33
Nodes (6): 3.3.1.1 Removed content, 3.3.1.2 Layout (content coordinates, panel 800x436), 3.3.1.4 Behaviour kept, 3.3.1.5 Open item, 3.3.1 Redesign plan — `/data` node picker, 3.3 Route `/data` — node picker

### Community 113 - "1. Hardware & OS"
Cohesion: 0.33
Nodes (6): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account

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

### Community 118 - "Stage"
Cohesion: 0.33
Nodes (6): Stage, STAGE_DONE, STAGE_PROMPT_PASS, STAGE_PROMPT_USER, STAGE_RECON, STAGE_WAIT_IP

### Community 119 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 120 - "Kiosk text scale"
Cohesion: 0.40
Nodes (5): 2026-10-20 change, Deploying a change to the kiosk, How it works, Kiosk text scale, Related documents

### Community 121 - "Documentation review — all of `docs/`"
Cohesion: 0.40
Nodes (4): 2. Checked and left alone, 3. Open items, 4. What a reader should take from this, Documentation review — all of `docs/`

### Community 123 - "5. Frontend UI — `templates/index.html`"
Cohesion: 0.50
Nodes (4): 5. Frontend UI — `templates/index.html`, Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer, Tabs, Temporary MOSFET Speed Control (`0–255` Debit Testing)

### Community 124 - "8. Auxiliary tools & scripts"
Cohesion: 0.50
Nodes (4): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc

### Community 125 - "10. Security & caveats"
Cohesion: 0.50
Nodes (4): 10. Security & caveats, Operational caveats, Recommendations, Security

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

## Knowledge Gaps
- **98 isolated node(s):** `name`, `cloudflareChallenge`, `formAction`, `hasForm`, `hasUsernameField` (+93 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 948 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **18 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `Display Link Serial Protocol` to `Sensor calibration firmware`, `sim/Arduino.h`, `Display Serial Passthrough`, `wt32-eth01_captive-recon/main.cpp`, `screen_control.cpp`, `Display V9 Screen Headers`, `ui_shell.cpp`, `Temperature Probe Sketch`, `master.cpp`, `ui_mk_button`, `Slave Sensor Firmware`, `analogRead`, `4. `methane-calibrate``?**
  _High betweenness centrality (0.078) - this node is a cross-community bridge._
- **Are the 37 inferred relationships involving `millis()` (e.g. with `loop()` and `readCo2Uart()`) actually correct?**
  _`millis()` has 37 INFERRED edges - model-reasoned connections that need verification._
- **What connects `name`, `cloudflareChallenge`, `formAction` to the rest of the system?**
  _98 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Display Fake Data Harness` be split into smaller, more focused modules?**
  _Cohesion score 0.10588972431077694 - nodes in this community are weakly interconnected._
- **Why does `Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)` connect `Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)` to `Display Fake Data Harness`, `display-provisioning-ap.md`, `ui_shell.cpp`, `LVGL v8 to v9 migration report`?**
  _High betweenness centrality (0.061) - this node is a cross-community bridge._
- **Are the 26 inferred relationships involving `delay()` (e.g. with `loop()` and `setup()`) actually correct?**
  _`delay()` has 26 INFERRED edges - model-reasoned connections that need verification._
- **Should `Display Link Serial Protocol` be split into smaller, more focused modules?**
  _Cohesion score 0.0824524312896406 - nodes in this community are weakly interconnected._