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
    'water_low': False,
    'water_adc': -1,          # 16-sample averaged bath water sensor ADC on A7 (0..1023)
    'water_thr': 200,         # dry-run ADC threshold on A7 (< 200 trips fault)
    'pump_leak': False,       # pump leak fault flag on A8
    'leak_adc': -1,           # 16-sample averaged pump leak sensor ADC on A8 (0..1023)
    'leak_thr': 200,          # wet-leak ADC threshold on A8 (>= 200 trips fault)
    'system_mode': 'AUTO',    # 'AUTO' | 'OFF'
    'bottles': []             # up to 4 bottle dicts
}
```

Plus module-level `serial_conn`, `serial_write_lock`, `logging_active`,
`last_slave_rx_time`, `last_master_rx_time`, `packet_stats`,
`cached_sys_stats`, and the raw line buffer described below.

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
  opens the first present port at 115200, with a 2 s settle.
- Each line is recorded in the raw buffer, then classified if it is JSON:
  1. `bottles` array or `type == 'slave_telemetry'` → merge into
     `current_data['bottles']` by id.
  2. Single bottle (`id` + `temp`) → merge by id.
  3. `type == 'master_status'` → update bath temp, bottle average,
     heater / Pump 1 (`D3`) / Pump 2 (`D7`) / Stirrer (`D4`) states, modes,
     target PWM ceilings (`pwm_d3`, `pwm_d7`, `pwm_d4`), live PWM outputs
     (`out_d3`, `out_d7`, `out_d4`), bath water level (`water_low`, `water_adc`,
     `water_thr`), pump leak sensor (`pump_leak`, `leak_adc`, `leak_thr`),
     and `system_mode`.
  4. Anything else → `current_data.update(payload)`.

### `polling_worker` (daemon thread)

- On the first loop where `serial_conn` is open, writes the boot-default
  commands `HEATER_OFF`, `PUMP1_OFF`, `PUMP2_OFF`, and `STIR:0:10:30` in that
  order, then sets `boot_defaults_applied = True`.
- Every 1.0 s, while connected, writes `b"STATUS\n"` under `serial_write_lock`
  and increments `outgoing`.
- While `logging_active`, calls `log_data_to_csv()` once per loop.

### `telemetry_worker` (daemon thread)

- Every 4 s reads `/proc/stat`, `/proc/meminfo`, the thermal zone and disk
  usage into `cached_sys_stats`. It also reads
  `/sys/class/hwmon/hwmon1/in0_lcrit_alarm` for `power_ok`.

## REST routes

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | Render `index.html` |
| `/status` | GET | `current_data` + `logging`, `packets`, `sys` |
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
| `/set_timezone` | POST | `sudo timedatectl set-timezone` |
| `/set_time` | POST | `sudo date -s` |

### `POST /control` Actions

- `{"action": "heater", "mode": "AUTO"|"ON"|"OFF"}` → sends `HEATER_<mode>`.
- `{"action": "pump1", "mode": "AUTO"|"ON"|"OFF"}` → sends `PUMP1_<mode>` (controls Pump 1 MOSFET on `Pin D3`).
- `{"action": "pump2", "mode": "AUTO"|"ON"|"OFF"}` → sends `PUMP2_<mode>` (controls Pump 2 MOSFET on `Pin D7`).
- `{"action": "pump", "mode": "AUTO"|"ON"|"OFF"}` → sends `PUMP_<mode>` (controls both Pump 1 and Pump 2).
- `{"action": "mosfet_pwm", "pin": "D3"|"D7"|"D4", "pwm": 0..255}` → sends `PWM:<pin>:<pwm>` to adjust the target PWM ceiling (`0`–`255`) for water debit testing on `Pin D3` (Pump 1), `Pin D7` (Pump 2), or `Pin D4` (Stirrer).
- `{"action": "stirrer", "mode": "OFF"|"ON"|"PULSE"|"TIMED", "minutes": int, "seconds": int}` → sends `STIR:<m>:<mins>:<secs>`.
- `{"action": "temp_target", "target": float, "hysteresis": float}` → sends `TEMP:<target>:<hyst>`.

---
Content generated by `google/gemini-4-argon` in `antigravity`.
