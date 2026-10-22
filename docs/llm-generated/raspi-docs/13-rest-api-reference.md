# 13. Flask REST API reference — `web_control.py`

Complete endpoint list for the Flask app on `invitrocontroller`. Every route is
served from `legacy/pi/water-control-system/web_control.py` (same file as
`~/water-control-system/web_control.py`). Server: `0.0.0.0:5000`,
`debug=False`, `threaded=True`.

All responses are JSON unless stated otherwise. Only `/m/<filename>` has any
authentication; every other route is open to anyone who can reach port 5000.

## Summary

| # | Route | Method | Body | Controls hardware? |
|---|---|---|---|---|
| 1 | `/` | GET | — | No |
| 2 | `/status` | GET | — | No |
| 3 | `/raw` | GET | query `after`, `limit` | No |
| 4 | `/sys_stats` | GET | — | No |
| 5 | `/reset_packets` | POST | — | No |
| 6 | `/toggle_logging` | POST | — | No |
| 7 | `/hotspot/start` | POST | `filename` | No |
| 8 | `/hotspot/stop` | POST | — | No |
| 9 | `/m/<filename>` | GET, POST | form `username`, `password` | No |
| 10 | `/set_mode` | POST | `mode` | **Yes — heater, pump, fan** |
| 11 | `/command` | POST | `command` | No (powers off the Pi) |
| 12 | `/files` | GET | — | No |
| 13 | `/read_log` | GET | query `filename`, `lines` | No |
| 14 | `/get_drives` | GET | — | No (mounts USB) |
| 15 | `/export` | POST | `filename`, `dest_path` | No |
| 16 | `/get_ip` | GET | — | No |
| 17 | `/get_timezones` | GET | — | No |
| 18 | `/set_timezone` | POST | `timezone` | No |
| 19 | `/set_time` | POST | `datetime` | No |

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

Expected: `"system_mode": "OFF"`, `"heater": false`, `"pump": false`,
`"fan": false`.

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

### Stopping the Flask app does not stop the heater

The master has no Pi heartbeat or command timeout. It keeps running whatever
`currentMode` it last received. If you stop Flask while the system is in
`MODE_AUTO`, the master continues heating on its own (subject to the bottle
thermostat, bath ceiling 39.8 °C, and the 12 s slave timeout). To make a
shutdown safe:

1. Send `MODE_OFF` and confirm `heater: false`.
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
| `fan` | bool | Last reported stirrer/fan state |
| `pump` | bool | Last reported pump state |
| `water_low` | bool | Always `false` in the live firmware (sensing bypassed) |
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

### 4. `GET /sys_stats`

Returns `cached_sys_stats` plus `packets`. Same data as `/status`'s `sys` and
`packets` fields, without the telemetry. The cache is refreshed every 4 s by
`telemetry_worker`.

### 5. `POST /reset_packets`

Zeros `total`, `outgoing`, `incoming`, `rx_master`, `rx_slave`, `lost` and
`loss_pct`.

```json
{"status": "success", "packets": {...}}
```

### 6. `POST /toggle_logging`

Flips `logging_active`. On the transition to active it immediately writes one
row via `log_data_to_csv()`. The polling worker then writes a row every second
while logging is on.

```json
{"status": "success", "logging": true}
```

### 7. `POST /hotspot/start`

Body: `{"filename": "2026-10-22.csv"}`. The file must exist in `logs/`,
otherwise HTTP 400 with `{"status":"error","message":"Select a valid log file first."}`.

Runs, ignoring failures:

1. `sudo nmcli radio wifi on`
2. `sudo nmcli con down invitro-hotspot`
3. `sudo nmcli device wifi hotspot ifname wlan0 con-name invitro-hotspot ssid InVitro-Hotspot password invitrowifi`

Then builds `http://<wlan-ip>:5000/m/<filename>` and returns:

| Field | Value |
|---|---|
| `ssid` | `InVitro-Hotspot` |
| `wifi_pass` | `invitrowifi` |
| `portal_user` | `invitro` |
| `portal_pass` | `password` |
| `url` | Download URL for the selected log |
| `wifi_qr` | PNG data URI of the WIFI: join string |
| `url_qr` | PNG data URI of the download URL |

`wifi_qr` and `url_qr` are empty strings when the `qrcode` package is missing.

Credentials are constants at the top of `web_control.py` and are returned to
any unauthenticated caller.

### 8. `POST /hotspot/stop`

Runs `sudo nmcli con down invitro-hotspot` in the background and returns
`{"status":"success"}`. It does not report the result of `nmcli`.

### 9. `GET|POST /m/<filename>`

Password-protected download portal for one log file. `filename` is reduced with
`os.path.basename`, then resolved inside `logs/`; a missing file is HTTP 404.

- `GET` renders a small HTML login form.
- `POST` reads form fields `username` and `password`. Correct values
  (`invitro` / `password`) return the CSV as an attachment. Wrong values
  re-render the form with `Invalid username or password.`

### 10. `POST /set_mode`

Body: `{"mode": "AUTO"}` or `{"mode": "OFF"}`.

- Writes `MODE_AUTO\n` or `MODE_OFF\n` to the serial port under
  `serial_write_lock` when connected.
- Increments `packet_stats['outgoing']`.
- Updates `current_data['system_mode']` regardless of whether the write
  happened.
- Returns `{"status":"success","system_mode":"<mode>"}`.
- Any other value returns `{"status":"error"}` with HTTP 200.

This is the only endpoint that changes actuator state. See
[the heater section](#turning-the-heater-off).

### 11. `POST /command`

Body: `{"command": "SHUTDOWN"}`.

- On `SHUTDOWN`: runs `sync`, then `sudo shutdown -h now` in the background and
  returns `{"status":"shutting down"}`.
- Any other value returns `{"status":"ignored"}`.

No authentication. Anyone reaching port 5000 can power off the Pi.

### 12. `GET /files`

Returns a JSON array of `*.csv` basenames in `logs/`, sorted reverse
lexicographically (newest date first). Empty array when there are none.

### 13. `GET /read_log`

Query: `filename` (required), `lines` (default 40, capped at 200).

- `filename` is reduced with `os.path.basename`.
- Missing or nonexistent file → HTTP 404 `{"status":"error"}`.
- Response: `{"status":"success","filename":...,"total_shown":N,"raw":"..."}`.
- `raw` is the header line followed by the last `N` lines in **newest-first**
  order (`deque` holds them in file order, then joins `reversed`), so the text
  is not the same order as the file.
- A non-integer `lines` raises `ValueError` inside the handler and returns
  HTTP 500.

### 14. `GET /get_drives`

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

### 15. `POST /export`

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

### 16. `GET /get_ip`

Opens a UDP socket to `8.8.8.8:1` to learn the outbound interface address, then
runs `iwgetid -r` for the SSID.

```json
{"ip": "10.92.128.23", "ssid": "SomeWiFi"}
```

Defaults to `"Unavailable"` and `"Ethernet (LAN)"` on failure.

### 17. `GET /get_timezones`

Returns `timedatectl list-timezones` as a JSON array of strings. Returns `[]`
on failure.

### 18. `POST /set_timezone`

Body: `{"timezone": "Asia/Jakarta"}`. Runs
`sudo timedatectl set-timezone <value>` with `check=True`. Returns
`{"status":"success"}` or `{"status":"error"}`. The value is passed as a single
argv element, so shell metacharacters are not interpreted.

### 19. `POST /set_time`

Body: `{"datetime": "2026-10-22 14:30:00"}`. Runs
`sudo date -s <value>` with `check=True`. Returns `{"status":"success"}` or
`{"status":"error"}`.

## Source-level notes

- `PORTAL_USER`, `PORTAL_PASS`, `HOTSPOT_SSID` and `HOTSPOT_WIFI_PASS` are
  hard-coded at the top of the file.
- Passwordless sudo is assumed for `nmcli`, `mount`, `shutdown`, `timedatectl`
  and `date`.
- The serial port is opened once by `serial_worker`; `/raw` reads the in-memory
  copy. Two programs cannot hold `/dev/ttyUSB0` at the same time, so do not
  write to it by hand while Flask runs.
- `/status`, `/raw`, `/sys_stats` and all control routes have no CSRF or origin
  check. On the LAN, any page can POST to them.

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.
