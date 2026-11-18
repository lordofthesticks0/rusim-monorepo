# Methane calibration report generator

Script: `firmware/methane-calibrate/make_calibration_report.py`
Output: `firmware/methane-calibrate/report/`

This is an offline analysis tool. It does not run on the microcontroller. It reads
the serial log `results.txt` written by `firmware/methane-calibrate/main.cpp`
and produces charts and tables for the four TGS2611 methane sensors (sensor 0 to 3).
Firmware behaviour is described in `sensor-calibration-firmware.md` section 4.

## Run

```bash
python firmware/methane-calibrate/make_calibration_report.py
```

Requires Python 3 with `numpy` and `matplotlib`. Output is regenerated every run.

## Data: 5 points per sensor

| Point | Reference CH4 (ppm) | Status | Source |
|---|---|---|---|
| 1 | 1470 | Terukur (measured) | `results.txt` lines 2298-2309 (12 samples) |
| 2 | 1707 | Terukur (measured) | `results.txt` lines 2269-2281 (13 samples) |
| 3 | 3000 | Simulasi (simulated) | computed, see below |
| 4 | 4500 | Simulasi (simulated) | computed, see below |
| 5 | 6064 | Terukur (measured) | `results.txt` lines 2202-2209 (8 samples) |

The reference values 1470, 1707 and 6064 ppm were carried over from the earlier
calibration CSVs. Their original source (for example the reference gas meter) is
not recorded in the repository. Record it in the lab notes before reporting.

Measured points are the mean of the logged samples in each line range. `time_s`
restarts at every reboot, so the log is split into two sessions: A (lines 2093-2237)
and B (lines 2243-2411).

### How the two simulated points are made

They are not measurements. They exist to complete a 5-point table.

1. For each measured point, the relative error is `(sensor - reference) / reference`.
2. The relative error at 3000 and 4500 ppm is linearly interpolated between the
   three measured points (`numpy.interp`).
3. Simulated sensor reading = `reference x (1 + interpolated error)`.
4. Rs is back-calculated with the firmware model
   `ppm = PPM_AIR x (RS_AIR / Rs)^n`, solved for Rs.

The result is deterministic (no random numbers). Every table row and chart marker
for these points is labelled `Simulasi` or drawn hollow.

## Formulas

| Quantity | Formula |
|---|---|
| Ro | `RS_AIR` from `main.cpp` (Rs in clean air, about 4 ppm) |
| Voltage (V) | `VCC x RL / (Rs + RL)` with `VCC = 5 V`, `RL = 1000 ohm` |
| Rs/Ro | measured or simulated Rs divided by Ro |
| Calibration curve | `ppm = a x (Rs/Ro)^b`, fitted by linear regression of `ln(ppm)` on `ln(Rs/Ro)` |
| R2 (calibration) | computed in log-log space, the same way as a spreadsheet power trendline |
| Error (ppm) | sensor - reference |
| Error (%) | error / reference x 100 |

The constants `RS_AIR`, `RS_SPAN`, `PPM_AIR`, `PPM_SPAN`, `VCC` and `RL` are copied
from `main.cpp` at the top of the script. Update both if the firmware changes.

## Output files

`report/` holds:

| File | Content |
|---|---|
| `tabel_kalibrasi.csv` | Sensor, point, status, CH4 (ppm), voltage (V), Rs (ohm), Ro (ohm), Rs/Ro |
| `tabel_pengujian.csv` | Sensor, point, status, reference (ppm), sensor (ppm), error (ppm), error (%), SD, sample count |
| `ringkasan_regresi.csv` | Equation, a, b, R2 (5 points and 3 measured points), mean absolute % error |
| `sensor_N/1_kalibrasi.png` | CH4 (ppm) versus Rs/Ro, scatter, regression curve, equation, R2 |
| `sensor_N/2_respons_waktu.png` | Rs (ohm) and output voltage versus time, with the averaging windows shaded |
| `sensor_N/3_akurasi.png` | Sensor versus reference, ideal line `y = x`, linear fit, error labels |
| `sensor_N/4_repeatability.png` | Sensor reading at each repetition, per concentration |

CSV files use UTF-8 with a byte-order mark and comma separators with a dot as the
decimal point. In a spreadsheet set to a locale that uses a comma decimal (for
example Indonesian), use the import dialog and choose comma as the delimiter.

## Limits

- The response chart and the repeatability chart use measured data only. The
  simulated points have no time series or repeated readings, so they are not drawn
  there.
- A repetition is one consecutive 1-second reading inside a measurement window, not
  an independent re-run of the experiment. Readings are correlated in time, so the
  standard deviation understates true run-to-run variation.
- Sensor 1 and sensor 3 have large errors at the extremes (up to about 38% and 42%).
  Their power-law exponents differ the most from the others, and the 6064 ppm window
  of sensor 1 is still drifting (SD 722 ppm).
- R2 of the 5-point fit is partly built on simulated points, so it is not an
  independent measure of sensor quality. The summary CSV also gives R2 for the
  3 measured points.

---
Updated at 2026-11-20-15:36.
