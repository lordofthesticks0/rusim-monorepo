# Display <-> WT32 link: `rusim-line` v1

Scope: the serial link between the display MCU and the WT32-ETH01 network
master. It replaces the Modbus RTU master/slave link documented in
`display-wt32-link.md` (retired). Both ends are peers: either may send at any
time, and neither polls the other.

- Display end: `firmware/display-v9/link_serial.{h,cpp}`.
- WT32 end: `firmware/network-master/main.cpp`.
- Display summary: `display-docs.md` §10.
- WT32 summary: `network-master.md` §2-4.

## 1. Physical layer

| WT32 pin | Display pin | Direction | Note |
|---|---|---|---|
| Debug-header TXD = **IO1** (UART0 TX) | IO18 (RX) | WT32 -> display | commands + log forwarding |
| Debug-header RXD = **IO3** (UART0 RX) | IO17 (TX) | display -> WT32 | commands |
| GND | GND | - | direct wire; a shared USB ground is not acceptable |

Settings: **115200 8N1** on both ends. The display uses `Serial1`
(RX 18 / TX 17); the WT32 uses UART0, so `Serial` is both the link and the log
sink there.

`IO1` and `IO3` carry no boot strapping on the ESP32. A UART transmitter idles
HIGH, so the display drives `IO3` correctly while the WT32 resets; power-up
order no longer matters. Do not confuse these with the ESP32-S3 Sunton display
board, where GPIO1/GPIO3 are RGB-panel blue-channel pins. The reserved
USB-looking pair exists only on the WT32 Debug header.

Bring-up: if no lines arrive, swap the two data wires first, then check the
direct GND. The display's `PASSTH ON` tunnel (§7) is the other quick check.

## 2. Line rules (normative)

- Every message is one ASCII line, `\n` terminated. Bytes `0x20`-`0x7E` only.
  Receivers ignore stray `\r`.
- Words are exact-match UPPERCASE; argument keys and values are lowercase as
  sent. Unknown or wrong-direction lines are silently ignored. This is also
  how power-on noise and WT32 ROM boot text are swallowed.
- Maximum line length 1024 bytes in both directions. A partially received line
  is discarded when no byte arrives for 50 ms (`LINK_GAP_MS`), so a truncated
  line never wedges the parser.
- Arguments are `key=value` tokens separated by one space.
- Percent-encoding (`%XX`, uppercase hex) applies only to `user=` and `pass=`
  values: any byte outside `[0-9A-Za-z-._~]`, plus `%` and `=` itself. No
  encoding anywhere else; `LOG` payloads are verbatim text.
- `LOG` payloads must not contain raw CR/LF. The WT32 helper escapes
  `\n` -> `\\n` and `\r` -> `\\r`, so one log entry is always one line.
- No checksums and no acks. A corrupted line is either ignored or carries a
  wrong value that the next heartbeat overwrites within 5 s. Idempotent
  re-transmission is the whole error strategy.

## 3. Command set

### 3.1 Display <- WT32

| Command | Payload | Sender cadence | Display action |
|---|---|---|---|
| `HELLO` | - | once at WT32 boot | log link-up; clear local verdict/creds-pending bookkeeping (the staged pair is kept and re-pushed) |
| `LOGINREQ` | - | every heartbeat while the WT32 needs credentials | raise `/setup` (idempotent) |
| `RESULT` | `OK` or `FAIL` | after every login attempt; once more on the HELLO snapshot if running | `OK`: stop credential re-push, clear the gate, route home; `FAIL`: `/setup` retry text + phone-page banner |
| `NET` | `ip=<str> up=<0\|1> [ping=<n\|->]` | every heartbeat + on every ETH event | `/info` status store. `up=0` carries empty `ip=` and no `ping=` |
| `DB` | `latest=<n\|-> time=<0\|1> bottles=<n\|-> drops=<n>` | every heartbeat + after each DB pass | `/info` DB store. `-` = unknown; `latest=0` means "no rows yet", which the display maps to -1 |
| `LOG` | `<text>` | continuously | print `"[W32] " + text` on the display's USB console, verbatim, never through a printf format string |

### 3.2 WT32 <- display

| Command | Payload | Sender cadence | WT32 action |
|---|---|---|---|
| `HELLO` | - | once at display boot | log link-up; immediately re-send current truth (`NET`, `DB`, `LOGINREQ` if needed, `RESULT OK` if running) |
| `EXPERIMENT` | `run=<0\|1> num=<n> dur=<1-99>` | on every change + every heartbeat | store; log only on change |
| `CREDS` | `user=<pct> pass=<pct>` | when a pair is staged, then every heartbeat while no verdict for it has arrived | decode, trim, empty -> ignore. Same pair as last attempt -> re-emit the stored verdict. Changed pair -> run the portal login |
| `DUMP` | - | bench only, `IS_DEBUG` builds | emit a `LOG [DUMP] ...` snapshot of local state |

The display ignores display-direction words arriving on its RX; the WT32
likewise. Loopback benching (RX shorted to TX) therefore produces no loops.

### 3.3 Gone from the Modbus design

- Master/slave roles, FC codes, slave ID, the duplicated register map.
- The `CREDS_READY` handshake and the `IO11 -> IO14` "poll now" line. Staging
  a pair transmits `CREDS` immediately, so a corrected password takes effect
  on the spot instead of up to 5 s later.
- Polling entirely: both ends declare truth on a cadence; there is no
  request/response anywhere on the link.
- CRC16. The display module no longer carries it; the WT32 keeps an unused
  helper (§8). The Mega slave link has no checksum either.

## 4. State and flows

### 4.1 WT32 state machine

```
WAIT_IP  ->  NEED_CREDS  ->  LOGIN  ->  RUN
   ^             ^                      |
   `-------------`----------------------`
        link lost     fail / walled mid-run
```

- `WAIT_IP`: heartbeat `NET up=0`; logs every 2 s while waiting for DHCP.
- `NEED_CREDS`: every 5 s emits `NET up=1`, `DB` and `LOGINREQ`, plus the
  30 s already-authenticated probe. A `CREDS` line moves to `LOGIN`.
- `LOGIN`: runs `doLogin()`, emits `RESULT OK` or `RESULT FAIL`, wipes the
  working pair, then moves to `RUN` or back to `NEED_CREDS`.
- `RUN`: every 5 s pings, fetches the data URL, runs the DB pass, and emits
  `NET` + `DB`. A WALLED fetch returns to `NEED_CREDS`.

`loop()` pulls and dispatches link lines first, then runs a `millis()`-gated
stage tick. `doLogin()` and the HTTPS work still block, but the UART FIFO
absorbs a heartbeat and the next pass drains it.

### 4.2 Credential and verdict flow

```
display: user confirms      WT32: NEED_CREDS
   |  CREDS user=.. pass=..  ->  |
   |                             |  decode, trim, stage LOGIN
   |                             |  doLogin()
   |  <- RESULT OK / FAIL        |
   |  stop re-pushing this pair  |
```

The display keeps the pair in RAM for the session. A WT32 `HELLO` (reboot)
clears the display's verdict bookkeeping, so the pair is re-pushed and the
rebooted master can log in again without retyping. A display `HELLO` (reboot)
makes the WT32 re-send its truth, so a display that rebooted mid-run leaves
the login gate as soon as the snapshot `RESULT OK` arrives.

Same-pair rule: if the WT32 receives a pair identical to the last attempt, it
re-emits the stored verdict instead of logging in again. This makes a retry tap
of an unchanged pair harmless.

### 4.3 Identity and privacy

- `user=` and `pass=` are percent-encoded by the display and decoded by the
  WT32. Empty values are ignored at the WT32.
- The display logs only the username. The `CREDS` local echo is the word alone
  (`[LINK>] CREDS`), never the payload.
- The WT32 never logs the password and wipes its working buffers after each
  attempt. The last attempted pair stays in RAM so the same-pair rule can
  answer a repeat; nothing enters NVS or flash.

## 5. Timing budget

| Event | Period |
|---|---|
| Heartbeat both ends (`LINK_BEAT_MS`), carrying each side's full truth | 5 s |
| Immediate pushes | credentials staged, experiment change, ETH event, after DB pass, after login attempt |
| Line gap discard | 50 ms |
| HELLO snapshots | once per boot, both ways |
| Worst-case wire load | a few hundred bytes/s against ~11 KB/s capacity at 115200 |

## 6. Log forwarding

The WT32's log helper (`logf`) formats into a fixed 1024-byte buffer, escapes
CR/LF, and writes one `LOG <text>` line to `Serial` (UART0). The display
recognises the `LOG ` prefix, prints `[W32] <payload>` on its own USB console,
and ignores every other unrecognised line.

Rules for WT32 code:

- Every diagnostic goes through `logf(...)`. Never leave a bare `Serial.print`
  fragment: two fragments split by a newline become a protocol line and an
  unknown line, and the display drops both.
- Password values never enter a log call.
- ROM boot text is emitted before UART0 is configured for the link and comes
  out unprefixed. The display drops it; a raw adapter still shows it.

## 7. Passthrough tunnel (`PASSTH`)

The display's USB serial command `PASSTH ON` opens a raw bit tunnel:
USB bytes stream to `Serial1` and `Serial1` bytes stream to USB, with both
parsers suspended and no link commands emitted. `PASSTH OFF` at the start of a
line closes it. The exit word is only matched at a line start, so after binary
payload send a newline first (`\nPASSTH OFF`); the WT32 ignores the empty line.
The state is runtime only and never survives a reboot.

While the tunnel is active, the display turns off the backlight, skips its
screen-model polling and `lv_timer_handler()`, and continues polling both serial
ports. On exit it wakes the backlight, marks the active LVGL screen dirty, and
resumes normal UI work. `Serial1` gets a 4096-byte RX buffer before `begin()` to
absorb transfer bursts while the bridge loop alternates between USB and UART.

Two uses:

1. Watch the WT32's unprefixed ROM output or test the wire by hand.
2. Flash the WT32 with the adapter on the display's USB instead of the WT32's
   Debug header.

The WT32-ETH01 has no USB port. The bench drill is a USB-UART adapter wired to
5V, GND, RX and TX only, with `IO0` jumpered to GND for the upload and a power
cycle afterwards (jumper removed) to run the firmware. That drill is the same
whether the adapter sits on the WT32 Debug header or on the display's USB; the
tunnel carries data only and neither helps nor hinders bootloader entry.

The display's USB is a CH340 bridge on UART0 (`GPIO43` TX / `GPIO44` RX), not
the ESP32-S3 native USB-Serial/JTAG peripheral. It has an auto-reset circuit,
and an ordinary serial open is enough to reset the display: the open drives
DTR/RTS, the board reboots within 50 ms, and the runtime-only tunnel is gone.
Measured at the bench: pyserial's `open()` resets the board even with
`dtr=False, rts=False` set before `open()`; opening with `dsrdtr=True`, or with
`os.open()` plus termios, does not. That is why a plain `pio device monitor` on
this port reboots the display, and why esptool cannot be pointed at it
directly. `--before no_reset` skips esptool's own reset sequence but not the
reset raised by the serial library while opening the port.

The upload hook therefore never hands the display port to esptool. It opens the
port once with `os.open()` and termios, keeps that handle for the whole upload,
and gives esptool a pseudoterminal: two blocking pumps relay bytes between the
pty and the display port for the duration. A byte count is printed afterwards,
so a silent tunnel is visible in the upload log. Two settings still apply:

- Force 115200 baud. The tunnel runs at the display's fixed rate, while the
  `network-master` board default is 460800 (`upload_speed = 115200` for this
  path).
- Leave `board_upload.before_reset` and `board_upload.after_reset` at
  `no_reset`. The reset would land on the pty now, so this is belt and braces.

`pio run -e network-master -t upload` runs the hook. It uses `upload_port` or
`--upload-port` when set; otherwise it accepts exactly one Espressif
USB-Serial/JTAG, CH34x or CP210x port (the Sunton display is a CH340,
`1a86:7523`). It sends `PASSTH ON` and waits up to 1.5 s for the confirmation,
retrying up to three times; a missing port, a failed open or no confirmation
after three attempts cancels the upload before esptool runs, and the message
includes the display's last output. A repeat sent into an already open tunnel
is harmless, because those bytes reach the WT32, which ignores them until
esptool starts its sync. The marker test must lowercase both sides: comparing
a capitalised marker against lowercased output makes every confirmation look
like a timeout. The port is pinned in
`UPLOAD_PORT` so PlatformIO's own autodetection cannot switch devices. After the
upload it sends `\nPASSTH OFF` and waits 1.5 s for the exit confirmation. If the
upload command fails, process cleanup makes the same best-effort close. Plain
`pio run -e network-master` remains build-only.

A second serial device with the same VID/PID (for example the display's CH340
and a CH340 adapter on the WT32 Debug header) makes autodetection refuse to
choose; pass `--upload-port` in that case.

Ground WT32 `IO0` and power-cycle the WT32 before starting the upload; after it
completes, remove the jumper and power-cycle the WT32 to run the new firmware.

The relay and the exit handshake were verified on the host with a
pseudo-terminal standing in for the display, then at the bench: a full
`network-master` upload through the tunnel wrote 988944 bytes in 56 s and
esptool reported `Hash of data verified`. The display never rebooted (no
`ESP-ROM` banner appeared) and resumed its UI after the exit handshake.

Known edge: flash payload bytes containing the exact `PASSTH OFF` line at a
line boundary would close the tunnel. Accepted as negligible; a failed flash
just needs `PASSTH ON` again.

## 8. Follow-on: Mega slave link (`IO5`/`IO17`)

Moving the display link to the Debug header freed the WT32 `IO5`/`IO17` pair.
It now carries the legacy Mega slave directly, over TTL, not through a MAX485
bus: the slave never drives `DE`/`RE`, and a cross-connected RX/TX pair needs
no direction control.

| Signal | WT32 pin | Mega pin | Direction |
|---|---|---|---|
| Data | `IO5` (`Serial1` RX) | D18 (TX1) | Mega -> WT32 |
| Data | `IO17` (`Serial1` TX) | D19 (RX1) | WT32 -> Mega |
| Ground | `GND` | `GND` | common |

9600 8N1, matching the slave's `Serial1.begin(9600)`. The WT32 pins are
remapped in `Serial1.begin(9600, SERIAL_8N1, 5, 17)` because its UART1
defaults (`IO9`/`IO10`) are flash pins. Request `#1:PING*`, reply one JSON
line; no checksum, no acks, no retry.

The Mega drives 5 V, so D18 -> `IO5` needs a divider or a level shifter. The
full wiring, electrical warning and parse rules are in `network-master.md`
§2.2-2.3; the plan is `migration-plan.md`. `IO14` stays spare, and the
`CRC16` helper stays in the firmware unused. The line link shares no state
with this port.

## 9. Failure modes

| Symptom | Likely cause |
|---|---|
| Nothing received on the display | data wires swapped (swap first), missing direct GND |
| Commands arrive, logs never | WT32 still emits multi-part `Serial.print` fragments instead of single `logf` lines |
| WT32 never reacts to `CREDS` | percent-decoding bug, empty pair, or the staging path bypassed `link_serial_set_credentials` |
| `/info` stale but heartbeats seen on the wire | heartbeat tick skipped by a blocking loop on either board |
| Flash over the tunnel fails | WT32 not in download mode (`IO0` jumpered, then power-cycled), or its wires are not on display `IO17`/`IO18` |
| Tunnel disappears while a monitor is open | any ordinary open of the display's CH340 port resets the board; close the monitor before uploading |
| Upload starts then stalls | adapter baud left at the 460800 board default; the tunnel cannot change rate |
| No `[SENS]` lines | Mega unpowered, its TX wire open, or no common ground; the ESP logs `[SENS] poll failed: reply timeout` on the first failure and then every 20th |

## 10. Bench checks

1. **Loopback, display only:** short display IO18 to IO17. `LINK` prints state;
   `PASSTH ON` echoes typed characters; every emitted command logs one
   `[LINK>]` line, and its looped-back copy is silently ignored (direction
   check).
2. **One board, display only:** expect an `EXPERIMENT` heartbeat every 5 s and
   no errors.
3. **Both boards:** see `[W32]`-forwarded WT32 logs, then `NET`/`DB`/
   `LOGINREQ`; `/info` shows the live IP after the HELLO exchange.
4. **Portal round trip:** stage credentials; `RESULT OK` routes home, `FAIL`
   shows the retry path; a corrected pair takes effect immediately.
5. **Reboot mixes:** WT32 reboot mid-run is released by the snapshot
   `RESULT OK`; display reboot mid-run re-raises `/setup` and replies to
   `HELLO` with experiment state.
6. **Mega slave:** Mega powered from the same ground, WT32 running. Expect
   one `[SENS]` line per 30 s with values that match the JSON the Mega prints
   on its own USB at the same time. Pull the Mega's TX wire: the ESP logs a
   timeout, the display link stays up, and readings resume when the wire
   returns.
7. Three build targets must pass before claiming done: `display-v9`,
   `display-sim`, `network-master`.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.
