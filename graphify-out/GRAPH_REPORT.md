# Graph Report - rusim-monorepo  (2026-11-10)

## Corpus Check
- 166 files · ~479,079 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 12 file(s) not represented in the graph (top: (none) 4, .toml 2, .example 1)

## Summary
- 2111 nodes · 3875 edges · 125 communities (114 shown, 11 thin omitted)
- Extraction: 82% EXTRACTED · 18% INFERRED · 0% AMBIGUOUS · INFERRED: 685 edges (avg confidence: 0.91)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `922622bf`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- getParameterValue
- display-v9/main.cpp
- String
- screen_control.cpp
- web_control.py
- link_provision_stub.cpp
- network-master-passthrough.py
- monitor-legacy-pi.ts
- millis
- millis
- wt32-eth01_captive-recon/main.cpp
- link_serial.cpp
- db_contract.h
- ui_modal_show
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
- SimESPClass
- screen_setup_poll
- auth.ts
- ui_modal_show
- serial_worker
- Display <-> WT32 link: `rusim-line` v1
- render_current
- Legacy slave firmware (`legacy/slave/new-slave.cpp`)
- ATmega2560 Mega Mini — Development & Reference Documentation
- LVGL v8 to v9 migration report
- DbFault
- ui_shell.cpp
- data.ts
- Endpoint details
- WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)
- hardware.md
- Reference: `device_data` column detail
- raspi-docs/README.md
- ESP32-8048S050C · Board & Archive Guide
- dbPassTick
- history.ts
- ui_mk_label
- render_current
- info_render
- WT32-ETH01 — LLM Documentation
- csv.ts
- DbPass
- Phone credential provisioning on the display (`provision_ap`)
- PageScan
- Dashboard — full walkthrough
- screen_data.cpp
- 13. Flask REST API reference — `web_control.py`
- provision_ap.cpp
- ESP32-8048S050 — LLM Generated Summary
- temp-probe/main.cpp
- Display <-> WT32 link: `rusim-line` v1
- Stage
- log_data_to_csv
- network-master/main.cpp
- Memory Safety Patterns
- Repository Index
- flask_cli.py
- Stack
- authenticate
- delay
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
- DB experiment sync + NTP — LLM Documentation
- Arduino.h
- compare.ts
- PlatformIO build environments (`platformio.ini`)
- Raspberry Pi `water-control-system` mirror
- 1. Hardware & OS
- raspi-docs/README.md
- Dashboard public-deploy security report
- `leaf` terminal reader on the Pi
- master.cpp
- sens_poll
- 8. Auxiliary tools & scripts
- Display `IS_DEBUG` flag
- learned-mistakes.md
- Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)
- index.md
- 10. ESP-IDF Recommendations
- 6. Common Pin Map Reference
- 9. Network & services
- Display LVGL 9.3.0 -> 9.6.0 port
- co2-calibrate/main.cpp
- 7. Data & logging
- Stage
- 11. LVGL Recommendations
- 12. Arduino / PlatformIO Notes
- 14. Boot, Flashing, and Debugging
- 16. Performance and Stability Checklist
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants

## God Nodes (most connected - your core abstractions)
1. `millis()` - 61 edges
2. `logf()` - 28 edges
3. `delay()` - 28 edges
4. `Dashboard()` - 25 edges
5. `ui_update_bell()` - 23 edges
6. `render_current()` - 23 edges
7. `handle_line()` - 23 edges
8. `Endpoint details` - 21 edges
9. `ESP32-8048S050 — LLM Generated Summary` - 21 edges
10. `dbPassTick()` - 20 edges

## Surprising Connections (you probably didn't know these)
- `3.1.1 `SENS` sensor values` --references--> `sens_apply_real()`  [INFERRED]
  docs/llm-generated/link-serial-protocol.md → firmware/display-v9/fake_data.cpp
- `3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative)` --references--> `ui_mk_button()`  [INFERRED]
  docs/llm-generated/lvgl-redesign.md → firmware/display-v9/ui_shell.cpp
- `6. Log forwarding` --references--> `logf()`  [INFERRED]
  docs/llm-generated/link-serial-protocol.md → firmware/network-master/main.cpp
- `9. Failure modes` --references--> `logf()`  [INFERRED]
  docs/llm-generated/link-serial-protocol.md → firmware/network-master/main.cpp
- `8. Credential lifetime` --references--> `clearCreds()`  [INFERRED]
  docs/llm-generated/display-provisioning-ap.md → firmware/network-master/main.cpp

## Import Cycles
- None detected.

## Communities (125 total, 11 thin omitted)

### Community 0 - "getParameterValue"
Cohesion: 0.16
Nodes (16): createChartPoints(), getMetricValue(), getParameterValue(), ReadingWithStatus, Metadata, 12.1 The experiment list is bounded by the monitoring fetch's row cap, 12.3 Dead code, 12.5 Security posture (+8 more)

### Community 2 - "String"
Cohesion: 0.16
Nodes (24): 1. Usage, 2. The five steps, 3. Helper functions and their firmware counterparts, 4. Differences from the firmware, 5. Testing, 6. Related documents, Captive portal login from Linux (bash), 7.1 `Serial` becomes the link (+16 more)

### Community 3 - "screen_control.cpp"
Cohesion: 0.05
Nodes (71): Display (`firmware/display-v9/`), 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.3 State and data model changes needed, 3.2.1.4 Functions to add / change, 3.2.1.5 Polling, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels), 3.2.1.8 Out of scope for this screen (stays on /control) (+63 more)

### Community 4 - "web_control.py"
Cohesion: 0.09
Nodes (22): UI, 14. `GET /read_log`, download_info(), export_data(), get_clock_payload(), get_drives(), get_interface_ip(), get_ip() (+14 more)

### Community 5 - "link_provision_stub.cpp"
Cohesion: 0.05
Nodes (8): link_serial_get_sens(), link_serial_init(), link_serial_poll(), link_serial_set_exp_state(), provision_ap_passphrase(), provision_ap_ssid(), provision_ap_wifi_qr_payload(), main()

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 7 - "monitor-legacy-pi.ts"
Cohesion: 0.07
Nodes (45): 1. Usage, 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents (+37 more)

### Community 8 - "millis"
Cohesion: 0.14
Nodes (13): 1. Environment, 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential state (+5 more)

### Community 9 - "millis"
Cohesion: 0.24
Nodes (18): Master (`firmware/network-master/main.cpp`), Pull: partially built, 4. State machine, 6.1 Database path (configuration, runs and uploads), 6. Memory and limits, millis(), dbFaultName(), dbPassFinish() (+10 more)

### Community 10 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.14
Nodes (20): 2. What it does, extractWindowLocation(), loop(), onEthEvent(), PageScan, cloudflareChallenge, formAction, hasForm (+12 more)

### Community 11 - "link_serial.cpp"
Cohesion: 0.10
Nodes (30): arg_lookup(), echo_line(), emit_creds(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line() (+22 more)

### Community 12 - "db_contract.h"
Cohesion: 0.06
Nodes (42): BottleSample, bottleId, ch4, co2, nullMask, ph, pressure, tempC (+34 more)

### Community 13 - "ui_modal_show"
Cohesion: 0.23
Nodes (12): 9.1 The association popup, 1.1 Router (`ui_shell.cpp`, `s_route`), 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 3.10 Overheat warning, 41 °C, 3.11 Overheat breaker trip, 45 °C, screen_setup_show() (+4 more)

### Community 14 - "compilerOptions"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 15 - "Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)"
Cohesion: 0.17
Nodes (7): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)

### Community 16 - "Pin Layout Overview"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 17 - "Display UI — LLM Documentation (`display-v9`, fake-data pass)"
Cohesion: 0.11
Nodes (25): 4.1 pH, 4.4 Pressure (HX711 / HX710B), 1. Where the data comes from today, 2. Decisions taken (2026-10-24), 3. Target wiring, 4. Protocol facts the ESP has to honour, 6. Deferred, with the hook each needs, 7. Verification (+17 more)

### Community 18 - "legacy-master-usb.ts"
Cohesion: 0.06
Nodes (46): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_WIDTHS, clock(), configurePort(), createLineGate(), Device (+38 more)

### Community 19 - "touch.h"
Cohesion: 0.12
Nodes (10): 2. Hardware / platform notes, 3.14 Backlight sleep, my_touchpad_read(), setup(), sleep_note_input(), touch(), touch_has_signal(), touch_init() (+2 more)

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
Cohesion: 0.10
Nodes (19): 1. Current state, 2. The database forbids writes, 3. Nothing reads the sensors, 4. Why the device must not send `token_hash`, 5. Credential exposure on the WT32, 7. Correctness rules for the push (decided), 8. Build plan, 9. Related documents (+11 more)

### Community 26 - "captive-portal-login.sh"
Cohesion: 0.32
Nodes (11): attempt_login(), extract_window_location(), form_action(), log(), probe_once(), prompt_credentials(), resolve_action(), captive-portal-login.sh script (+3 more)

### Community 27 - "sync_pi.sh"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 28 - "dev-db.ts"
Cohesion: 0.10
Nodes (26): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+18 more)

### Community 29 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 30 - "Network master ↔ database communication plan"
Cohesion: 0.12
Nodes (16): 1. Recommendation and evidence, 2. Confirmed gaps to address, 3.1 Authenticate, 3.2 Pull device and experiment information, 3.3 Upload one collected round, 3.4 Failure handling, 3. HTTP contract to implement, 4. Confirmed decisions (+8 more)

### Community 32 - "screen_setup_poll"
Cohesion: 0.09
Nodes (34): 13. Light theme and the /info network QR, 6. Identity and memory, 3.13 Phone-associated popup, 3.15 SoftAP lifecycle, 3.16 Debug-only UI, 3.1 Route `/setup`, 3.7.1.1 Removed content, 3.7.1.3 Gauge colour (+26 more)

### Community 33 - "auth.ts"
Cohesion: 0.10
Nodes (23): dependencies, react@19.1.1, react-dom@19.1.1, App(), AuthState, readStoredSession(), StoredSession, useAuth() (+15 more)

### Community 34 - "ui_modal_show"
Cohesion: 0.13
Nodes (17): 2.1 Styling, 2.2 Navigation, 2.5 Write commands, 2.6 LVGL configuration, 2.7 Shared hold-to-confirm round button, 2. Cross-cutting, 3.7.1.2 Layout, 3.9 Blocking modal primitive (+9 more)

### Community 35 - "serial_worker"
Cohesion: 0.10
Nodes (24): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Runtime, Three copies exist, What it does, 4. Backend — `web_control.py`, Background workers, `POST /control` Actions (+16 more)

### Community 36 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.05
Nodes (35): Next step (open), 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1.1 `SENS` sensor values, 3.1 Display <- WT32, 3.2.1 `DB` configuration fields, 3.2 WT32 <- display (+27 more)

### Community 37 - "render_current"
Cohesion: 0.14
Nodes (17): 3.3.1.1 Removed content, 3.3.1.2 Layout (content coordinates, panel 800x436), 3.3.1.4 Behaviour kept, 3.3.1.5 Open item, 3.3.1 Redesign plan — `/data` node picker, 3.3 Route `/data` — node picker, 3.4.1.1 Removed content, 3.4.1.2 Layout (content coordinates, panel 800x436) (+9 more)

### Community 38 - "Legacy slave firmware (`legacy/slave/new-slave.cpp`)"
Cohesion: 0.13
Nodes (15): 1. What the program does, 2. Build environment, 3. Pin map, 4.3 CO2 (MH-Z19C, PWM), 4.5 Temperature (DS18B20), 4. Per-sensor processing, 5.1 Request, 5.2 Response (+7 more)

### Community 39 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 41 - "LVGL v8 to v9 migration report"
Cohesion: 0.15
Nodes (12): 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, File-by-file summary, LVGL v8 to v9 migration report, Memory and runtime considerations (+4 more)

### Community 42 - "DbFault"
Cohesion: 0.25
Nodes (8): DbFault, DB_FAULT_AUTH, DB_FAULT_CLOCK, DB_FAULT_IDENTITY, DB_FAULT_NET, DB_FAULT_NONE, DB_FAULT_PAYLOAD, DB_FAULT_SCHEMA

### Community 43 - "ui_shell.cpp"
Cohesion: 0.13
Nodes (15): 4. Screen tree / state machine (as built), 3.8.1.1 Removed content, 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative), 3.8.1.3 Clock, 3.8.1.5 Interaction notes, 3.8.1 Redesign plan — top bar, 3.8 Persistent status bar, build_status_bar() (+7 more)

### Community 44 - "data.ts"
Cohesion: 0.14
Nodes (26): BottleChart(), BrandMark(), Chart(), ChartInteraction, chartPath(), CompareActivePoint, CompareChart(), CompareIcon() (+18 more)

### Community 45 - "Endpoint details"
Cohesion: 0.09
Nodes (23): 10. `GET|POST /m/<filename>`, 11. `POST /set_mode`, 12. `POST /command`, 13. Flask REST API reference — `web_control.py`, 13. `GET /files`, 15. `GET /get_drives`, 16. `POST /export`, 17. `GET /get_ip` (+15 more)

### Community 46 - "WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)"
Cohesion: 0.25
Nodes (7): 1. Environment, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)

### Community 47 - "hardware.md"
Cohesion: 0.20
Nodes (10): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+2 more)

### Community 48 - "Reference: `device_data` column detail"
Cohesion: 0.10
Nodes (20): Access control, Chamber numbering, Database schema catalog — LLM Documentation, Development tooling, `device_data`: active application schema, `device_data.device_config`, `device_data.devices`, `device_data.experiments` (+12 more)

### Community 49 - "raspi-docs/README.md"
Cohesion: 0.13
Nodes (7): Raspberry Pi kiosk UI CSS redesign, 5. Frontend UI — `templates/index.html`, Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer, Tabs, Temporary MOSFET Speed Control (`0–255` Debit Testing), 11. Key paths quick reference, Documentation destination

### Community 50 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 51 - "dbPassTick"
Cohesion: 0.13
Nodes (22): dbArrayIsEmpty(), dbClassify(), DbFetch, DB_FETCH_AUTH, DB_FETCH_FAIL, DB_FETCH_OK, DB_FETCH_PAYLOAD, dbGetJson() (+14 more)

### Community 52 - "history.ts"
Cohesion: 0.13
Nodes (21): ExperimentHistoryView(), formatDate(), isTruncated(), ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber(), HISTORY_PARAMETERS (+13 more)

### Community 53 - "ui_mk_label"
Cohesion: 0.20
Nodes (8): `clock_sync_worker` (daemon thread), clock_sync_worker(), _extract_portal_redirect_url(), _fetch_http_date(), _get_default_gateway_ip(), _NoRedirectHandler, _parse_http_date_header(), sync_clock_from_lan_http()

### Community 54 - "render_current"
Cohesion: 0.20
Nodes (18): Decoded backtrace, 4. Lifecycle, Widget lifetime rule, 3.12 Bell and invalid-field list, 3.3.1.3 Back button, 3.8.1.4 Functions in `ui_shell.cpp` / `ui_shell.h`, screen_control_hide(), screen_home_hide() (+10 more)

### Community 55 - "info_render"
Cohesion: 0.12
Nodes (17): Current state of the code, Experiments and results, /info Reboot: Heap-Walk Hang on `display-v9`, Symptom, 3.7.1.4 Gauge fill rules, link_serial_db_ready(), link_serial_get_db_ack_age_ms(), link_serial_get_db_active_dur() (+9 more)

### Community 56 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.11
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 57 - "csv.ts"
Cohesion: 0.19
Nodes (14): csvDisabledReason(), downloadExperimentCsv(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv() (+6 more)

### Community 58 - "DbPass"
Cohesion: 0.22
Nodes (9): DbPass, DB_PASS_AUTH, DB_PASS_CONFIG, DB_PASS_DEVICES, DB_PASS_EXPERIMENTS, DB_PASS_IDLE, DB_PASS_LATEST, DB_PASS_PUSH (+1 more)

### Community 59 - "Phone credential provisioning on the display (`provision_ap`)"
Cohesion: 0.13
Nodes (14): 10. Serial commands, 11. What is not covered, 12. Verify, 13. Related documents, 1. Why, 2. The two-step flow, 3. QR payload format, 5. Cloaked and encrypted, and why (+6 more)

### Community 60 - "PageScan"
Cohesion: 0.29
Nodes (7): PageScan, formAction, hasForm, hasUsernameField, magic, redir, totalBytes

### Community 61 - "Dashboard — full walkthrough"
Cohesion: 0.06
Nodes (36): summarizeParameter(), 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 11. The writer, 1.1 File map, 1.2 What it is not (+28 more)

### Community 62 - "screen_data.cpp"
Cohesion: 0.31
Nodes (7): on_back_bottle(), on_back_cluster(), on_back_picker(), on_pick_bottle(), on_pick_cluster(), ui_show_bottle(), ui_show_cluster()

### Community 63 - "13. Flask REST API reference — `web_control.py`"
Cohesion: 0.23
Nodes (14): link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), link_serial_set_exp_state(), loop(), handle_creds(), handle_line(), parse_onoff() (+6 more)

### Community 64 - "provision_ap.cpp"
Cohesion: 0.19
Nodes (4): braced(), declarations(), MethaneCalibrationRegression, native_source()

### Community 65 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.15
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 66 - "temp-probe/main.cpp"
Cohesion: 0.46
Nodes (7): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup()

### Community 67 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.17
Nodes (16): link_serial_creds_ready(), link_serial_result(), build_identity(), handle_creds(), handle_status(), note_activity(), provision_ap_awaiting_verdict(), provision_ap_begin() (+8 more)

### Community 68 - "Stage"
Cohesion: 0.33
Nodes (6): Stage, STAGE_DONE, STAGE_PROMPT_PASS, STAGE_PROMPT_USER, STAGE_RECON, STAGE_WAIT_IP

### Community 69 - "log_data_to_csv"
Cohesion: 0.17
Nodes (13): 7. Data & logging, CSV format (current schema), Existing logs, Older schema (2025-12-12.csv), Storage, USB export, 7. `POST /toggle_logging`, fmt_val() (+5 more)

### Community 70 - "network-master/main.cpp"
Cohesion: 0.12
Nodes (30): 7.2 Link plumbing, sleep_tick(), digitalWrite(), arg_lookup(), clearCreds(), dbApplyRunState(), handle_creds(), handle_experiment() (+22 more)

### Community 71 - "Memory Safety Patterns"
Cohesion: 0.18
Nodes (10): 1. Memory Bug Categories, 2. Safety Spectrum, Best Practices, Core Concepts, Debugging Tools, Detailed patterns and worked examples, Do's, Don'ts (+2 more)

### Community 72 - "Repository Index"
Cohesion: 0.18
Nodes (11): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 4. Hardware references, 5. Human documents, 6. Where new work belongs (+3 more)

### Community 73 - "flask_cli.py"
Cohesion: 0.23
Nodes (5): get_json(), main(), _open(), post_download(), post_json()

### Community 74 - "Stack"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 75 - "authenticate"
Cohesion: 0.21
Nodes (14): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), CONCLUDED_AFTER_MS, fetchDevice(), fetchExperimentReadings(), fetchReadings(), MAX_READING_ROWS (+6 more)

### Community 76 - "delay"
Cohesion: 0.27
Nodes (10): delay(), loop(), rampUp(), setup(), stopMotor(), initializeSensor(), loop(), setup() (+2 more)

### Community 77 - "compile_commands.json and gen-compile-db.sh"
Cohesion: 0.29
Nodes (6): 1. What the file is, 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh

### Community 78 - "ui_mk_label"
Cohesion: 0.17
Nodes (10): Global state, `polling_worker` (daemon thread), control_api(), desired_state_commands(), polling_worker(), state_mismatch_commands(), stir_command(), threshold_command() (+2 more)

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
Cohesion: 0.25
Nodes (7): 12. Copies & git state, Current state, Divergence between copies (historical), Firmware (water-level handling), Initial commit, Local documentation origin, `web_control.py`

### Community 87 - "DB experiment sync + NTP — LLM Documentation"
Cohesion: 0.50
Nodes (4): Auth used, Build, DB experiment sync + NTP — LLM Documentation, Link (`rusim-line` v1)

### Community 90 - "Arduino.h"
Cohesion: 0.12
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
Cohesion: 0.29
Nodes (6): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account

### Community 96 - "raspi-docs/README.md"
Cohesion: 0.29
Nodes (6): 2. Boot & kiosk session model, Boot sequence, Chromium kiosk, Notes / caveats, openbox autostart (`~/.config/openbox/autostart`), PM2 service (`/etc/systemd/system/pm2-invitro.service`)

### Community 97 - "Dashboard public-deploy security report"
Cohesion: 0.17
Nodes (11): 1. Blocker — legacy `public` schema is public-read (live, confirmed), 2. High — `authenticate()` has no brute-force protection, 3. High — Netlify bundle + env handling, 4. Medium — session hygiene and PII, 5. Low — functional / ops, Already fixed / verified good (keep these), Dashboard public-deploy security report, Findings (ordered by severity) (+3 more)

### Community 98 - "`leaf` terminal reader on the Pi"
Cohesion: 0.29
Nodes (6): How the cross-compile works, `leaf` terminal reader on the Pi, Rebuilding and removing, Running it, Verification, Why it is not compiled on the Pi

### Community 100 - "master.cpp"
Cohesion: 0.05
Nodes (62): 10. Known Gaps, 1. Overview, 2. Pin Assignment, 3. Boot Defaults, 4.1 Trip thresholds, 4.2 Asymmetric Fast-Trip / Slow-Recovery Debouncing, 4.3 Optional Pump Leak Detection Probe (`A8`), 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing (+54 more)

### Community 101 - "sens_poll"
Cohesion: 0.14
Nodes (16): The Backlog, display-v9: DB fields from the `DB` line, Memory-safety hardening (`display-v9`, `network-master`), Verification, 5.1 As built, 5. ESP implementation steps, Source references, BottleReading (+8 more)

### Community 102 - "8. Auxiliary tools & scripts"
Cohesion: 0.18
Nodes (8): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc, 10. Security & caveats, Operational caveats, Recommendations, Security

### Community 103 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Disabling it, Display `IS_DEBUG` flag, Rules for new uses

### Community 105 - "learned-mistakes.md"
Cohesion: 0.29
Nodes (6): 2026-10-05, 2026-10-06, 2026-10-08, 2026-10-20, 2026-10-24, 2026-10-31

### Community 106 - "Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`)"
Cohesion: 0.29
Nodes (6): 1. Usage, 2. Controls (`POST /control`), 3. Server (`POST` + `GET`), Legacy Flask CLI (`legacy/pi/water-control-system/flask_cli.py`), 8. `POST /download/info`, status()

### Community 108 - "index.md"
Cohesion: 0.14
Nodes (6): 3D Models, PCB, Display <-> WT32 link (`display-wt32-link`) — replaced, Water level sensor test, gen-compile-db.sh script, install-leaf.sh script

### Community 109 - "10. ESP-IDF Recommendations"
Cohesion: 0.33
Nodes (6): 10. ESP-IDF Recommendations, ESP-IDF components you may use, Important menuconfig areas, Recommended minimum version, Target setup, Typical required features

### Community 110 - "6. Common Pin Map Reference"
Cohesion: 0.33
Nodes (6): 6.1 RGB display control pins, 6.2 RGB data pins, common capacitive variant, 6.3 GT911 capacitive touch pins, 6.4 Other common board pins, 6.5 Known documentation trap, 6. Common Pin Map Reference

### Community 111 - "9. Network & services"
Cohesion: 0.29
Nodes (6): 9. Network & services, Enabled services (selected), Listening ports, Network Architecture (LAN Primary), Security note (see §10), disable_wifi_client_and_stop_hotspot()

### Community 113 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 114 - "co2-calibrate/main.cpp"
Cohesion: 0.60
Nodes (4): loop(), readCo2Ppm(), setup(), waitForLevel()

### Community 115 - "7. Data & logging"
Cohesion: 0.09
Nodes (28): 10. WT32 link (`rusim-line` v1, production), 11. Setup route, 12. Credential entry by QR (`provision_ap`), 14. Debug flag `IS_DEBUG`, 1. Overview, 3. File map, 5. Fake-data serial protocol, 6. Bell / NULL semantics (fake) (+20 more)

### Community 117 - "Stage"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

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

## Knowledge Gaps
- **86 isolated node(s):** `DB_FAULT_NONE`, `DB_FAULT_NET`, `DB_FAULT_AUTH`, `DB_FAULT_PAYLOAD`, `DB_FAULT_SCHEMA` (+81 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 899 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **11 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `millis` to `display-v9/main.cpp`, `String`, `screen_control.cpp`, `wt32-eth01_captive-recon/main.cpp`, `link_serial.cpp`, `Pin Layout Overview`, `Display UI — LLM Documentation (`display-v9`, fake-data pass)`, `touch.h`, `analogRead`, `screen_setup_poll`, `ui_modal_show`, `ui_shell.cpp`, `info_render`, `13. Flask REST API reference — `web_control.py``, `temp-probe/main.cpp`, `Display <-> WT32 link: `rusim-line` v1`, `network-master/main.cpp`, `delay`, `Arduino.h`, `master.cpp`, `sens_poll`, `co2-calibrate/main.cpp`, `7. Data & logging`?**
  _High betweenness centrality (0.097) - this node is a cross-community bridge._
- **Are the 60 inferred relationships involving `millis()` (e.g. with `PWM Output Pins` and `7.3 State machine (semantics kept, shape changed)`) actually correct?**
  _`millis()` has 60 INFERRED edges - model-reasoned connections that need verification._
- **What connects `DB_FAULT_NONE`, `DB_FAULT_NET`, `DB_FAULT_AUTH` to the rest of the system?**
  _86 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `screen_control.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.05479059093516925 - nodes in this community are weakly interconnected._
- **Why does `ESP32-8048S050 — LLM Generated Summary` connect `ESP32-8048S050 — LLM Generated Summary` to `5. Touch Variants`, `index.md`, `10. ESP-IDF Recommendations`, `6. Common Pin Map Reference`, `11. LVGL Recommendations`, `12. Arduino / PlatformIO Notes`, `14. Boot, Flashing, and Debugging`, `16. Performance and Stability Checklist`, `13. ESPHome Notes`, `15. Filesystem and Asset Storage`, `4. Display Subsystem`?**
  _High betweenness centrality (0.062) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `logf()` (e.g. with `6. Log forwarding` and `9. Failure modes`) actually correct?**
  _`logf()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Should `web_control.py` be split into smaller, more focused modules?**
  _Cohesion score 0.08558558558558559 - nodes in this community are weakly interconnected._