# Legacy slave firmware (`legacy/slave/new-slave.cpp`)

Scope: the one-file Mega Mini program that reads four bottles of sensors and
publishes them as JSON, both on the RS485 line when the master asks and on USB
serial once per second. This is the legacy implementation kept in `legacy/`; the
production replacement described in `esp-db-sync.md` §3 does not exist yet.

Source: `legacy/slave/new-slave.cpp`. Build environment: `[env:legacy-slave]` in
`platformio.ini` (see `platformio-environments.md` §3).

## 1. What the program does

1. `setup()` starts both serial ports, builds the methane calibration curves,
   starts the four DS18B20 buses, and probes the four HX711 pressure ADCs.
2. `loop()` samples every sensor into a set of cached arrays.
3. On `#1:PING*`, it serialises the cache to the port the PING arrived on.
4. Independently, it serialises the cache to USB every 1000 ms.

Sampling is continuous and free-running. A PING never triggers a measurement; it
only reports the most recent cached values. This keeps the reply fast and makes
the reply independent of sensor timing.

## 2. Build environment

```ini
[env:legacy-slave]
board = megaatmega2560
framework = arduino
monitor_speed = 9600
build_src_filter = +<../legacy/slave/>
platform_packages = platformio/tool-esptoolpy@^2.41100.260830
lib_deps =
	paulstoffregen/OneWire@^2.3.8
	milesburton/DallasTemperature@^4.0.6
	robtillaart/HX711@^0.6.5
	bblanchon/ArduinoJson@^6.21.0
```

`[platformio] src_dir = firmware` is the repo default, so this source lives
outside the source root. The `../legacy/slave/` path in `build_src_filter` is
what reaches back out of it. The environment is otherwise identical in shape to
the other Mega Mini environments.

Build and flash:

```bash
pio run -e legacy-slave
pio run -e legacy-slave -t upload
pio device monitor -e legacy-slave     # 9600 baud on the USB port
```

`monitor_speed` is 9600 because that is what `Serial.begin(9600)` uses. A monitor
opened at 115200 prints nothing readable.

## 3. Pin map

Four bottles, indexed 0–3. Bottle numbers in the JSON are index + 1.

| Function | Bottle 1 | Bottle 2 | Bottle 3 | Bottle 4 | Array name |
|---|---|---|---|---|---|
| pH (PH-402C, analog) | A2 | A5 | A8 | A11 | `PIN_PH` |
| Methane (TGS2611, analog) | A1 | A4 | A7 | A10 | `PIN_METHANE` |
| Temperature (DS18B20, 1-Wire) | D4 | D7 | D10 | D22 | `PIN_TEMP` |
| CO2 (MH-Z19C, PWM) | D14 | D15 | D16 | D17 | `PIN_CO2_PWM` |
| Pressure DOUT (HX711) | D2 | D5 | D8 | D11 | `PIN_PRESS_OUT` |
| Pressure SCK (HX711) | D3 | D6 | D9 | D12 | `PIN_PRESS_SCK` |

The firmware uses Arduino digital numbering, not port names. `PIN_TEMP[3] = 22`
means Arduino D22, which is ATmega `PA0` and the first pin of the left header on
the Mega Mini. It is not `A0`: `A0` is Arduino pin 54 (`PF0`/ADC0) on the right
header and is a different physical pin.

Serial assignment:

| Port | Pins | Baud | Purpose |
|---|---|---|---|
| `Serial` | D0/D1 (USB / FTDI) | 9600 | Telemetry stream and boot marker |
| `Serial1` | D18 TX / D19 RX | 9600 | master link: WT32 UART1 over TTL, or the old MAX485 bus (`migration-plan.md`) |

Each bottle gets its own `OneWire` bus on its own pin, so four DS18B20s can share
one address (`index 0`) without a bus scan.

## 4. Per-sensor processing

### 4.1 pH

`updatePhAndTemp()` scales `analogRead(PIN_PH[i])` with `5.0f / 1023.0f` and
feeds the voltage to `convertPhVoltage(v, i)`, a 3-point piecewise linear
conversion per bottle:

- `PH_V_401`, `PH_V_686`, `PH_V_918` hold the voltages measured at pH 4.01,
  6.86 and 9.18. They are the table the Pi used to keep in `PH_CALIBRATION`;
  bottle 1 has its own three points, bottles 2-4 share one set.
- Acidic segment (`v > v_686`):
  `pH = 4.01 + (6.86 - 4.01) / (v_686 - v_401) * (v - v_401)`.
- Alkaline segment (`v <= v_686`):
  `pH = 6.86 + (9.18 - 6.86) / (v_918 - v_686) * (v - v_686)`.
- `v <= 0` or `v >= 5` (which covers raw ADC 0 and 1023) is invalid and reports
  `-1`. The result is clamped to 0-14 and rounded to two decimals, which is what
  the Pi computed and what the CSV records.

The serialised `ph` field is therefore **pH units**. The Flask server used to
apply this conversion on ingest; it now passes the value through, so the two
must be deployed together.

### 4.2 Methane (TGS2611)

`updateAllMethane()` takes 32 samples per channel with a `delay(2)` between
rounds, discarding the first `analogRead` after each channel switch (the ADC
needs a settling read after the multiplexer moves).

Any sample at 0 or 1023 marks the whole channel clipped, and a clipped channel
reports `-1`. A clean channel becomes:

```
Rs  = (1024 / raw_avg - 1) * RL[i]          RL = 1000 ohm
ppm = PPM_SPAN * (RS_SPAN[i] / Rs) ^ curve_exp[i]
```

`curve_exp[i]` is fitted at boot from the two calibration points per sensor
(`RS_AIR` at `PPM_AIR`, `RS_SPAN` at `PPM_SPAN`). If the fit is degenerate, or
`Rs` falls below `rs_at_max[i]`, the channel reports `-1` instead of an
extrapolated number. `PPM_MAX` is 10000 ppm.

`PPM_MIN_TYPICAL` (500 ppm) is declared but never used. It documents the sensor's
lower detection limit only.

The calibration constants are the ones recorded in `../calibration.md` §5. That
document also notes the sensors read roughly 7× low against a reference meter, so
treat absolute methane values as uncalibrated.

### 4.3 CO2 (MH-Z19C, PWM)

The MH-Z19C outputs a ~1 s period PWM whose high time encodes concentration.
`readCo2Ppm()` measures three edges with `micros()`:

1. wait for LOW, then HIGH — that edge is `tRise`
2. wait for LOW — `tFall`, giving `highUs`
3. wait for HIGH — `tRise2`, giving the period

Validation rejects a period outside 950–1060 ms or a high time outside 1–1002 ms.
The accepted reading is:

```
ppm = 5000 * (highMs - 2) / (periodMs - 4)
```

clamped at 0. Each bottle is read in turn in `loop()`, so one full pass costs
about 4 s.

Two things to know before trusting the numbers:

- The sensor tops out at 5000 ppm. `../hardware.md` §5.3 and `../calibration.md`
  §2 record that rumen headspace is roughly 70 % CO2, which is far outside the
  range. The reading is not physically meaningful for this application.
- `pingHandledDuringCo2` suppresses the `-1` write when a PING was serviced
  during the measurement. A reply sent mid-measurement is still the previous
  value; the flag exists so the failed measurement does not overwrite a good
  cached value.

### 4.4 Pressure (HX711 / HX710B)

`PA_PER_UNIT` is 100.0, applied to `get_units(5)`. Each bottle's
`CALIBRATION_FACTORS` entry comes from `../calibration.md` §3.

Initialisation is deliberately defensive:

```cpp
pressureSensors[idx]->begin(PIN_PRESS_OUT[idx], PIN_PRESS_SCK[idx],
                            false, false);
```

The fourth argument is the library's `doReset` flag. Left at its default, the
HX711 library's reset path calls `read()`, which spins on `while (digitalRead(_dataPin) == HIGH)`
with no timeout. An absent or unpowered HX711 therefore hangs `setup()` forever
and the board never reaches `loop()`. Passing `false` skips that path, and the
program's own `waitHx711Ready()` timeout (1500 ms at init, 250 ms per read)
decides whether the sensor is present. `firmware/pressure-calibrate/main.cpp`
uses the same call for the same reason.

A sensor that fails init reports `-1` and is retried on the next
`updatePressure()` call, so a load cell connected after boot starts working
without a reset.

`tare(3)` runs only after the sensor has answered ready. It averages three raw
reads and stores them as the zero offset, so the reading is relative to whatever
load was present at init.

### 4.5 Temperature (DS18B20)

The sensors are non-blocking: `setWaitForConversion(false)`, then
`requestTemperatures()` starts a conversion and the next pass reads the previous
result. A new conversion is requested at most every 800 ms, which is longer than
the 750 ms worst-case 12-bit conversion time.

`-999` means "no reading". The sentinel is chosen so it cannot collide with a
real temperature; `85.0` is the DS18B20 power-on value and is also rejected.

Bottle 4's bus was originally on D13, which is also `LED_BUILTIN`. On the Mega
2560 PRO Mini the onboard LED is connected to D13 through a series resistor to
GND. While D13 is an input, that resistor acts as a pull-down and the usual
4.7 kΩ bus pull-up cannot reach the ATmega's input-high threshold of 3.0 V (a
1 kΩ LED resistor gives about 0.9 V). The OneWire reset/presence pulse never
succeeded, `getTempCByIndex(0)` returned `DEVICE_DISCONNECTED_C` (`-127`), and the
channel reported `-999`. The other three temp pins have no LED. D13 works
normally as an output; the problem only appears when it is used as an input.

The bus is now on D22. When a 1-Wire sensor is moved off D13, check three things:

- The bus pull-up (4.7 kΩ to 5 V) must be on the new net. If it was fitted on the
  old D13 branch and only the data wire was moved, D22 has no pull-up.
- D13 must no longer load the new net. If the sensor data wire was moved to D22,
  the D13 branch is isolated and its LED is harmless. If D13 was jumpered to D22
  instead, the LED resistor is in parallel with the bus and must be removed (LED
  or series resistor), or the line still sits near 1 V.
- Leave D13 unconfigured. The core sets it as `INPUT` at startup and nothing in
  the firmware drives it.

The ATmega's internal pull-up is not a substitute for the external resistor.
`pinMode(pin, INPUT_PULLUP)` is undone by the OneWire library: on AVR,
`DIRECT_MODE_INPUT` clears only `DDR`, and `DIRECT_WRITE_LOW` clears `PORT`
before every read slot and again at the end of every byte, so the internal
pull-up is off exactly when the bus needs it. Even if it stayed enabled, the
20-50 kΩ value is roughly ten times weaker than the 4.7 kΩ the bus needs. Fit an
external 4.7 kΩ resistor from the data line to 5 V.

## 5. Serial protocol

### 5.1 Request

```
#<id>:<command>*
```

`SLAVE_ID` is 1, so the only accepted request is `#1:PING*`. Anything before the
`#` is ignored, and anything after the `*` is left in the buffer.

`handlePingStream()` waits up to 50 ms to collect a frame once a byte arrives. It
then trims to the first `#`, splits on `:`, parses the id with `toInt()`, and
compares the trimmed command to `"PING"`. An id of 0 (unparsable) never matches.

There is no checksum, no length field, and no retry. A partial frame times out
and is discarded.

### 5.2 Response

One JSON object followed by `\n`:

```json
{"bottles":[{"id":1,"ph":6.84,"methane":812.4,"temp":38.5,"co2":1204,"press":1013.2},{"id":2,"ph":6.81,"methane":790.1,"temp":38.2,"co2":1198,"press":1012.8},{"id":3,"ph":6.87,"methane":824.9,"temp":38.7,"co2":1210,"press":1013.5},{"id":4,"ph":6.80,"methane":801.3,"temp":38.4,"co2":1202,"press":1013.0}]}
```

| Field | Type | Unit | Invalid marker |
|---|---|---|---|
| `id` | integer | — | never invalid |
| `ph` | float | pH units (see §4.1) | `-1` |
| `methane` | float | ppm | `-1` |
| `temp` | float | °C | `-999` |
| `co2` | integer | ppm | `-1` |
| `press` | float | Pa | `-1` |

`StaticJsonDocument<1536>` holds the whole document on the stack. The four-bottle
object measures well under that, and the build reports 980 bytes of static RAM
used out of 8192.

### 5.3 Which port gets the reply

`serviceMasterPing()` polls both `Serial` and `Serial1`, and
`sendTelemetryJson(port)` writes back to the port the request came from. A PING
typed into the USB monitor therefore gets its answer on USB, and a PING on RS485
gets its answer on RS485.

### 5.4 USB stream

`serviceUsbTelemetry()` emits the same JSON to `Serial` every 1000 ms, whether or
not a master is attached. It is called from several points in `loop()` so the
cadence survives the multi-second CO2 and pressure passes rather than slipping to
one message per full cycle.

`setup()` prints `{"status":"ready"}` after sensor init. If that line never
appears, the board did not finish `setup()` — check the HX711 path first (§4.4).

To read the stream as a per-bottle table, run
`bun scripts-hardware/read-legacy-slave.ts`; see
[legacy-slave-json-wrapper.md](legacy-slave-json-wrapper.md). It opens the port
directly, without needing a TTY, and discards the first five seconds of
telemetry after it starts, because opening the port resets the board.

## 6. Responsiveness

Every long operation is instrumented with `serviceMasterPing()`:

| Blocking point | Instrumentation |
|---|---|
| 32-sample methane loop | `serviceMasterPing()` after each `delay(2)` |
| HX711 readiness wait | `serviceMasterPing()` on each 5 ms poll |
| DS18B20 request loop | `serviceMasterPing()` per sensor |
| CO2 edge wait | checks `Serial`/`Serial1` and services the PING inline |

This is the whole reason the firmware is structured the way it is. A PING can be
answered during a methane sweep, during an HX711 timeout, or while waiting for a
CO2 PWM edge. The cost is that `serviceMasterPing()` can itself block for 50 ms
while collecting a frame, which can push a CO2 edge measurement out of tolerance.
The `pingHandledDuringCo2` flag exists to keep that failure from discarding a
valid cached reading.

## 7. Known gaps

- **No RS485 direction control.** The program never touches a DE/RE pin. Per
  `../hardware.md` §5 the MAX485 modules in use are not bidirectional and manual
  direction control is required, with DE and RE tied together on the module. As
  written, the slave can only transmit if the wiring holds the driver enabled
  permanently, which will also echo its own bytes back. The former standalone
  loopback sketch was removed; verify direction control with the connected WT32
  master on the bench.
- **The Pi UI expects fields this firmware does not send.**
  `legacy/pi/water-control-system/templates/index.html` renders `ch4_ml` and
  `co2_ml` per bottle and a top-level `bottle_temp_avg`.
  None of those appear in the JSON here. The UI falls back to "Unavailable" for
  them, and computes the average from the four `temp` values itself.
- **No hardware verification of the USB stream.** The code builds and uploads,
  but no serial output has been captured from the board yet. The sensor wiring
  was unpowered during the last attempt.

## 8. Related documents

- `platformio-environments.md` — the `legacy-slave` environment.
- `../hardware.md` §4, §5 — Mega Mini pin map, sensor and MAX485 wiring.
- `../calibration.md` — the constants hard-coded in this file.
- `sensor-calibration-firmware.md` — the standalone programs that produced them.
- `esp-db-sync.md` §3 — the planned Modbus replacement for this program.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.

---
Content generated by `openai/gpt-6-codex` in `harness`.
