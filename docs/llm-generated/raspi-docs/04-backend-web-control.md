# 4. Backend — `web_control.py`

Plain Flask, no SocketIO. Listens on `0.0.0.0:5000` with
`debug=False, threaded=True`. The kiosk page polls `/status` every 1.5 seconds.

## Global state

```python
current_data = {
    'temp': -999.0,           # bath DS18B20 reading
    'bottle_avg_temp': -999.0,# cascaded bottle average from master
    'heater': False,
    'pump': False,            # True if either pump1 or pump2 is active
    'pump1': False,           # Pump 1 MOSFET (Pin D3) active state
    'pump2': False,           # Pump 2 MOSFET (Pin D7) active state
    'stirrer': False,         # Stirrer MOSFET (Pin D4) active state
    'heater_mode': 'OFF',
    'pump_mode': 'OFF',
    'pump1_mode': 'OFF',
    'pump2_mode': 'OFF',
    'pwm_d3': 255,            # Target PWM ceiling (0..255) for Pump 1 (Pin D3)
    'pwm_d7': 255,            # Target PWM ceiling (0..255) for Pump 2 (Pin D7)
    'pwm_d4': 255,            # Target PWM ceiling (0..255) for Stirrer (Pin D4)
    'out_d3': 0,              # Instantaneous output PWM (0..255) on Pin D3
    'out_d7': 0,              # Instantaneous output PWM (0..255) on Pin D7
    'out_d4': 0,              # Instantaneous output PWM (0..255) on Pin D4
    'stir_mode': 'OFF',
    'stir_int_min': 10,
    'stir_pulse_sec': 30,
    'stir_timed_min': 15,
    'stir_rem_sec': 0,
    'target_temp': 39.0,
    'hysteresis': 0.3,
    'water_low': False,       # master's debounced waterDepleted flag: Pump 2 throttled to 100 PWM
    'water_crit': False,      # master's debounced waterCriticalLow flag: Heater & Pump 2 cutoff
    'water_full': False,      # master's debounced inletWaterFull flag: Pump 1 cutoff
    'water_adc': -1,          # interquartile-mean bath water sensor ADC on A9 (0..1023, rises when submerged)
    'water_thr': 460,         # WATER_DRY_THRESHOLD reported by master
    'out_stop_thr': 400,      # OUT_PUMP_STOP_ADC reported by master
    'in_stop_thr': 550,       # IN_PUMP_STOP_ADC reported by master
    'leak_enabled': False,    # True only when ENABLE_LEAK_SENSOR == 1 on the master
    'pump_leak': False,       # pump leak fault flag on A8 (forced False when leak_enabled is False)
    'leak_adc': -1,           # 16-sample averaged pump leak sensor ADC on A8 (-1 when disabled)
    'leak_thr': 200,          # wet-leak ADC threshold on A8 (>= 200 trips fault)
    'system_mode': 'AUTO',    # 'AUTO' | 'OFF'
    'bottles': []             # up to 4 bottle dicts
}
```

Plus module-level `serial_conn`, `serial_write_lock`, `logging_active`,
`last_slave_rx_time`, `last_master_rx_time`, `packet_stats`,
`cached_sys_stats`, and the raw line buffer described below.

Water-level thresholds are owned by the Pi. The four module constants
`OUT_PUMP_STOP_ADC=400`, `WATER_DRY_THRESHOLD=460`,
`IN_PUMP_STOP_ADC=550`, `LEAK_THRESHOLD=200` seed the volatile
`thresholds` dict and are built into a `THRESH:...` line by
`threshold_command()`. Trip-only: no recover deadbands. Tweak live without
a restart via `POST /control` `action=thresholds` (volatile — lost on Flask
restart, which falls back to the file constants), or edit the constants
and restart Flask — no reflash needed in either case. The master keeps
them in RAM only, so it forgets them on reboot; the Pi re-sends on every
serial reconnect and when the master's reported values drift (2 s
debounce), via `thresholds_sent` / `thresholds_match_current()`. Both the
Pi (`validate_thresholds()`) and the master validate `0–1023` and ignore
a malformed line. Pump `FORCE ON` (`/control` `mode=ON`) is mirrored
optimistically as naively on (ignores water levels, like the master's
debug behaviour; `MODE_OFF`/leak still cut it on the master).

## Raw line buffer

`serial_worker` stores every line it reads before any parsing, so the wire
traffic stays available without opening `/dev/ttyUSB0` a second time.

| Name | Meaning |
|---|---|
| `raw_lines` | `deque(maxlen=500)` of `{'seq', 'ts', 'line'}` |
| `raw_line_seq` | Counter, incremented once per stored line |
| `raw_lines_lock` | `threading.Lock` around both |

## Background workers

### `serial_worker` (daemon thread)

- Scans `['/dev/ttyACM0', '/dev/ttyUSB0', '/dev/ttyACM1', '/dev/ttyUSB1']` and
  opens the first present port at 115200 (`timeout=1.0`, `write_timeout=1.0`), with a 2 s settle.
- Tracks reception freshness using `time.monotonic()` (`last_slave_rx_time`, `last_master_rx_time`, and `last_serial_rx_mono`) so wall-clock jumps from `/set_time` or `sync_clock_from_lan_http()` never corrupt the 12-second staleness window.
- Enforces a 10-second silent-Master watchdog (`MASTER_SILENCE_RECONNECT_SECONDS = 10.0`): if no valid line arrives from the Master for `>= 10 s` (or if `send_serial_cmd()` hits a serial write error), `close_serial_conn()` closes the serial port so `serial_worker` reopens it on the next loop, pulsing DTR to reset a wedged Master MCU.
- Each line is recorded in the raw buffer, then classified if it is JSON:
  1. `bottles` array or `type == 'slave_telemetry'` → validate each bottle via `is_valid_bottle_payload()` (`id` integer in `VALID_BOTTLE_IDS = (1, 2, 3, 4)` and finite numeric values for all `REQUIRED_BOTTLE_KEYS = ('id', 'temp', 'ph', 'methane', 'co2', 'press')`), then merge into `current_data['bottles']` by `id` (retaining only valid IDs `1..4` sorted by `id`).
  2. Single bottle (`is_valid_bottle_payload(payload)`) → merge by `id`.
  3. `type == 'master_status'` → update bath temp, bottle average,
     heater / Pump 1 (`D3`) / Pump 2 (`D7`) / Stirrer (`D4`) states, modes,
     target PWM ceilings (`pwm_d3`, `pwm_d7`, `pwm_d4`), live PWM outputs
     (`out_d3`, `out_d7`, `out_d4`), bath water level (`water_low`, `water_crit`,
     `water_full`, `water_adc`, `water_thr`, `out_stop_thr`, `in_stop_thr`),
     optional pump leak sensor (`leak_enabled`, `pump_leak`,
     `leak_adc`, `leak_thr`; `pump_leak` is forced `False` and `leak_adc` `-1`
     when `leak_enabled` is false/absent), and `system_mode`.
  4. Anything else → `current_data.update(payload)`.
- In `/status`, when `master_fresh` (`now_mono - last_master_rx_time < 12.0`) is `False`, `temp` and `bottle_avg_temp` are reported as `-999.0`; when `slave_fresh` (`now_mono - last_slave_rx_time < 12.0`) is `False`, `bottles` is returned as `[]` and `bottle_avg_temp` as `-999.0`.

### `polling_worker` (daemon thread)

- On the first loop where `serial_conn` is open, writes the boot-default
  commands `HEATER_OFF`, `PUMP1_OFF`, `PUMP2_OFF`, and `STIR:0:10:30` in that
  order, then sets `boot_defaults_applied = True`.
- On every (re)connect, writes `desired_state_commands()` (`MODE_*`, `HEATER_*`, `PUMP1_*`, `PUMP2_*`, `PWM:D3:*`, `PWM:D7:*`, `PWM:D4:*`, `STIR:*`, `TEMP:*`) and sets `state_sent = True`, followed by `THRESH:...` from `threshold_command()` (`thresholds_sent = True`).
- While connected, if `state_mismatch_commands()` is non-empty (e.g., the Master rebooted into `OFF` defaults or dropped a serial command) and at least `1.5 s` have elapsed since the last operator command (`last_user_cmd_mono`), `polling_worker` re-sends the mismatched commands (debounced every `2.0 s` via `last_state_send_mono`). Likewise, if `thresholds_match_current()` is false, it re-sends `THRESH:...` at most once per `2.0 s`. Both `state_sent` and `thresholds_sent` reset to `False` when `serial_conn` is `None`.




- Every 1.0 s, while connected, writes `b"STATUS\n"` under `serial_write_lock`
  and increments `outgoing`.
- While `logging_active`, calls `log_data_to_csv()` once per loop.

### `telemetry_worker` (daemon thread)

- Every 4 s reads `/proc/stat`, `/proc/meminfo`, the thermal zone and disk
  usage into `cached_sys_stats`. It also reads
  `/sys/class/hwmon/hwmon1/in0_lcrit_alarm` for `power_ok`.

### `clock_sync_worker` (daemon thread)

- Syncs the Pi's system clock from HTTP `Date` response headers over the LAN (`sync_clock_from_lan_http()`) without logging into the captive portal:
  - Probes `http://connectivitycheck.gstatic.com/generate_204` and `http://example.com/` (plus the default gateway as fallback).
  - If the port is walled by a captive portal that returns a bare JavaScript `window.location` redirect stub without a `Date` header, follows that redirect URL once (with an unverified SSL context) to read the `Date` header from the captive-portal web server itself.
  - When the UTC timestamp differs from the local clock by more than 2 s (or on first sync), disables `systemd-timesyncd` override (`sudo timedatectl set-ntp false`), sets the UTC system clock (`sudo date -u -s ...`), and calls `time.tzset()`.
  - Retries every 5 s until the first sync succeeds, then re-checks every 900 s unless manual time was set via `/set_time`.

## REST routes

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | Render `index.html` |
| `/status` | GET | `current_data` + `logging`, `packets`, `sys`, `clock` (`time`, `date`, `iso`, `synced`) |
| `/raw` | GET | Buffered received lines |
| `/control` | POST | Heater, Pump 1 (`D3`), Pump 2 (`D7`), Stirrer (`D4`), per-pin `0–255` PWM (`mosfet_pwm`), and hysteresis target updates |
| `/sys_stats` | GET | `cached_sys_stats` + `packets` |
| `/reset_packets` | POST | Zero the packet counters |
| `/toggle_logging` | POST | Flip `logging_active` |
| `/set_mode` | POST | Send `MODE_AUTO` / `MODE_OFF`, update the cache |
| `/command` | POST | `SHUTDOWN` → `sudo shutdown -h now` |
| `/files` | GET | List `*.csv` in `logs/`, newest first |
| `/read_log` | GET | First line plus the last `lines` (max 200) of a log |
| `/get_drives` | GET | Mount the first `sd*` at `/media/usb`, return free space |
| `/export` | POST | Copy a log to the mounted USB, then `sync` |
| `/download/info` | POST | Set active CSV for the LAN download server on port 7514 |
| `/m/<filename>` | GET, POST | Password-protected log download page |
| `/get_ip` | GET | Local IP plus interface mode string |
| `/get_timezones` | GET | `timedatectl list-timezones` |
| `/set_timezone` | POST | `sudo timedatectl set-timezone`, `time.tzset()`, returns updated `clock` |
| `/set_time` | POST | `sudo timedatectl set-ntp false` then `sudo date -s`, pauses background HTTP clock resync, returns updated `clock` |

### `POST /control` Actions

- `{"action": "heater", "mode": "AUTO"|"ON"|"OFF"}` → sends `HEATER_<mode>`.
- `{"action": "pump1", "mode": "AUTO"|"ON"|"OFF"}` → sends `PUMP1_<mode>` (controls Pump 1 MOSFET on `Pin D3`).
- `{"action": "pump2", "mode": "AUTO"|"ON"|"OFF"}` → sends `PUMP2_<mode>` (controls Pump 2 MOSFET on `Pin D7`).
- `{"action": "pump", "mode": "AUTO"|"ON"|"OFF"}` → sends `PUMP_<mode>` (controls both Pump 1 and Pump 2).
- `{"action": "mosfet_pwm", "pin": "D3"|"D7"|"D4", "pwm": 0..255}` → sends `PWM:<pin>:<pwm>` to adjust the target PWM ceiling (`0`–`255`) for water debit testing on `Pin D3` (Pump 1), `Pin D7` (Pump 2), or `Pin D4` (Stirrer).
- `{"action": "stirrer", "mode": "OFF"|"ON"|"PULSE"|"TIMED", "minutes": int, "seconds": int}` → sends `STIR:<m>:<mins>:<secs>`.
- `{"action": "temp_target", "target": float, "hysteresis": float}` → sends `TEMP:<target>:<hyst>`.
- `{"action": "thresholds", "out_stop": int, "dry": int, "in_stop": int, "leak": int}` (example `{"action":"thresholds","out_stop":400,"dry":460,"in_stop":550,"leak":200}`) → validates (`0–1023` each, trip-only), updates the volatile `thresholds` dict, sends `THRESH:...`, and caches the values optimistically. `400` on malformed/unknown action. Volatile: lost on Flask restart.

Actuator modes are cached optimistically when a command is accepted, until the next `STATUS` reply overwrites them (polled every 1 s). The optimistic write reflects what the firmware will actually do:

- Heater: `ON` caches `heater = True` only while `water_crit` is false (`water_adc > out_stop_thr`, `400`) and no leak trip is active.
- Pump 1: `ON` caches `pump1 = True` while no leak trip is active and its PWM ceiling is nonzero (debug `FORCE ON` ignores `water_full`; the master still honours `MODE_OFF`/leak).
- Pump 2: `ON` caches `pump2 = True` while no leak trip is active and its PWM ceiling is nonzero (debug `FORCE ON` ignores `water_crit`; the master still honours `MODE_OFF`/leak).

---
Content generated by LLM.
