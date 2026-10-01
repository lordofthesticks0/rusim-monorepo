# RS-485 loopback test (`firmware/max485-test`)

Scope: the single MAX485 test program, which runs two transceivers on one
ATmega2560 and proves that each direction of the bus works.

`docs/hardware.md` §5 covers the MAX485 module itself, the DE/RE truth table, and
why the RS-485 library must not be used on this board's wiring. This document
covers the firmware: what it drives, what it prints, and how to read the result.

## 1. Purpose and shape

One Mega Mini, two MAX485 modules, both ends of one bus. The program sends a
message from module A to module B, then sends one from B to A, forever.

| Direction | What a pass proves |
|---|---|
| A → B | A's driver output reaches the bus, and B's receiver reads it back |
| B → A | B's driver output reaches the bus, and A's receiver reads it back |

Both directions must pass before any sensor is trusted on the bus. A failure in
one direction only points at that module's transmit or receive side.

This is a bring-up test, not a Modbus test. There is no addressing, no CRC, and
no register access. The payload is a plain ASCII line.

## 2. Wiring

| Module | RO | DI | Direction pins |
|---|---|---|---|
| A | RX3, D15 | TX3, D14 | D2 and D3, tied together on the module |
| B | RX2, D17 | TX2, D16 | D4 and D5, tied together on the module |

Bus wiring: A to A, B to B, and a common ground between the two module grounds
and the Mega. A 120 Ω termination resistor across A and B is needed if the cable
is long.

Pin numbers follow `docs/hardware.md` §4: on the Mega Mini, Serial 3 is D14/D15
and Serial 2 is D16/D17. Serial 0 (D0/D1) is left alone because it is the
USB-serial programming link.

## 3. Direction control

DE is active HIGH and RE is active LOW. Because the two are tied together on these
modules, the tie produces exactly two usable states:

| Both direction pins | Driver | Receiver | Meaning |
|---|---|---|---|
| HIGH | on | off | transmit |
| LOW | off | on | receive |

The program therefore drives two MCU pins per module and always writes the same
value to both. `rs485Dir()` is the only place direction is changed, and it writes
both pins in the same call.

**Splitting the two pins would short the microcontroller.** Driving one HIGH and
the other LOW would push current through the tie between them, from one output pin
into the other, for as long as the transmission lasts. This is the reason the
program does not use `circuitstate/CSE_ArduinoRS485`, which is the library that
asserts DE and RE oppositely. `docs/hardware.md` §5 records that an earlier
implementation used the library against this wiring and did not work; this is the
most likely reason.

Both `max485-test` environments previously declared that library in `lib_deps`.
The declarations have been removed. See `platformio-environments.md` §6.

Because the receiver is off while transmitting, a module never hears its own
bytes. A test that expects an echo will fail against correct hardware.

## 4. Code structure

| Function | Role |
|---|---|
| `rs485Dir(port, transmit)` | Writes the same level to both direction pins of a port |
| `rs485InitPort(port)` | Sets both pins to output, puts the port in receive, then starts the UART |
| `drainPort(port)` | Discards anything already buffered |
| `sendMessage(txPort, msg)` | Enters transmit, writes the message and a newline, flushes, returns to receive |
| `receiveMessage(rxPort, timeoutMs, gotLine)` | Blocks up to the timeout for a newline-terminated line |
| `testDirection(label, tx, rx, payload)` | Drains both ports, sends, waits, compares, logs, returns pass or fail |

`Rs485Port` bundles a `HardwareSerial*` with its two direction pins. Two instances
exist:

```
portA = { &Serial3, 2, 3 }
portB = { &Serial2, 4, 5 }
```

`rs485InitPort()` sets receive mode before starting the UART, so the bus is never
driven while the port is still coming up.

## 5. Timing constants

| Constant | Value | Reason |
|---|---|---|
| `RS485_BAUD` | 9600 | Matches `monitor_speed`; slow enough to read on a 9600 baud monitor |
| `RECV_TIMEOUT_MS` | 500 | Receive window per direction |
| `DIR_SETTLE_MS` | 2 | Wait after entering transmit, and again before releasing the bus |
| `CYCLE_DELAY_MS` | 2000 | Pause between cycles, plus 200 ms between the two directions |

`sendMessage()` waits 2 ms after asserting direction, because the transceiver
needs time to enable its driver before the first bit is valid. It calls
`flush()` before releasing direction, which blocks until the last stop bit has
actually left the UART shift register. Releasing direction without that wait
truncates the final byte. A second 2 ms wait acts as a turnaround guard so the
driver is not disabled while the last bit is still leaving.

`testDirection()` blocks for up to `RECV_TIMEOUT_MS` per direction, so one full
cycle takes at least 1.2 s. Each line only appears once per second or slower;
this is not a throughput test.

## 6. Message format and comparison

Messages are newline-terminated ASCII:

```
Hello from A #7
Hello from B #7
```

`receiveMessage()` returns as soon as it sees `\n`, discards `\r`, and trims
whitespace. A pass requires both that a line arrived before the timeout and that
it matches the sent payload exactly:

```cpp
bool pass = gotLine && (rx == payload);
```

This means byte-exact matching, including the cycle number. A pass is therefore
also a check that the two directions are not crossing, because A only ever
receives B's number for the same cycle. Whitespace differences would show as a
mismatch rather than being silently accepted.

## 7. Board guard

The whole test is inside `#if defined(ARDUINO_AVR_MEGA2560)`. The `#else` branch
prints

```
SKIP: dual MAX485 A<->B test requires a Mega2560 (Serial2 + Serial3).
```

and idles. This exists because `max485-test1` (Mega) and `max485-test2` (Nano)
build the same source directory. The Nano has only one hardware UART, so the
multi-UART test cannot run there; the fallback keeps the build valid instead of
failing at compile time. Flashing `max485-test2` to a Nano is only useful for
confirming that a Nano boots and prints.

## 8. Running it

```bash
pio run -e max485-test1 -t upload
pio device monitor -e max485-test1
```

Expected output:

```
=== MAX485 A<->B loopback test (Mega2560) ===
Module A: Serial3 (TX3=14,RX3=15) DIR=D2+D3 (tied)
Module B: Serial2 (TX2=16,RX2=17) DIR=D4+D5 (tied)
Bus wiring: A-A, B-B, GND-GND
Direction mode: single-pin (HIGH=TX, LOW=RX), pins never split
Starting A->B then B->A cycles...

----- Cycle 1 -----
[SEND A->B] TX: "Hello from A #1"
        (bytes written: 16)
[RECV A->B] RX: "Hello from A #1" | PASS
[SEND B->A] TX: "Hello from B #1"
        (bytes written: 16)
[RECV B->A] RX: "Hello from B #1" | PASS
Summary: A->B 1/1 pass | B->A 1/1 pass

```

## 9. Reading the result

The running summary line at the end of each cycle is the number to watch. A test
left running for several minutes and holding at `A->B 240/240 pass | B->A 240/240
pass` is a bus that is good.

| Symptom | Likely cause |
|---|---|
| `FAIL (mismatch)` on one line, passes after | A byte was lost or corrupted. Check termination, ground, and cable length. |
| `FAIL` after a timeout, both directions | The bus is not connected, A and B are swapped, or the common ground is missing. |
| `FAIL` after a timeout, one direction only | That module's DI, RO, or direction pin is miswired. The other direction's pass localises it to the transmitting or receiving side. |
| Passes at 9600 but not at a higher baud | Termination or cabling. Not tested by this program as written. |
| Nothing received on either module, but the bytes written count is correct | Direction pins on both modules are not being driven, or DE and RE are not actually tied on the board. |

The `bytes written` count is printed for every send. If it is zero or short, the
problem is in the UART and the message construction, before the transceiver is
involved.

## 10. Limits

- Two nodes only. Multi-drop addressing, termination at both ends, and bus
  biasing are not exercised.
- No Modbus framing. See `captive-portal-workaround.md` for the planned use of
  this bus with the WT32-ETH01.
- Direction is tied, so shutdown mode and self-echo monitoring are unavailable
  on this wiring.

## 11. Related documents

- `../hardware.md` §5 — MAX485 module, DE/RE truth table, and the warning
  against mixing tied and separate control.
- `platformio-environments.md` — the two build environments for this source.
- `wt32-eth01-ethernet-demo.md` — the board that is expected to sit on the other
  end of this bus.
- `captive-portal-workaround.md` — the plan for headless network authentication
  over this hardware.
- `subsystem-index.md` — the rest of the repo.

---
Content generated by `stealth/space-bunny-alpha` in `opencode`.
