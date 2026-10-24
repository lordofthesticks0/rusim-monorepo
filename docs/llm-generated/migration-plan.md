# Migration plan: ESP pulls telemetry from the legacy Mega slave

Status: plan only. No firmware in this repository has been changed for it yet.
Read `legacy-slave.md` for the slave, `network-master.md` for the ESP, and
`link-serial-protocol.md` §8 for the pin reservation this replaces.

## 1. Where the data comes from today

| Piece | Behaviour |
|---|---|
| `legacy/slave/new-slave.cpp` | Samples 4 bottles (pH, methane, DS18B20, MH-Z19C, HX710B). Answers `#1:PING*` on `Serial1` (D18 TX / D19 RX, 9600 8N1) with one JSON line, and streams the same JSON to USB every 1 s. |
| Raspberry Pi (`legacy/pi/water-control-system`) | Reads the Mega over its USB serial. Converts `ph` from volts to pH units with the 3-point `PH_CALIBRATION` table in `web_control.py`. |
| RS485 | Never worked: the MAX485 modules tie `DE`/`RE` and need manual direction control that the slave does not drive (`legacy-slave.md` §7). |
| `network-master` | Has a dormant push queue (`pushEnqueueRound`, `sbPushReadings`) that nothing feeds. `network-master.md` §6 keeps it folded away until a caller exists. |

## 2. Decisions taken (2026-10-24)

1. **Log only, for now.** The ESP decodes the reply and prints one `LOG [SENS]`
   line per round. It does not enqueue to the push queue, so nothing reaches
   Supabase in this step. The queue stays dormant as `network-master.md` §6
   describes.
2. **pH conversion belongs in the slave, not the ESP.** Until the slave
   converts, the ESP forwards `ph` as raw volts and marks the invalid `-1` as
   null. No calibration table is copied into the ESP firmware.

## 3. Target wiring

Direct TTL between the two boards; no MAX485, because the request assumes
`RXD`/`TXD` are cross-connected.

| Signal | WT32 pin | Mega pin | Direction |
|---|---|---|---|
| Data | `IO5` (UART1 RX) | D18 (TX1) | Mega -> WT32 |
| Data | `IO17` (UART1 TX) | D19 (RX1) | WT32 -> Mega |
| Ground | GND | GND | common |

Settings: 9600 8N1, matching `Serial1.begin(9600)` in the slave. `IO5` and
`IO17` are free because the display link moved to UART0 `IO1`/`IO3`
(`network-master.md` §2); `IO14` stays spare. `Serial1.begin(9600,
SERIAL_8N1, 5, 17)` remaps the pins, since the ESP32 defaults for UART1
(`IO9`/`IO10`) are flash pins on this module.

Electrical warning, unresolved: the Mega drives 5 V and the WT32's pins are
3.3 V only. `D18 -> IO5` needs a divider (2k2/3k3) or a level shifter.
`IO17 -> D19` at 3.3 V sits close to the ATmega's `VIH` (about 3.0 V at 5 V
supply), so it works in practice but is not guaranteed. Common ground is
mandatory; do not rely on the two boards sharing a USB ground.

## 4. Protocol facts the ESP has to honour

- Request: `#1:PING*`, 8 bytes. No checksum, no length field, no retry.
  The slave collects a frame with a 50 ms window after the first byte and
  replies on the port the request arrived on.
- Reply: one JSON object followed by `\n`. Four bottle objects, roughly
  300-500 bytes, so about 310 ms of line time at 9600.
- Fields and invalid markers (`legacy-slave.md` §5.2):

| Field | Unit | Invalid | ESP mapping |
|---|---|---|---|
| `id` | 1-4 | never | bottle index `id - 1` |
| `ph` | volts (raw, by decision 2) | `-1` | `nullMask` bit 0 |
| `methane` | ppm | `-1` | bit 4 (`ch4`) |
| `temp` | degC | `-999` | bit 2 |
| `co2` | ppm | `-1` | bit 3 |
| `press` | Pa | `-1` | bit 1 (`pressure`) |

- Latency: the slave services PING inside every blocking section (methane
  sweep, HX711 wait, CO2 edge wait), so the answer normally starts within tens
  of milliseconds. Its own 50 ms capture window plus the ~310 ms transmit set
  the floor. Budget **1500 ms** for a full reply.
- The slave's `Serial1` TX buffer is 64 bytes, so writing the reply blocks it
  for about 300 ms. A poll every 30 s adds one such stall per round; the
  `serviceMasterPing()` window can already push a CO2 edge measurement out of
  tolerance (`legacy-slave.md` §6).
- `bottle_id` in the database is 0-based (`database.md`, `readings` table), so
  ids 1-4 must be shifted down by one before they can ever be enqueued.

## 5. ESP implementation steps

1. `setup()`: after the UART0 link is up, `Serial1.begin(9600, SERIAL_8N1, 5,
   17)` and one `logf` line naming the pins and rate.
2. Receive buffer: `static char sensRx[513]`, fixed, plus a length and an
   overflow flag. No `String`, no ArduinoJson: flash is at 75 %, and the
   document shape is fixed (`network-master.md` §6).
3. `sens_poll()`, gated to one round every 30 s:
   - drain stale RX bytes,
   - write `#1:PING*`,
   - read until `\n`, buffer full, or 1500 ms,
   - reject a frame that does not begin with `{` before parsing.
4. Parse with `strstr` on `"id":`, `"ph":`, `"methane":`, `"temp":`, `"co2":`,
   `"press":` and `strtof`. Ignore ids outside 1-4. All reads bounded by the
   buffer's terminating NUL.
5. Convert the invalid markers to a `nullMask` instead of passing `-1` / `-999`
   through, so the shape already matches `PushRow` when the push step lands.
6. Emit one line per round, for example:
   `LOG [SENS] 4 bottles ok=12 fail=0 b1 ph=2.51 ch4=812 t=38.5 co2=1204 p=1013`
   with `null` in place of a missing value. On failure, log the first one and
   then every 20th, so a disconnected Mega cannot flood the display's log.
7. Call `sens_poll()` in `loop()` before the stage switch, so it runs in every
   stage and is not skipped while the portal gate is open.
8. Add the sensor counters to `link_log_dump()` so the existing `DUMP` command
   reports them without extending the link protocol.

## 6. Deferred, with the hook each needs

| Step | Hook that already exists |
|---|---|
| Enqueue readings to the push queue | Build `BottleReading[4]` from the parsed round, call `pushEnqueueRound(time(nullptr), -1, readings, 4)`. Passing `-1` omits `experiment_id`, so the server applies the device's active experiment. `syncDbLatest()` then drains through `sbPushReadings()`. |
| Convert pH in the slave | Port `PH_CALIBRATION` and `convert_ph_voltage()` from `legacy/pi/water-control-system/web_control.py` into `new-slave.cpp`, per bottle, 3-point piecewise at pH 4.01 / 6.86 / 9.18. Once the slave sends pH units, the ESP logs it unchanged. |
| Push cadence | The flush is 2 rounds per 30 s, so sampling faster than every 15 s would overrun the 1200-row ring and drop the oldest rows. 30 s is the safe starting point. |

## 7. Verification

1. Bench: Mega powered with at least one sensor per type, WT32 on the same
   ground, `SENS` request visible on a scope or a second adapter if nothing
   answers.
2. Expect `[SENS]` once per round in the display's `[W32]` log, with values
   that match what the Mega prints on its own USB at the same time.
3. Unplug the Mega's TX wire: the ESP must log a timeout, keep the display
   link alive, and recover when the wire returns.
4. Size check: capture one reply at 9600 and count the bytes, confirming the
   512-byte buffer holds the worst case (four bottles, all values present).
5. Builds that must still pass: `network-master`, plus `legacy-slave` if the
   slave is touched for the pH step.

## 8. Open risks

- 5 V into `IO5` can damage the WT32; the divider is not optional.
- The slave's `serviceMasterPing()` blocks for up to 50 ms and can spoil a CO2
  edge; a 30 s poll adds one such window per round.
- `press` is Pa and no unit conversion is planned anywhere in this path.
- If the Pi stays attached to the Mega's USB port, two masters can poll. That
  is harmless at the protocol level, but the WiFi/USB split needs a decision
  before both run at once.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.
