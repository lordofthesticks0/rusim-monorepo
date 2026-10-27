# 13. Flask REST API reference — `web_control.py`

Complete endpoint list for the Flask app on `invitrocontroller`. Every route is
served from `legacy/pi/water-control-system/web_control.py` (same file as
`~/water-control-system/web_control.py`). Server: `0.0.0.0:5000`,
`debug=False`, `threaded=True`.

The same process also runs a bare CSV download server on `0.0.0.0:7514`. It has
no HTML UI and no authentication: every request returns a CSV file.

All responses from port 5000 are JSON unless stated otherwise. Only
`/m/<filename>` has any authentication; every other route is open to anyone who
can reach port 5000. The port 7514 server is open to anyone who can reach it.

## Summary

| # | Route | Method | Body | Controls hardware? |
|---|---|---|---|---|
| 1 | `/` | GET | — | No |
| 2 | `/status` | GET | — | No |
| 3 | `/raw` | GET | query `after`, `limit` | No |
| 4 | `/control` | POST | JSON `action`: `heater` / `pump` / `stirrer` / `temp_target` | **Yes — heater, pump, stirrer, target** |
| 5 | `/sys_stats` | GET | — | No |
| 6 | `/reset_packets` | POST | — | No |
| 7 | `/toggle_logging` | POST | — | No |
| 8 | `/download/info` | POST | `filename` | No |
| 9 | `GET http://<pi>:7514/` | GET | — | No (separate server) |
| 10 | `/m/<filename>` | GET, POST | form `username`, `password` | No |
| 11 | `/set_mode` | POST | `mode` | **Yes — system mode (`MODE_AUTO` / `MODE_OFF`)** |
| 12 | `/command` | POST | `command` | No (powers off the Pi) |
| 13 | `/files` | GET | — | No |
| 14 | `/read_log` | GET | query `filename`, `lines` | No |
| 15 | `/get_drives` | GET | — | No (mounts USB) |
| 16 | `/export` | POST | `filename`, `dest_path` | No |
| 17 | `/get_ip` | GET | — | No |
| 18 | `/get_timezones` | GET | — | No |
| 19 | `/set_timezone` | POST | `timezone` | No |
| 20 | `/set_time` | POST | `datetime` | No |

## Turning the heater off

There is **no heater-specific endpoint**. The only software path is `/set_mode`
with `OFF`:

```bash
curl -s -X POST http://127.0.0.1:5000/set_mode \
  -H 'Content-Type: application/json' \
  -d '{"mode":"OFF"}'
```

Effect inside the firmware: the Pi writes `MODE_OFF\n` to the master over USB
serial. The master sets `currentMode = MODE_OFF` and immediately calls
`forceAllOff()`: heater `LOW`, pump high-Z, stirrer `LOW`. It stays off until
`MODE_AUTO` is sent or the board is reset. The master defaults to `MODE_AUTO` on
boot (`currentMode = MODE_AUTO`).

Verify:

```bash
curl -s http://127.0.0.1:5000/status | python3 -m json.tool
```

Expected: `"system_mode": "OFF"`, `"heater": false`, `"pump": false`. The
heater and pump also start at `"heater_mode": "OFF"` / `"pump_mode": "OFF"`
because `web_control.py` sends `HEATER_OFF`, `PUMP_OFF` and `STIR:0:10:30` once
on its first serial connection.

Caveats:

- `/set_mode` returns `{"status":"success","system_mode":"OFF"}` even when
  `serial_conn` is `None`. It updates its own cache but sends nothing to the
  board. The JSON response is not proof the relay opened; check `/status` for
  `"heater": false` on the next `master_status` line, or check the board.
- Invalid `mode` returns `{"status":"error"}` with HTTP 200.
- If `request.json` is missing or not JSON, Flask raises 400/415 before the
  handler.
- The physical switch on master `D6` also sets `MODE_OFF` (`LOW` = OFF). This
  works with the Pi powered down.
- `MODE_AUTO` does not immediately re-enable the heater; the normal thermostat
  conditions apply on the next loop.
- `POST /control` with `{"action":"heater","mode":"OFF"}` is the narrower
  alternative: it sends `HEATER_OFF`, which sets the heater override to forced
  off while leaving `system_mode` alone. The same works for `pump`.

### Stopping the Flask app does not stop the heater

The master has no Pi heartbeat or command timeout. It keeps running whatever
`currentMode` it last received. If you stop Flask while the system is in
`MODE_AUTO`, the master continues heating on its own (subject to the bottle
thermostat, the 41.0 °C hard bath ceiling, and the 12 s slave timeout). To make a
shutdown safe:

The master boots with `currentMode = MODE_AUTO` but `heaterOverride` and
`pumpOverride` already forced off, so a reset leaves the heater off until
something selects `ON` or `AUTO` for it.

1. Send `MODE_OFF` (or `POST /control` with `action=heater`, `mode=OFF`) and
   confirm `heater: false`.
2. Then stop the service.

PM2 runs the app as `water-ui` (id 0), resurrected at boot by
`pm2-invitro.service`:

```bash
pm2 stop water-ui        # stop now, still resurrects on boot
pm2 delete water-ui      # remove from PM2
pm2 save                 # persist the empty process list
sudo systemctl disable --now pm2-invitro   # stop boot resurrection
```

To bring it back: `pm2 start ~/water-control-system/web_control.py --name water-ui`
(or the original start command), then `pm2 save`.

## Endpoint details

### 1. `GET /`

Renders `templates/index.html` (the kiosk dashboard). No parameters.

### 2. `GET /status`

The polling endpoint used by the kiosk every second.

Response fields:

| Field | Type | Meaning |
|---|---|---|
| `temp` | float | Bath DS18B20 temperature; `-999.0` when invalid |
| `bottle_avg_temp` | float | Average bottle temperature from the master; `-999.0` when invalid |
| `heater` | bool | Last reported heater state |
| `stirrer` | bool | Last reported stirrer state. There is no `fan` field; the exhaust-fan badge in the UI is a static label, not telemetry. |
| `pump` | bool | Last reported pump state |
| `heater_mode` / `pump_mode` / `stir_mode` | string | Cached override mode (`AUTO` / `ON` / `OFF`, and `OFF` / `ON` / `PULSE` / `TIMED` for the stirrer) from the last master frame or `POST /control` |
| `target_temp` / `hysteresis` | float | Setpoint sent with `TEMP:` (defaults 39.0 / 0.3) |
| `water_low` | bool | Dry-run flag from the master. `false` until the master reports it; the current firmware drives it from `A7` with a 5-check debounce (see `06-master-firmware.md`). An older firmware build reported a constant `false`. |
| `water_adc` / `water_thr` | int | 16-sample averaged water-sensor ADC and the threshold (`200`) the master trips on |
| `system_mode` | string | `AUTO` or `OFF`, cached from the last master frame or `/set_mode` |
| `bottles` | array | Per-bottle objects; forced to `[]` when the slave is older than 12 s |
| `logging` | bool | Whether CSV logging is active |
| `packets` | object | `total`, `outgoing`, `incoming`, `rx_master`, `rx_slave`, `lost`, `loss_pct` |
| `sys` | object | Cached CPU/RAM/temperature/disk/power figures |

`bottles` comes from the slave JSON, merged by bottle `id`. It is cleared when
the slave has not reported for `SLAVE_STALE_SECONDS = 12.0`.

### 3. `GET /raw`

Serves the in-memory ring buffer of every serial line received, including
non-JSON boot output. It never opens the port, so it is safe to poll.

| Query | Default | Limits |
|---|---|---|
| `after` | absent | Return only entries with `seq > after`, oldest first |
| `limit` | 100 | Clamped to 1..500; non-integer → 400 |

Response:

```json
{
  "seq": 1234,
  "count": 1,
  "buffered": 500,
  "connected": true,
  "lines": [{"seq": 1234, "ts": 1792642177.702, "line": "..."}]
}
```

- `seq` is the newest sequence number overall.
- If `seq` decreases between calls, the Flask process restarted.
- A gap after `after + 1` means the buffer wrapped; those lines are gone.
- `Cache-Control: no-store`.
- At the live rate (~2.1 lines/s) 500 lines cover about four minutes.

### 4. `POST /control`

The actuator and setpoint endpoint used by the Controls tab. The body is JSON
with an `action` field; an unknown or missing action returns HTTP 400
`{"status":"error","message":"Invalid control action"}`.

| `action` | Extra fields | Behaviour |
|---|---|---|
| `heater` | `mode` = `AUTO` / `ON` / `OFF` (default `AUTO`) | Sends `HEATER_<mode>` and updates `current_data['heater_mode']`. `ON` sets the cached `heater` true unless `water_low` is set; `OFF`, or `water_low`, sets it false. |
| `pump` | `mode` = `AUTO` / `ON` / `OFF` (default `AUTO`) | Sends `PUMP_<mode>` and updates `pump_mode`, with the same `water_low` guard. |
| `stirrer` | `mode` = `OFF` / `ON` / `PULSE` / `TIMED` (default `OFF`), `minutes` (default 10), `seconds` (default 30) | Sends `STIR:<0-3>:<mins>:<secs>` and updates `stir_mode`, `stir_int_min` and `stir_pulse_sec` (or `stir_timed_min`). The cached `stirrer` is true when the mode is not `OFF` and `water_low` is false. |
| `temp_target` | `target` (default 39.0), `hysteresis` (default 0.3) | Rounds both to one decimal and sends `TEMP:<target>:<hysteresis>`; updates `target_temp` and `hysteresis`. |

A valid action returns `{"status":"success"}`. As with `/set_mode`, the cache is
updated whether or not the serial write happened, so the response is not proof
that the board changed.

### 5. `GET /sys_stats`

Returns `cached_sys_stats` plus `packets`. Same data as `/status`'s `sys` and
`packets` fields, without the telemetry. The cache is refreshed every 4 s by
`telemetry_worker`.

### 6. `POST /reset_packets`

Zeros `total`, `outgoing`, `incoming`, `rx_master`, `rx_slave`, `lost` and
`loss_pct`.

```json
{"status": "success", "packets": {...}}
```

### 7. `POST /toggle_logging`

Flips `logging_active`. On the transition to active it immediately writes one
row via `log_data_to_csv()`. The polling worker then writes a row every second
while logging is on.

```json
{"status": "success", "logging": true}
```

### 8. `POST /download/info`

Body: `{"filename": "2026-10-22.csv"}`. The file must exist in `logs/` and end
in `.csv`, otherwise HTTP 400 with
`{"status":"error","message":"Select a valid log file first."}`. If `eth0` has
no IPv4 address, HTTP 503 with
`{"status":"error","message":"No LAN connection available."}`.

Sets the in-memory `download_target` to the selected basename and returns:

| Field | Value |
|---|---|
| `status` | `success` |
| `filename` | Selected log basename |
| `ip` | `eth0` IPv4 address |
| `port` | `7514` |
| `url` | `http://<eth0-ip>:7514/` |
| `url_qr` | PNG data URI of `url` |

`url_qr` is an empty string when the `qrcode` package is missing. No hotspot is
started; `wlan0` stays off.

### 9. `GET http://<pi>:7514/` (separate server)

A `ThreadingHTTPServer` started by `web_control.py` on `0.0.0.0:7514`. It has
no HTML page, no login form, and no directory listing.

- `GET /` returns the current `download_target` as an attachment. If no target
  has been selected since the process started, it returns the newest `*.csv` in
  `logs/`.
- `GET /<filename>` returns that CSV from `logs/`.
- Paths are reduced with `os.path.basename` and must end in `.csv`; everything
  else is HTTP 404.
- Headers include `Content-Type: text/csv; charset=utf-8`,
  `Content-Disposition: attachment; filename="<filename>"`, and
  `Cache-Control: no-store`.
- `HEAD` returns the same headers without a body.
- The server streams the file in 64 KiB chunks.

Because there is no authentication, anyone on the same LAN can download any CSV
in `logs/` by name.

### 10. `GET|POST /m/<filename>`

Password-protected download portal for one log file. `filename` is reduced with
`os.path.basename`, then resolved inside `logs/`; a missing file is HTTP 404.

- `GET` renders a small HTML login form.
- `POST` reads form fields `username` and `password`. Correct values
  (`invitro` / `password`) return the CSV as an attachment. Wrong values
  re-render the form with `Invalid username or password.`

### 11. `POST /set_mode`

Body: `{"mode": "AUTO"}` or `{"mode": "OFF"}`.

- Writes `MODE_AUTO\n` or `MODE_OFF\n` to the serial port under
  `serial_write_lock` when connected.
- Increments `packet_stats['outgoing']`.
- Updates `current_data['system_mode']` regardless of whether the write
  happened.
- Returns `{"status":"success","system_mode":"<mode>"}`.
- Any other value returns `{"status":"error"}` with HTTP 200.

This endpoint changes the system mode only. Per-actuator overrides and the
temperature setpoint go through [`POST /control`](#4-post-control). See
[the heater section](#turning-the-heater-off).

### 12. `POST /command`

Body: `{"command": "SHUTDOWN"}`.

- On `SHUTDOWN`: runs `sync`, then `sudo shutdown -h now` in the background and
  returns `{"status":"shutting down"}`.
- Any other value returns `{"status":"ignored"}`.

No authentication. Anyone reaching port 5000 can power off the Pi.

### 13. `GET /files`

Returns a JSON array of `*.csv` basenames in `logs/`, sorted reverse
lexicographically (newest date first). Empty array when there are none.

### 14. `GET /read_log`

Query: `filename` (required), `lines` (default 40, capped at 200).

- `filename` is reduced with `os.path.basename`.
- Missing or nonexistent file → HTTP 404 `{"status":"error"}`.
- Response: `{"status":"success","filename":...,"total_shown":N,"raw":"..."}`.
- `raw` is the header line followed by the last `N` lines in **newest-first**
  order (`deque` holds them in file order, then joins `reversed`), so the text
  is not the same order as the file.
- A non-integer `lines` raises `ValueError` inside the handler and returns
  HTTP 500.

### 15. `GET /get_drives`

Runs `lsblk -r -n -o NAME,TYPE,MOUNTPOINT`, picks the first device whose name
starts with `sd`, and mounts it at `/media/usb` with
`sudo mount -o uid=1000,gid=1000 /dev/<name> /media/usb` if nothing is mounted
there.

Returns an array with at most one object:

```json
[{"path": "/media/usb", "name": "USB Drive", "free_gb": 27.5, "total_gb": 29.7}]
```

Returns `[]` when no drive is mounted. Mount failures are swallowed.
`mkfs`/partition handling does not exist; the first `sd*` may be the SD card.

### 16. `POST /export`

Body: `{"filename": "<name>.csv", "dest_path": "/media/usb"}`.

- `src = os.path.join('logs', filename)` and `dest = os.path.join(dest_path, filename)`.
- If `src` does not exist or `dest_path` is not a mount point:
  `{"status":"error","message":"Drive or log unavailable."}` with HTTP 200.
- On success: `shutil.copy2` then `sync`, and
  `{"status":"success","message":"Export Successful!"}`.
- Exceptions return the exception text in `message`.

`filename` is **not** run through `basename`. An absolute path in `filename`
escapes `logs/` and is copied to the USB drive. `dest_path` is only checked with
`os.path.ismount`.

### 17. `GET /get_ip`

Opens a UDP socket to `8.8.8.8:1` to learn the outbound interface address, then
runs `iwgetid -r` for the SSID.

```json
{"ip": "10.92.128.23", "ssid": "SomeWiFi"}
```

Defaults to `"Unavailable"` and `"Ethernet (LAN)"` on failure.

### 18. `GET /get_timezones`

Returns `timedatectl list-timezones` as a JSON array of strings. Returns `[]`
on failure.

### 19. `POST /set_timezone`

Body: `{"timezone": "Asia/Jakarta"}`. Runs
`sudo timedatectl set-timezone <value>` with `check=True`. Returns
`{"status":"success"}` or `{"status":"error"}`. The value is passed as a single
argv element, so shell metacharacters are not interpreted.

### 20. `POST /set_time`

Body: `{"datetime": "2026-10-22 14:30:00"}`. Runs
`sudo date -s <value>` with `check=True`. Returns `{"status":"success"}` or
`{"status":"error"}`.

## Source-level notes

- `DOWNLOAD_PORT = 7514` and the in-memory `download_target` control the direct
  download server.
- `PORTAL_USER` and `PORTAL_PASS` are used only by the legacy `/m/<filename>`
  portal. `HOTSPOT_SSID` and `HOTSPOT_WIFI_PASS` are no longer used by the
  download flow.
- `disable_wifi_client_and_stop_hotspot()` still runs at startup and turns the
  Wi-Fi radio off; the download server uses `eth0`.
- Passwordless sudo is assumed for `nmcli`, `mount`, `shutdown`, `timedatectl`
  and `date`.
- The serial port is opened once by `serial_worker`; `/raw` reads the in-memory
  copy. Two programs cannot hold `/dev/ttyUSB0` at the same time, so do not
  write to it by hand while Flask runs.
- `/status`, `/raw`, `/sys_stats` and all control routes have no CSRF or origin
  check. On the LAN, any page can POST to them.
- The port 7514 server has no authentication. Treat every CSV in `logs/` as
  readable by anyone on the LAN.

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.

Update by `xiaomi/mimo-v2.6-flash-slow` in `freebuff` (2026-10-27): added the missing `POST /control` endpoint, fixed the `/status` field list, and corrected the heater-off procedure.
