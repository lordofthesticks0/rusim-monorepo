#!/usr/bin/env python3
"""User-friendly CLI for the water-control Flask server (web_control.py).

Covers every /control action and every other POST/GET endpoint with
friendly subcommand names. Stdlib only (no requests needed).

Examples:
  flask_cli.py --base-url http://10.92.128.23:5000 status
  flask_cli.py heater ON
  flask_cli.py thresholds --out-stop 400 --dry 460 --in-stop 550 --leak 200
  flask_cli.py --help
  flask_cli.py help
"""

import argparse
import json
import sys
import urllib.error
import urllib.parse
import urllib.request

DEFAULT_BASE = "http://127.0.0.1:5000"

HELP_TEXT = """\
Flask web-control CLI — friendly names for every endpoint.

Global option:
  --base-url URL     Flask base URL (default http://127.0.0.1:5000).
                     When you run this on the Pi, the default works.
                     From a laptop, use --base-url http://10.92.128.23:5000

Controls (POST /control):
  heater AUTO|ON|OFF
  pump AUTO|ON|OFF            Both pumps
  pump-in AUTO|ON|OFF         Pump 1 (D3, IN)
  pump-out AUTO|ON|OFF        Pump 2 (D7, OUT)
  pwm PIN 0-255               PIN is D3, D7 or D4
  stirrer MODE [MIN SEC]      MODE is OFF|ON|PULSE|TIMED
  temp-target TARGET HYST     e.g. 39.0 0.3
  thresholds OUT DRY IN LEAK  Trip-only, e.g. 400 460 550 200
  mode AUTO|OFF               System mode (POST /set_mode)

Server (POST, no /control):
  reset-packets
  toggle-logging
  download-info FILENAME.csv  Enable LAN download for one log file
  export FILENAME DEST        Copy a log to a mounted USB drive
  set-timezone TIMEZONE       e.g. Asia/Jakarta
  set-time "DATETIME"         e.g. "2026-11-06 12:00:00"
  shutdown                    Power off the Pi (requires --force)

Read (GET):
  status                      Full current_data JSON
  raw [LIMIT]                 Raw serial buffer (default 100, max 500)
  sys-stats
  files                       List log CSV names
  read-log FILENAME [LINES]   Last N lines of one log (default 40)
  get-drives
  get-ip
  get-timezones

Download portal (POST form to /m/<name>):
  download FILENAME.csv [OUT] Downloads the CSV using the portal login

Try:  flask_cli.py --help  for full per-command help.
"""


def post_json(base, path, payload):
    data = json.dumps(payload).encode("utf-8") if payload is not None else b""
    req = urllib.request.Request(
        base.rstrip("/") + path, data=data, method="POST",
        headers={"Content-Type": "application/json",
                 "Accept": "application/json"},
    )
    return _open(req)


def get_json(base, path, params=None):
    url = base.rstrip("/") + path
    if params:
        url += "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    return _open(req)


def _open(req):
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            body = resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        print(f"HTTP {e.code} {e.reason}", file=sys.stderr)
        try:
            print(json.dumps(json.loads(body), indent=2))
        except ValueError:
            print(body)
        return 1
    except urllib.error.URLError as e:
        print(f"Cannot reach {req.full_url}: {e.reason}", file=sys.stderr)
        return 1
    try:
        print(json.dumps(json.loads(body), indent=2))
    except ValueError:
        print(body)
    return 0


def post_download(base, filename, out_path):
    """POST the login form to /m/<filename> and save the CSV."""
    form = urllib.parse.urlencode(
        {"username": "invitro", "password": "password"}).encode()
    req = urllib.request.Request(
        base.rstrip("/") + "/m/" + urllib.parse.quote(filename),
        data=form, method="POST",
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            body = resp.read()
    except urllib.error.HTTPError as e:
        print(f"HTTP {e.code}: {e.reason}", file=sys.stderr)
        return 1
    except urllib.error.URLError as e:
        print(f"Cannot reach server: {e.reason}", file=sys.stderr)
        return 1
    if body.lstrip().startswith(b"<!"):
        # Login form / 404 page instead of a CSV.
        text = body.decode("utf-8", errors="replace")
        print("Download failed (check filename/login). Server replied:",
              file=sys.stderr)
        print(text[:500], file=sys.stderr)
        return 1
    with open(out_path, "wb") as f:
        f.write(body)
    print(f"Saved {out_path} ({len(body)} bytes)")
    return 0


def main():
    parser = argparse.ArgumentParser(
        prog="flask_cli.py",
        description="User-friendly POST/GET client for web_control.py",
        epilog=HELP_TEXT, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("--base-url", default=DEFAULT_BASE,
                        help=f"Flask base URL (default {DEFAULT_BASE})")
    sub = parser.add_subparsers(dest="cmd", metavar="COMMAND")

    sub.add_parser("help", help="print the friendly help page")

    p = sub.add_parser("heater", help="heater: AUTO|ON|OFF")
    p.add_argument("mode", choices=["AUTO", "ON", "OFF"])

    p = sub.add_parser("pump", help="both pumps: AUTO|ON|OFF")
    p.add_argument("mode", choices=["AUTO", "ON", "OFF"])

    p = sub.add_parser("pump-in", aliases=["pump1"], help="pump 1 (D3, IN): AUTO|ON|OFF")
    p.add_argument("mode", choices=["AUTO", "ON", "OFF"])

    p = sub.add_parser("pump-out", aliases=["pump2"], help="pump 2 (D7, OUT): AUTO|ON|OFF")
    p.add_argument("mode", choices=["AUTO", "ON", "OFF"])

    p = sub.add_parser("pwm", help="set MOSFET PWM ceiling: pwm PIN 0-255")
    p.add_argument("pin", choices=["D3", "D7", "D4"])
    p.add_argument("value", type=int)

    p = sub.add_parser("stirrer", help="stirrer: stirrer MODE [MIN SEC]")
    p.add_argument("mode", choices=["OFF", "ON", "PULSE", "TIMED"])
    p.add_argument("minutes", nargs="?", type=int, default=10)
    p.add_argument("seconds", nargs="?", type=int, default=30)

    p = sub.add_parser("temp-target", help="target temp + hysteresis: temp-target TARGET HYST")
    p.add_argument("target", type=float)
    p.add_argument("hysteresis", type=float)

    p = sub.add_parser("thresholds", help="trip thresholds OUT DRY IN LEAK (0-1023)")
    p.add_argument("out_stop", type=int)
    p.add_argument("dry", type=int)
    p.add_argument("in_stop", type=int)
    p.add_argument("leak", type=int)

    p = sub.add_parser("mode", help="system mode: AUTO|OFF")
    p.add_argument("mode", choices=["AUTO", "OFF"])

    sub.add_parser("reset-packets", help="zero the packet counters")
    sub.add_parser("toggle-logging", help="flip CSV logging on/off")
    p = sub.add_parser("download-info", help="enable LAN download for one log: download-info FILE.csv")
    p.add_argument("filename")
    p = sub.add_parser("export", help="copy a log to a USB mount: export FILE DEST")
    p.add_argument("filename")
    p.add_argument("dest_path")
    p = sub.add_parser("set-timezone", help="set timezone: set-timezone TIMEZONE")
    p.add_argument("timezone")
    p = sub.add_parser("set-time", help='set clock: set-time "YYYY-MM-DD HH:MM:SS"')
    p.add_argument("datetime")

    p = sub.add_parser("shutdown", help="power off the Pi (needs --force)")
    p.add_argument("--force", action="store_true")

    sub.add_parser("status", help="full status JSON")
    p = sub.add_parser("raw", help="raw serial buffer: raw [LIMIT]")
    p.add_argument("limit", nargs="?", type=int, default=100)
    sub.add_parser("sys-stats", help="system stats + packet counters")
    sub.add_parser("files", help="list log CSV files")
    p = sub.add_parser("read-log", help="last lines of one log: read-log FILE [LINES]")
    p.add_argument("filename")
    p.add_argument("lines", nargs="?", type=int, default=40)
    sub.add_parser("get-drives", help="mounted drives")
    sub.add_parser("get-ip", help="LAN IP + mode string")
    sub.add_parser("get-timezones", help="supported timezones")

    p = sub.add_parser("download", help="download a CSV via the login portal: download FILE [OUT]")
    p.add_argument("filename")
    p.add_argument("out", nargs="?")

    args = parser.parse_args()
    if args.cmd is None:
        parser.print_help()
        return 0
    base = args.base_url

    if args.cmd == "help":
        print(HELP_TEXT)
        return 0
    if args.cmd == "heater":
        return post_json(base, "/control", {"action": "heater", "mode": args.mode})
    if args.cmd == "pump":
        return post_json(base, "/control", {"action": "pump", "mode": args.mode})
    if args.cmd in ("pump-in", "pump1"):
        return post_json(base, "/control", {"action": "pump1", "mode": args.mode})
    if args.cmd in ("pump-out", "pump2"):
        return post_json(base, "/control", {"action": "pump2", "mode": args.mode})
    if args.cmd == "pwm":
        return post_json(base, "/control",
                         {"action": "mosfet_pwm", "pin": args.pin, "pwm": args.value})
    if args.cmd == "stirrer":
        return post_json(base, "/control",
                         {"action": "stirrer", "mode": args.mode,
                          "minutes": args.minutes, "seconds": args.seconds})
    if args.cmd == "temp-target":
        return post_json(base, "/control",
                         {"action": "temp_target", "target": args.target,
                          "hysteresis": args.hysteresis})
    if args.cmd == "thresholds":
        return post_json(base, "/control",
                         {"action": "thresholds", "out_stop": args.out_stop,
                          "dry": args.dry, "in_stop": args.in_stop,
                          "leak": args.leak})
    if args.cmd == "mode":
        return post_json(base, "/set_mode", {"mode": args.mode})
    if args.cmd == "reset-packets":
        return post_json(base, "/reset_packets", {})
    if args.cmd == "toggle-logging":
        return post_json(base, "/toggle_logging", {})
    if args.cmd == "download-info":
        return post_json(base, "/download/info", {"filename": args.filename})
    if args.cmd == "export":
        return post_json(base, "/export",
                         {"filename": args.filename, "dest_path": args.dest_path})
    if args.cmd == "set-timezone":
        return post_json(base, "/set_timezone", {"timezone": args.timezone})
    if args.cmd == "set-time":
        return post_json(base, "/set_time", {"datetime": args.datetime})
    if args.cmd == "shutdown":
        if not args.force:
            print("Refusing to shut down without --force", file=sys.stderr)
            return 1
        return post_json(base, "/command", {"command": "SHUTDOWN"})
    if args.cmd == "status":
        return get_json(base, "/status")
    if args.cmd == "raw":
        return get_json(base, "/raw", {"limit": args.limit})
    if args.cmd == "sys-stats":
        return get_json(base, "/sys_stats")
    if args.cmd == "files":
        return get_json(base, "/files")
    if args.cmd == "read-log":
        return get_json(base, "/read_log",
                        {"filename": args.filename, "lines": args.lines})
    if args.cmd == "get-drives":
        return get_json(base, "/get_drives")
    if args.cmd == "get-ip":
        return get_json(base, "/get_ip")
    if args.cmd == "get-timezones":
        return get_json(base, "/get_timezones")
    if args.cmd == "download":
        out = args.out or args.filename
        return post_download(base, args.filename, out)
    return 1


if __name__ == "__main__":
    sys.exit(main())
