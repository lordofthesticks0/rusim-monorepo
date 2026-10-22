# 4. Backend — `web_control.py`

Plain Flask, no SocketIO. Listens on `0.0.0.0:5000` with
`debug=False, threaded=True`. The kiosk page polls `/status` once per second.

## Global state

```python
current_data = {
    'temp': -999.0,           # bath DS18B20 reading
    'bottle_avg_temp': -999.0,# cascaded bottle average from master
    'heater': False,
    'fan': False,             # stirrer, kept under the old key
    'pump': False,
    'water_low': False,
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
| `raw_seq` | Counter, incremented once per stored line |
| `raw_lock` | `threading.Lock` around both |
| `RAW_BUFFER_LINES` | Buffer size, `500` |

`record_raw_line()` strips the line, appends it with `time.time()` rounded to
milliseconds, and takes the lock. Non-JSON lines are kept; the debug output of
a board that is still booting is exactly what the buffer is for. The buffer is
in memory, so a restart clears it and resets `seq` to `0`. At the live rate of
about 2.1 lines/s (one `master_status` per second plus one slave packet per
3.5 s) 500 lines cover roughly four minutes.

## Background workers

### `serial_worker` (daemon thread)

- Scans `['/dev/ttyACM0', '/dev/ttyUSB0', '/dev/ttyACM1', '/dev/ttyUSB1']` and
  opens the first present port at 115200, with a 2 s settle.
- Each line is recorded in the raw buffer, then classified if it is JSON:
  1. `bottles` array or `type == 'slave_telemetry'` → merge into
     `current_data['bottles']` by id.
  2. Single bottle (`id` + `temp`) → merge by id.
  3. `type == 'master_status'` → update bath temp, bottle average,
     heater/pump/fan, water_low and system_mode.
  4. Anything else → `current_data.update(payload)`.
- `rx_slave` and `rx_master` are incremented per accepted message, then
  `update_packet_metrics()` recomputes totals and loss.
- On exception it closes the port, sets `serial_conn = None` and retries after
  2 s.

### `polling_worker` (daemon thread)

- Every 1.0 s, while connected, writes `b"STATUS\n"` under `serial_write_lock`
  and increments `outgoing`.
- While `logging_active`, calls `log_data_to_csv()` once per loop.

### `telemetry_worker` (daemon thread)

- Every 4 s reads `/proc/stat`, `/proc/meminfo`, the thermal zone and disk
  usage into `cached_sys_stats`. It also reads
  `/sys/class/hwmon/hwmon1/in0_lcrit_alarm` for `power_ok`.

## CSV logging

- `LOG_DIR = 'logs'`, one file per day, `YYYY-MM-DD.csv`.
- Header columns: `Timestamp, Bath_C, BAvg_C, Mode, Heater, Pump, Fan, Water,
  Pkt_Tot, Pkt_TX, Pkt_RX, Pkt_Lost, Loss_%`, then per bottle `B{i}_Temp,
  B{i}_pH, B{i}_CH4, B{i}_CH4mL, B{i}_CO2, B{i}_CO2mL, B{i}_Pa`.
- `fmt_val()` writes `null` for `None`, `-1`, `-999` and anything below `-50`.
- Rows are aligned with spaces and joined by `" , "`.
- Slave values are written only while the slave is fresh; master values only
  while the master is fresh. `SLAVE_STALE_SECONDS = 12.0` controls both.

## REST routes

| Route | Method | Purpose |
|---|---|---|
| `/` | GET | Render `index.html` |
| `/status` | GET | `current_data` + `logging`, `packets`, `sys` |
| `/raw` | GET | Buffered received lines, see below |
| `/sys_stats` | GET | `cached_sys_stats` + `packets` |
| `/reset_packets` | POST | Zero the packet counters |
| `/toggle_logging` | POST | Flip `logging_active` |
| `/set_mode` | POST | Send `MODE_AUTO` / `MODE_OFF`, update the cache |
| `/command` | POST | `SHUTDOWN` → `sudo shutdown -h now` (no auth) |
| `/files` | GET | List `*.csv` in `logs/`, newest first |
| `/read_log` | GET | First line plus the last `lines` (max 200) of a log |
| `/get_drives` | GET | Mount the first `sd*` at `/media/usb`, return free space |
| `/export` | POST | Copy a log to the mounted USB, then `sync` |
| `/hotspot/start` | POST | Raise the `InVitro-Hotspot` AP with QR codes |
| `/hotspot/stop` | POST | Bring the AP down |
| `/m/<filename>` | GET, POST | Password-protected log download page |
| `/get_ip` | GET | Local IP plus SSID from `iwgetid` |
| `/get_timezones` | GET | `timedatectl list-timezones` |
| `/set_timezone` | POST | `sudo timedatectl set-timezone` |
| `/set_time` | POST | `sudo date -s` |

Full request/response reference for every route:
[13-rest-api-reference.md](13-rest-api-reference.md). It also covers how to
turn the heater off and how to stop the Flask app without leaving the master in
`AUTO`.

### `GET /raw`

Reads only the in-memory buffer. It never opens the serial port, so it is safe
to poll while the dashboard runs.

```bash
# newest 100 lines (default limit)
curl -s http://127.0.0.1:5000/raw

# newest 20 lines
curl -s 'http://127.0.0.1:5000/raw?limit=20'

# only lines newer than sequence 1234
curl -s 'http://127.0.0.1:5000/raw?after=1234'
```

Response:

```json
{
  "seq": 1234,
  "count": 1,
  "buffered": 500,
  "connected": true,
  "lines": [
    {"seq": 1234, "ts": 1792642177.702, "line": "{\"type\":\"master_status\", ...}"}
  ]
}
```

| Field | Meaning |
|---|---|
| `seq` | Sequence number of the newest stored line |
| `count` | Lines in this response |
| `buffered` | Lines currently in the ring buffer |
| `connected` | Whether `serial_conn` is open |
| `lines` | Oldest first, `ts` in epoch seconds |

- Without `after`, the response is the tail of the buffer.
- With `after`, only `seq > after` is returned, oldest first. A poller stores
  `seq` and sends it back on the next request.
- `limit` defaults to 100, is clamped to 500, and must be at least 1.
- A non-integer `after` or `limit` is a 400.
- If `seq` decreases, the Flask process restarted. If the first returned
  `seq` is greater than `after + 1`, the buffer wrapped and those lines are
  gone.
- `Cache-Control: no-store`.
- Like `/status`, the route has no authentication. Both expose the same
  telemetry; `/raw` adds the exact wire lines.

## pH Sensor Conversion (PH-4502C / PH-402C)

The slave firmware emits raw analog voltage (0–5 V) under the `ph` key in its telemetry JSON. `web_control.py` maps this voltage to actual pH units using `convert_ph_voltage(raw_v, bottle_id)` before merging into `current_data['bottles']`.

- Raw voltage is retained under `b['ph_raw_v']`.
- Converted pH is stored under `b['ph']` and logged to `logs/YYYY-MM-DD.csv` in columns `B1_pH` through `B4_pH`.
- Per-bottle 3-point piecewise linear calibration constants are defined in `PH_CALIBRATION` (referencing standard buffer solutions at pH `4.01`, `6.86`, and `9.18`):
  - **Bottle 1**: `v_401 = 4.120 V`, `v_686 = 3.833 V`, `v_918 = 3.620 V`
  - **Bottles 2–4**: `v_401 = 4.160 V`, `v_686 = 3.833 V`, `v_918 = 3.630 V`
  - **Acidic range (`v > v_686`)**: `pH = 4.01 + ((6.86 - 4.01) / (v_686 - v_401)) * (v - v_401)`
  - **Alkaline range (`v <= v_686`)**: `pH = 6.86 + ((9.18 - 6.86) / (v_918 - v_686)) * (v - v_686)`

## Concurrency model

- `serial_write_lock` guards writes to `serial_conn` from `polling_worker` and
  `/set_mode`.
- Reading has a single consumer, `serial_worker`.
- `raw_lock` guards `raw_lines` and `raw_seq`; the `/raw` handler copies the
  deque under the lock and serialises after releasing it.
- No database. All state is in-process; a restart loses telemetry, the raw
  buffer and the packet counters. CSV files persist.

## Observations

- `/command` SHUTDOWN has no authentication. Anyone who can reach port 5000 can
  power off the Pi.
- `/get_drives` mounts the first `sd*` device with `sudo mount`; this assumes
  passwordless sudo, which is true here.
- `master_status` carries one cascaded `bottle_temp_avg`; per-bottle detail
  comes from the slave JSON.
- `water_low` is always 0 in the live firmware because the water-level sensing
  is bypassed.

---
Content generated by `google/gemini-4-argon` in `antigravity`.
