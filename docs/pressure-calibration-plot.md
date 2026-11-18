# Pressure calibration plot

Script that builds an interactive HTML chart of the pressure readings logged by
[`firmware/pressure-calibrate/`](../../firmware/pressure-calibrate/). The
firmware itself is described in
[sensor-calibration-firmware.md](sensor-calibration-firmware.md) §3.

## 1. Files

| File | Role |
|---|---|
| `firmware/pressure-calibrate/results.txt` | Input. Raw PlatformIO serial monitor log. |
| `firmware/pressure-calibrate/plot_results.py` | Script that reads the log and writes the HTML chart. |
| `firmware/pressure-calibrate/results_plot.html` | Output chart (generated). Open it in any browser. |

## 2. Run it

From `firmware/pressure-calibrate/`:

```bash
python plot_results.py
```

Optional arguments: `python plot_results.py <input.txt> <output.html>`.
Only the Python standard library is needed. The HTML file has no external
libraries or internet requirement, so it also works offline. Hover over the
chart to see the sample number, raw value and median of the nearest point.

## 3. How it works

1. **Parse.** `results.txt` is a copy of the serial monitor output. It contains
   the monitor header (lines starting with `---`), boot messages and the data.
   The script keeps only lines that match the regular expression
   `^Pressure:\s*(-?\d+(\.\d+)?)\s*Pa\s*$`, so everything else is ignored
   without having to be listed. Each match becomes one sample.
2. **Smooth.** A rolling median over 9 samples is computed. A median ignores
   single-sample spikes, which a plain average would spread over neighbouring
   points.
3. **Embed.** The raw list and the median list are written into the HTML
   template as JSON (`__RAW__`, `__MED__`), together with a summary line.
4. **Draw (in the browser).** JavaScript draws the axes, the raw samples
   (light blue) and the rolling median (red) on a `<canvas>`, plotted against
   the sample number. A dashed line marks 0 Pa. On `pointermove` the script
   converts the cursor X position to the nearest sample index, then draws a
   vertical guide, a dot on each line and a tooltip with that sample's values.
   The same code works for touch input.

The log has no timestamps, so the X axis is the sample number, not time.
The firmware averages 10 HX711 readings per printed line
(`scale.get_units(10)`), so the time per sample is set by the HX711 rate and
the firmware loop, not by this script.

## 4. Stable windows and their averages

The chart shows four pressure steps. After each rise the reading levels off
for a while. `PLATEAUS` in `plot_results.py` lists one stable window per step
(1-based sample numbers, inclusive). The windows were chosen by eye from the
rolling median: each starts after the rise has levelled off and ends before the
next rise. If the data changes, edit `PLATEAUS`.

The script computes, from the raw readings inside each window: mean, sample
standard deviation (SD), min, max and drift (least-squares slope, Pa per
sample). It prints them, shows them as a table under the chart, and draws each
window as a green band with a dashed mean line.

Result for the current `results.txt`:

| Class | Samples | n | Mean (Pa) | SD (Pa) | Drift (Pa/sample) |
|---|---|---|---|---|---|
| 1 | 131-170 | 40 | 895.1 | 34.7 | -1.12 |
| 2 | 196-228 | 33 | 2224.8 | 33.9 | -1.91 |
| 3 | 251-305 | 55 | 3412.7 | 16.3 | +0.61 |
| 4 | 321-425 | 105 | 3922.7 | 149.3 | -4.79 |

Class 4 is not flat: it falls from about 4175 Pa to about 3680 Pa over its
window, so its mean depends on the window chosen. Sub-ranges for comparison:

| Samples | n | Mean (Pa) | SD (Pa) | Drift (Pa/sample) |
|---|---|---|---|---|
| 321-350 | 30 | 4102.8 | 34.1 | -3.21 |
| 321-375 | 55 | 4043.0 | 80.8 | -4.72 |
| 376-425 | 50 | 3790.4 | 77.8 | -4.76 |

The log has no timestamps, so "stable" is judged by sample number only.

## 5. What the current log shows

Values read from the chart and the data summary, not interpreted:

- 491 samples, minimum -188 Pa, maximum 4199 Pa.
- The highest reading, 4199 Pa, is near sample 319 (hover to read it exactly).
- Samples 1 to about 90: readings stay close to 0 Pa, drifting between about
  -190 and +130 Pa.
- Samples about 95 to 440: readings rise in steps, with plateaus near 900,
  2200, 3400 and 4100 Pa.
- Sample 440 onwards: readings fall back to about 12 Pa.

## 6. Limits

- No timestamps in the log, so rates of change cannot be computed.
- Readings are in Pa as printed by the firmware. They depend on the
  `CALIBRATION_FACTORS` entry selected by `SENSORID` in `main.cpp`. The plot
  does not verify that calibration.
- The script parses only the `Pressure: <value> Pa` format. If the firmware
  print format changes, update `DATA_LINE` in `plot_results.py`.

---
Updated at 2026-11-20-15:36.
