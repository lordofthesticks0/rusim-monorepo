# Legacy Master Firmware (`legacy/master/master.cpp`)

## 1. Overview

[`legacy/master/master.cpp`](../../legacy/master/master.cpp) is the ATmega2560 Mega Mini firmware for the water-bath master controller (PlatformIO environment `[env:legacy-master]` in [`platformio.ini`](../../platformio.ini)). It manages:

- Non-blocking water-bath temperature acquisition via a DS18B20 sensor (`D2`).
- Polling the 4-bottle sensor slave (`#1:PING*` over `Serial1` at 9600 baud) and parsing the returned JSON telemetry to compute average bottle temperature.
- Time-gated, multi-sample analog water-level sensing on `A7` (dry-run protection when `ADC < 200`) and pump leak detection sensing on `A8` (flood/leak protection when `ADC >= 200`), both triggering immediate cutoff across all actuators.
- Hysteresis control of the 300 W heater SSR (`D5`) and two circulation pump MOSFET channels (`D3` for Pump 1 and `D7` for Pump 2), with per-actuator manual overrides (`AUTO`, `FORCE ON`, `OFF`).
- Unified non-blocking 3-second MOSFET PWM ramping (`slewMosfet`) across all 3 MOSFET channels (`Pin D3` Pump 1, `Pin D7` Pump 2, and `Pin D4` Stirrer), with individually adjustable target PWM ceilings (`0`–`255`) for water debit (flow rate) testing.
- Bidirectional USB serial (`Serial` at 115200 baud) command and status telemetry with the Raspberry Pi Flask backend ([`web_control.py`](../../legacy/pi/water-control-system/web_control.py)).

## 2. Pin Assignment

| Constant | Arduino Pin | ATmega2560 Port | Direction / Type | Function |
|---|---|---|---|---|
| `TEMP_SENSOR_PIN` | `D2` | `PE4` | 1-Wire Digital | DS18B20 water bath temperature sensor |
| `PUMP1_PIN` (`PUMP_PIN`) | `D3` | `PE5` (`OC3C`) | 8-bit PWM Output | Circulation Pump 1 MOSFET gate driver (Active-HIGH PWM, `0`–`255`, Timer 3 `OC3C`) |
| `STIRRER_PIN` | `D4` | `PG5` (`OC0B`) | 8-bit PWM Output | Stirrer MOSFET gate driver (Active-HIGH PWM, `0`–`255`, Timer 0 `OC0B`) |
| `HEATER_PIN` | `D5` | `PE3` | Digital Output | 300 W Heater SSR trigger (Active-HIGH) |
| `SYSTEM_SWITCH_PIN` | `D6` | `PH3` | `INPUT_PULLUP` | Physical `AUTO` / `OFF` safety switch (`LOW` forces `MODE_OFF`) |
| `PUMP2_PIN` | `D7` | `PH4` (`OC4B`) | 16-bit PWM Output | Circulation Pump 2 MOSFET gate driver (Active-HIGH PWM, `0`–`255`, Timer 4 `OC4B`) |
| `WATER_SENSOR_PIN` | `A7` | `PF7` (`ADC7`) | Analog Input | Water bath level sensor probe (`ADC < 200` indicates dry bath) |
| `LEAK_SENSOR_PIN` | `A8` | `PK0` (`ADC8`) | Analog Input | Pump leak detection sensor probe (`ADC >= 200` indicates water leak) |
| `Serial` | `D0`/`D1` | `PE0`/`PE1` | UART0 (115200) | USB serial link to Raspberry Pi |
| `Serial1` | `D19`/`D18` | `PD2`/`PD3` | UART1 (9600) | Direct UART link to Slave Mega (`#1`) |

## 3. Boot Defaults

The firmware boots with all actuators OFF and all MOSFET target PWM ceilings set to `255`:

| Variable | Boot value | Meaning |
|---|---|---|
| `currentMode` | `MODE_AUTO` | System mode; the physical `D6` switch can pull it to `MODE_OFF`. |
| `heaterOverride` | `2` | `FORCE OFF`. Heater remains off until `HEATER_AUTO` or `HEATER_ON`. |
| `pump1Override` | `2` | `FORCE OFF`. Pump 1 (`Pin D3`) remains off until `PUMP1_AUTO`, `PUMP1_ON`, `PUMP_AUTO`, or `PUMP_ON`. |
| `pump2Override` | `2` | `FORCE OFF`. Pump 2 (`Pin D7`) remains off until `PUMP2_AUTO`, `PUMP2_ON`, `PUMP_AUTO`, or `PUMP_ON`. |
| `stirMode` | `0` | `OFF`. |
| `pump1Mosfet.maxPwm` | `255` | Target PWM ceiling (`0`–`255`) for `Pin D3`. |
| `pump2Mosfet.maxPwm` | `255` | Target PWM ceiling (`0`–`255`) for `Pin D7`. |
| `stirMosfet.maxPwm` | `255` | Target PWM ceiling (`0`–`255`) for `Pin D4`. |

`setup()` calls `forceAllOff()` and drives `D3`, `D7`, `D4`, and `D5` `LOW` before the first loop iteration. `web_control.py` also sends `HEATER_OFF`, `PUMP1_OFF`, `PUMP2_OFF`, and `STIR:0:10:30` on its first serial connection.

## 4. Water-Level (`A7`) and Pump Leak (`A8`) Sensing

Both analog water sensors are sampled every `WATER_CHECK_MS = 500UL` using `readAdcAveraged(uint8_t pin)`, which discards one initial `analogRead(pin)` to settle the AVR ADC multiplexer sample-and-hold capacitor and averages `WATER_ADC_SAMPLES = 16` consecutive readings.

1. **Water Bath Level Probe (`WATER_SENSOR_PIN = A7`)**:
   - Normally submerged (`ADC >= WATER_THRESHOLD`, `200`).
   - **Dry-run fault**: 5 consecutive `500 ms` checks below `200` (`2.5 s` total) set `waterDepleted = true`.
   - **Recovery**: 5 consecutive `500 ms` checks at or above `200` clear `waterDepleted = false`.
2. **Pump Leak Detection Probe (`LEAK_SENSOR_PIN = A8`)**:
   - Normally dry (`ADC < LEAK_THRESHOLD`, `200`).
   - **Leak fault**: 5 consecutive `500 ms` checks at or above `200` (`2.5 s` total) set `pumpLeakDetected = true`.
   - **Recovery**: 5 consecutive `500 ms` checks below `200` clear `pumpLeakDetected = false`.
3. **Automatic Cutoff**:
   - Whenever `currentMode == MODE_OFF`, `waterDepleted == true`, or `pumpLeakDetected == true`, `forceAllOff()` is called every loop pass, immediately cutting the heater (`D5`), Pump 1 (`D3`), Pump 2 (`D7`), and Stirrer (`D4`) to `0`.

## 5. Unified 3-Channel MOSFET PWM Ramping and `0–255` Debit Control

All 3 MOSFET channels (`pump1Mosfet` on `D3`, `pump2Mosfet` on `D7`, and `stirMosfet` on `D4`) use the `MosfetChannel` struct and `slewMosfet(MosfetChannel &ch, bool wantOn, unsigned long now)`:

- `STIR_RAMP_MS = 3000UL`: 3-second soft-start and soft-stop linear ramp between `0` and `ch.maxPwm` (`0`–`255`).
- Whenever `wantOn` or `ch.maxPwm` changes, `slewMosfet` records `ch.rampOriginPwm = ch.currentPwm` and `ch.rampOriginMs = now`, then interpolates smoothly toward `desiredTarget` (`ch.maxPwm` when on, `0` when off) over `3000 ms`.
- For the stirrer (`D4`), `slewMosfet` is used across all operating modes:
  - `stirMode == 0` (`OFF`): 3-second ramp down to `0`.
  - `stirMode == 1` (`CONSTANT ON`): 3-second ramp up to `stirMosfet.maxPwm`.
  - `stirMode == 2` (`PULSE INTERVAL`): 3-second ramp up at the start of each pulse window and 3-second ramp down before the pulse window ends.
  - `stirMode == 3` (`TIMED RUN`): 3-second ramp up, continuous hold at `stirMosfet.maxPwm`, and 3-second ramp down at the end of the timed duration.
- For Pump 1 (`D3`) and Pump 2 (`D7`), `slewMosfet` ramps each pump over 3 seconds to `pump1Mosfet.maxPwm` / `pump2Mosfet.maxPwm` when active (`FORCE ON`, or `AUTO` while bath and slave sensors are healthy) and ramps down to `0` over 3 seconds when switched to `OFF`.

## 6. Raspberry Pi USB Serial Protocol

Commands are newline-terminated ASCII strings received on `Serial` (115200 baud):

- `STATUS`: Emits a `{"type":"master_status", ...}` JSON line containing:
  - `water_temp`, `bottle_temp_avg`, `target_temp`, `hysteresis`, `system_mode`
  - `heater`, `pump` (combined `pump1 || pump2`), `pump1`, `pump2`, `stirrer`
  - `heater_mode`, `pump_mode`, `pump1_mode`, `pump2_mode`, `stir_mode`, `stir_int_min`, `stir_pulse_sec`, `stir_timed_min`, `stir_rem_sec`
  - `pwm_d3`, `pwm_d7`, `pwm_d4` (configured `0`–`255` target PWM ceilings for `D3`, `D7`, `D4`)
  - `out_d3`, `out_d7`, `out_d4` (instantaneous `0`–`255` PWM output values for `D3`, `D7`, `D4`)
  - `water_low`, `water_adc`, `water_thr` (Bath water level sensor on `A7`)
  - `pump_leak`, `leak_adc`, `leak_thr` (Pump leak sensor on `A8`)
- `MODE_AUTO` / `MODE_OFF`: Switches master system mode.
- `HEATER_AUTO` / `HEATER_ON` / `HEATER_OFF`: Sets heater override mode (`0`, `1`, `2`).
- `PUMP1_AUTO` / `PUMP1_ON` / `PUMP1_OFF`: Sets Pump 1 (`Pin D3`) override mode (`0`, `1`, `2`).
- `PUMP2_AUTO` / `PUMP2_ON` / `PUMP2_OFF`: Sets Pump 2 (`Pin D7`) override mode (`0`, `1`, `2`).
- `PUMP_AUTO` / `PUMP_ON` / `PUMP_OFF`: Sets both Pump 1 and Pump 2 override modes simultaneously.
- `PWM:<D3|D7|D4>:<0-255>`: Sets the target PWM ceiling (`0`–`255`) for `Pin D3` (Pump 1), `Pin D7` (Pump 2), or `Pin D4` (Stirrer).
- `STIR:<mode>:<mins>:<secs>`: Configures stirrer mode (`0`–`3`), interval/timed minutes, and pulse duration seconds.
- `TEMP:<target>:<hysteresis>`: Updates `targetBottleTemp` (`20.0`–`50.0` °C) and `bottleHysteresis` (`0.1`–`5.0` °C).

---
Content generated by `google/gemini-4-argon` in `antigravity`.
