# 4. Backend — `web_control.py`

Flask + Flask-SocketIO application. Listens on `0.0.0.0:5000`, debug off, `allow_unsafe_werkzeug=True`.

## Global state

```python
current_data = {
    'temp': 0.0,              # bath DS18B20 reading
    'bottle_avg_temp': 0.0,   # cascaded bottle average from master
    'heater': False,
    'fan': False,
    'pump': False,
    'water_low': False,
    'system_mode': 'AUTO',    # 'AUTO' | 'OFF'
    'bottles': []             # up to 4 bottle dicts
}
```

Plus module-level `serial_conn`, `serial_lock`, `logging_active`.

## Background workers

### `serial_worker` (daemon thread)
- Scans `['/dev/ttyACM0','/dev/ttyUSB0','/dev/ttyACM1','/dev/ttyUSB1']` and opens the first present port at 115200 (2 s settle for bootloader).
- Ingest loop reads lines; if a line is JSON it is classified:
  1. `bottles` array or `type == 'slave_telemetry'` → merge into `current_data['bottles']` by id; log CSV.
  2. Single bottle (`id` + `temp`) → merge by id; log CSV.
  3. `type == 'master_status'` → update bath temp, bottle avg, heater/pump/fan, water_low, system_mode.
  4. Otherwise `current_data.update(payload)`.
- After handling, emits `update` with the whole `current_data` over SocketIO.
- On exception, closes the port and retries after 2 s.

### `polling_worker` (daemon thread)
- Every 1.0 s, if connected, writes `b"STATUS\n"` to the serial port (non-blocking, lock-protected).

## CSV logging

- `LOG_DIR = 'logs'`, files named `YYYY-MM-DD.csv`.
- Headers written once per new file: `Timestamp, Bath_Temp_C, Bottle_Avg_Temp_C, Mode, Heater, Pump, Fan, Water_Low` + per-bottle `B{i}_Temp, _pH, _CH4_ppm, _CH4_mL, _CO2_ppm, _CO2_mL, _Press_hPa`.
- `log_data_to_csv()` is called whenever bottle/master telemetry arrives, but only writes if `logging_active`.
- `logging_active` toggled by `POST /toggle_logging`.

## REST routes

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | Render `index.html` |
| `/status` | GET | Return `current_data` + `logging` flag |
| `/toggle_logging` | POST | Flip logging on/off |
| `/set_mode` | POST | Send `MODE_AUTO`/`MODE_OFF` to master; update cache |
| `/command` | POST | `SHUTDOWN` → `sudo shutdown -h now` (no auth!) |
| `/files` | GET | List `*.csv` in `logs/` (sorted desc) |
| `/get_drives` | GET | Find an `sd*` block device, mount it at `/media/usb` (uid/gid 1000), return free space |
| `/export` | POST | `shutil.copy2` a log file to the mounted USB; `sync` |
| `/get_ip` | GET | Socket-based local IP + SSID via `iwgetid` |
| `/get_timezones` | GET | `timedatectl list-timezones` |
| `/set_timezone` | POST | `sudo timedatectl set-timezone` |
| `/set_time` | POST | `sudo date -s` |

## SocketIO events

- Server → client: `update` (full `current_data`) after each ingested serial line.

## Concurrency model

Serial access is guarded by `serial_lock`. The serial worker holds the lock only briefly (read/write); the polling worker writes under the lock. The Flask request handlers that need serial (`/set_mode`) also take the lock. No database; all state is in-process, so a restart loses in-memory telemetry (CSV files persist).

## Observations

- `/command` SHUTDOWN has no authentication — anyone who can reach port 5000 can shut down the Pi.
- `/get_drives` mounts the first `sd*` device it finds with `sudo mount` — assumes passwordless sudo (true here).
- The `master_status` from the master reports `bottle_temp_avg` as a single cascaded value; per-bottle detail comes from slave JSON.
- `water_low` from the master is currently always 0 in the live firmware (water-level sensing bypassed).
