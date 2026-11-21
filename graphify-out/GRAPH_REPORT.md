# Graph Report - rusim-monorepo  (2026-11-21)

## Corpus Check
- 168 files · ~223,200 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 13 file(s) not represented in the graph (top: (none) 5, .toml 2, .example 1)

## Summary
- 2169 nodes · 4038 edges · 133 communities (124 shown, 9 thin omitted)
- Extraction: 83% EXTRACTED · 17% INFERRED · 0% AMBIGUOUS · INFERRED: 690 edges (avg confidence: 0.91)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `760aad54`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Chart
- display-v9/main.cpp
- network-master: bounded buffers
- HoldBtnCtx
- web_control.py
- link_provision_stub.cpp
- network-master-passthrough.py
- monitor-legacy-pi.ts
- network-master (`firmware/network-master`) â€” production WT32 firmware
- screen_home.cpp
- authenticate
- link_serial.cpp
- screen_control.cpp
- new-slave.cpp
- compilerOptions
- screen_control.cpp
- Pin Layout Overview
- new-slave.cpp
- _before_upload
- touch.h
- package.json
- master.cpp
- flash-factory.ts
- analogRead
- polling_worker
- serviceMasterPing
- screen_data.cpp
- handle_line
- dev-db.ts
- 5. Checklist
- PushRow
- data.ts
- qr_refresh
- auth.ts
- 2. Cross-cutting
- serial_worker
- screen_setup_poll
- ui_mk_button
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- ATmega2560 Mega Mini — Development & Reference Documentation
- my_include
- Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)
- install-leaf.sh
- ui_shell.cpp
- App.tsx
- Endpoint details
- forceAllOff
- serial_worker
- Pressure calibration plot
- index.md
- ESP32-8048S050C · Board & Archive Guide
- syncDbLatest
- history.ts
- Kiosk text scale
- Methane calibration report generator
- screen_info_poll
- WT32-ETH01 — LLM Documentation
- csv.ts
- 3.8.1 Redesign plan — top bar
- _fetch_http_date
- WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)
- Dashboard — full walkthrough
- MosfetChannel
- handle_line
- test_methane_calibration.py
- ESP32-8048S050 — LLM Generated Summary
- Migration plan: ESP pulls telemetry from the legacy Mega slave
- provision_ap.cpp
- forceAllOff
- log_data_to_csv
- network-master/main.cpp
- Memory Safety Patterns
- Repository Index
- millis
- data.ts
- display-wt32-link.md
- compile_commands.json and gen-compile-db.sh
- polling_worker
- _Relay
- /info Reboot: Heap-Walk Hang on `display-v9`
- DbExperiment
- Stage
- Display V9 PC Simulator (`[env:display-sim]`)
- 3. Project overview — `water-control-system`
- CsvDownloadHandler
- 4. Authentication, end to end
- ESP to database sync — plan
- 11. Key paths quick reference
- co2-calibrate/main.cpp
- compare.ts
- PlatformIO build environments (`platformio.ini`)
- delay
- Raspberry Pi `water-control-system` mirror
- 1. Hardware & OS
- 8. Auxiliary tools & scripts
- 3.8.1 Redesign plan — top bar
- `leaf` terminal reader on the Pi
- 5. Frontend UI — `templates/index.html`
- master.cpp
- Verification
- 10. Security & caveats
- Display `IS_DEBUG` flag
- 5. Frontend UI — `templates/index.html`
- learned-mistakes.md
- SystemMode
- 6. From a row to a chart point
- DB experiment sync + NTP — LLM Documentation
- 10. ESP-IDF Recommendations
- 3. States
- 9. Network & services
- gen-compile-db.sh
- install-leaf.sh
- co2-calibrate/main.cpp
- Display UI — LLM Documentation (`display-v9`, fake-data pass)
- 10. Deployment and the legacy schema
- Stage
- 13. Reference tables
- 11. LVGL Recommendations
- 16. Performance and Stability Checklist
- LVGL v8 to v9 migration report
- Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants
- backlog-filling.md
- millis
- display-wt32-link.md
- 11-key-paths.md
- water-level-test.md
- handlePingStream

## God Nodes (most connected - your core abstractions)
1. `millis()` - 53 edges
2. `Dashboard()` - 32 edges
3. `delay()` - 30 edges
4. `logf()` - 28 edges
5. `ui_update_bell()` - 23 edges
6. `render_current()` - 23 edges
7. `sens_apply_real()` - 21 edges
8. `dbPassTick()` - 21 edges
9. `ESP32-8048S050 — LLM Generated Summary` - 21 edges
10. `Endpoint details` - 21 edges

## Surprising Connections (you probably didn't know these)
- `10.3 The lockdown migration` --references--> `authenticate()`  [INFERRED]
  docs/dashboard.md → dashboard/src/data.ts
- `13.3 Migration timeline` --references--> `authenticate()`  [INFERRED]
  docs/dashboard.md → dashboard/src/data.ts
- `4.5 Client-side rate limiting` --references--> `authenticate()`  [INFERRED]
  docs/dashboard.md → dashboard/src/data.ts
- `Link (`rusim-line` v1)` --references--> `authenticate()`  [INFERRED]
  docs/db-sync-experiment-ntp.md → dashboard/src/data.ts
- `4. Why the device must not send `token_hash`` --references--> `authenticate()`  [INFERRED]
  docs/plans-and-audits/esp-db-sync.md → dashboard/src/data.ts

## Import Cycles
- None detected.

## Communities (133 total, 9 thin omitted)

### Community 0 - "Chart"
Cohesion: 0.10
Nodes (46): 10. WT32 link (`rusim-line` v1, production), 3. File map, 2.4 Real vs fake labelling, 3.2.1.3 State and data model changes needed, clear_bottle(), fake_any_null(), fake_init(), fake_null_count() (+38 more)

### Community 1 - "display-v9/main.cpp"
Cohesion: 0.08
Nodes (37): arg_lookup(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line(), handle_log(), handle_login_req() (+29 more)

### Community 2 - "network-master: bounded buffers"
Cohesion: 0.09
Nodes (20): 14. `GET /read_log`, download_info(), export_data(), get_clock_payload(), get_drives(), get_interface_ip(), get_ip(), get_lan_ip() (+12 more)

### Community 3 - "HoldBtnCtx"
Cohesion: 0.08
Nodes (39): BottleChart(), BrandMark(), ChartInteraction, ChevronIcon(), CompareActivePoint, CompareIcon(), CompareSeries, Dashboard() (+31 more)

### Community 4 - "web_control.py"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 5 - "link_provision_stub.cpp"
Cohesion: 0.07
Nodes (45): 1. Usage, 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents (+37 more)

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.16
Nodes (6): _after_upload(), _cleanup_after_failed_upload(), _configured_port(), _display_ports(), _find_port(), _shutdown()

### Community 7 - "monitor-legacy-pi.ts"
Cohesion: 0.06
Nodes (5): link_serial_get_sens(), link_serial_init(), link_serial_poll(), link_serial_set_exp_state(), main()

### Community 8 - "network-master (`firmware/network-master`) â€” production WT32 firmware"
Cohesion: 0.10
Nodes (19): 7. Correctness rules for the push (decided), arg_lookup(), BottleReading, copyFormArg(), DbMethod, DB_GET, DB_POST, handleLogin() (+11 more)

### Community 9 - "screen_home.cpp"
Cohesion: 0.06
Nodes (46): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+38 more)

### Community 10 - "authenticate"
Cohesion: 0.06
Nodes (41): App(), AuthState, readStoredSession(), StoredSession, useAuth(), writeStoredSession(), authenticate(), fetchDevice() (+33 more)

### Community 11 - "link_serial.cpp"
Cohesion: 0.14
Nodes (20): 2. What it does, extractWindowLocation(), loop(), onEthEvent(), PageScan, cloudflareChallenge, formAction, hasForm (+12 more)

### Community 12 - "screen_control.cpp"
Cohesion: 0.05
Nodes (75): Display (`firmware/display-v9/`), Widget lifetime rule, 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.4 Functions to add / change, 3.2.1.5 Polling, 3.2.1.6 Integration with other screens, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels) (+67 more)

### Community 13 - "new-slave.cpp"
Cohesion: 0.12
Nodes (25): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_WIDTHS, clock(), configurePort(), createLineGate(), Device (+17 more)

### Community 14 - "compilerOptions"
Cohesion: 0.15
Nodes (4): display-v9: RAII ownership, DispBufDeleter, lvgl_tick_get(), my_disp_flush()

### Community 15 - "screen_control.cpp"
Cohesion: 0.10
Nodes (44): Auth used, Build, DB experiment sync + NTP (change record), Link (`rusim-line` v1), Master (`firmware/network-master/main.cpp`), 6.1 Database path (configuration, runs and uploads), 6. Memory and limits, 5. Content audit, 2026-11-18 (+36 more)

### Community 16 - "Pin Layout Overview"
Cohesion: 0.05
Nodes (44): dependencies, react@19.1.1, react-dom@19.1.1, @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13 (+36 more)

### Community 17 - "new-slave.cpp"
Cohesion: 0.06
Nodes (31): summarizeParameter(), 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 11. The writer, 13.1 Sensors and units, 13.3 Migration timeline (+23 more)

### Community 18 - "_before_upload"
Cohesion: 0.25
Nodes (6): _arm_tunnel(), _before_upload(), _close_tunnel(), _read_until(), _saw(), _write_all()

### Community 19 - "touch.h"
Cohesion: 0.15
Nodes (22): digitalRead(), millis(), Channel, haveReading, haveRise, level, periodUs, pin (+14 more)

### Community 20 - "package.json"
Cohesion: 0.12
Nodes (26): 1. Usage, 2. The five steps, 3. Helper functions and their firmware counterparts, 4. Differences from the firmware, 5. Testing, 6. Related documents, Captive portal login from Linux (bash), network-master: bounded buffers (+18 more)

### Community 21 - "master.cpp"
Cohesion: 0.18
Nodes (16): 4.1 Trip thresholds, 4.2 Asymmetric Fast-Trip / Leaky-Bucket Recovery Debouncing, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing, 8. Slave UART Polling, Validation, and Watchdog (`Serial1`), computeWaterRingAverage(), isValidBottleObject(), loop() (+8 more)

### Community 22 - "flash-factory.ts"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 23 - "analogRead"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 24 - "polling_worker"
Cohesion: 0.17
Nodes (10): Global state, `polling_worker` (daemon thread), control_api(), desired_state_commands(), polling_worker(), state_mismatch_commands(), stir_command(), threshold_command() (+2 more)

### Community 25 - "serviceMasterPing"
Cohesion: 0.15
Nodes (6): main(), run(), braced(), declarations(), MethaneCalibrationRegression, native_source()

### Community 26 - "screen_data.cpp"
Cohesion: 0.12
Nodes (30): 1.1 Router (`ui_shell.cpp`, `s_route`), 2.1 Styling, 3.4.1.1 Removed content, 3.4.1.2 Layout (content coordinates, panel 800x436), 3.4.1.3 Tile contents, 3.4.1.4 Behaviour kept, 3.4.1 Redesign plan — `/data/X` cluster, 3.4 Route `/data/X` — cluster (+22 more)

### Community 27 - "handle_line"
Cohesion: 0.21
Nodes (15): 3.14 Backlight sleep, link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), loop(), sleep_note_input(), sleep_tick(), handle_line() (+7 more)

### Community 28 - "dev-db.ts"
Cohesion: 0.13
Nodes (24): 4.1 pH, 4.4 Pressure (HX711 / HX710B), 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, 7. Verification (+16 more)

### Community 29 - "5. Checklist"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 30 - "PushRow"
Cohesion: 0.17
Nodes (14): 4.2 Methane (TGS2611), 6. `ph-calibrate`, Correctness limits, Sampling, Sampling and output, Wiring, analogRead(), loop() (+6 more)

### Community 31 - "data.ts"
Cohesion: 0.15
Nodes (18): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), CONCLUDED_AFTER_MS, ExperimentSummary, ReadingRow, DashboardView, DEV_FORCE_LIVE_EXPERIMENT (+10 more)

### Community 32 - "qr_refresh"
Cohesion: 0.15
Nodes (19): csvDisabledReason(), downloadExperimentCsv(), getMetricValue(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow() (+11 more)

### Community 33 - "auth.ts"
Cohesion: 0.20
Nodes (15): ParameterStats, ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber(), HISTORY_PARAMETERS, HistoryPointDetails, parameterValue() (+7 more)

### Community 34 - "2. Cross-cutting"
Cohesion: 0.15
Nodes (3): HostSerial, rx_, tx_

### Community 35 - "serial_worker"
Cohesion: 0.13
Nodes (14): Backlog and Continuity, Context Management, Dashboard, Description, Documentation, Documentation Format, Firmware, General (+6 more)

### Community 36 - "screen_setup_poll"
Cohesion: 0.14
Nodes (3): What is shared vs stubbed, random(), randomSeed()

### Community 37 - "ui_mk_button"
Cohesion: 0.09
Nodes (21): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+13 more)

### Community 38 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.21
Nodes (16): civilFromDays(), daysFromCivil(), findField(), finiteValue(), formatPushEnvelope(), formatUtcTimestamp(), formatValue(), nextObject() (+8 more)

### Community 39 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.29
Nodes (13): CompareView(), displayChamberNumber(), addSelection(), canAddSelection(), COMPARE_SERIES_COLORS, CompareSelection, compareSeriesColor(), compareSeriesLabel() (+5 more)

### Community 40 - "my_include"
Cohesion: 0.25
Nodes (15): 7.2 Link plumbing, millis(), clearCreds(), dbPassFinish(), link_emit_db(), link_emit_loginreq(), link_emit_net(), link_emit_result() (+7 more)

### Community 41 - "Display UI — LLM Documentation (`display-v9`, WT32 sensor feed)"
Cohesion: 0.20
Nodes (10): 12. WT32 login page QR, 13. Light theme and the /setup QR, 14. Debug flag `IS_DEBUG`, 1. Overview, 5. USB control and diagnostic protocol, 6. Bell / NULL semantics, 7. Hold-to-confirm implementation, 8. Verify without hardware signals (+2 more)

### Community 42 - "install-leaf.sh"
Cohesion: 0.32
Nodes (11): attempt_login(), extract_window_location(), form_action(), log(), probe_once(), prompt_credentials(), resolve_action(), captive-portal-login.sh script (+3 more)

### Community 43 - "ui_shell.cpp"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 44 - "App.tsx"
Cohesion: 0.15
Nodes (4): advance_ms(), delay(), loop(), setup()

### Community 45 - "Endpoint details"
Cohesion: 0.15
Nodes (13): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 3c. Plans and audits, 4. Hardware references, 5. Human documents (+5 more)

### Community 46 - "forceAllOff"
Cohesion: 0.27
Nodes (5): get_json(), main(), _open(), post_download(), post_json()

### Community 47 - "serial_worker"
Cohesion: 0.10
Nodes (25): 1. Fixes applied, 2. Checked and left alone, 3. Open items, 4. What a reader should take from this, Documentation review — all of `docs/`, Runtime, 4. Backend — `web_control.py`, Background workers (+17 more)

### Community 48 - "Pressure calibration plot"
Cohesion: 0.22
Nodes (7): 1. Files, 2. Run it, 3. How it works, 4. Stable windows and their averages, 5. What the current log shows, 6. Limits, Pressure calibration plot

### Community 49 - "index.md"
Cohesion: 0.22
Nodes (9): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA (+1 more)

### Community 50 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): Access control, Chamber numbering, Database schema catalog, Development tooling, `device_data`: active application schema, `device_data.device_config`, `device_data.devices`, `device_data.experiments` (+12 more)

### Community 51 - "syncDbLatest"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 52 - "history.ts"
Cohesion: 0.46
Nodes (7): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup()

### Community 53 - "Kiosk text scale"
Cohesion: 0.40
Nodes (5): 2026-10-20 change, Deploying a change to the kiosk, How it works, Kiosk text scale, Related documents

### Community 54 - "Methane calibration report generator"
Cohesion: 0.25
Nodes (7): Data: 5 points per sensor, Formulas, How the two simulated points are made, Limits, Methane calibration report generator, Output files, Run

### Community 55 - "screen_info_poll"
Cohesion: 0.25
Nodes (8): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC

### Community 56 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.20
Nodes (10): PushRow, bottleId, ch4, co2, experimentId, nullMask, ph, pressure (+2 more)

### Community 57 - "csv.ts"
Cohesion: 0.14
Nodes (16): display-v9: DB fields from the `DB` line, Memory-safety hardening (`display-v9`, `network-master`), Verification, 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, 5.1 As built (+8 more)

### Community 58 - "3.8.1 Redesign plan — top bar"
Cohesion: 0.36
Nodes (8): contains(), main(), test_ack_and_errors(), test_array_iteration(), test_envelope(), test_field_parsing(), test_sanitize(), test_timestamps()

### Community 59 - "_fetch_http_date"
Cohesion: 0.33
Nodes (4): _extract_portal_redirect_url(), _fetch_http_date(), _NoRedirectHandler, _parse_http_date_header()

### Community 60 - "WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)"
Cohesion: 0.25
Nodes (7): 1. Environment, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)

### Community 61 - "Dashboard — full walkthrough"
Cohesion: 0.15
Nodes (17): 3.7.1.1 Removed content, 3.7.1.2 Layout, 3.7.1.3 Gauge colour, 3.7.1.4 Gauge fill rules, 3.7.1.5 Network state tile, 3.7.1.6 Font change, 3.7.1.7 Functions, 3.7.1.8 Freshness (+9 more)

### Community 62 - "MosfetChannel"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 65 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 66 - "Migration plan: ESP pulls telemetry from the legacy Mega slave"
Cohesion: 0.29
Nodes (7): 1. Where the data comes from today, 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour, 6. Deferred, with the hook each needs, 8. Open risks, Migration plan: ESP pulls telemetry from the legacy Mega slave

### Community 68 - "forceAllOff"
Cohesion: 0.17
Nodes (17): 10. Known Gaps, 1. Overview, 2. Pin Assignment, 3. Boot Defaults, 5. Automatic Pump Interlocks, 6. Mode and Safety Cutoffs, 7. Unified 3-Channel MOSFET PWM Ramping and `0–255` Debit Control, 9. Raspberry Pi USB Serial Protocol (+9 more)

### Community 70 - "network-master/main.cpp"
Cohesion: 0.11
Nodes (19): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display, 3.3 Gone from the Modbus design (+11 more)

### Community 71 - "Memory Safety Patterns"
Cohesion: 0.17
Nodes (13): 7. Data & logging, CSV format (current schema), Existing logs, Older schema (2025-12-12.csv), Storage, USB export, 7. `POST /toggle_logging`, fmt_val() (+5 more)

### Community 72 - "Repository Index"
Cohesion: 0.12
Nodes (17): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. `GET /files`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip`, 18. `GET /get_timezones` (+9 more)

### Community 73 - "millis"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 75 - "data.ts"
Cohesion: 0.13
Nodes (15): 1. Common structure, 2. `co2-calibrate`, 3. `pressure-calibrate`, 4. `methane-calibrate`, 5. `temp-probe`, 7. Related documents, Correctness limits, Field correction (+7 more)

### Community 76 - "display-wt32-link.md"
Cohesion: 0.12
Nodes (33): 11. Setup route, 4. Screen tree / state machine (as built), Decoded backtrace, 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 3.11 Overheat breaker trip, 45 °C, 3.12 Bell and invalid-field list (+25 more)

### Community 78 - "polling_worker"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 79 - "_Relay"
Cohesion: 0.29
Nodes (3): _configure_tty(), _open_display(), _Relay

### Community 80 - "/info Reboot: Heap-Walk Hang on `display-v9`"
Cohesion: 0.33
Nodes (5): Current state of the code, Experiments and results, /info Reboot: Heap-Walk Hang on `display-v9`, Next step (open), Symptom

### Community 81 - "DbExperiment"
Cohesion: 0.33
Nodes (6): DbExperiment, durationH, id, sampleIntervalS, setpoints, dbFindExperiment()

### Community 82 - "Stage"
Cohesion: 0.33
Nodes (6): Stage, STAGE_DONE, STAGE_PROMPT_PASS, STAGE_PROMPT_USER, STAGE_RECON, STAGE_WAIT_IP

### Community 84 - "Display V9 PC Simulator (`[env:display-sim]`)"
Cohesion: 0.40
Nodes (4): Configuration, Display V9 PC Simulator (`[env:display-sim]`), Run it, Scope note

### Community 85 - "3. Project overview — `water-control-system`"
Cohesion: 0.40
Nodes (4): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Three copies exist, What it does

### Community 86 - "CsvDownloadHandler"
Cohesion: 0.07
Nodes (18): 2. Hardware / platform notes, 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, File-by-file summary, LVGL v8 to v9 migration report (+10 more)

### Community 87 - "4. Authentication, end to end"
Cohesion: 0.21
Nodes (19): Chart(), chartAreaPath(), chartGeometry(), chartPath(), CompareChart(), createChartPoints(), createTimeLabels(), createYTicks() (+11 more)

### Community 88 - "ESP to database sync — plan"
Cohesion: 0.13
Nodes (14): 1. Current state, 2. The database forbids writes, 3. Nothing reads the sensors, 4. Why the device must not send `token_hash`, 5. Credential exposure on the WT32, 9. Related documents, `authenticate()` is unthrottled, ESP to database sync — plan (+6 more)

### Community 89 - "11. Key paths quick reference"
Cohesion: 0.25
Nodes (8): 1. Layout, 2. Shared settings, 3. Environment table, 4. The ESP32-S3 display environments, 5. Commands, 6. History of this configuration, 7. Related documents, PlatformIO build environments (`platformio.ini`)

### Community 90 - "co2-calibrate/main.cpp"
Cohesion: 0.12
Nodes (16): 1. Recommendation and evidence, 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions (+8 more)

### Community 91 - "compare.ts"
Cohesion: 0.14
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 92 - "PlatformIO build environments (`platformio.ini`)"
Cohesion: 0.17
Nodes (16): 1. Environment, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential handling, 3. Line protocol, 4. State machine, 5. Portal login (`doLogin()`), 7. Expected output (+8 more)

### Community 93 - "delay"
Cohesion: 0.23
Nodes (13): loop(), readCo2Ppm(), readCo2Uart(), setup(), waitForLevel(), delay(), convertCountToPh(), loop() (+5 more)

### Community 94 - "Raspberry Pi `water-control-system` mirror"
Cohesion: 0.13
Nodes (5): Retired: display-hosted Wi-Fi provisioning, Display <-> WT32 link (`display-wt32-link`) — replaced, 3D Models, PCB, Raspberry Pi kiosk UI CSS redesign

### Community 95 - "1. Hardware & OS"
Cohesion: 0.20
Nodes (10): 1. What the program does, 2. Build environment, 3. Pin map, 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing, 6. Responsiveness, 7. Known gaps (+2 more)

### Community 96 - "8. Auxiliary tools & scripts"
Cohesion: 0.40
Nodes (4): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc

### Community 97 - "3.8.1 Redesign plan — top bar"
Cohesion: 0.29
Nodes (7): 3.8.1.1 Removed content, 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative), 3.8.1.3 Clock, 3.8.1.5 Interaction notes, 3.8.1 Redesign plan — top bar, 3.8 Persistent status bar, ui_update_clock()

### Community 98 - "`leaf` terminal reader on the Pi"
Cohesion: 0.18
Nodes (10): 2.2 Navigation, 2.3 Data refresh, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting, 4. Cross-cutting redesign table, 5. Discrepancies found while writing this (+2 more)

### Community 99 - "5. Frontend UI — `templates/index.html`"
Cohesion: 0.25
Nodes (7): 1. What the file is, Network Architecture (LAN Primary), 13. Flask REST API reference — `web_control.py`, Source-level notes, Summary, command(), disable_wifi_client_and_stop_hotspot()

### Community 100 - "master.cpp"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 101 - "Verification"
Cohesion: 0.20
Nodes (10): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+2 more)

### Community 102 - "10. Security & caveats"
Cohesion: 0.40
Nodes (4): 10. Security & caveats, Operational caveats, Recommendations, Security

### Community 103 - "Display `IS_DEBUG` flag"
Cohesion: 0.22
Nodes (8): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, UI, `web_control.py`

### Community 104 - "5. Frontend UI — `templates/index.html`"
Cohesion: 0.50
Nodes (4): 5. Frontend UI — `templates/index.html`, Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer, Tabs, Temporary MOSFET Speed Control (`0–255` Debit Testing)

### Community 105 - "learned-mistakes.md"
Cohesion: 0.33
Nodes (5): 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh

### Community 106 - "SystemMode"
Cohesion: 0.22
Nodes (8): 2026-10-05, 2026-10-06, 2026-10-08, 2026-10-20, 2026-10-24, 2026-10-31, 2026-11-10, 2026-11-17

### Community 107 - "6. From a row to a chart point"
Cohesion: 0.29
Nodes (6): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`), 8. `POST /download/info`, status()

### Community 108 - "DB experiment sync + NTP — LLM Documentation"
Cohesion: 0.25
Nodes (7): 1. Purpose, 2. Wire protocol, 3. How it opens the port, 4. REPL behaviour, 5. Usage, 6. Limits, Legacy Master USB Tool (`scripts-hardware/legacy-master-usb.ts`)

### Community 109 - "10. ESP-IDF Recommendations"
Cohesion: 0.29
Nodes (7): Checksums, Layout, Raspberry Pi `water-control-system` mirror, Refreshing the mirror, Related documents, Relationship to the deleted `main_ui.html`, What is not included

### Community 110 - "3. States"
Cohesion: 0.09
Nodes (23): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses, 3.10 Overheat warning, 41 °C, 3.13 Phone-associated popup, 3.15 SoftAP lifecycle (+15 more)

### Community 111 - "9. Network & services"
Cohesion: 0.29
Nodes (7): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)

### Community 114 - "co2-calibrate/main.cpp"
Cohesion: 0.29
Nodes (6): 2. Boot & kiosk session model, Boot sequence, Chromium kiosk, Notes / caveats, openbox autostart (`~/.config/openbox/autostart`), PM2 service (`/etc/systemd/system/pm2-invitro.service`)

### Community 115 - "Display UI — LLM Documentation (`display-v9`, fake-data pass)"
Cohesion: 0.29
Nodes (6): How the cross-compile works, `leaf` terminal reader on the Pi, Rebuilding and removing, Running it, Verification, Why it is not compiled on the Pi

### Community 116 - "10. Deployment and the legacy schema"
Cohesion: 0.33
Nodes (6): 10. ESP-IDF Recommendations, ESP-IDF components you may use, Important menuconfig areas, Recommended minimum version, Target setup, Typical required features

### Community 117 - "Stage"
Cohesion: 0.33
Nodes (6): 6.1 RGB display control pins, 6.2 RGB data pins, common capacitive variant, 6.3 GT911 capacitive touch pins, 6.4 Other common board pins, 6.5 Known documentation trap, 6. Common Pin Map Reference

### Community 118 - "13. Reference tables"
Cohesion: 0.20
Nodes (10): Failure behaviour, First run, Limitations, Master firmware flashing, Pi sync script — `sync_pi.sh`, Prerequisites, Rollback, Usage (+2 more)

### Community 119 - "11. LVGL Recommendations"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 122 - "16. Performance and Stability Checklist"
Cohesion: 0.22
Nodes (9): 6. Master firmware — `legacy/master/master.cpp`, Boot defaults, Commands, Control parameters, Deployment, Libraries, Pins, Stirrer modes (+1 more)

### Community 123 - "LVGL v8 to v9 migration report"
Cohesion: 0.25
Nodes (8): MosfetChannel, currentPwm, maxPwm, pin, rampOriginMs, rampOriginPwm, state, targetPwm

### Community 124 - "Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)"
Cohesion: 0.09
Nodes (17): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account, 9. Network & services, Enabled services (selected) (+9 more)

### Community 125 - "13. ESPHome Notes"
Cohesion: 0.29
Nodes (10): setup(), digitalWrite(), pinMode(), touch_init(), loop(), rampUp(), setup(), stopMotor() (+2 more)

### Community 126 - "15. Filesystem and Asset Storage"
Cohesion: 0.50
Nodes (4): 11. LVGL Recommendations, Important LVGL settings, LVGL performance tips, Recommended LVGL version

### Community 127 - "4. Display Subsystem"
Cohesion: 0.50
Nodes (4): 12. Arduino / PlatformIO Notes, Arduino IDE typical board settings, PlatformIO example concepts, Useful Arduino/PlatformIO libraries

### Community 128 - "5. Touch Variants"
Cohesion: 0.50
Nodes (4): 14. Boot, Flashing, and Debugging, Boot button, Native USB conflict, UART console

### Community 129 - "backlog-filling.md"
Cohesion: 0.50
Nodes (4): 16. Performance and Stability Checklist, Display checklist, LVGL checklist, Touch checklist

### Community 130 - "millis"
Cohesion: 0.67
Nodes (3): 13. ESPHome Notes, ESPHome cautions, Relevant ESPHome components

### Community 131 - "display-wt32-link.md"
Cohesion: 0.67
Nodes (3): 15. Filesystem and Asset Storage, Asset recommendations, Practical partition advice

### Community 132 - "11-key-paths.md"
Cohesion: 0.67
Nodes (3): 4. Display Subsystem, Display interface, Typical timing parameters

### Community 133 - "water-level-test.md"
Cohesion: 0.67
Nodes (3): 5. Touch Variants, Capacitive touch, GT911-class, Resistive touch, XPT2046-class

### Community 134 - "handlePingStream"
Cohesion: 0.67
Nodes (3): SystemMode, MODE_AUTO, MODE_OFF

## Knowledge Gaps
- **97 isolated node(s):** `RFC-4180`, `btn`, `ring`, `halo`, `label` (+92 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 910 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **9 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `my_include` to `Chart`, `display-v9/main.cpp`, `screen_setup_poll`, `network-master (`firmware/network-master`) â€” production WT32 firmware`, `link_serial.cpp`, `screen_control.cpp`, `compilerOptions`, `screen_control.cpp`, `index.md`, `Dashboard — full walkthrough`, `history.ts`, `master.cpp`, `csv.ts`, `screen_data.cpp`, `handle_line`, `dev-db.ts`, `delay`, `PushRow`?**
  _High betweenness centrality (0.150) - this node is a cross-community bridge._
- **Are the 52 inferred relationships involving `millis()` (e.g. with `loop()` and `readCo2Uart()`) actually correct?**
  _`millis()` has 52 INFERRED edges - model-reasoned connections that need verification._
- **What connects `RFC-4180`, `btn`, `ring` to the rest of the system?**
  _97 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Chart` be split into smaller, more focused modules?**
  _Cohesion score 0.09508196721311475 - nodes in this community are weakly interconnected._
- **Why does `authenticate()` connect `authenticate` to `screen_control.cpp`, `new-slave.cpp`, `ESP to database sync — plan`, `csv.ts`, `data.ts`?**
  _High betweenness centrality (0.124) - this node is a cross-community bridge._
- **Are the 29 inferred relationships involving `delay()` (e.g. with `loop()` and `setup()`) actually correct?**
  _`delay()` has 29 INFERRED edges - model-reasoned connections that need verification._
- **Should `display-v9/main.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.080338266384778 - nodes in this community are weakly interconnected._