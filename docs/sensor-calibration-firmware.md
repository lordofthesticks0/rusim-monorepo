# Sensor calibration firmware

Scope: five small programs in `firmware/` that read sensors and produce values
for calibration. Each is a standalone PlatformIO environment with no shared
code between them.

| Directory | Environment | Board | Purpose |
|---|---|---|---|
| `co2-calibrate/` | `co2-calibrate` | ATmega2560 Mega Mini | Read the MH-Z19C PWM output |
| `pressure-calibrate/` | `pressure-calibrate` | ATmega328 Uno | Read the HX710B pressure sensor in Pa |
| `methane-calibrate/` | `methane-calibrate` | ATmega2560 Mega Mini | Read four TGS2611 sensors in ppm |
| `temp-probe/` | `temp-probe` | ATmega2560 Mega Mini | Read one DS18B20 on D22, with bus diagnostics |
| `ph-calibrate/` | `ph-calibrate` | ATmega2560 Mega Mini | Read four PH-402C probes and print volts and pH |

None of these programs write to the database, accept commands, or talk to the
display. They print to serial and nothing else. Recorded results belong in
`docs/calibration.md`, which is maintained by hand.

## 1. Common structure

All four follow the same shape:

1. `setup()` starts `Serial` at the environment's `monitor_speed`, optionally
   prints a banner, and initialises the sensor.
2. `loop()` measures once, prints one line, and blocks briefly.

## 2. `co2-calibrate`

`firmware/co2-calibrate/main.cpp`. Reads four MH-Z19C sensors and prints one
JSON line per sweep. `[env:co2-calibrate]` targets the Mega Mini
(`megaatmega2560`), moved from `nanoatmega328` on 2026-10-05.

| Constant | Value | Meaning |
|---|---|---|
| `NUM_SENSORS` | 4 | Sensors read per sweep |
| `SENSOR_PINS` | 6, 3, 4, 7 | PWM output per sensor (D6, D3, D4, D7) |
| `kWaitTimeoutUs` | 2000000 | Longest wait for any single edge |
| `kPeriodMinUs` / `kPeriodMaxUs` | 950000 / 1060000 | Accepted pulse period window |
| `kHighMinUs` / `kHighMaxUs` | 1000 / 1002000 | Accepted high-time window |
| `kSweepDelayMs` | 1000 | Pause after one full four-sensor sweep |
| `kUartTimeoutMs` | 250 | Read window for the UART reply |
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

Output, one JSON object per sweep:

```
MH-Z19C 4-sensor PWM CO2 logger started
{"time_s":42,"raw_0":412.0,"ppm_0":412.0,"raw_1":null,"ppm_1":null,"raw_2":null,"ppm_2":null,"raw_3":null,"ppm_3":null,"uart_0":405}
```

- `raw_<i>` is the PWM-derived ppm for sensor `i`, in `SENSOR_PINS` order;
  `ppm_<i>` is the same value after the per-sensor linear correction below.
- A rejected or absent pulse prints `null`, never `0`, so a dead sensor stays
  distinguishable from one reading zero.
- `uart_0` is one reading taken over UART (`readCo2Uart()`): 9600 8N1 on
  `Serial1` (RX D19, TX D18), request `FF 01 86 00 00 00 00 00 79`, reply
  `FF 86 <ppmHi> <ppmLo> ...` plus a checksum byte. Only one sensor may share
  that bus, because they use the same address; the PWM path covers the rest.
- The four PWM sensors are read one after another, a full ~1 s pulse period
  each, so a sweep takes roughly 4-5 s plus `kSweepDelayMs`.

A sensor that is unpowered, wired to the wrong pin, or still inside its
one-minute preheat produces `null` for its slot on every sweep.

### Field correction

`CORR_SLOPE` / `CORR_OFFSET` apply `ppm = slope * raw + offset` per sensor after
the PWM transfer function. Both are the identity (`1.0` / `0.0`) until a
two-point capture exists; the source comment gives the procedure for fitting
them in ppm space.

### Correctness limits

The MH-Z19C ranges from 400 to 5000 ppm, and `docs/hardware.md` records that
rumen headspace gas is roughly 70% CO2, which is far outside that range. This
program can only be used to prove the sensor works on ambient air. It cannot
measure the chamber. See `hardware.md` §5.3.

The sensor also needs about a minute of preheat after power-up before its output
is meaningful.

### Plotting a serial capture

`firmware/co2-calibrate/plot_co2.py` builds a standalone HTML chart from a JSONL
serial capture such as `firmware/co2-calibrate/co2.txt`:

```bash
python firmware/co2-calibrate/plot_co2.py
```

The default output is `firmware/co2-calibrate/co2-fluctuation.html`. Open it in
a browser; hover or move a touch pointer over the chart to read the ppm values
at each sample. The chart plots only `pins[].ppm`, labeled by the pin recorded
in each row, against `uptime_ms`. Its min, mean and max summaries use available
ppm values; null readings remain gaps and are not treated as zero. The HTML has
no external dependencies. This reader supports the older `pins[]` log shape in
`co2.txt`; the current `main.cpp` emits `ppm_0` through `ppm_3` instead, so do
not treat the old capture as output from the current firmware.

## 3. `pressure-calibrate`

`firmware/pressure-calibrate/main.cpp`. Reads an HX710B through the HX711 library
and prints pascals. `[env:pressure-calibrate]` targets the Uno (`uno`), not the
Mega Mini.

| Constant | Value |
|---|---|
| `HX711_DOUT` | 4 |
| `HX711_SCK` | 7 |
| `SENSORID` | 0 |
| `CALIBRATION_FACTORS` | `{75200.6, 70802.7743, 82656.645503906, 60223.70030581}` |
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
before its output is trusted. Current recorded values are in `calibration.md`
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
curve_exp[i] = log10(PPM_SPAN[i] / PPM_AIR) / log10(RS_AIR[i] / RS_SPAN[i])
```

with `PPM_AIR = 4.0` (assumed clean-air background methane) and a separate
reference concentration `PPM_SPAN[i]` for each sensor (currently 5000 ppm for
all four). At runtime:

```
rsToPpmRaw = PPM_AIR * (RS_AIR[i] / rs) ^ curve_exp[i]
```

The exponent is per sensor because sensor-to-sensor spread in `RS_AIR` is large.
The fit needs `RS_AIR[i] > RS_SPAN[i]` for a positive exponent, which holds for
all four current entries.

### Reference-data refit

The current logger constants come from one `results.txt` capture: the near-zero
reading at `time_s 34` supplies `RS_AIR`, and the 5000 ppm methane reference at
`time_s 906` supplies `RS_SPAN`. There is one resistance pair per sensor.

| Sensor | `RS_AIR` (Ω) | `RS_SPAN` (Ω) | `PPM_SPAN` (ppm) | Fitted exponent |
|---|---|---|---|---|
| 0 | 914.2 | 99.2 | 5000 | 3.210798 |
| 1 | 1309.9 | 402.5 | 5000 | 6.043078 |
| 2 | 1540.9 | 175.2 | 5000 | 3.279790 |
| 3 | 1723.4 | 152.2 | 5000 | 2.938324 |

The exponents are fitted at boot from those pairs, not stored as rounded
constants. `CORR_SLOPE` and `CORR_OFFSET` remain identity values.

Assumptions and limits:

- Clean air is assumed to be 4 ppm methane; this concentration was not measured
  with a reference instrument.
- The span concentration is treated as 5000 ppm methane, as labelled on the
  reference gas. It is a single reading per sensor; the data do not establish
  reference-gas or reference-meter accuracy.
- The sensor order, load resistors, supply conditions and warmed-up state must
  match the calibration setup. This logger uses A0–A3.
- Two points set the exponent, so accuracy between and beyond them is an
  assumption of the power law, not a measurement.
- Each point is a single reading (`time_s 34` and `time_s 906` in the capture).
  Longer captures and a measured air reference would improve confidence.

These constants update `methane-calibrate` only. The separate legacy slave
firmware has its own calibration values and is not updated by this change.

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

The layer-1 refit above replaces the logger's old 300 ppm baseline curve. The
slave's previous span resistances already matched these sample means, but its
shared 442 ppm concentration is now replaced by the four reference values.
`calibration.md` §5 contains historical human-recorded notes, not this refit.

### Local validation

Run `python3 firmware/tests/test_methane_calibration.py` from the repository root
for native C++ regression checks of the actual calibration code, then build:

```sh
pio run -e methane-calibrate -e legacy-slave
```

These are local checks only. No firmware is uploaded and no Pi files are deployed.
After flashing with explicit approval, verify each sensor in clean air and known
methane concentrations before relying on absolute ppm values.

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

## 6. `ph-calibrate`

`firmware/ph-calibrate/main.cpp`. Reads four PH-402C / PH-4502C probe modules on
the Mega Mini's A0-A3 and prints one JSON line per second. Each probe is reported
as an averaged ADC count, the corresponding voltage, and a pH value.

| Constant | Value | Meaning |
|---|---|---|
| `NUM_PROBES` | 4 | Probes read per sweep |
| `PROBE_PINS` | `A0, A1, A2, A3` | Analog input per probe, in index order |
| `ADC_SAMPLES` | 16 | `analogRead()` samples averaged per reading |
| `ADC_SAMPLE_GAP_MS` | 1 | Gap between samples |
| `SWEEP_INTERVAL_MS` | 1000 | Pause after one full four-probe sweep |
| `ADC_REF_V` | 5.0 | ADC reference, the Mega's 5 V rail |
| `ADC_MAX_COUNT` | 1023 | Full-scale 10-bit count |
| `PH_ANCHOR_COUNT` | 512 | ADC count that reads pH 7.00 (2.500 V) |
| `PH_ANCHOR_VALUE` | 7.0 | pH at the anchor count |
| `PH_PER_COUNT` | 14 / 1023 | pH change per count; the 0-5 V span covers 0-14 pH |
| `PH_SIGN` | -1 | `-1`: higher voltage gives lower pH (measured polarity); `+1` inverts |

### Wiring

Per probe: module `VCC` to 5 V, module `GND` to GND, module `PO` to the matching
analog pin. The BNC shield goes to the module's dedicated probe ground. The
module's `DO` comparator output and `TO` temperature output are not read.

### Linear conversion (temporary)

The map is linear and shared by all four probes. It is a temporary assumption,
not a fit to measured data. The three-point piecewise fit used by
`convertPhVoltage()` in `legacy/slave/new-slave.cpp` was replaced by this map.
The old table is kept as a comment in the source for reference.

```
ph = 7.00 + PH_SIGN * (count - 512) * (14 / 1023)
```

- The anchor is pH 7.00 at 512 counts (2.500 V), which is the midpoint of the
  0-5 V output.
- The slope covers 14 pH over the full 1023-count span. This follows
  `hardware.md` §5.5, which gives the PO output as 0-5 V for pH 0-14.
- `PH_SIGN` is `-1`, so a higher voltage gives a lower pH. That is the polarity
  of the earlier measured table. Set it to `+1` if a probe reads the other way.
- The result is clamped to 0-14 and rounded to two decimals.

The slope is not measured. A real probe may not be linear across 0-14 pH, and
the module's output span may differ from the datasheet. Check the anchor first:
a pH 7 buffer should give a count near 512. Re-check in pH 4.01 and 9.18 buffers
before trusting the other readings.

A reading is rejected when the count is 0 or at/above 1023 (0 V or the 5 V rail),
or when the result is not finite. This catches a floating input and a shorted
input instead of printing a plausible-looking number.

### Sampling and output

`readAverageCount()` sums 16 `analogRead()` samples with a 1 ms gap and returns
the mean count. Voltage is `count * (5.0 / 1023.0)`, the same scale the legacy
slave uses.

```json
{"time_s":12,"raw_0":812,"v_0":3.969,"ph_0":2.89,"raw_1":801,"v_1":3.915,"ph_1":3.04,"raw_2":0,"v_2":null,"ph_2":null,"raw_3":795,"v_3":3.886,"ph_3":3.13}
```

- `raw_<i>` is the averaged count, printed with no decimals.
- `v_<i>` is volts to three decimals, or `null` when rejected.
- `ph_<i>` is pH to two decimals, or `null` when rejected.
- `time_s` is `millis() / 1000`, that is, uptime since boot, not a wall clock.
- `raw_<i>` is always printed, even for a rejected reading, so a dead channel is
  still visible. `0` raw means the input is at ground; `1023` means it is at the
  5 V rail.

There is no command parser and no persistence. To capture a calibration session,
log the serial output to a file on the host, exactly as for `methane-calibrate`.

### Correctness limits

The probe and module are specified to ±0.1 pH at 25 °C. `calibration.md` §4
records roughly ±0.3 pH on the legacy rig, so treat the two-decimal output as
resolution, not accuracy. The conversion does not apply temperature compensation;
`TO` is ignored, so readings drift with temperature. The module's output range
and the ADC share the 5 V rail, so a noisy or low 5 V supply moves every reading.

## 7. Related documents

- `platformio-environments.md` — build environments for these programs and their
  known gaps.
- `calibration.md` — human-recorded calibration values and dates.
- `hardware.md` §5 — sensor pinouts and specifications.
- `index.md` — the repository index and the rest of the docs.

---
Updated at 2026-11-20-15:36.
