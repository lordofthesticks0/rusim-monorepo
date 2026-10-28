# Sensor calibration firmware

Scope: four small programs in `firmware/` that read sensors and produce values
for calibration. Each is a standalone PlatformIO environment with no shared
code between them.

| Directory | Environment | Board | Purpose |
|---|---|---|---|
| `co2-calibrate/` | `co2-calibrate` | ATmega2560 Mega Mini | Read the MH-Z19C PWM output |
| `pressure-calibrate/` | `pressure-calibrate` | ATmega328 Uno | Read the HX710B pressure sensor in Pa |
| `methane-calibrate/` | `methane-calibrate` | ATmega2560 Mega Mini | Read four TGS2611 sensors in ppm |
| `temp-probe/` | `temp-probe` | ATmega2560 Mega Mini | Read one DS18B20 on D22, with bus diagnostics |

None of these programs write to the database, accept commands, or talk to the
display. They print to serial and nothing else. Recorded results belong in
`docs/calibration.md`, which is maintained by hand.

## 1. Common structure

All four follow the same shape:

1. `setup()` starts `Serial` at the environment's `monitor_speed`, optionally
   prints a banner, and initialises the sensor.
2. `loop()` measures once, prints one line, and blocks briefly.

## 2. `co2-calibrate`

`firmware/co2-calibrate/main.cpp`. Reads the MH-Z19C's PWM output pin and
converts the pulse timing to ppm. `[env:co2-calibrate]` targets the Mega Mini
(`megaatmega2560`), moved from `nanoatmega328` on 2026-10-05, and the PWM pin
is D7.

| Constant | Value | Meaning |
|---|---|---|
| `kPwmPin` | 7 | MH-Z19C PWM output |
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

## 3. `pressure-calibrate`

`firmware/pressure-calibrate/main.cpp`. Reads an HX710B through the HX711 library
and prints pascals. `[env:pressure-calibrate]` targets the Uno (`uno`), not the
Mega Mini.

| Constant | Value |
|---|---|
| `HX711_DOUT` | 4 |
| `HX711_SCK` | 7 |
| `SENSORID` | 0 |
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

## 4. `methane-calibrate`

`firmware/methane-calibrate/main.cpp`. The most involved of the four. It reads
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

## 5. `temp-probe`

`firmware/temp-probe/main.cpp`. Reads one DS18B20 on D22 and prints the
temperature as plain text. It exists to test a single 1-Wire bus without the
slave firmware's JSON, RS485, or four-bottle loop.

| Constant | Value | Meaning |
|---|---|---|
| `TEMP_PIN` | 22 | Arduino D22 (ATmega `PA0`). Not `A0`, which is 54 |
| `SERIAL_BAUD` | 115200 | |
| `REPORT_INTERVAL_MS` | 1000 | Minimum gap between readings |
| `RESCAN_INTERVAL_MS` | 2000 | Re-scan period while no device answers |

`setup()` prints three diagnostics before the first reading:

1. The idle level of D22 with the ATmega's internal pull-up enabled. LOW means
   the line is shorted to GND or loaded by something like an LED circuit.
2. The result of a raw `OneWire::reset()`. `no` means no device asserted a
   presence pulse.
3. Every ROM code found by `OneWire::search()`, then the device count from
   `DallasTemperature::begin()`.

Those separate failures that all look like `-127` in the slave firmware: a
loaded line, a missing sensor or pull-up, and a device found by the search but
not by the library.

Output:

```
temp-probe: one DS18B20 on D22
expect VDD=5V, GND=GND, DQ=D22, 4.7k DQ to 5V
Idle D22 with internal pull-up: HIGH
OneWire reset presence: yes
Searching the bus:
  ROM #0: 28:ff:64:1e:...:42
ROM codes found: 1
DallasTemperature device count: 1
[1234 ms] temp = 27.812 C
```

While no device is present the program re-runs the diagnostics every 2 s, so a
sensor connected after boot appears without a reset. `read failed (-127.000 C)`
means the ROM search inside `getTempCByIndex()` failed; three consecutive
failures trigger another `begin()`.

The internal pull-up is a diagnostic only. It is 20-50 kΩ, and OneWire clears it
during every read slot, so it cannot replace the external 4.7 kΩ resistor.

## 8. Related documents

- `platformio-environments.md` — build environments for these programs and their
  known gaps.
- `../calibration.md` — human-recorded calibration values and dates.
- `../hardware.md` §5 — sensor pinouts and specifications.
- `../index.md` — the repository index and the rest of the docs.

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.

---
Content generated by `openai/gpt-6-codex` in `harness`.
