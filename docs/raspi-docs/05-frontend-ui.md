# 5. Frontend UI — `templates/index.html`

Single-page kiosk dashboard rendered by Flask. The live copy uses a **CarPlay-style** layout: a left dock with clock + nav (Home/Controls/Bottle/Logs/Settings) + REC button, and a main viewport. The frontend polls `GET /status` every `500 ms` (2 Hz) while keeping live CSV log tailing throttled to every 3rd poll (`1.5 s`).

## Tabs

| Tab | Content |
|---|---|
| **Home (Water)** | Bath water temp card; avg bottle temp card (expandable per-bottle breakdown); system status card (`WATER LEVEL (A9)` with tap-to-expand ADC drawer, `HEATER (D5)`, `PUMP 1 (D3)`, `PUMP 2 (D7)`, `STIRRER (D4)`, and `EXHAUST FAN`; optional `PUMP LEAK (A8)` row and pills are hidden unless `leak_enabled` is true). |
| **Controls** | Per-actuator mode overrides (`AUTO` / `FORCE ON` / `OFF`) for `HEATER (D5)`, `PUMP 1 (D3)`, and `PUMP 2 (D7)`; temporary `0–255` MOSFET PWM speed controls labeled by pin (`Pin D3`, `Pin D7`, `Pin D4`) for water debit testing; stirrer schedule controller (`Pin D4`); bottle hysteresis target controls. |
| **Bottle** | 4-bottle grid: temp, pH, methane ppm + mL, CO2 ppm + mL, pressure. Filters `data.bottles` to valid bottle IDs `1..4`, updates card titles and metric rows regardless of active tab, and resets the grid to `"Waiting for Sensor Data..."` whenever `data.bottles` becomes empty (e.g., when the slave goes offline for `>= 12 s`). `fmtMetric(v, u, sentinel, maxLimit)` displays `"Over Limit"` when a sensor reports its sentinel (`-1` / `-999`) or reaches/exceeds its upper limit (`>= 10000` ppm for methane, `>= 5000` ppm for CO2), while omitted/non-numeric fields (`ch4_ml`, `co2_ml`) continue to display `"Unavailable"`. |
| **Logs (Data)** | List of CSV log files; live log inspector; LAN QR download and USB drive export. |
| **Settings** | Pi perf strip (CPU load, RAM, SoC temp, SD storage, 5 V rail); packet statistics strip; device info (SSID/IP); refresh + shutdown; timezone + manual date/time. |

## Expandable Water Level (`A9`) and Optional Pump Leak (`A8`) ADC Drawer

On the Home (`water`) tab, the right-hand system status card (`#waterStatusCard`) is clickable (`onclick="toggleWaterDetails()"`):
- `WATER LEVEL (A9)` displays `NORMAL`, `FULL`, `LOW`, or `CRITICAL LOW` using the master's debounced flags (`data.water_crit`, `data.water_low`, `data.water_full`), with `0.5 s` trip and `1.5 s` leaky-bucket recovery:
  - `CRITICAL LOW` reflects `data.water_crit` (tripped at `ADC <= out_stop_thr`, default `385`), when both the Heater and Pump 2 stop.
  - `LOW` reflects `data.water_low` (tripped at `ADC <= water_thr`, default `445`), when Pump 2 is throttled to `100` PWM.
  - `FULL` reflects `data.water_full` (tripped at `ADC >= in_stop_thr`, default `535`), when Pump 1 stops filling.
- Tapping `#waterStatusCard` toggles `.expanded`, revealing `.water-adc-drawer` with the bath sensor pills:
  - **BATH A9 ADC** (`#waterAdcVal`), **BATH VOLT** (`#waterVoltVal`), **BATH THR** (`#waterThrVal`, following `data.water_thr`), and **OUT PUMP STOP** (`#outPumpStopVal`, following `data.out_stop_thr`).
- Optional `PUMP LEAK (A8)` row (`#pumpLeakRow`) and its three drawer pills (`#leakAdcPill`, `#leakVoltPill`, `#leakThrPill`) are hidden (`display: none`) while `data.leak_enabled` is false (`ENABLE_LEAK_SENSOR == 0` in `master.cpp`).
- Actuator badges reflect both the active mode and the master's debounced interlocks:
  - When an actuator's mode (`heater_mode`, `pump1_mode`, `pump2_mode`, `stir_mode`) is `OFF`, its Home tab badge explicitly displays `OFF` rather than `IDLE` or a sensor stop label, while the Controls tab `ON` / `OFF` / `AUTO` buttons (`updateActuatorButtons`) continuously sync with every `/status` poll.
  - `HEATER` shows `STOPPED (LOW WATER)` when enabled and `data.water_crit` is true, and `STOPPED (LEAK)` on a leak trip.
  - `PUMP 1` shows `STOPPED (HIGH WATER)` in `AUTO` when `data.water_full` is true.
  - `PUMP 2` shows `STOPPED (CRITICAL LOW)` in `AUTO` when `data.water_crit` is true. See [legacy-master.md](../legacy-master.md#5-automatic-pump-interlocks).

## Temporary MOSFET Speed Control (`0–255` Debit Testing)

In the Controls (`#pageControls`) tab, the **MOSFET Speed Control (0–255 Debit Test)** card exposes per-channel PWM target ceiling controls explicitly labeled by their Arduino Mega Mini pin assignments:
- **`Pin D3` — Pump 1 MOSFET**: displays target PWM (`0–255` and `%`) and instantaneous slewed output (`Out: <out_d3>`), with a range slider (`0–255`) and `-25` / `-5` / `+5` / `+25` stepper buttons.
- **`Pin D7` — Pump 2 MOSFET**: displays target PWM (`0–255` and `%`) and instantaneous slewed output (`Out: <out_d7>`), with a range slider (`0–255`) and `-25` / `-5` / `+5` / `+25` stepper buttons.
- **`Pin D4` — Stirrer MOSFET**: displays target PWM (`0–255` and `%`) and instantaneous slewed output (`Out: <out_d4>`), with a range slider (`0–255`) and `-25` / `-5` / `+5` / `+25` stepper buttons.

Changing any slider or stepper button sends `POST /control` with `{"action": "mosfet_pwm", "pin": "D3"|"D7"|"D4", "pwm": 0..255}`, which transmits `PWM:<pin>:<0-255>` to the Master Arduino. Each MOSFET channel slews smoothly over 3 seconds (`3000 ms`) to/from its configured `0–255` target ceiling according to its operating mode.

## Dormant Clean User Template (`templates/Clean User Version.html`)

[`legacy/pi/water-control-system/templates/Clean User Version.html`](../../legacy/pi/water-control-system/templates/Clean%20User%20Version.html) stores the pre-developer 5-tab UI layout (based on commit `c4eb81d` plus the Settings centering fix from `65983a3`), kept dormant beside `templates/index.html`:

- **Inactive by default**: `web_control.py` renders only `index.html` on `/`; `Clean User Version.html` is not served unless `render_template('index.html')` is explicitly switched to it (or it replaces `index.html`).
- **Removed developer widgets**: Omits the `MOSFET Speed Control (0–255 Debit Test)` card, live `Out:` PWM counters, clickable `WATER LEVEL` ADC/Volt/Threshold drawer, dual Pump 1 / Pump 2 split, and hardware pin labels (`A9`, `A8`, `D5`, `D3`, `D7`, `D4`).
- **Preserved current backend compatibility**: Uses the unified `POST /control` `pump` action (which controls both Pump 1 and Pump 2 together in `web_control.py`), `500 ms` `/status` polling (1.5 s live log tail), server clock synchronization (`data.clock`), `water_crit` / `water_low` / `water_full` status mapping, `OFF` / safety stop actuator badges with continuous Controls button sync, Bottle ID `1..4` filtering and empty-grid reset, and `"Over Limit"` formatting for methane (`>= 10000` ppm) and CO2 (`>= 5000` ppm).

---
Updated at 2026-11-23-15:20.
