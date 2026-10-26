# Graph Report - rusim-monorepo  (2026-10-26)

## Corpus Check
- 159 files · ~448,475 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 12 file(s) not represented in the graph (top: (none) 4, .toml 2, .example 1)

## Summary
- 1907 nodes · 3431 edges · 131 communities (120 shown, 11 thin omitted)
- Extraction: 82% EXTRACTED · 18% INFERRED · 0% AMBIGUOUS · INFERRED: 631 edges (avg confidence: 0.91)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `df24b67e`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- App.tsx
- display-v9/main.cpp
- network-master/main.cpp
- millis
- web_control.py
- link_provision_stub.cpp
- network-master-passthrough.py
- monitor-legacy-pi.ts
- wt32-eth01_ping-test/main.cpp
- dev-db.ts
- wt32-eth01_captive-recon/main.cpp
- link_serial.cpp
- read-legacy-slave.ts
- new-slave.cpp
- compilerOptions
- master.cpp
- Pin Layout Overview
- screen_setup_poll
- handle_line
- touch.h
- package.json
- Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)
- flash-factory.ts
- delay
- Arduino.h
- fake_init
- captive-portal-login.sh
- sync_pi.sh
- fake_data.cpp
- 5. Checklist
- PushRow
- SimESPClass
- temp-probe/main.cpp
- auth.ts
- ui_modal_show
- 4. Backend — `web_control.py`
- methane-calibrate/main.cpp
- render_current
- co2-calibrate/main.cpp
- ATmega2560 Mega Mini — Development & Reference Documentation
- gen-compile-db.sh
- install-leaf.sh
- ui_shell.cpp
- data.ts
- Endpoint details
- getParameterValue
- index.md
- Reference: `device_data` column detail
- raspi-docs/README.md
- ESP32-8048S050C · Board & Archive Guide
- syncDbLatest
- history.ts
- screen_setup_poll
- millis
- screen_info.cpp
- WT32-ETH01 — LLM Documentation
- csv.ts
- ui_init
- Phone credential provisioning on the display (`provision_ap`)
- BottleReading
- Dashboard — full walkthrough
- 5. Frontend UI — `templates/index.html`
- WT32-ETH01 Ethernet ping test (`firmware/wt32-eth01_ping-test`)
- Sensor test and calibration firmware
- ESP32-8048S050 — LLM Generated Summary
- Display UI — LLM Documentation (`display-v9`, fake-data pass)
- Display <-> WT32 link: `rusim-line` v1
- network-master: bounded buffers
- log_data_to_csv
- logf
- Memory Safety Patterns
- Repository Index
- network-master (`firmware/network-master`) â€” production WT32 firmware
- Stack
- authenticate
- CALIBRATION NOTES
- compile_commands.json and gen-compile-db.sh
- Pi sync script — `sync_pi.sh`
- 6. Master firmware — `legacy/master/master.cpp`
- screen_data.cpp
- main
- LLM Agents Guide
- Patterns by Language
- handlePingStream
- 12. Copies & git state
- CsvDownloadHandler
- Captive portal login from Linux (bash)
- my_touchpad_read
- .begin
- ESP to database sync — plan
- Legacy Pi remote monitor (`scripts-hardware/monitor-legacy-pi.ts`)
- PlatformIO build environments (`platformio.ini`)
- LVGL v8 to v9 migration report
- Raspberry Pi `water-control-system` mirror
- 1. Hardware & OS
- 2. Boot & kiosk session model
- 9. Network & services
- `leaf` terminal reader on the Pi
- PageScan
- getSupabase
- 4. Authentication, end to end
- 9. Development tooling: `bun run dev-db`
- Display `IS_DEBUG` flag
- 5. Credential exposure on the WT32
- learned-mistakes.md
- Required changes
- send_serial_cmd
- Dashboard public-deploy security report
- 10. ESP-IDF Recommendations
- 6. Common Pin Map Reference
- 6. From a row to a chart point
- DB experiment sync + NTP — LLM Documentation
- Display LVGL 9.3.0 -> 9.6.0 port
- Display V9 PC Simulator (`[env:display-sim]`)
- 8. Auxiliary tools & scripts
- 10. Security & caveats
- Stage
- 4. State and flows
- 11. LVGL Recommendations
- 12. Arduino / PlatformIO Notes
- 14. Boot, Flashing, and Debugging
- 16. Performance and Stability Checklist
- 1. What it is
- 11-key-paths.md
- 13. ESPHome Notes
- 15. Filesystem and Asset Storage
- 4. Display Subsystem
- 5. Touch Variants

## God Nodes (most connected - your core abstractions)
1. `millis()` - 52 edges
2. `delay()` - 36 edges
3. `handle_line()` - 23 edges
4. `ui_update_bell()` - 23 edges
5. `render_current()` - 23 edges
6. `logf()` - 23 edges
7. `ESP32-8048S050 — LLM Generated Summary` - 21 edges
8. `Dashboard()` - 20 edges
9. `compilerOptions` - 20 edges
10. `screen_setup_poll()` - 20 edges

## Surprising Connections (you probably didn't know these)
- `5.5 The Monitoring view` --references--> `displayChamberNumber()`  [INFERRED]
  docs/llm-generated/dashboard.md → dashboard/src/chambers.ts
- `10.3 The lockdown migration` --references--> `authenticate()`  [INFERRED]
  docs/llm-generated/dashboard.md → dashboard/src/data.ts
- `12.5 Security posture` --references--> `authenticate()`  [INFERRED]
  docs/llm-generated/dashboard.md → dashboard/src/data.ts
- `13.3 Migration timeline` --references--> `authenticate()`  [INFERRED]
  docs/llm-generated/dashboard.md → dashboard/src/data.ts
- `4.5 Client-side rate limiting` --references--> `authenticate()`  [INFERRED]
  docs/llm-generated/dashboard.md → dashboard/src/data.ts

## Import Cycles
- None detected.

## Communities (131 total, 11 thin omitted)

### Community 0 - "App.tsx"
Cohesion: 0.18
Nodes (25): BottleChart(), Chart(), ChartInteraction, createTimeLabels(), Dashboard(), DashboardView, ExperimentHistoryView(), ExperimentResultsView() (+17 more)

### Community 2 - "network-master/main.cpp"
Cohesion: 0.19
Nodes (15): doLogin(), extractWindowLocation(), fetchData(), fetchLoginPage(), fetchPortalUrl(), onEthEvent(), postCredentials(), readBody() (+7 more)

### Community 3 - "millis"
Cohesion: 0.05
Nodes (72): Display (`firmware/display-v9/`), 3.2.1.1 Symbols, 3.2.1.2 Layout (content coordinates, panel 800x436), 3.2.1.3 State and data model changes needed, 3.2.1.4 Functions to add / change, 3.2.1.5 Polling, 3.2.1.7 Final text inventory (uses `LV_SYMBOL_*`, no fake labels), 3.2.1.8 Out of scope for this screen (stays on /control) (+64 more)

### Community 4 - "web_control.py"
Cohesion: 0.09
Nodes (17): download_info(), export_data(), get_drives(), get_interface_ip(), get_ip(), get_lan_ip(), get_timezones(), get_wlan_ip() (+9 more)

### Community 5 - "link_provision_stub.cpp"
Cohesion: 0.07
Nodes (3): provision_ap_passphrase(), provision_ap_ssid(), provision_ap_wifi_qr_payload()

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 7 - "monitor-legacy-pi.ts"
Cohesion: 0.09
Nodes (38): 1. Usage, Source-level notes, Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_IDS, BOTTLE_WIDTHS, cell() (+30 more)

### Community 8 - "wt32-eth01_ping-test/main.cpp"
Cohesion: 0.11
Nodes (31): doLogin(), extractWindowLocation(), fetchData(), fetchLoginPage(), fetchPortalUrl(), loop(), onEthEvent(), PageScan (+23 more)

### Community 9 - "dev-db.ts"
Cohesion: 0.10
Nodes (25): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+17 more)

### Community 10 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.07
Nodes (33): 1. Environment, 2. What it does, 3. Memory behaviour, 4. Capturing the output, 5. Serial credential entry, 6. Limits, 7. Related documents, WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`) (+25 more)

### Community 11 - "link_serial.cpp"
Cohesion: 0.08
Nodes (37): arg_lookup(), echo_line(), emit_creds(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line() (+29 more)

### Community 12 - "read-legacy-slave.ts"
Cohesion: 0.10
Nodes (28): 1. Usage, 2. Startup settling, 3. Output, 4. How it reads the port, 5. Limits, 6. Related documents, Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`), ALIGN (+20 more)

### Community 13 - "new-slave.cpp"
Cohesion: 0.09
Nodes (32): 1. What the program does, 2. Build environment, 3. Pin map, 4.1 pH, 4.2 Methane (TGS2611), 4.3 CO2 (MH-Z19C, PWM), 4.4 Pressure (HX711 / HX710B), 4.5 Temperature (DS18B20) (+24 more)

### Community 14 - "compilerOptions"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 15 - "master.cpp"
Cohesion: 0.12
Nodes (29): 1. Overview, 2. Pin Assignment, 3. Boot Defaults, 4. Stirrer MOSFET PWM Ramping, 5. Pump MOSFET PWM Ramping, 6. Raspberry Pi USB Serial Protocol, 7. Build and Deployment, Legacy Master Firmware (`legacy/master/master.cpp`) (+21 more)

### Community 16 - "Pin Layout Overview"
Cohesion: 0.05
Nodes (39): 1. Motor Driver Module, 2. WT32-ETH01, 3. ESP32-8048S050, 4.4 DS18B20 (temperature), 4. ATmega2560 Mega Mini, 5.1 HX710B (pressure), 5.2 TGS2611 (CH4), 5.3 MH-Z19C (CO2) (+31 more)

### Community 17 - "screen_setup_poll"
Cohesion: 0.15
Nodes (15): 6. Identity and memory, provision_ap_active(), provision_ap_client_count(), provision_ap_hidden(), provision_ap_passphrase(), provision_ap_ssid(), provision_ap_url(), provision_ap_wifi_qr_payload() (+7 more)

### Community 18 - "handle_line"
Cohesion: 0.20
Nodes (16): 3.10 Overheat warning, 41 °C, link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), loop(), provision_ap_end(), provision_ap_poll(), handle_creds() (+8 more)

### Community 20 - "package.json"
Cohesion: 0.05
Nodes (40): @supabase/supabase-js@2.57.4, devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13, @types/react-dom@19.1.9, typescript@5.9.2, vite@7.1.7 (+32 more)

### Community 21 - "Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)"
Cohesion: 0.06
Nodes (45): 10. Failure modes summary, 11. Follow-on: RS485 (later phase, separate firmware work), 12. Documentation updates to make when implementing, 1. Scope, 2. Current state (what is being replaced), 3. Target physical layer, 4.1 Line rules (normative), 4.2 Command set (+37 more)

### Community 22 - "flash-factory.ts"
Cohesion: 0.15
Nodes (21): 1. What it flashes, 2. Running it, 3. Port selection, 4. Flashing and retry, 5. Requirements, 6. Local assets, 7. Known gaps, 8. Related documents (+13 more)

### Community 23 - "delay"
Cohesion: 0.26
Nodes (11): delay(), initializeSensor(), loop(), setup(), loop(), rampUp(), setup(), stopMotor() (+3 more)

### Community 25 - "fake_init"
Cohesion: 0.40
Nodes (5): 3. File map, fake_init(), setup(), randomSeed(), touch_init()

### Community 26 - "captive-portal-login.sh"
Cohesion: 0.35
Nodes (9): extract_window_location(), form_action(), log(), probe_once(), resolve_action(), captive-portal-login.sh script, url_base(), usage() (+1 more)

### Community 27 - "sync_pi.sh"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 28 - "fake_data.cpp"
Cohesion: 0.38
Nodes (5): fake_any_null(), fake_bottle_seed(), fake_null_count(), frand(), random()

### Community 29 - "5. Checklist"
Cohesion: 0.07
Nodes (29): 1. What Are We Doing?, 2.1 Hardware Platform: WT32-ETH01, 2.2 Software Architecture, 2.3 Key Challenges, 2.4 What the Unpad Portal Actually Does (measured 2026-10-02), 2.5 Browser capture findings (`pintas.unpad.ac.id_Archive...har`, 2026-10-02), 2. How Can We Do It?, 3. Relevant Document Links, Datasheets, and Repositories (+21 more)

### Community 30 - "PushRow"
Cohesion: 0.14
Nodes (14): 2.1 Display link (UART0), 2.2 Mega slave link (UART1), 2.3 Mega slave poll behaviour, 2. Wiring, PushRow, bottleId, ch4, co2 (+6 more)

### Community 32 - "temp-probe/main.cpp"
Cohesion: 0.46
Nodes (7): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup()

### Community 33 - "auth.ts"
Cohesion: 0.12
Nodes (19): dependencies, react@19.1.1, react-dom@19.1.1, App(), LoginScreen(), AuthState, readStoredSession(), StoredSession (+11 more)

### Community 34 - "ui_modal_show"
Cohesion: 0.10
Nodes (26): 11. Setup route, 1.2 Persistent shell, 1.3 Non-route states, 1. Inventory, 2.1 Styling, 2.2 Navigation, 2.3 Data refresh, 2.4 Real vs fake labelling (+18 more)

### Community 35 - "4. Backend — `web_control.py`"
Cohesion: 0.10
Nodes (26): 3. Project overview — `water-control-system`, Repository layout (live copy `~/water-control-system`), Runtime, Three copies exist, What it does, 4. Backend — `web_control.py`, Background workers, Concurrency model (+18 more)

### Community 36 - "methane-calibrate/main.cpp"
Cohesion: 0.43
Nodes (7): Sampling, analogRead(), loop(), readRs(), rsToPpm(), rsToPpmRaw(), setup()

### Community 37 - "render_current"
Cohesion: 0.12
Nodes (24): 1.1 Router (`ui_shell.cpp`, `s_route`), 3.13 Phone-associated popup, 3.16 Debug-only UI, 3.3.1.1 Removed content, 3.3.1.2 Layout (content coordinates, panel 800x436), 3.3.1.4 Behaviour kept, 3.3.1.5 Open item, 3.3.1 Redesign plan — `/data` node picker (+16 more)

### Community 38 - "co2-calibrate/main.cpp"
Cohesion: 0.60
Nodes (4): loop(), readCo2Ppm(), setup(), waitForLevel()

### Community 39 - "ATmega2560 Mega Mini — Development & Reference Documentation"
Cohesion: 0.08
Nodes (23): Analog Inputs (A0–A15), ATmega2560 Mega Mini — Development & Reference Documentation, Bootloader, Bootloader Behavior, Bypassing the Bootloader (ICSP), Common Use Cases, Communication Interfaces, Custom Board Definition (Optional) (+15 more)

### Community 43 - "ui_shell.cpp"
Cohesion: 0.18
Nodes (20): Decoded backtrace, 4. Lifecycle, 9.1 The association popup, 3.12 Bell and invalid-field list, 3.3.1.3 Back button, 3.8.1.4 Functions in `ui_shell.cpp` / `ui_shell.h`, setup_close_success(), on_bell_clicked() (+12 more)

### Community 44 - "data.ts"
Cohesion: 0.17
Nodes (16): FIRST_INTERNAL_CHAMBER_ID, internalChamberIds(), splitBottlesByPresence(), CONCLUDED_AFTER_MS, ExperimentSummary, MAX_READING_ROWS, POLL_INTERVAL_MS, ReadingRow (+8 more)

### Community 45 - "Endpoint details"
Cohesion: 0.09
Nodes (23): `telemetry_worker` (daemon thread), 10. `POST /set_mode`, 11. `POST /command`, 12. `GET /files`, 13. `GET /read_log`, 14. `GET /get_drives`, 15. `POST /export`, 16. `GET /get_ip` (+15 more)

### Community 46 - "getParameterValue"
Cohesion: 0.10
Nodes (22): chartPath(), createChartPoints(), csvDisabledReason(), getMetricValue(), getParameterValue(), ReadingWithStatus, summarizeParameter(), Metadata (+14 more)

### Community 47 - "index.md"
Cohesion: 0.18
Nodes (3): 3D Models, PCB, Display <-> WT32 link (`display-wt32-link`) — replaced

### Community 48 - "Reference: `device_data` column detail"
Cohesion: 0.10
Nodes (20): Access control, Chamber numbering, Database schema catalog — LLM Documentation, Development tooling, `device_data`: active application schema, `device_data.device_config`, `device_data.devices`, `device_data.experiments` (+12 more)

### Community 49 - "raspi-docs/README.md"
Cohesion: 0.16
Nodes (6): 2026-10-20 change, Deploying a change to the kiosk, How it works, Kiosk text scale, Related documents, Raspberry Pi kiosk UI CSS redesign

### Community 50 - "ESP32-8048S050C · Board & Archive Guide"
Cohesion: 0.10
Nodes (20): 1. Prepare the toolchain, 2. Open the simplest display example, 3. Build and upload, 4. Add capacitive touch, Archive map and reference documents, Board at a glance, Board-specific cautions from the archive, Bring-up checkpoints (+12 more)

### Community 51 - "syncDbLatest"
Cohesion: 0.23
Nodes (18): The Backlog, Master (`firmware/network-master/main.cpp`), 7. Correctness rules for the push (decided), 8. Build plan, Deferred (2026-10-16): sensor chain, Done criteria, Failure mode if the pin does break, Pull: partially built (+10 more)

### Community 52 - "history.ts"
Cohesion: 0.19
Nodes (16): ParameterStats, ChamberSummary, getExperimentChambers(), getHistoryPointDetails(), groupReadingsByChamber(), HISTORY_PARAMETERS, HistoryPointDetails, parameterValue() (+8 more)

### Community 53 - "screen_setup_poll"
Cohesion: 0.14
Nodes (19): 13. Light theme and the /info network QR, Widget lifetime rule, 3.15 SoftAP lifecycle, 3.7.1.1 Removed content, 3.7.1.2 Layout, 3.7.1.3 Gauge colour, 3.7.1.5 Network state tile, 3.7.1.6 Font change (+11 more)

### Community 54 - "millis"
Cohesion: 0.19
Nodes (18): 7.1 `Serial` becomes the link, 7.2 Link plumbing, 7.3 State machine (semantics kept, shape changed), 7.4 Porting pitfalls to check at review, 7. WT32 end (`firmware/network-master/main.cpp`), What is shared vs stubbed, 5.1 As built, 5. ESP implementation steps (+10 more)

### Community 55 - "screen_info.cpp"
Cohesion: 0.19
Nodes (14): Current state of the code, Experiments and results, /info Reboot: Heap-Walk Hang on `display-v9`, Symptom, 3.7.1.4 Gauge fill rules, 3.7.1.7 Functions, link_serial_get_db_bottles(), link_serial_get_db_latest() (+6 more)

### Community 56 - "WT32-ETH01 — LLM Documentation"
Cohesion: 0.12
Nodes (17): 1. Overview, 2. Key Specifications, 3.1 Debug / Programming Header (6 pins), 3.2 Main IO Header (2×10 pins), 3.3 Critical Pin Warnings, 3. Pinout (WT32‑ETH01 & WT32‑ETH02), 4. Product Variants, 5. Power Supply (+9 more)

### Community 57 - "csv.ts"
Cohesion: 0.18
Nodes (12): downloadExperimentCsv(), COLUMNS, csvFileName(), downloadCsv(), formatCell(), headerRow(), toCsv(), withUnit() (+4 more)

### Community 58 - "ui_init"
Cohesion: 0.17
Nodes (14): 4. Screen tree / state machine (as built), 3.1 Route `/setup`, 3.8.1.1 Removed content, 3.8.1.2 Layout (bar is 800×44, coordinates are bar-relative), 3.8.1.3 Clock, 3.8.1.5 Interaction notes, 3.8.1 Redesign plan — top bar, 3.8 Persistent status bar (+6 more)

### Community 59 - "Phone credential provisioning on the display (`provision_ap`)"
Cohesion: 0.13
Nodes (14): 10. Serial commands, 11. What is not covered, 12. Verify, 13. Related documents, 1. Why, 2. The two-step flow, 3. QR payload format, 5. Cloaked and encrypted, and why (+6 more)

### Community 60 - "BottleReading"
Cohesion: 0.14
Nodes (14): 1. Where the data comes from today, BottleReading, ch4, co2, nullMask, ph, pressure, tempC (+6 more)

### Community 61 - "Dashboard — full walkthrough"
Cohesion: 0.14
Nodes (13): 10.1 Security headers, 10.2 The legacy `public` schema, 10.3 The lockdown migration, 10. Deployment and the legacy schema, 11. The writer, 2.1 Environment, 2.2 The static host, 2. Running it (+5 more)

### Community 62 - "5. Frontend UI — `templates/index.html`"
Cohesion: 0.14
Nodes (14): 5. Frontend UI — `templates/index.html`, Bottle Cards Styling, Caveats, Controls Layout Simplification, Export flow, Live data flow, pH Metric Display, Physical Buttons Info Bar (+6 more)

### Community 63 - "WT32-ETH01 Ethernet ping test (`firmware/wt32-eth01_ping-test`)"
Cohesion: 0.14
Nodes (13): 10. Related documents, 1. Environment, 2. What it does, 3. RMII pin configuration, 4. Event handling, 5. Ping test, 5a. Portal login (`doLogin()`), 5b. HTTPS data fetch (+5 more)

### Community 64 - "Sensor test and calibration firmware"
Cohesion: 0.15
Nodes (12): 1. Common structure, 2. `blink-test`, 3. `loadcell-test`, 4. `co2-calibrate`, 5. `pressure-calibrate`, 6. `methane-calibrate`, 8. Related documents, Correctness limits (+4 more)

### Community 65 - "ESP32-8048S050 — LLM Generated Summary"
Cohesion: 0.15
Nodes (13): 17. Troubleshooting Matrix, 18. Recommended Development Flow, 19. Summary, 1. What the ESP32-8048S050 Is, 20. Sources and References, 2. Core Hardware Specification, 3. Memory Architecture, 7. GPIOs to Treat Carefully (+5 more)

### Community 66 - "Display UI — LLM Documentation (`display-v9`, fake-data pass)"
Cohesion: 0.18
Nodes (11): 10. WT32 link (`rusim-line` v1, production), 12. Credential entry by QR (`provision_ap`), 14. Debug flag `IS_DEBUG`, 1. Overview, 5. Fake-data serial protocol, 6. Bell / NULL semantics (fake), 7. Hold-to-confirm implementation, 8. Verify without hardware signals (+3 more)

### Community 67 - "Display <-> WT32 link: `rusim-line` v1"
Cohesion: 0.17
Nodes (12): 10. Bench checks, 1. Physical layer, 2. Line rules (normative), 3.1 Display <- WT32, 3.2 WT32 <- display, 3.3 Gone from the Modbus design, 3. Command set, 5. Timing budget (+4 more)

### Community 68 - "network-master: bounded buffers"
Cohesion: 0.24
Nodes (11): display-v9: DB fields from the `DB` line, Memory-safety hardening (`display-v9`, `network-master`), network-master: bounded buffers, Verification, hexVal(), pct_decode(), pushFormatHeadRound(), sbParseAuthMeta() (+3 more)

### Community 69 - "log_data_to_csv"
Cohesion: 0.18
Nodes (12): 7. Data & logging, CSV format (current schema), Existing logs, Older schema (2025-12-12.csv), Storage, USB export, 6. `POST /toggle_logging`, format_aligned_row() (+4 more)

### Community 70 - "logf"
Cohesion: 0.26
Nodes (12): arg_lookup(), handle_creds(), handle_experiment(), handle_line(), link_emit_loginreq(), link_emit_net(), link_emit_result(), link_emit_truth() (+4 more)

### Community 71 - "Memory Safety Patterns"
Cohesion: 0.18
Nodes (10): 1. Memory Bug Categories, 2. Safety Spectrum, Best Practices, Core Concepts, Debugging Tools, Detailed patterns and worked examples, Do's, Don'ts (+2 more)

### Community 72 - "Repository Index"
Cohesion: 0.18
Nodes (11): 1. Repository layout, 2. Firmware, 3. Dashboard, 3a. Legacy / Raspberry Pi, 3b. Raspberry Pi system documentation (`raspi-docs/`), 4. Hardware references, 5. Human documents, 6. Where new work belongs (+3 more)

### Community 73 - "network-master (`firmware/network-master`) â€” production WT32 firmware"
Cohesion: 0.18
Nodes (10): 1. Environment, 3.1 Received from the display, 3.2 Sent to the display, 3.3 Credential state, 3. Line protocol, 4. State machine, 5. Portal login (`doLogin()`), 7. Expected output (+2 more)

### Community 74 - "Stack"
Cohesion: 0.18
Nodes (11): 1. Dashboard, 1. [Documentation and notes](docs/), 2. Database, 2. [MCU code](firmware/), 3. Firmware, 3. [Website](dashboard/), 4. Hardware, In-Vitro Rumen Simulation (+3 more)

### Community 75 - "authenticate"
Cohesion: 0.24
Nodes (10): authenticate(), 6.1 Write grant: `SECURITY DEFINER` RPC, 6.2 Pull scope: full `experiments` plus one config row, scoped to same `device_id`, 6. Decisions (resolved 2026-10-16), 1. Blocker — legacy `public` schema is public-read (live, confirmed), 2. High — `authenticate()` has no brute-force protection, 3. High — Netlify bundle + env handling, 4. Medium — session hygiene and PII (+2 more)

### Community 76 - "CALIBRATION NOTES"
Cohesion: 0.20
Nodes (10): 1. Temperature sensor, 2. CO2 sensor - READ UPDATE, 3. Pressure sensor, 4. pH sensor, 5. CH4 sensor, CALIBRATION NOTES, UPDATE 08/09/2026, UPDATE 09/09/2026 (+2 more)

### Community 77 - "compile_commands.json and gen-compile-db.sh"
Cohesion: 0.20
Nodes (9): 1. What the file is, 2. Why one PlatformIO command does not produce it, 3. The generator script, 4. What the database fixes, and what it does not, 5. Git status, compile_commands.json and gen-compile-db.sh, 13. Flask REST API reference — `web_control.py`, Summary (+1 more)

### Community 78 - "Pi sync script — `sync_pi.sh`"
Cohesion: 0.20
Nodes (10): Failure behaviour, First run, Limitations, Master firmware flashing, Pi sync script — `sync_pi.sh`, Prerequisites, Rollback, Usage (+2 more)

### Community 79 - "6. Master firmware — `legacy/master/master.cpp`"
Cohesion: 0.20
Nodes (10): 6. Master firmware — `legacy/master/master.cpp`, Boot defaults, Commands, Control parameters, Deployment, Libraries, Main loop, Pins (+2 more)

### Community 80 - "screen_data.cpp"
Cohesion: 0.31
Nodes (7): on_back_bottle(), on_back_cluster(), on_back_picker(), on_pick_bottle(), on_pick_cluster(), ui_show_bottle(), ui_show_cluster()

### Community 81 - "main"
Cohesion: 0.24
Nodes (5): SimSerial, link_serial_init(), link_serial_poll(), link_serial_set_exp_state(), main()

### Community 82 - "LLM Agents Guide"
Cohesion: 0.22
Nodes (9): 1. Firmware engineer, 2. Full-stack web developer, 3. Legacy System Administrator, Context and Documentation, Decisions, LLM Agents Guide, Prerequisites, Restrictions (+1 more)

### Community 83 - "Patterns by Language"
Cohesion: 0.22
Nodes (8): memory-safety-patterns — detailed patterns and worked examples, Pattern 1: RAII in C++, Pattern 2: Smart Pointers in C++, Pattern 3: Ownership in Rust, Pattern 4: Safe Resource Management in C, Pattern 5: Bounds Checking, Pattern 6: Preventing Data Races, Patterns by Language

### Community 84 - "handlePingStream"
Cohesion: 0.28
Nodes (8): 5.1 Request, 5.2 Response, 5.3 Which port gets the reply, 5.4 USB stream, 5. Serial protocol, handlePingStream(), sendTelemetryJson(), serviceUsbTelemetry()

### Community 85 - "12. Copies & git state"
Cohesion: 0.22
Nodes (9): 12. Copies & git state, Divergence between copies, Firmware (water-level handling), Implications, Initial commit, Local documentation origin, UI, Uncommitted changes (identical git status in all three copies) (+1 more)

### Community 87 - "Captive portal login from Linux (bash)"
Cohesion: 0.25
Nodes (7): 1. Usage, 2. The four steps, 3. Helper functions and their firmware counterparts, 4. Differences from the firmware, 5. Testing, 6. Related documents, Captive portal login from Linux (bash)

### Community 88 - "my_touchpad_read"
Cohesion: 0.29
Nodes (6): 2. Hardware / platform notes, 3.14 Backlight sleep, my_touchpad_read(), sleep_note_input(), touch_has_signal(), touch_touched()

### Community 89 - ".begin"
Cohesion: 0.25
Nodes (5): Next step (open), 7. Passthrough tunnel (`PASSTH`), display-v9: RAII ownership, 7. `temp-probe`, my_disp_flush()

### Community 90 - "ESP to database sync — plan"
Cohesion: 0.25
Nodes (7): 1. Current state, 2. The database forbids writes, 3. Nothing reads the sensors, 4. Why the device must not send `token_hash`, 9. Related documents, ESP to database sync — plan, Push: absent

### Community 91 - "Legacy Pi remote monitor (`scripts-hardware/monitor-legacy-pi.ts`)"
Cohesion: 0.25
Nodes (8): 2. The two sources, 3. Output, 4. How `--raw` works, 5. Side effects, 6. Failure handling, 7. Limits, 8. Related documents, Legacy Pi remote monitor (`scripts-hardware/monitor-legacy-pi.ts`)

### Community 92 - "PlatformIO build environments (`platformio.ini`)"
Cohesion: 0.25
Nodes (8): 1. Layout, 2. Shared settings, 3. Environment table, 4. The ESP32-S3 display environments, 5. Commands, 6. History of this configuration, 7. Related documents, PlatformIO build environments (`platformio.ini`)

### Community 93 - "LVGL v8 to v9 migration report"
Cohesion: 0.29
Nodes (6): File-by-file summary, LVGL v8 to v9 migration report, Memory and runtime considerations, References, Scope and current setup, Suggested migration sequence

### Community 94 - "Raspberry Pi `water-control-system` mirror"
Cohesion: 0.29
Nodes (7): Checksums, Layout, Raspberry Pi `water-control-system` mirror, Refreshing the mirror, Related documents, Relationship to the deleted `main_ui.html`, What is not included

### Community 95 - "1. Hardware & OS"
Cohesion: 0.29
Nodes (6): 1. Hardware & OS, Board, Misc installed utilities, OS, Toolchain & versions, User account

### Community 96 - "2. Boot & kiosk session model"
Cohesion: 0.29
Nodes (6): 2. Boot & kiosk session model, Boot sequence, Chromium kiosk, Notes / caveats, openbox autostart (`~/.config/openbox/autostart`), PM2 service (`/etc/systemd/system/pm2-invitro.service`)

### Community 97 - "9. Network & services"
Cohesion: 0.29
Nodes (6): 9. Network & services, Enabled services (selected), Listening ports, Network Architecture (LAN Primary), Security note (see §10), disable_wifi_client_and_stop_hotspot()

### Community 98 - "`leaf` terminal reader on the Pi"
Cohesion: 0.29
Nodes (6): How the cross-compile works, `leaf` terminal reader on the Pi, Rebuilding and removing, Running it, Verification, Why it is not compiled on the Pi

### Community 99 - "PageScan"
Cohesion: 0.29
Nodes (7): PageScan, formAction, hasForm, hasUsernameField, magic, redir, totalBytes

### Community 100 - "getSupabase"
Cohesion: 0.47
Nodes (6): fetchDevice(), fetchExperimentReadings(), fetchReadings(), toReading(), getSupabase(), 5.3 First data load

### Community 101 - "4. Authentication, end to end"
Cohesion: 0.33
Nodes (6): 4.1 Sign-in, 4.2 Per-request scoping, 4.3 Grants, 4.4 Sign-out, 4.5 Client-side rate limiting, 4. Authentication, end to end

### Community 102 - "9. Development tooling: `bun run dev-db`"
Cohesion: 0.33
Nodes (6): 9.1 Token half, 9.2 Readings half, 9.3 Why a script and not a migration, 9.4 Safety guards, 9.5 Initial data, 9. Development tooling: `bun run dev-db`

### Community 103 - "Display `IS_DEBUG` flag"
Cohesion: 0.33
Nodes (5): Current effects, Definition, Display `IS_DEBUG` flag, Enabling it, Rules for new uses

### Community 104 - "5. Credential exposure on the WT32"
Cohesion: 0.33
Nodes (6): 5. Credential exposure on the WT32, `authenticate()` is unthrottled, The token is baked into flash, TLS is unverified, What a pin breaks, and what it does not, What Postgres already computes

### Community 105 - "learned-mistakes.md"
Cohesion: 0.33
Nodes (5): 2026-10-05, 2026-10-06, 2026-10-08, 2026-10-20, 2026-10-24

### Community 106 - "Required changes"
Cohesion: 0.33
Nodes (6): 1. Select and pin an LVGL v9 package, 2. Replace the display registration and flush API, 3. Replace input-driver registration, 4. Update the active-screen call and check widget/config names, 5. Refresh the LVGL configuration, Required changes

### Community 107 - "send_serial_cmd"
Cohesion: 0.33
Nodes (4): Mode button, control_api(), send_serial_cmd(), set_mode()

### Community 108 - "Dashboard public-deploy security report"
Cohesion: 0.33
Nodes (5): Already fixed / verified good (keep these), Dashboard public-deploy security report, Live state (verified against remote), Pre-public checklist, Verification queries used (read-only)

### Community 109 - "10. ESP-IDF Recommendations"
Cohesion: 0.33
Nodes (6): 10. ESP-IDF Recommendations, ESP-IDF components you may use, Important menuconfig areas, Recommended minimum version, Target setup, Typical required features

### Community 110 - "6. Common Pin Map Reference"
Cohesion: 0.33
Nodes (6): 6.1 RGB display control pins, 6.2 RGB data pins, common capacitive variant, 6.3 GT911 capacitive touch pins, 6.4 Other common board pins, 6.5 Known documentation trap, 6. Common Pin Map Reference

### Community 111 - "6. From a row to a chart point"
Cohesion: 0.40
Nodes (5): 6.1 Scaling, 6.2 Path construction, 6.3 Rendering, 6.4 What the chart is not, 6. From a row to a chart point

### Community 112 - "DB experiment sync + NTP — LLM Documentation"
Cohesion: 0.40
Nodes (4): Auth used, Build, DB experiment sync + NTP — LLM Documentation, Link (`rusim-line` v1)

### Community 113 - "Display LVGL 9.3.0 -> 9.6.0 port"
Cohesion: 0.40
Nodes (4): Code (9.6-clean, proven with `-DLV_DISABLE_API_MAPPING`), Config (`lv_conf.h` + `sim/lv_conf.h`), Deliberately not changed, Display LVGL 9.3.0 -> 9.6.0 port

### Community 114 - "Display V9 PC Simulator (`[env:display-sim]`)"
Cohesion: 0.40
Nodes (4): Configuration, Display V9 PC Simulator (`[env:display-sim]`), Run it, Scope note

### Community 115 - "8. Auxiliary tools & scripts"
Cohesion: 0.40
Nodes (4): 8. Auxiliary tools & scripts, `bin/`, In home (`/home/invitro`), Misc

### Community 116 - "10. Security & caveats"
Cohesion: 0.40
Nodes (4): 10. Security & caveats, Operational caveats, Recommendations (not applied), Security

### Community 117 - "Stage"
Cohesion: 0.40
Nodes (5): Stage, STAGE_LOGIN, STAGE_NEED_CREDS, STAGE_RUN, STAGE_WAIT_IP

### Community 118 - "4. State and flows"
Cohesion: 0.50
Nodes (4): 4.1 WT32 state machine, 4.2 Credential and verdict flow, 4.3 Identity and privacy, 4. State and flows

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

### Community 123 - "1. What it is"
Cohesion: 0.67
Nodes (3): 1.1 File map, 1.2 What it is not, 1. What it is

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
- **64 isolated node(s):** `RFC-4180`, `btn`, `ring`, `halo`, `label` (+59 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 818 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **11 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `authenticate()` connect `authenticate` to `auth.ts`, `getSupabase`, `4. Authentication, end to end`, `5. Credential exposure on the WT32`, `data.ts`, `Dashboard public-deploy security report`, `getParameterValue`, `DB experiment sync + NTP — LLM Documentation`, `syncDbLatest`, `ESP to database sync — plan`, `Dashboard — full walkthrough`?**
  _High betweenness centrality (0.139) - this node is a cross-community bridge._
- **Are the 51 inferred relationships involving `millis()` (e.g. with `PWM Output Pins` and `7.3 State machine (semantics kept, shape changed)`) actually correct?**
  _`millis()` has 51 INFERRED edges - model-reasoned connections that need verification._
- **What connects `RFC-4180`, `btn`, `ring` to the rest of the system?**
  _64 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `millis` be split into smaller, more focused modules?**
  _Cohesion score 0.054901960784313725 - nodes in this community are weakly interconnected._
- **Why does `millis()` connect `millis` to `display-v9/main.cpp`, `millis`, `wt32-eth01_ping-test/main.cpp`, `wt32-eth01_captive-recon/main.cpp`, `link_serial.cpp`, `new-slave.cpp`, `master.cpp`, `Pin Layout Overview`, `handle_line`, `Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)`, `delay`, `Arduino.h`, `fake_init`, `temp-probe/main.cpp`, `methane-calibrate/main.cpp`, `co2-calibrate/main.cpp`, `syncDbLatest`, `screen_setup_poll`, `screen_info.cpp`, `ui_init`, `logf`, `network-master (`firmware/network-master`) â€” production WT32 firmware`, `handlePingStream`, `my_touchpad_read`, `4. State and flows`?**
  _High betweenness centrality (0.137) - this node is a cross-community bridge._
- **Are the 35 inferred relationships involving `delay()` (e.g. with `What is shared vs stubbed` and `loop()`) actually correct?**
  _`delay()` has 35 INFERRED edges - model-reasoned connections that need verification._
- **Should `web_control.py` be split into smaller, more focused modules?**
  _Cohesion score 0.08912655971479501 - nodes in this community are weakly interconnected._