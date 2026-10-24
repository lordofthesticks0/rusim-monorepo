# Legacy Pi remote monitor (`scripts-hardware/monitor-legacy-pi.ts`)

Scope: the Bun script that reads the water bath master on `invitrocontroller`
over SSH and prints its values. It replaces typing a `curl` by hand. It does
not change the firmware, the Flask app, or the protocol.

Source: `scripts-hardware/monitor-legacy-pi.ts`. Live system:
[raspi-docs/04-backend-web-control.md](raspi-docs/04-backend-web-control.md),
[raspi-docs/06-master-firmware.md](raspi-docs/06-master-firmware.md).

## 1. Usage

```bash
# Parsed state from the Flask app, one row per second
bun scripts-hardware/monitor-legacy-pi.ts

# Every serial line the master sent, service untouched
bun scripts-hardware/monitor-legacy-pi.ts --raw

# Untouched wire lines, useful for piping into another tool
bun scripts-hardware/monitor-legacy-pi.ts --raw --json --duration 30
```

| Option | Meaning |
|---|---|
| `--raw` | Read the Flask app's `/raw` buffer and print every received serial line. `water-ui` keeps running and the board is not reset. |
| `--json` | Print each message exactly as received instead of the table. |
| `--interval <seconds>` | Poll and read interval. Default `1`. Also the maximum delay before a new line appears. |
| `--duration <seconds>` | Stop after this long. Default is to run until `Ctrl+C`. |
| `--host <target>` | SSH target. Default `invitro@10.92.128.23`. |
| `-h`, `--help` | Print usage. |

Exit codes: `0` on duration expiry or a clean end of stream, `1` on argument
errors, `130` on `Ctrl+C`, `143` on `SIGTERM`. A transient `/raw` outage is a
warning, not an exit; with `--duration` the monitor still ends with `0`.

The script needs only `bun` and `ssh` on the machine that runs it. It has no
local serial dependency, so it runs unchanged on Linux, macOS and Windows. SSH
keys are expected: `runRemote()` uses `BatchMode=yes` so a poll cannot stop for
a password prompt.

## 2. The two sources

`/dev/ttyUSB0` is a single port, and the Flask app under pm2 (`water-ui`) keeps
it open. The script never opens the port; it only reads HTTP endpoints on the
Pi over SSH.

- Default mode reads `/status`, the parsed cache. The master's `master_status`
  and the slave's `bottles` messages are merged into one object, and
  `water_adc` is dropped because the Flask app never stores it.
- `--raw` reads `/raw`, the exact lines recorded by `serial_worker()` before
  parsing. This includes non-JSON lines that `/status` ignores. The service
  keeps running, the board is not reset, and the packet counters keep their
  values.

## 3. Output

Default mode, one row per second:

```
time        bath°C    avg°C  mode   heat  pump  stir    rx        B1        B2        B3        B4
--------  --------  -------  -----  ----  ----  ----  ----  --------  --------  --------  --------
10:55:49     31.75    29.15  AUTO   ON    ON    OFF    289   -999.00     29.00     28.94     29.50
```

- `bath°C` is the bath DS18B20, `avg°C` the master's average of the valid
  bottle temperatures.
- `stir` is the `fan` field. The firmware drives the stirrer on pin D4 but keeps
  the telemetry key `fan` so the dashboard and `web_control.py` still parse it.
- `rx` is `packets.rx_slave`, counted since the Flask process started. It rises
  once per 3.5 s slave poll. A frozen value means the slave is silent.
- `-999.00` means the master had no valid reading. It is printed as sent, not
  translated.

`--raw` prints two message shapes, because the master forwards both:

```
11:18:00  master  bath  32.19  avg  29.31  AUTO  heat ON  pump ON  stir OFF  water OK  adc 1023
time      bottle   pH    CH4 (ppm)  temp (°C)  CO2 (ppm)  press (Pa)
--------  ------  -------  ---------  ---------  ---------  ----------
11:18:01       1     5.00      527.2    -999.00        682          16
               2     5.00      228.8      29.19        604         950
               3     5.00       61.9      29.06        669          50
               4     5.00      130.2      29.69        527          -1
```

That capture predates the in-slave pH conversion: its `pH` values are the old
raw volts, and 5.00 V is the rail, which the slave now reports as `-1`.

The per-bottle block mirrors
[legacy-slave-json-wrapper.md](legacy-slave-json-wrapper.md) §3, so a capture
from either script reads the same way. In `--raw` the timestamps come from the
Pi's `ts` field, so they show when the line was received, not when the client
polled. `water OK` comes from `water_low`; note that `master.cpp` hard-codes
`waterRaw = 1023` and zeroes the fault counter every loop, so that column cannot
report a dry bath.

`--json` prints the `line` field untouched, without the table or timestamps.

## 4. How `--raw` works

1. The first request is `?limit=1`. It only learns the newest sequence number,
   so a long-running dashboard does not dump its whole buffer at startup. The
   monitor starts from "now".
2. Each interval runs one SSH command:
   `curl -fs 'http://127.0.0.1:5000/raw?after=<seq>&limit=500'`. The response is
   printed in order and the cursor advances to the last line's `seq`.
3. If the first new line's `seq` is greater than `after + 1`, the 500-line ring
   buffer wrapped and the missing count is logged:
   `[warn] ... N line(s) dropped by the 500-line buffer`.
4. If `seq` goes backwards, the Flask process restarted. The cursor resets and
   the new buffer is read from the start:
   `[warn] ... water-ui restarted (seq N); resuming from the new buffer`.
5. A change in the response's `connected` flag is logged once per transition:
   `serial link up` / `serial link down`.
6. `Ctrl+C` or `SIGTERM` kills the in-flight SSH process and exits. There is no
   service to restore and no port to close.

The endpoint is documented in
[raspi-docs/04-backend-web-control.md](raspi-docs/04-backend-web-control.md).

## 5. Side effects

`--raw` has no side effects on the Pi. It only issues GET requests against the
Flask app, which already reads the serial port. The service, the board state,
the heater and the packet counters are unaffected.

The costs are:

- Up to one `--interval` of latency, plus the SSH round trip, before a line is
  printed.
- One short SSH connection per poll. Keys must be set up; see §1.
- Lines older than the last 500 are gone, about four minutes at the live rate
  of 2.1 lines/s.

## 6. Failure handling

- If `/raw` does not answer, the script logs one warning per outage and keeps
  retrying:
  `[warn] ... cannot read http://127.0.0.1:5000/raw: <ssh error>`.
  The warning is repeated only after a successful read.
- SSH errors appear in that same warning. The usual causes are a missing key,
  an unknown host key, or an unreachable network. `ConnectTimeout=8` bounds the
  wait.
- If the serial link is down, `/raw` still answers and `connected` becomes
  `false`; the script logs `serial link down` once. No lines arrive until the
  Flask app reopens the port.
- A non-JSON line is printed as-is, so boot noise is visible.

## 7. Limits

- The Flask buffer is 500 lines. A client that stops polling for longer than
  that loses lines; the warning reports how many.
- There is no replay of history before startup. To inspect the buffer, use
  `curl -s 'http://<pi>:5000/raw?limit=20'` (or over SSH).
- `seq` is in-process. It restarts at `0` when `water-ui` restarts, which the
  script detects and reports.
- The pm2 app name and the `/status` and `/raw` URLs are fixed. A renamed app
  needs an edit here.
- The script assumes the field names in `master.cpp` and `new-slave.cpp`
  described in [raspi-docs/06-master-firmware.md](raspi-docs/06-master-firmware.md)
  and [legacy-slave.md](legacy-slave.md) §5.
- It records nothing. For CSV capture, use the dashboard's logging toggle; this
  script only displays.

## 8. Related documents

- [legacy-slave.md](legacy-slave.md) — the slave firmware and its JSON fields.
- [legacy-slave-json-wrapper.md](legacy-slave-json-wrapper.md) — the local,
  single-port reader for the same slave.
- [raspi-docs/04-backend-web-control.md](raspi-docs/04-backend-web-control.md) —
  the Flask app, the `/status` cache and the `/raw` buffer.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.
