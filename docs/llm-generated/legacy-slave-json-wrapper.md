# Legacy slave JSON table wrapper (`scripts-hardware/read-legacy-slave.ts`)

Scope: the Bun script that reads the JSON telemetry from
`legacy/slave/new-slave.cpp` and prints one table row per bottle. It replaces
eyeballing a single-line JSON object at 9600 baud. It does not change the
firmware or the protocol.

Source: `scripts-hardware/read-legacy-slave.ts`. Firmware and protocol:
[legacy-slave.md](legacy-slave.md) §5.

## 1. Usage

```bash
# Autodetect the USB serial port, wait for the board to settle, then print
bun scripts-hardware/read-legacy-slave.ts

# Same, with an explicit port and a longer startup wait
bun scripts-hardware/read-legacy-slave.ts --port /dev/ttyUSB0 --settle 8

# Replay a captured log through stdin
bun scripts-hardware/read-legacy-slave.ts --stdin < captured.log

# Or pipe a monitor process into it
pio device monitor -e legacy-slave --quiet --raw \
  | bun scripts-hardware/read-legacy-slave.ts --stdin
```

| Option | Meaning |
|---|---|
| `--port <port>` | Serial port. Default is autodetected with `pio device list --json-output`. |
| `--baud <baud>` | Baud rate passed to `stty`. Default `9600`, matching `Serial.begin(9600)`. |
| `--settle <seconds>` | Discard telemetry for this long after the first data arrives. Default `5`. `0` disables the wait. |
| `--stdin` | Read JSON lines from stdin instead of opening a serial port. |
| `-h`, `--help` | Print usage. |

Exit code is `0` on a normal end of stream, `1` on a port, read, or argument
error, and `130` when interrupted with `Ctrl+C`.

## 2. Startup settling

Opening a USB serial port typically asserts DTR and resets the Mega. `setup()`
then probes the HX711s, starts the DS18B20 buses, and only afterwards starts the
USB stream. During the first loop passes the firmware cache still holds its
invalid markers, so early messages contain `-1` / `-999`.

The wrapper therefore discards telemetry for `--settle` seconds (default 5).
The timer starts when the first serial bytes arrive, not when the port is
opened, so bootloader and `setup()` time do not eat the warm-up budget. Five
seconds after data starts covers the first temperature conversion and one full
CO2/pressure round. When the timer expires the wrapper resynchronises to the
next newline, so the first printed line is a complete JSON message.

```
Opened /dev/ttyUSB0 at 9600 baud.
Telemetry started. Waiting 5s for the sensor cache to fill...
Telemetry settled; showing live data.
time      bottle   pH    CH4 (ppm)  temp (°C)  CO2 (ppm)  press (Pa)
...
```

The progress messages go to stderr; the table goes to stdout.

Settling only hides startup misses. After settling, `-1` and `-999` are printed
as sent. They mean the firmware reported an invalid reading: the sensor is
absent, its read timed out, or its value was rejected. Check the firmware rules
in [legacy-slave.md](legacy-slave.md) §4.

## 3. Output

One header block, then four rows per telemetry message:

```
time      bottle   pH    CH4 (ppm)  temp (°C)  CO2 (ppm)  press (Pa)
--------  ------  -------  ---------  ---------  ---------  ----------
10:47:58       1     6.84      812.4      38.50       1204        1013
               2     6.81      790.1      38.20       1198        1013
               3     6.87      824.9      38.70       1210        1014
               4     6.80      801.3      38.40       1202        1013
```

- The initial `{"status":"ready"}` boot marker arrives inside the settle
  window and is normally discarded. A marker printed after a later reset is
  shown, because `printLine()` handles it at any time.
- The time column is the local arrival time (`HH:MM:SS`). It appears only on
  the first bottle row of each message.
- `pH` is pH units. The slave firmware applies its per-bottle 3-point
  calibration; see [legacy-slave.md](legacy-slave.md) §4.1.
- Numbers are printed as the firmware sent them. `-1` (pH, methane, CO2,
  pressure) and `-999` (temperature) are the invalid markers and are not
  translated to `n/a`. Negative pressure readings are also printed unchanged.
- `{"status":"ready"}` becomes `[status] ready` and makes the next telemetry
  message print the header again. A board reset therefore restarts the table
  cleanly.
- Lines that are not valid JSON are printed unchanged. Bootloader noise and a
  frame split by a reconnect stay visible instead of being dropped.

The script does not record data, convert units, check calibration, or send
PINGs. It only formats the USB stream.

## 4. How it reads the port

The script no longer uses `pio device monitor` for I/O. That command builds an
interactive terminal and fails with `termios.error: (25, 'Inappropriate ioctl
for device')` when stdin is not a TTY, for example when it runs under an agent
harness, a pipe, or a service.

Instead it does three steps:

1. `pio device list --json-output` finds the port. `/dev/ttyS*` entries with
   `hwid: "n/a"` are ignored. If several real ports remain, the script stops
   and asks for `--port` rather than guessing.
2. `stty -F <port> <baud> cs8 cread clocal -hupcl raw -echo ...` puts the
   device in raw 8-bit mode with no echo or line buffering. On macOS the flag
   is `-f` rather than `-F`.
3. `createReadStream` from `node:fs` reads the bytes. Input is decoded as
   UTF-8 and split on `\r?\n`, so a partial line cannot be parsed before its
   newline arrives. A complete line is then held for another 100 ms; if no new
   bytes follow, it is parsed. That quiet gap is the frame boundary.

The warm-up timer in §2 starts on the first byte received, and the 100 ms quiet
gate runs continuously. This needs no new package and no TTY, so it works in
the same shells as the rest of `scripts-hardware/`.

## 5. Limits

- Serial open/configuration uses POSIX `stty`; Windows is not supported.
- The port must not be held by another program. `pio device monitor`, the
  Arduino IDE, and this wrapper all need exclusive access. The script prints a
  hint if the read fails.
- A line longer than the read chunks is buffered until a newline arrives. The
  firmware's messages are about 500 bytes, so this is not a practical issue.
- The wrapper assumes the firmware's current field names and invalid markers.
  If either changes, update `printBottles()` and this document.

## 6. Related documents

- [legacy-slave.md](legacy-slave.md) — firmware, pin map, JSON protocol.
- [platformio-environments.md](platformio-environments.md) — the
  `legacy-slave` build environment.
- [flash-factory-tool.md](flash-factory-tool.md) — the other hardware script
  in `scripts-hardware/`.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.
