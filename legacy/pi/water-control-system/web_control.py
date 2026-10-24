#!/usr/bin/env python3
import io
import os
import glob
import json
import time
import base64
import socket
import shutil
import threading
import subprocess
from collections import deque
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse
from flask import (
    Flask, render_template, render_template_string,
    jsonify, request, send_from_directory
)
import serial

try:
    import qrcode
    QR_AVAILABLE = True
except ImportError:
    QR_AVAILABLE = False

app = Flask(__name__)

# --- FILE SYSTEM & HOTSPOT CONFIGURATION ---
LOG_DIR = 'logs'
MOUNT_POINT = '/media/usb'
SLAVE_STALE_SECONDS = 12.0

HOTSPOT_CON_NAME = "invitro-hotspot"
HOTSPOT_SSID = "InVitro-Hotspot"
HOTSPOT_WIFI_PASS = "invitrowifi"
PORTAL_USER = "invitro"
PORTAL_PASS = "password"

# --- DIRECT CSV DOWNLOAD SERVER ---
# A minimal HTTP server that serves the selected CSV over the LAN.
# It intentionally has no HTML UI: every request returns the file.
DOWNLOAD_PORT = 7514
download_target = None

os.makedirs(LOG_DIR, exist_ok=True)
os.makedirs(MOUNT_POINT, exist_ok=True)

serial_write_lock = threading.Lock()
raw_lines_lock = threading.Lock()
raw_lines = deque(maxlen=500)
raw_line_seq = 0
serial_conn = None
logging_active = False
last_slave_rx_time = 0.0
last_master_rx_time = 0.0

# Actuators start OFF. These are sent once on the first serial connection so
# the master is off even when it was flashed with AUTO boot defaults.
BOOT_DEFAULT_COMMANDS = ("HEATER_OFF", "PUMP_OFF", "STIR:0:10:30")
boot_defaults_applied = False

packet_stats = {
    'total': 0,
    'outgoing': 0,
    'incoming': 0,
    'rx_master': 0,
    'rx_slave': 0,
    'lost': 0,
    'loss_pct': 0.0
}

current_data = {
    'temp': -999.0,
    'bottle_avg_temp': -999.0,
    'heater': False,
    'pump': False,
    'stirrer': False,
    'heater_mode': 'OFF',
    'pump_mode': 'OFF',
    'stir_mode': 'OFF',
    'stir_int_min': 10,
    'stir_pulse_sec': 30,
    'stir_timed_min': 15,
    'stir_rem_sec': 0,
    'target_temp': 39.0,
    'hysteresis': 0.3,
    'water_low': False,
    'system_mode': 'AUTO',
    'bottles': []
}

# pH arrives from the Mega slave in pH units: the slave now applies its own
# per-bottle 3-point calibration (PH_V_401/686/918 in legacy/slave/new-slave.cpp)
# and sends -1 for an invalid reading. Do not convert it here again.

cached_sys_stats = {
    "cpu_percent": 0.0,
    "ram_percent": 0.0,
    "ram_used_mb": 0,
    "ram_total_mb": 0,
    "cpu_temp": 0.0,
    "disk_percent": 0.0,
    "power_ok": True
}

CACHED_TIMEZONES = []
try:
    CACHED_TIMEZONES = subprocess.check_output(['timedatectl', 'list-timezones']).decode('utf-8').splitlines()
except Exception:
    CACHED_TIMEZONES = ["Asia/Jakarta", "UTC"]

_prev_cpu_times = None

def update_packet_metrics():
    tx = packet_stats['outgoing']
    rx_m = packet_stats['rx_master']
    rx_s = packet_stats['rx_slave']
    packet_stats['incoming'] = rx_m + rx_s
    packet_stats['total'] = tx + packet_stats['incoming']
    lost = max(0, tx - rx_m)
    packet_stats['lost'] = lost
    packet_stats['loss_pct'] = round((lost / tx) * 100.0, 1) if tx > 0 else 0.0

def send_serial_cmd(cmd_str):
    """Thread-safe helper to send a command line to the Master Arduino."""
    if serial_conn:
        try:
            with serial_write_lock:
                serial_conn.write(f"{cmd_str}\n".encode('utf-8'))
            packet_stats['outgoing'] += 1
            update_packet_metrics()
            return True
        except Exception:
            pass
    return False

def make_qr_data_uri(payload_text):
    if not QR_AVAILABLE:
        return ""
    qr = qrcode.QRCode(version=1, error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=6, border=2)
    qr.add_data(payload_text)
    qr.make(fit=True)
    img = qr.make_image(fill_color="#1c1c1e", back_color="white")
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return f"data:image/png;base64,{base64.b64encode(buf.getvalue()).decode('ascii')}"

hotspot_active = False


class CsvDownloadHandler(BaseHTTPRequestHandler):
    """Serve one CSV file. No index page, no login form, no directory listing."""

    def _resolve_filename(self, path):
        path = unquote(urlparse(path).path)
        if path in ("", "/"):
            if download_target:
                return download_target
            files = sorted(glob.glob(os.path.join(LOG_DIR, "*.csv")), reverse=True)
            return os.path.basename(files[0]) if files else None
        return os.path.basename(path)

    def _send_csv(self, include_body=True):
        filename = self._resolve_filename(self.path)
        if not filename or not filename.lower().endswith(".csv"):
            self.send_error(404, "No CSV selected")
            return

        filepath = os.path.join(LOG_DIR, filename)
        if not os.path.isfile(filepath):
            self.send_error(404, "CSV not found")
            return

        try:
            size = os.path.getsize(filepath)
            self.send_response(200)
            self.send_header("Content-Type", "text/csv; charset=utf-8")
            self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
            self.send_header("Content-Length", str(size))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            if include_body:
                with open(filepath, "rb") as f:
                    shutil.copyfileobj(f, self.wfile, length=64 * 1024)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except OSError:
            pass

    def do_GET(self):
        self._send_csv(include_body=True)

    def do_HEAD(self):
        self._send_csv(include_body=False)

    def log_message(self, fmt, *args):
        pass


def start_download_server():
    try:
        server = ThreadingHTTPServer(("0.0.0.0", DOWNLOAD_PORT), CsvDownloadHandler)
        server.daemon_threads = True
        threading.Thread(target=server.serve_forever, daemon=True).start()
    except OSError:
        pass


def get_interface_ip(ifname):
    try:
        out = subprocess.check_output(["ip", "-4", "-o", "addr", "show", ifname], timeout=1.0).decode()
        for part in out.split():
            if "/" in part and not part.startswith("127."):
                return part.split("/")[0]
    except Exception:
        pass
    return None

def get_lan_ip():
    ip = get_interface_ip("eth0")
    if ip:
        return ip
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 1))
        ip = s.getsockname()[0]
        s.close()
        if ip and not ip.startswith("10.42."):
            return ip
    except Exception:
        pass
    return "Disconnected"

def get_wlan_ip():
    for _ in range(5):
        ip = get_interface_ip("wlan0")
        if ip:
            return ip
        time.sleep(0.4)
    return "10.42.0.1"

def disable_wifi_client_and_stop_hotspot():
    """
    Reserves wlan0 exclusively for on-demand log download hotspot:
    - Disables autoconnect and default routing on all 802-11-wireless profiles
      so wlan0 never joins external Wi-Fi as a client or hijacks eth0 LAN routing.
    - Brings down the hotspot and turns off the Wi-Fi radio while idle.
    """
    global hotspot_active
    hotspot_active = False
    try:
        out = subprocess.check_output(
            ["nmcli", "-t", "-f", "UUID,TYPE", "connection", "show"],
            timeout=3.0
        ).decode("utf-8", errors="ignore")
        for line in out.splitlines():
            parts = line.strip().split(":")
            if len(parts) >= 2 and parts[1] == "802-11-wireless":
                uuid = parts[0]
                subprocess.run(
                    ["sudo", "nmcli", "connection", "modify", uuid,
                     "connection.autoconnect", "no",
                     "ipv4.never-default", "yes"],
                    check=False, timeout=3
                )
    except Exception:
        pass

    try:
        subprocess.run(["sudo", "nmcli", "con", "down", HOTSPOT_CON_NAME], check=False, timeout=4)
        subprocess.run(["sudo", "nmcli", "device", "disconnect", "wlan0"], check=False, timeout=4)
        subprocess.run(["sudo", "nmcli", "radio", "wifi", "off"], check=False, timeout=4)
        subprocess.run(["sudo", "nmcli", "device", "connect", "eth0"], check=False, timeout=4)
    except Exception:
        pass


# --- BACKGROUND WORKER 0: ZERO-SUBPROCESS HARDWARE TELEMETRY ---
def telemetry_worker():
    global _prev_cpu_times
    while True:
        try:
            with open("/proc/stat", "r") as f:
                fields = [float(x) for x in f.readline().split()[1:]]
            idle = fields[3] + (fields[4] if len(fields) > 4 else 0)
            total = sum(fields)
            if _prev_cpu_times is not None:
                prev_idle, prev_total = _prev_cpu_times
                d_total = total - prev_total
                d_idle = idle - prev_idle
                if d_total > 0:
                    cached_sys_stats["cpu_percent"] = round((1.0 - (d_idle / d_total)) * 100.0, 1)
            _prev_cpu_times = (idle, total)
        except Exception:
            pass

        try:
            mem = {}
            with open("/proc/meminfo", "r") as f:
                for line in f:
                    parts = line.split(":")
                    if len(parts) == 2:
                        mem[parts[0].strip()] = int(parts[1].strip().split()[0])
            total_kb = mem.get("MemTotal", 0)
            avail_kb = mem.get("MemAvailable", mem.get("MemFree", 0))
            if total_kb > 0:
                used_kb = total_kb - avail_kb
                cached_sys_stats["ram_percent"] = round((used_kb / total_kb) * 100.0, 1)
                cached_sys_stats["ram_used_mb"] = round(used_kb / 1024)
                cached_sys_stats["ram_total_mb"] = round(total_kb / 1024)
        except Exception:
            pass

        try:
            with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
                cached_sys_stats["cpu_temp"] = round(int(f.read().strip()) / 1000.0, 1)
        except Exception:
            pass

        try:
            usage = shutil.disk_usage("/")
            cached_sys_stats["disk_percent"] = round((usage.used / usage.total) * 100.0, 1)
        except Exception:
            pass

        try:
            if os.path.exists("/sys/class/hwmon/hwmon1/in0_lcrit_alarm"):
                with open("/sys/class/hwmon/hwmon1/in0_lcrit_alarm", "r") as f:
                    cached_sys_stats["power_ok"] = (f.read().strip() == "0")
        except Exception:
            pass

        time.sleep(4.0)


# --- BACKGROUND WORKER 1: LOCK-FREE SERIAL INGESTION ---
def serial_worker():
    global serial_conn, last_slave_rx_time, last_master_rx_time, raw_line_seq
    possible_ports = ['/dev/ttyACM0', '/dev/ttyUSB0', '/dev/ttyACM1', '/dev/ttyUSB1']

    while True:
        if serial_conn is None:
            for p in possible_ports:
                if os.path.exists(p):
                    try:
                        s = serial.Serial(p, 115200, timeout=0.2)
                        time.sleep(2)
                        serial_conn = s
                        print(f"Serial connected successfully on {p}")
                        break
                    except Exception:
                        pass
            time.sleep(2)
            continue

        try:
            if serial_conn and serial_conn.in_waiting:
                wire_line = serial_conn.readline().decode('utf-8', errors='ignore').rstrip('\r\n')
                raw_line = wire_line.strip()
                if wire_line:
                    with raw_lines_lock:
                        raw_line_seq += 1
                        raw_lines.append({
                            'seq': raw_line_seq,
                            'ts': time.time(),
                            'line': wire_line
                        })
            else:
                time.sleep(0.02)
                continue

            if raw_line.startswith('{') and raw_line.endswith('}'):
                try:
                    payload = json.loads(raw_line)
                    now_ts = time.time()

                    if 'bottles' in payload or payload.get('type') == 'slave_telemetry':
                        packet_stats['rx_slave'] += 1
                        last_slave_rx_time = now_ts
                        new_bottles = payload.get('bottles', [])
                        existing_map = {b['id']: b for b in current_data['bottles'] if isinstance(b, dict) and 'id' in b}
                        for nb in new_bottles:
                            if isinstance(nb, dict) and 'id' in nb:
                                existing_map[nb['id']] = nb
                        current_data['bottles'] = list(existing_map.values())

                    elif 'id' in payload and 'temp' in payload:
                        packet_stats['rx_slave'] += 1
                        last_slave_rx_time = now_ts
                        existing_map = {b['id']: b for b in current_data['bottles'] if isinstance(b, dict) and 'id' in b}
                        existing_map[payload['id']] = payload
                        current_data['bottles'] = list(existing_map.values())

                    elif payload.get('type') == 'master_status':
                        packet_stats['rx_master'] += 1
                        last_master_rx_time = now_ts
                        current_data['temp'] = float(payload.get('water_temp', current_data['temp']))
                        current_data['bottle_avg_temp'] = float(payload.get('bottle_temp_avg', current_data['bottle_avg_temp']))
                        current_data['heater'] = bool(payload.get('heater', 0))
                        current_data['pump'] = bool(payload.get('pump', 0))
                        current_data['stirrer'] = bool(payload.get('stirrer', 0))
                        current_data['heater_mode'] = payload.get('heater_mode', current_data['heater_mode'])
                        current_data['pump_mode'] = payload.get('pump_mode', current_data['pump_mode'])
                        current_data['stir_mode'] = payload.get('stir_mode', current_data['stir_mode'])
                        current_data['stir_int_min'] = int(payload.get('stir_int_min', current_data['stir_int_min']))
                        current_data['stir_pulse_sec'] = int(payload.get('stir_pulse_sec', current_data['stir_pulse_sec']))
                        current_data['stir_timed_min'] = int(payload.get('stir_timed_min', current_data['stir_timed_min']))
                        current_data['stir_rem_sec'] = int(payload.get('stir_rem_sec', 0))
                        current_data['target_temp'] = float(payload.get('target_temp', current_data['target_temp']))
                        current_data['hysteresis'] = float(payload.get('hysteresis', current_data['hysteresis']))
                        current_data['water_low'] = bool(payload.get('water_low', 0))
                        current_data['system_mode'] = payload.get('system_mode', current_data['system_mode'])

                    else:
                        packet_stats['rx_master'] += 1
                        last_master_rx_time = now_ts
                        current_data.update(payload)

                    update_packet_metrics()
                except json.JSONDecodeError:
                    pass
        except Exception:
            if serial_conn:
                try:
                    serial_conn.close()
                except Exception:
                    pass
                serial_conn = None
            time.sleep(2)


# --- BACKGROUND WORKER 2: POLLING & LOGGING ---
def polling_worker():
    global boot_defaults_applied
    while True:
        if serial_conn:
            if not boot_defaults_applied:
                # Force OFF on first connection only, so a later reconnect does
                # not override a mode the operator selected.
                if all(send_serial_cmd(c) for c in BOOT_DEFAULT_COMMANDS):
                    boot_defaults_applied = True
                    current_data['heater_mode'] = 'OFF'
                    current_data['pump_mode'] = 'OFF'
                    current_data['stir_mode'] = 'OFF'
                    current_data['heater'] = False
                    current_data['pump'] = False
                    current_data['stirrer'] = False
                    print("Boot defaults applied: heater OFF, pump OFF, stirrer OFF")
            send_serial_cmd("STATUS")
        elif logging_active:
            packet_stats['outgoing'] += 1
            update_packet_metrics()

        if logging_active:
            log_data_to_csv()

        time.sleep(1.0)


# --- LEGIBLE, ALIGNED CSV LOGGING ENGINE ---
CSV_COLUMNS = [
    ("Timestamp",    10), ("Bath_C",        7), ("BAvg_C",        7),
    ("Targ_C",        7), ("Heater",        6), ("Pump",          5),
    ("Stir",          5), ("Water",         6), ("Pkt_Tot",       8),
    ("Pkt_TX",        7), ("Pkt_RX",        7), ("Pkt_Lost",      8),
    ("Loss_%",        7),
]
for _i in range(1, 5):
    _p = f"B{_i}"
    CSV_COLUMNS.extend([
        (f"{_p}_Temp",  8), (f"{_p}_pH",    7), (f"{_p}_CH4",   9),
        (f"{_p}_CH4mL", 8), (f"{_p}_CO2",   8), (f"{_p}_CO2mL", 8),
        (f"{_p}_Pa",    9),
    ])

def fmt_val(val, decimals=2, sentinels=(-999, -999.0, -1, -1.0)):
    if val is None or val == "":
        return "null"
    try:
        n = float(val)
        if n in sentinels or n <= -50.0:
            return "null"
        if decimals == 0 or n.is_integer():
            return str(int(round(n)))
        return f"{n:.{decimals}f}"
    except (ValueError, TypeError):
        return "null"

def format_aligned_row(values):
    padded = []
    for (col_name, width), val in zip(CSV_COLUMNS, values):
        s = str(val)
        if col_name in ("Timestamp", "Heater", "Pump", "Stir", "Water"):
            padded.append(s.ljust(width))
        else:
            padded.append(s.rjust(width))
    return " , ".join(padded) + "\n"

def get_todays_filename():
    return os.path.join(LOG_DIR, f"{datetime.now().strftime('%Y-%m-%d')}.csv")

def write_headers_if_new(filepath):
    if not os.path.exists(filepath) or os.path.getsize(filepath) == 0:
        header_names = [col[0] for col in CSV_COLUMNS]
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(format_aligned_row(header_names))

def log_data_to_csv():
    if not logging_active:
        return
    try:
        fp = get_todays_filename()
        write_headers_if_new(fp)

        now_ts = time.time()
        master_fresh = (now_ts - last_master_rx_time) < SLAVE_STALE_SECONDS
        slave_fresh = (now_ts - last_slave_rx_time) < SLAVE_STALE_SECONDS

        row = [
            datetime.now().strftime("%H:%M:%S"),
            fmt_val(current_data.get('temp'), 2) if master_fresh else "null",
            fmt_val(current_data.get('bottle_avg_temp'), 2) if slave_fresh else "null",
            fmt_val(current_data.get('target_temp'), 1) if master_fresh else "null",
            "ON" if (master_fresh and current_data.get('heater')) else ("OFF" if master_fresh else "null"),
            "ON" if (master_fresh and current_data.get('pump')) else ("OFF" if master_fresh else "null"),
            "ON" if (master_fresh and current_data.get('stirrer')) else ("OFF" if master_fresh else "null"),
            "LOW" if (master_fresh and current_data.get('water_low')) else ("OK" if master_fresh else "null"),
            packet_stats['total'],
            packet_stats['outgoing'],
            packet_stats['incoming'],
            packet_stats['lost'],
            f"{packet_stats['loss_pct']:.1f}%"
        ]

        bottles = current_data.get('bottles', []) if slave_fresh else []
        b_map = {b.get('id'): b for b in bottles if isinstance(b, dict) and 'id' in b}

        for i in range(1, 5):
            b = b_map.get(i)
            if b and slave_fresh:
                row.extend([
                    fmt_val(b.get('temp'), 2, (-999, -999.0)),
                    fmt_val(b.get('ph'), 2, (-1, -1.0)),
                    fmt_val(b.get('methane'), 1, (-1, -1.0)),
                    fmt_val(b.get('ch4_ml'), 2, (-1, -1.0)),
                    fmt_val(b.get('co2'), 0, (-1, -1.0, -2)),
                    fmt_val(b.get('co2_ml'), 2, (-1, -1.0)),
                    fmt_val(b.get('press'), 0, (-1, -1.0))
                ])
            else:
                row.extend(['null', 'null', 'null', 'null', 'null', 'null', 'null'])

        with open(fp, 'a', encoding='utf-8') as f:
            f.write(format_aligned_row(row))
    except Exception:
        pass


threading.Thread(target=disable_wifi_client_and_stop_hotspot, daemon=True).start()
threading.Thread(target=start_download_server, daemon=True).start()
threading.Thread(target=telemetry_worker, daemon=True).start()
threading.Thread(target=serial_worker, daemon=True).start()
threading.Thread(target=polling_worker, daemon=True).start()


MOBILE_PORTAL_HTML = """
<!doctype html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1" />
    <title>InVitro Log Download</title>
    <style>
        * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body {
            margin: 0; min-height: 100vh; display: flex; align-items: center; justify-content: center;
            background: #e4ecf7; padding: 20px; color: #1c1c1e;
        }
        .card {
            background: #ffffff; border: 1px solid #dfe4ec; border-radius: 20px;
            padding: 26px 22px; width: 100%; max-width: 360px; text-align: center;
        }
        h2 { margin: 0 0 6px 0; font-size: 20px; }
        .file-pill {
            display: inline-block; background: #edf1f7; color: #007aff; font-weight: 600;
            font-size: 13px; padding: 5px 12px; border-radius: 10px; margin-bottom: 18px;
        }
        .field { text-align: left; margin-bottom: 12px; }
        label { display: block; font-size: 11px; font-weight: 600; color: #58585c; text-transform: uppercase; margin-bottom: 4px; }
        input {
            width: 100%; padding: 10px 12px; font-size: 16px; border-radius: 10px;
            border: 1px solid #dfe4ec; background: #edf1f7;
        }
        button {
            width: 100%; padding: 12px; font-size: 15px; font-weight: 600; color: #004099;
            background: #d6e8ff; border: 1px solid #8ec0ff; border-radius: 12px; cursor: pointer; margin-top: 6px;
        }
        .err { color: #ff3b30; font-size: 13px; font-weight: 600; margin-bottom: 10px; }
    </style>
</head>
<body>
    <div class="card">
        <h2>InVitro Log Portal</h2>
        <div class="file-pill">{{ filename }}</div>
        {% if error %}<div class="err">{{ error }}</div>{% endif %}
        <form method="POST">
            <div class="field"><label>Username</label><input type="text" name="username" placeholder="invitro" required autocapitalize="none" /></div>
            <div class="field"><label>Password</label><input type="password" name="password" placeholder="••••••••" required /></div>
            <button type="submit">LOG IN & DOWNLOAD CSV</button>
        </form>
    </div>
</body>
</html>
"""


# --- WEB & REST ROUTES ---
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    now_ts = time.time()
    slave_fresh = (now_ts - last_slave_rx_time) < SLAVE_STALE_SECONDS
    res = current_data.copy()
    if not slave_fresh:
        res['bottles'] = []
    res['logging'] = logging_active
    res['packets'] = packet_stats
    res['sys'] = cached_sys_stats
    return jsonify(res)

@app.route('/raw')
def raw():
    try:
        limit = int(request.args.get('limit', '100'))
        after_arg = request.args.get('after')
        after = int(after_arg) if after_arg is not None else None
        if limit < 1 or (after is not None and after < 0):
            raise ValueError
    except (TypeError, ValueError):
        return jsonify({'error': 'after must be non-negative and limit must be at least 1'}), 400

    limit = min(limit, 500)
    with raw_lines_lock:
        buffered_lines = list(raw_lines)
        newest_seq = raw_line_seq
        selected = buffered_lines[-limit:] if after is None else [line for line in buffered_lines if line['seq'] > after][:limit]
        response = jsonify({
            'seq': newest_seq,
            'count': len(selected),
            'buffered': len(buffered_lines),
            'connected': serial_conn is not None,
            'lines': selected
        })
    response.headers['Cache-Control'] = 'no-store'
    return response

@app.route('/control', methods=['POST'])
def control_api():
    """Handles Heater, Pump, Stirrer, and Hysteresis Target updates from the Controls tab."""
    body = request.json or {}
    action = body.get('action')

    if action == 'heater':
        mode = body.get('mode', 'AUTO')
        if mode in ('AUTO', 'ON', 'OFF'):
            send_serial_cmd(f"HEATER_{mode}")
            current_data['heater_mode'] = mode
            if mode == 'ON': current_data['heater'] = True
            elif mode == 'OFF': current_data['heater'] = False
            return jsonify({'status': 'success'})

    elif action == 'pump':
        mode = body.get('mode', 'AUTO')
        if mode in ('AUTO', 'ON', 'OFF'):
            send_serial_cmd(f"PUMP_{mode}")
            current_data['pump_mode'] = mode
            if mode == 'ON': current_data['pump'] = True
            elif mode == 'OFF': current_data['pump'] = False
            return jsonify({'status': 'success'})

    elif action == 'stirrer':
        mode_str = body.get('mode', 'OFF')
        mode_map = {'OFF': 0, 'ON': 1, 'PULSE': 2, 'TIMED': 3}
        m_int = mode_map.get(mode_str, 0)
        mins = int(body.get('minutes', 10))
        secs = int(body.get('seconds', 30))
        send_serial_cmd(f"STIR:{m_int}:{mins}:{secs}")
        current_data['stir_mode'] = mode_str
        if mode_str == 'PULSE':
            current_data['stir_int_min'] = mins
            current_data['stir_pulse_sec'] = secs
        elif mode_str == 'TIMED':
            current_data['stir_timed_min'] = mins
            current_data['stir_rem_sec'] = mins * 60
        current_data['stirrer'] = (mode_str != 'OFF')
        return jsonify({'status': 'success'})

    elif action == 'temp_target':
        target = round(float(body.get('target', 39.0)), 1)
        hyst = round(float(body.get('hysteresis', 0.3)), 1)
        send_serial_cmd(f"TEMP:{target:.1f}:{hyst:.1f}")
        current_data['target_temp'] = target
        current_data['hysteresis'] = hyst
        return jsonify({'status': 'success'})

    return jsonify({'status': 'error', 'message': 'Invalid control action'}), 400

@app.route('/sys_stats')
def sys_stats():
    out = cached_sys_stats.copy()
    out['packets'] = packet_stats
    return jsonify(out)

@app.route('/reset_packets', methods=['POST'])
def reset_packets():
    for k in ('total', 'outgoing', 'incoming', 'rx_master', 'rx_slave', 'lost'):
        packet_stats[k] = 0
    packet_stats['loss_pct'] = 0.0
    return jsonify({'status': 'success', 'packets': packet_stats})

@app.route('/toggle_logging', methods=['POST'])
def toggle_logging():
    global logging_active
    logging_active = not logging_active
    if logging_active:
        log_data_to_csv()
    return jsonify({'status': 'success', 'logging': logging_active})

@app.route('/download/info', methods=['POST'])
def download_info():
    global download_target
    filename = os.path.basename((request.json or {}).get('filename', ''))
    if not filename or not filename.lower().endswith('.csv') or not os.path.exists(os.path.join(LOG_DIR, filename)):
        return jsonify({'status': 'error', 'message': 'Select a valid log file first.'}), 400

    ip = get_lan_ip()
    if ip == "Disconnected":
        return jsonify({'status': 'error', 'message': 'No LAN connection available.'}), 503

    download_target = filename
    url = f"http://{ip}:{DOWNLOAD_PORT}/"
    return jsonify({
        'status': 'success',
        'filename': filename,
        'ip': ip,
        'port': DOWNLOAD_PORT,
        'url': url,
        'url_qr': make_qr_data_uri(url)
    })

@app.route('/m/<path:filename>', methods=['GET', 'POST'])
def mobile_download_portal(filename):
    safe_name = os.path.basename(filename)
    filepath = os.path.join(LOG_DIR, safe_name)
    if not os.path.exists(filepath):
        return "Log file not found.", 404

    error = None
    if request.method == 'POST':
        if request.form.get('username', '').strip() == PORTAL_USER and request.form.get('password', '') == PORTAL_PASS:
            return send_from_directory(os.path.abspath(LOG_DIR), safe_name, as_attachment=True, download_name=safe_name)
        error = "Invalid username or password."

    return render_template_string(MOBILE_PORTAL_HTML, filename=safe_name, error=error)

@app.route('/set_mode', methods=['POST'])
def set_mode():
    mode = request.json.get('mode')
    if mode in ['AUTO', 'OFF']:
        send_serial_cmd(f"MODE_{mode}")
        current_data['system_mode'] = mode
        return jsonify({'status': 'success', 'system_mode': mode})
    return jsonify({'status': 'error'})

@app.route('/command', methods=['POST'])
def command():
    if request.json.get('command') == "SHUTDOWN":
        subprocess.run(["sync"])
        subprocess.Popen(["sudo", "shutdown", "-h", "now"])
        return jsonify({'status': 'shutting down'})
    return jsonify({'status': 'ignored'})

@app.route('/files')
def list_files():
    files = glob.glob(os.path.join(LOG_DIR, "*.csv"))
    names = sorted([os.path.basename(f) for f in files], reverse=True)
    return jsonify(names)

@app.route('/read_log')
def read_log():
    filename = os.path.basename(request.args.get('filename', ''))
    max_lines = min(int(request.args.get('lines', 40)), 200)
    filepath = os.path.join(LOG_DIR, filename)

    if not filename or not os.path.exists(filepath):
        return jsonify({'status': 'error'}), 404

    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            first_line = f.readline()
            tail_lines = list(deque(f, maxlen=max_lines))

        raw_text = first_line + "".join(reversed(tail_lines))
        return jsonify({
            'status': 'success',
            'filename': filename,
            'total_shown': len(tail_lines),
            'raw': raw_text
        })
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/get_drives')
def get_drives():
    drives = []
    try:
        out = subprocess.check_output(['lsblk', '-r', '-n', '-o', 'NAME,TYPE,MOUNTPOINT']).decode('utf-8')
        for line in out.strip().split('\n'):
            parts = line.split()
            if len(parts) >= 1 and parts[0].startswith('sd'):
                dev = f"/dev/{parts[0]}"
                if not os.path.ismount(MOUNT_POINT):
                    subprocess.run(f"sudo mount -o uid=1000,gid=1000 {dev} {MOUNT_POINT}", shell=True)
                break
    except Exception:
        pass

    if os.path.ismount(MOUNT_POINT):
        try:
            total, used, free = shutil.disk_usage(MOUNT_POINT)
            drives.append({
                'path': MOUNT_POINT,
                'name': "USB Drive",
                'free_gb': round(free / (2**30), 1),
                'total_gb': round(total / (2**30), 1)
            })
        except Exception:
            pass
    return jsonify(drives)

@app.route('/export', methods=['POST'])
def export_data():
    target = request.json.get('filename')
    dest = request.json.get('dest_path')
    src = os.path.join(LOG_DIR, target)
    out_file = os.path.join(dest, target)
    if not os.path.exists(src) or not os.path.ismount(dest):
        return jsonify({'status': 'error', 'message': 'Drive or log unavailable.'})
    try:
        shutil.copy2(src, out_file)
        subprocess.run(["sync"])
        return jsonify({'status': 'success', 'message': 'Export Successful!'})
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)})

@app.route('/get_ip')
def get_ip():
    ip = get_lan_ip()
    mode_str = "Ethernet (LAN) · Hotspot Active" if hotspot_active else "Ethernet (LAN)"
    return jsonify({'ip': ip, 'ssid': mode_str})

@app.route('/get_timezones')
def get_timezones():
    return jsonify(CACHED_TIMEZONES)

@app.route('/set_timezone', methods=['POST'])
def set_timezone():
    try:
        subprocess.run(['sudo', 'timedatectl', 'set-timezone', request.json.get('timezone')], check=True)
        return jsonify({'status': 'success'})
    except Exception:
        return jsonify({'status': 'error'})

@app.route('/set_time', methods=['POST'])
def set_time():
    try:
        subprocess.run(['sudo', 'date', '-s', request.json.get('datetime')], check=True)
        return jsonify({'status': 'success'})
    except Exception:
        return jsonify({'status': 'error'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)
