# Sensor test and calibration firmware

Scope: the five small programs in `firmware/` whose only job is to prove a sensor
is wired up and to produce the numbers that go into a calibration. Each is a
standalone PlatformIO environment with no shared code between them.

| Directory | Environment | Board | Purpose |
|---|---|---|---|
| `blink-test/` | `blink-test` | ATmega2560 Mega Mini | Prove the board flashes and runs |
| `loadcell-test/` | `loadcell-test` | ATmega2560 Mega Mini | Read a load cell / HX711 raw counts |
| `co2-calibrate/` | `co2-calibrate` | ATmega328 Nano | Read the MH-Z19C PWM output |
| `pressure-calibrate/` | `pressure-calibrate` | ATmega2560 Mega Mini | Read the HX710B pressure sensor in Pa |
| `methane-calibrate/` | `methane-calibrate` | ATmega2560 Mega Mini | Read four TGS2611 sensors in ppm |

`firmware/loadcell-test/` had no PlatformIO environment until one was added; it
builds now. See `platformio-environments.md`.

None of these programs write to the database, accept commands, or talk to the
display. They print to serial and nothing else. Recorded results belong in
`docs/calibration.md`, which is maintained by hand.

## 1. Common structure

All five follow the same shape:

1. `setup()` starts `Serial` at the environment's `monitor_speed`, optionally
   prints a banner, and initialises the sensor.
2. `loop()` measures once, prints one line, and blocks briefly.

Three of them share an initialisation pattern worth knowing before changing them:
`pressure-calibrate` and `loadcell-test` both call

```cpp
scale.begin(HX711_DOUT, HX711_SCK, false, false);
```

with the library's default `reset` and `gain` arguments overridden to `false`.
The comment in both files explains why: the library's default reset path performs
a blocking read, so a missing or unpowered HX711 would hang before the program's
own readiness timeout could run. Passing `false` skips that path, and the program
polls `is_ready()` against its own timeout instead.

Both programs also re-run initialisation from `loop()` when the first attempt
failed, so an HX711 that is powered up after the microcontroller still comes
online without a reset.

## 2. `blink-test`

`firmware/blink-test/main.cpp`, 12 lines. Toggles `LED_BUILTIN` once per second.
There is no configuration. Its only diagnostic value is: if the LED blinks, the
board, the USB-serial link, and the upload path all work, and any later failure
belongs to the sensor code rather than to flashing.

Serial is not opened at all, so `pio device monitor` shows nothing. This is
expected.

## 3. `loadcell-test`

`firmware/loadcell-test/main.cpp`. Verifies that an HX711 is wired up and
responding, before any calibration factor is applied.

| Constant | Value | Note |
|---|---|---|
| `HX711_DOUT` | 13 | Data line from the HX711 |
| `HX711_SCK` | 12 | Clock line |
| `CALIBRATION_FACTOR` | `1.0f` | Keeps `get_units()` at raw, tare-corrected counts |
| `SERIAL_BAUD` | 115200 | |
| `HX711_READY_TIMEOUT_MS` | 5000 | Per readiness attempt |
| `HX711_READY_POLL_MS` | 10 | Poll interval |

Output is one line per reading:

```
Loadcell initialized.
Raw reading: 4821337
```

With `CALIBRATION_FACTOR = 1.0`, the value is the HX711's 24-bit raw count minus
the tare taken at initialisation, averaged over 10 reads. The raw range is
roughly ±8,388,607. Treat any reading close to zero as a disconnected or
short-circuited bridge, and a stuck reading near a rail as a wiring fault.

Note that this program uses pins D12/D13 while `pressure-calibrate` uses D2/D3.
They target different wiring, not the same HX711 breakout.

## 4. `co2-calibrate`

`firmware/co2-calibrate/main.cpp`. Reads the MH-Z19C's PWM output pin and
converts the pulse timing to ppm. Targets an ATmega328 Nano, so the PWM pin is
D2.

| Constant | Value | Meaning |
|---|---|---|
| `kPwmPin` | 2 | MH-Z19C PWM output |
| `kWaitTimeoutUs` | 2000000 | Longest wait for any single edge |
| `kPeriodMinUs` / `kPeriodMaxUs` | 950000 / 1060000 | Accepted pulse period window |
| `kHighMinUs` / `kHighMaxUs` | 1000 / 1002000 | Accepted high-time window |
| `kMeasureIntervalMs` | 2000 | One measurement every 2 s |
| `kPwmRangePpm` | 5000.0 | Full-scale value of the sensor's PWM output |

### How a reading is taken

`readCo2Ppm()` measures four edges with `micros()` and times between them. It
first waits for LOW, then for HIGH, timestamps that as `tRise`, waits for the
falling edge as `tFall`, then waits for the next rising edge as `tRise2`.

From those it derives `highUs = tFall - tRise`, `lowUs = tRise2 - tFall`, and
`periodUs = highUs + lowUs`. The measurement is rejected if `periodUs` falls
outside 950–1060 ms or `highUs` outside 1–1002 ms, or if any edge takes longer
than 2 s to arrive.

ppm is then

```
ppm = 5000 * (highMs - 2) / (periodMs - 4)
```

which is the MH-Z19C PWM transfer function: 2 ms of the high time and 4 ms of
the period are fixed overhead that must be removed before scaling.

Output:

```
MH-Z19C PWM CO2 reader started
CO2: 412 ppm
CO2: ERROR - no valid PWM signal
```

The error line is printed on every rejected measurement. A sensor that is
unpowered, wired to the wrong pin, or still inside its one-minute preheat will
produce this line continuously rather than a reading.

### Correctness limits

The MH-Z19C ranges from 400 to 5000 ppm, and `docs/hardware.md` records that
rumen headspace gas is roughly 70% CO2, which is far outside that range. This
program can only be used to prove the sensor works on ambient air. It cannot
measure the chamber. See `../hardware.md` §5.3.

The sensor also needs about a minute of preheat after power-up before its output
is meaningful.

## 5. `pressure-calibrate`

`firmware/pressure-calibrate/main.cpp`. Reads an HX710B through the HX711 library
and prints pascals. Targets the Mega Mini.

| Constant | Value |
|---|---|
| `HX711_DOUT` | 2 |
| `HX711_SCK` | 3 |
| `SENSORID` | 3 |
| `CALIBRATION_FACTORS` | `{581040.0, 70802.7743, 82656.645503906, 60223.70030581}` |
| `PA_PER_UNIT` | `100.0f` |
| `SERIAL_BAUD` | 115200 |

`SENSORID` selects which entry of `CALIBRATION_FACTORS` is used. It is checked
against the array bounds at initialisation, and the program refuses to start with
a message if it is out of range. Change this constant to move between load cells;
the other three entries are the factors already fitted for the other cells.

`PA_PER_UNIT` converts HX711 units to pascals: `pressurePa = get_units(10) * 100`.
With this value, a reading of 500 units is 50,000 Pa.

The comment above the constant records that it was changed from 2280, based on a
measured 50,000 Pa at a 981 Pa reference. The arithmetic behind that change is
not in the code, so the constant should be confirmed against a known pressure
before its output is trusted. Current recorded values are in `../calibration.md`
§3.

Output:

```
Pressure calibration starting...
Waiting for HX711...
Pressure sensor initialized.
Pressure (Pa):
Pressure: 981 Pa
```

`HX711 not ready; retrying...` means the bridge is powered but is not producing
conversions. Check the clock line, the supply, and the sensor's own excitation.

## 6. `methane-calibrate`

`firmware/methane-calibrate/main.cpp`. The most involved of the five. It reads
four TGS2611 sensors on the Mega Mini's analog inputs and converts each sensor's
resistance to ppm methane using a two-point power law.

### Wiring and constants

| Constant | Value |
|---|---|
| `SENSOR_PINS` | `A0, A1, A2, A3` |
| `RL` | `1000.0` for all four (load resistors in ohms) |
| `VCC` | `5` |
| `SERIAL_BAUD` | 115200 |

Each sensor is a resistive divider: the sensor element is `Rs`, the load resistor
is `RL`, both across `VCC`. `RL` must be edited if the hardware's resistors differ.
The comment in the source notes that `VCC` should be measured with a multimeter
rather than assumed, and that it feeds directly into every reading.

### Two calibration layers

**Layer 1, fitted per sensor from `Rs` measurements.** Each sensor has two
measured resistances: `RS_AIR`, measured in clean air, and `RS_SPAN`, measured
in span gas. `setup()` fits one exponent per sensor:

```
curve_exp[i] = log10(PPM_SPAN / PPM_AIR) / log10(RS_AIR[i] / RS_SPAN[i])
```

with `PPM_AIR = 2.0` (background methane) and `PPM_SPAN = 300.0`. At runtime:

```
rsToPpmRaw = PPM_AIR * (RS_AIR[i] / rs) ^ curve_exp[i]
```

The exponent is per sensor because sensor-to-sensor spread in `RS_AIR` is large;
the values in the source range from about 5370 to 8592 ohms. Note that the fit
needs `RS_AIR[i] > RS_SPAN[i]` for a positive exponent, which holds for all four
current entries.

**Layer 2, optional linear correction in ppm space.** `CORR_SLOPE` and
`CORR_OFFSET` apply `ppm = slope * raw + offset` after layer 1. Both are currently
the identity (`1.0` / `0.0`). The comment block in the source gives the procedure
for deriving them from two known concentrations:

1. Flash with the identity values and record `READ_air` in clean air and
   `READ_span` in span gas.
2. With `TRUE_air` and `TRUE_span` taken from the gas bottle labels,
   `SLOPE = (TRUE_span - TRUE_air) / (READ_span - READ_air)` and
   `OFFSET = TRUE_air - SLOPE * READ_air`.

This layer corrects span error and offset without needing `Rs` again, which is
useful because gas-bottle concentrations are known exactly while raw resistances
are not.

### Sampling

`readRs()` takes 20 `analogRead()` samples with a 2 ms gap between them and
averages them, then converts counts to volts and to sensor resistance:

```
voltage = raw_avg * (VCC / 1024.0)
rs = ((VCC / voltage) - 1.0) * RL
```

`rs` returns `-1.0` if the computed voltage is at or below 0.01 V, which happens
when the analog input is floating or shorted to ground. On the ATmega the ADC is
10-bit and returns at most 1023, so the divisor of 1024 makes `voltage` slightly
low and `rs` slightly high. The error is under 0.1% and does not affect
calibration, but it is not a correction for the ADC's real range.

`loop()` runs once per second and prints one JSON object:

```json
{"time_s":412,"rs_0":6159.8,"ppm_0":2.1,"rs_1":null,"ppm_1":null, ...}
```

- `time_s` is `millis() / 1000`, that is, uptime since boot. It is not a wall
  clock. A host logger has to stamp the actual time on receipt.
- `rs_<i>` and `ppm_<i>` are per sensor, indexed 0 to 3, one decimal place.
- A sensor with no reading is emitted as `null`, never as `0`. This keeps a dead
  sensor distinguishable from a sensor reading zero methane, which matters because
  `rsToPpm()` clamps small negative corrected values to `0`.
- There is no command parser. The program only writes. To capture a
  calibration session, log the serial output to a file from the host.

Current layer-1 and layer-2 values in the source were fitted by hand; see
`../calibration.md` §5 for the recorded procedure.

## 7. Related documents

- `platformio-environments.md` — build environments for these programs and their
  known gaps.
- `rs485-loopback-test.md` — the other standalone test program.
- `../calibration.md` — human-recorded calibration values and dates.
- `../hardware.md` §5 — sensor pinouts and specifications.
- `subsystem-index.md` — the rest of the repo.
- `../../firmware/max485-test/main.cpp` — the RS-485 loopback test.

---
Content generated by `opencode/space-bunny-free` in `opencode`.