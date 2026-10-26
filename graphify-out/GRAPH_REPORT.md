# Graph Report - rusim-monorepo  (2026-10-26)

## Corpus Check
- cluster-only mode — file stats not available

## Summary
- 995 nodes · 2118 edges · 43 communities (38 shown, 5 thin omitted)
- Extraction: 87% EXTRACTED · 13% INFERRED · 0% AMBIGUOUS · INFERRED: 274 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `9e984e37`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- App.tsx
- ui_shell.cpp
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
- provision_ap.cpp
- screen_setup_poll
- handle_line
- touch.h
- package.json
- Rs485Port
- flash-factory.ts
- delay
- digitalWrite
- captive-portal-login.sh
- sync_pi.sh
- fake_data.cpp
- database.ts
- PushRow
- SimESPClass
- temp-probe/main.cpp
- index.tsx
- supabase.ts
- scripts
- methane-calibrate/main.cpp
- vite
- co2-calibrate/main.cpp
- join_details_refresh
- gen-compile-db.sh
- install-leaf.sh

## God Nodes (most connected - your core abstractions)
1. `millis()` - 45 edges
2. `delay()` - 35 edges
3. `handle_line()` - 23 edges
4. `logf()` - 21 edges
5. `ui_update_bell()` - 20 edges
6. `compilerOptions` - 20 edges
7. `Dashboard()` - 19 edges
8. `render_current()` - 19 edges
9. `ExperimentHistoryView()` - 18 edges
10. `HoldBtnCtx` - 17 edges

## Surprising Connections (you probably didn't know these)
- `loop()` --calls--> `digitalRead()`  [INFERRED]
  legacy/master/master.cpp → firmware/display-v9/sim/Arduino.h
- `handlePingStream()` --calls--> `millis()`  [INFERRED]
  legacy/slave/new-slave.cpp → firmware/display-v9/sim/Arduino.h
- `serviceUsbTelemetry()` --calls--> `millis()`  [INFERRED]
  legacy/slave/new-slave.cpp → firmware/display-v9/sim/Arduino.h
- `setup()` --calls--> `millis()`  [INFERRED]
  legacy/slave/new-slave.cpp → firmware/display-v9/sim/Arduino.h
- `setup()` --calls--> `pinMode()`  [INFERRED]
  legacy/slave/new-slave.cpp → firmware/display-v9/sim/Arduino.h

## Import Cycles
- None detected.

## Communities (43 total, 5 thin omitted)

### Community 0 - "App.tsx"
Cohesion: 0.05
Nodes (89): react@19.1.1, App(), BottleChart(), Chart(), ChartInteraction, chartPath(), createChartPoints(), createTimeLabels() (+81 more)

### Community 1 - "ui_shell.cpp"
Cohesion: 0.06
Nodes (52): fake_sensor_text(), link_serial_get_db_bottles(), link_serial_get_db_latest(), DispBufDeleter, my_disp_flush(), on_back_bottle(), on_back_cluster(), on_back_picker() (+44 more)

### Community 2 - "network-master/main.cpp"
Cohesion: 0.06
Nodes (74): arg_lookup(), BottleReading, ch4, co2, nullMask, ph, pressure, tempC (+66 more)

### Community 3 - "millis"
Cohesion: 0.06
Nodes (58): link_serial_set_exp_state(), lvgl_tick_get(), db_refresh_warn(), dur_show(), exp_mirror_link(), exp_show(), on_dur_down(), on_dur_up() (+50 more)

### Community 4 - "web_control.py"
Cohesion: 0.05
Nodes (34): command(), control_api(), CsvDownloadHandler, disable_wifi_client_and_stop_hotspot(), download_info(), export_data(), fmt_val(), format_aligned_row() (+26 more)

### Community 5 - "link_provision_stub.cpp"
Cohesion: 0.05
Nodes (8): SimSerial, link_serial_init(), link_serial_poll(), link_serial_set_exp_state(), provision_ap_passphrase(), provision_ap_ssid(), provision_ap_wifi_qr_payload(), main()

### Community 6 - "network-master-passthrough.py"
Cohesion: 0.09
Nodes (15): _after_upload(), _arm_tunnel(), _before_upload(), _cleanup_after_failed_upload(), _close_tunnel(), _configure_tty(), _configured_port(), _display_ports() (+7 more)

### Community 7 - "monitor-legacy-pi.ts"
Cohesion: 0.10
Nodes (36): Bottle, BOTTLE_ALIGN, BOTTLE_HEADERS, BOTTLE_IDS, BOTTLE_WIDTHS, cell(), clock(), fetchRaw() (+28 more)

### Community 8 - "wt32-eth01_ping-test/main.cpp"
Cohesion: 0.12
Nodes (29): doLogin(), extractWindowLocation(), fetchData(), fetchLoginPage(), fetchPortalUrl(), loop(), onEthEvent(), PageScan (+21 more)

### Community 9 - "dev-db.ts"
Cohesion: 0.10
Nodes (25): DEFAULTS, DELETE_EXPERIMENT, ENV_PATH, FieldRule, FIELDS, FlagMap, INSERT_READINGS, kebabCase() (+17 more)

### Community 10 - "wt32-eth01_captive-recon/main.cpp"
Cohesion: 0.11
Nodes (25): extractWindowLocation(), loop(), onEthEvent(), PageScan, cloudflareChallenge, formAction, hasForm, hasUsernameField (+17 more)

### Community 11 - "link_serial.cpp"
Cohesion: 0.13
Nodes (25): arg_lookup(), echo_line(), emit_creds(), emit_experiment(), emit_hello(), handle_db(), handle_hello(), handle_line() (+17 more)

### Community 12 - "read-legacy-slave.ts"
Cohesion: 0.14
Nodes (21): ALIGN, Bottle, configurePort(), createLineGate(), Device, formatCell(), formatInt(), formatNumber() (+13 more)

### Community 13 - "new-slave.cpp"
Cohesion: 0.19
Nodes (17): analogRead(), digitalRead(), convertPhVoltage(), fitExponent(), handlePingStream(), initPressureSensor(), loop(), readCo2Ppm() (+9 more)

### Community 14 - "compilerOptions"
Cohesion: 0.10
Nodes (20): compilerOptions, allowImportingTsExtensions, allowJs, jsx, lib, module, moduleDetection, moduleResolution (+12 more)

### Community 15 - "master.cpp"
Cohesion: 0.23
Nodes (16): pinMode(), driveHeater(), drivePumpPwm(), driveStirrer(), driveStirrerPwm(), forceAllOff(), loop(), sendSlavePing() (+8 more)

### Community 16 - "provision_ap.cpp"
Cohesion: 0.22
Nodes (12): link_serial_creds_ready(), link_serial_result(), build_identity(), handle_creds(), handle_status(), note_activity(), provision_ap_awaiting_verdict(), provision_ap_begin() (+4 more)

### Community 17 - "screen_setup_poll"
Cohesion: 0.15
Nodes (16): link_serial_take_login_request(), link_serial_take_result(), provision_ap_active(), provision_ap_client_count(), provision_ap_end(), provision_ap_poll(), provision_ap_url(), provision_ap_wifi_qr_payload() (+8 more)

### Community 18 - "handle_line"
Cohesion: 0.23
Nodes (14): link_serial_passthrough_active(), link_serial_passthrough_set(), link_serial_passthrough_write(), loop(), sleep_tick(), handle_creds(), handle_line(), parse_onoff() (+6 more)

### Community 19 - "touch.h"
Cohesion: 0.14
Nodes (6): my_touchpad_read(), sleep_note_input(), touch(), touch_has_signal(), touch_released(), touch_touched()

### Community 20 - "package.json"
Cohesion: 0.18
Nodes (14): devDependencies, netlify-cli@23.8.0, @types/bun@1.3.3, @types/react@19.1.13, @types/react-dom@19.1.9, typescript@5.9.2, name, private (+6 more)

### Community 21 - "Rs485Port"
Cohesion: 0.28
Nodes (12): drainPort(), loop(), receiveMessage(), rs485Dir(), rs485InitPort(), Rs485Port, dirPin1, dirPin2 (+4 more)

### Community 22 - "flash-factory.ts"
Cohesion: 0.32
Nodes (12): autodetectPort(), candidatePorts(), Device, flash(), getArg(), hasFlag(), listDevices(), main() (+4 more)

### Community 23 - "delay"
Cohesion: 0.32
Nodes (9): delay(), initializeSensor(), loop(), setup(), initializeSensor(), loop(), setup(), setup() (+1 more)

### Community 25 - "digitalWrite"
Cohesion: 0.25
Nodes (9): loop(), setup(), setup(), digitalWrite(), touch_init(), loop(), rampUp(), setup() (+1 more)

### Community 26 - "captive-portal-login.sh"
Cohesion: 0.35
Nodes (9): extract_window_location(), form_action(), log(), probe_once(), resolve_action(), captive-portal-login.sh script, url_base(), usage() (+1 more)

### Community 27 - "sync_pi.sh"
Cohesion: 0.51
Nodes (10): deploy_local(), ensure_arduino_toolchain(), find_arduino_port(), flash_master(), flash_pm2_cleanup(), master_flash_hash(), say(), sync_pi.sh script (+2 more)

### Community 28 - "fake_data.cpp"
Cohesion: 0.29
Nodes (8): fake_any_null(), fake_bottle_seed(), fake_init(), fake_null_count(), fake_wt32_push(), frand(), random(), randomSeed()

### Community 29 - "database.ts"
Cohesion: 0.20
Nodes (9): CompositeTypes, Constants, DatabaseWithoutInternals, DefaultSchema, Enums, Json, Tables, TablesInsert (+1 more)

### Community 30 - "PushRow"
Cohesion: 0.20
Nodes (10): PushRow, bottleId, ch4, co2, experimentId, nullMask, ph, pressure (+2 more)

### Community 32 - "temp-probe/main.cpp"
Cohesion: 0.46
Nodes (7): checkIdleLine(), loop(), printAddress(), printBusDiagnostics(), readAndReport(), scanBus(), setup()

### Community 33 - "index.tsx"
Cohesion: 0.29
Nodes (5): dependencies, react-dom@19.1.1, @supabase/supabase-js@2.57.4, react-dom, @supabase/supabase-js

### Community 34 - "supabase.ts"
Cohesion: 0.33
Nodes (5): buildClient(), DeviceDataClient, readConfig(), SESSION_HEADER, Database

### Community 35 - "scripts"
Cohesion: 0.33
Nodes (6): scripts, build, dev, dev-db, preview, test

### Community 36 - "methane-calibrate/main.cpp"
Cohesion: 0.53
Nodes (5): loop(), readRs(), rsToPpm(), rsToPpmRaw(), setup()

### Community 37 - "vite"
Cohesion: 0.40
Nodes (4): vite@7.1.7, @vitejs/plugin-react@5.0.4, vite, @vitejs/plugin-react

### Community 38 - "co2-calibrate/main.cpp"
Cohesion: 0.60
Nodes (4): loop(), readCo2Ppm(), setup(), waitForLevel()

### Community 39 - "join_details_refresh"
Cohesion: 0.50
Nodes (4): provision_ap_hidden(), provision_ap_passphrase(), provision_ap_ssid(), join_details_refresh()

## Knowledge Gaps
- **64 isolated node(s):** `RFC-4180`, `cloudflareChallenge`, `formAction`, `hasForm`, `hasUsernameField` (+59 more)
  These have ≤1 connection - possible missing edges. (Counts symbols only; 294 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `millis()` connect `millis` to `temp-probe/main.cpp`, `ui_shell.cpp`, `network-master/main.cpp`, `methane-calibrate/main.cpp`, `co2-calibrate/main.cpp`, `wt32-eth01_ping-test/main.cpp`, `wt32-eth01_captive-recon/main.cpp`, `link_serial.cpp`, `new-slave.cpp`, `master.cpp`, `provision_ap.cpp`, `screen_setup_poll`, `handle_line`, `touch.h`, `Rs485Port`, `delay`, `Arduino.h`, `fake_data.cpp`?**
  _High betweenness centrality (0.061) - this node is a cross-community bridge._
- **Are the 44 inferred relationships involving `millis()` (e.g. with `loop()` and `fake_init()`) actually correct?**
  _`millis()` has 44 INFERRED edges - model-reasoned connections that need verification._
- **What connects `RFC-4180`, `cloudflareChallenge`, `formAction` to the rest of the system?**
  _64 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `App.tsx` be split into smaller, more focused modules?**
  _Cohesion score 0.05007935108446482 - nodes in this community are weakly interconnected._
- **Why does `delay()` connect `delay` to `temp-probe/main.cpp`, `network-master/main.cpp`, `methane-calibrate/main.cpp`, `wt32-eth01_ping-test/main.cpp`, `wt32-eth01_captive-recon/main.cpp`, `link_serial.cpp`, `new-slave.cpp`, `handle_line`, `Rs485Port`, `Arduino.h`, `digitalWrite`?**
  _High betweenness centrality (0.024) - this node is a cross-community bridge._
- **Are the 34 inferred relationships involving `delay()` (e.g. with `loop()` and `link_serial_init()`) actually correct?**
  _`delay()` has 34 INFERRED edges - model-reasoned connections that need verification._
- **Should `ui_shell.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.056669339748730285 - nodes in this community are weakly interconnected._