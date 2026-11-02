# Legacy Master Firmware (`legacy/master/master.cpp`)

## 1. Overview

[`legacy/master/master.cpp`](../../legacy/master/master.cpp) is the ATmega2560 Mega Mini firmware for the water-bath master controller (PlatformIO environment `[env:legacy-master]` in [`platformio.ini`](../../platformio.ini)). It manages:

- Non-blocking water-bath temperature acquisition via a DS18B20 sensor (`D2`).
- Polling the 4-bottle sensor slave (`#1:PING*` over `Serial1` at 9600 baud) and parsing the returned JSON telemetry to compute average bottle temperature.
- Time-gated, multi-sample analog water-level sensing on `A9`. The probe reads high when submerged (`~1023`) and low when dry; `waterDepleted` trips at `ADC <= 500` and throttles Pump 2. The heater uses the OUT pump's dry cutoff at `ADC <= 400` instead. Optional pump leak detection sensing on `A8` (`ADC >= 200`) is guarded by `#define ENABLE_LEAK_SENSOR 0` (disabled by default while only the `A9` water presence probe is installed, preventing floating-pin charge carryover on `A8` from triggering false leak cutoffs).
- Two automatic pump interlocks driven by the same `A9` reading: Pump 1 (`D3`, pumps into the reservoir) stops at overflow (`ADC >= 550`), Pump 2 (`D7`, pumps out of the reservoir) stops when dry (`ADC <= 400`). Both run inside the band between those two values. See section 5.
- Bath-temperature ceiling control of the 300 W heater SSR (`D5`), with per-actuator manual overrides (`AUTO`, `FORCE ON`, `OFF`). In AUTO, it heats while the bath sensor is valid and below `50 °C`; bottle telemetry and the bottle target do not control it.
- Unified non-blocking 3-second MOSFET PWM ramping (`slewMosfet`) across all 3 MOSFET channels (`Pin D3` Pump 1, `Pin D7` Pump 2, and `Pin D4` Stirrer), with individually adjustable target PWM ceilings (`0`–`255`) for water debit (flow rate) testing.
- Bidirectional USB serial (`Serial` at 115200 baud) command and status telemetry with the Raspberry Pi Flask backend ([`web_control.py`](../../legacy/pi/water-control-system/web_control.py)).

## 2. Pin Assignment

| Constant | Arduino Pin | ATmega2560 Port | Direction / Type | Function |
|---|---|---|---|---|
| `TEMP_SENSOR_PIN` | `D2` | `PE4` | 1-Wire Digital | DS18B20 water bath temperature sensor |
| `PUMP1_PIN` (`PUMP_PIN`) | `D3` | `PE5` (`OC3C`) | 8-bit PWM Output | Pump 1 MOSFET gate driver, pumps **into** the reservoir (Active-HIGH PWM, `0`–`255`, Timer 3 `OC3C`) |
| `STIRRER_PIN` | `D4` | `PG5` (`OC0B`) | 8-bit PWM Output | Stirrer MOSFET gate driver (Active-HIGH PWM, `0`–`255`, Timer 0 `OC0B`) |
| `HEATER_PIN` | `D5` | `PE3` | Digital Output | 300 W Heater SSR trigger (Active-HIGH) |
| `SYSTEM_SWITCH_PIN` | `D6` | `PH3` | `INPUT_PULLUP` | Physical `AUTO` / `OFF` switch (`LOW` forces `MODE_OFF`) |
| `PUMP2_PIN` | `D7` | `PH4` (`OC4B`) | 16-bit PWM Output | Pump 2 MOSFET gate driver, pumps **out of** the reservoir (Active-HIGH PWM, `0`–`255`, Timer 4 `OC4B`) |
| `WATER_SENSOR_PIN` | `A9` | `PK1` (`ADC9`) | Analog Input | Water bath level probe. ADC rises as the probe submerges: dry at or below `500`, `~1023` when fully submerged |
| `LEAK_SENSOR_PIN` | `A8` | `PK0` (`ADC8`) | Analog Input (Disabled by default) | Optional pump leak detection probe (`ADC >= 200` indicates leak; guarded by `ENABLE_LEAK_SENSOR = 0`) |
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

`setup()` calls `forceAllOff()` and drives `D3`, `D7`, `D4`, and `D5` `LOW` before the first loop iteration. `web_control.py` also sends `HEATER_OFF`, `PUMP1_OFF`, `PUMP2_OFF`, and `STIR:0:10:30` on its first serial connection (`BOOT_DEFAULT_COMMANDS`).

Two consequences of these defaults:

- `MODE_AUTO` alone does not start the pumps. It sets `currentMode` but leaves the three override variables at `2`, so the pumps stay in `FORCE OFF` until `PUMP_AUTO`, `PUMP1_AUTO`, `PUMP2_AUTO`, or the `*_ON` variants arrive.
- The `D6` switch is one-way. Only `digitalRead(SYSTEM_SWITCH_PIN) == LOW` is acted on (`currentMode = MODE_OFF`); releasing the switch does not restore `MODE_AUTO`. Returning to `AUTO` requires a serial `MODE_AUTO`.

## 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing

Analog water sensing runs every `WATER_CHECK_MS = 500UL` using `readAdcAveraged(uint8_t pin)`, which discards one initial `analogRead(pin)` to settle the AVR ADC multiplexer sample-and-hold capacitor and averages `WATER_ADC_SAMPLES = 16` consecutive readings.

### 4.1 Level thresholds

| Constant | Value | Meaning |
|---|---|---|
| `WATER_DRY_THRESHOLD` | `500` | `waterDepleted` trip and clear point; also reported as `water_thr` |
| `IN_PUMP_STOP_ADC` | `550` | Pump 1 (`D3`) stops at or above this reading (overflow) |
| `OUT_PUMP_STOP_ADC` | `400` | Pump 2 (`D7`) stops at or below this reading (dry) |

The probe voltage rises with the water level, so a high ADC means a full bath and a low ADC means an empty one.

### 4.2 `waterDepleted` debounce

- **Trip**: 5 consecutive `500 ms` checks at or below `WATER_DRY_THRESHOLD` (`2.5 s` total) set `waterDepleted = true`. A single reading above the threshold resets the counter to `0`.
- **Clear**: while depleted, each check above the threshold decrements `waterFaultCounter` by one; five such checks (`2.5 s`) set `waterDepleted = false`. A reading at or below the threshold reloads the counter to `WATER_FAULT_LIMIT`.
- **Boot**: `setup()` takes one averaged reading and sets `waterDepleted` immediately, without the `2.5 s` debounce, and preloads `waterFaultCounter` to match.

`waterDepleted` has one effect in the `MODE_AUTO` branch of `loop()`: Pump 2's target PWM is capped at `DRY_OUT_PUMP_PWM` (`100`) instead of the normal `AUTO_PUMP_PWM_LIMIT` (`200`). It does **not** call `forceAllOff()`, and it does **not** stop the heater. The heater shares the OUT pump's dry cutoff at `OUT_PUMP_STOP_ADC` (`400`) instead; see section 6.

### 4.3 Optional Pump Leak Detection Probe (`A8`)

- Controlled at compile time by `#define ENABLE_LEAK_SENSOR 0`.
- When `ENABLE_LEAK_SENSOR == 0` (default when only the single `A9` water presence sensor is connected), `A8` is not sampled (`leakRaw = -1`, `pumpLeakDetected = false`, `leakFaultCounter = 0`). Sampling an unconnected `A8` pin immediately after `A9` would otherwise retain sample-and-hold charge and report a false leak.
- When `ENABLE_LEAK_SENSOR == 1`, 5 consecutive `500 ms` checks at or above `LEAK_THRESHOLD` (`200`) set `pumpLeakDetected = true`, and 5 consecutive checks below `200` clear it.

## 5. Automatic Pump Interlocks

`loop()` derives two booleans from the latest averaged `waterRaw` on every pass:

```cpp
bool inletLevelOk  = (waterRaw < IN_PUMP_STOP_ADC);   // 550
bool outletLevelOk = (waterRaw > OUT_PUMP_STOP_ADC);  // 400
```

Pump 1 (`D3`, into the reservoir) and Pump 2 (`D7`, out of the reservoir) both use the level boolean alone. Temperature is not part of either pump decision. The heater's AUTO mode requires a valid DS18B20 reading, but does not depend on the bottle slave; circulation and heating continue independently of bottle telemetry.

AUTO behaviour with `currentMode == MODE_AUTO` and no leak:

| `waterRaw` | State | Pump 1 (`D3`, IN) | Pump 2 (`D7`, OUT) |
|---|---|---|---|
| `>= 550` | overflow | **off** | on, `200` PWM |
| `501`–`549` | in band | on, `200` PWM | on, `200` PWM |
| `401`–`500` | `waterDepleted`, heater still allowed | on, `200` PWM | on, `100` PWM (`DRY_OUT_PUMP_PWM`) |
| `<= 400` | dry | on, `200` PWM | **off** |

Notes on that table:

- The overflow and dry cut-offs are two separate constants, not one band with a single dry threshold. The heater and the OUT pump both stop at `400`; the `401`–`500` row is only flagged `water_low` for the UI and throttles the OUT pump to `100` PWM.
- Pump 1 keeps running while depleted. That is what refills the reservoir; the `550` interlock is the backstop that stops it before the bath overflows.
- The AUTO rows all apply `AUTO_PUMP_PWM_LIMIT = 200` as a ceiling, or `min(ch.maxPwm, 200, onPwm)` in `slewMosfet` terms. `PWM:<channel>:<value>` can lower `ch.maxPwm` below `200` but cannot raise the AUTO output above it.

Override interaction:

| Override | Pump 1 (`D3`) | Pump 2 (`D7`) |
|---|---|---|
| `0` AUTO | `inletLevelOk`, PWM `<= 200` | `outletLevelOk`, PWM `200` or `100` |
| `1` FORCE ON | `inletLevelOk`, PWM `<= 255` (`maxPwm`) | `outletLevelOk`, PWM `<= 255` (`maxPwm`) |
| `2` FORCE OFF | forced `0` | forced `0` |

`FORCE ON` bypasses the `200` PWM cap and the `100` dry throttle. It does **not** bypass the level interlocks: `FORCE ON` still stops Pump 1 at `>= 550` and Pump 2 at `<= 400`.

## 6. Mode and Safety Cutoffs

`loop()` evaluates one gate before any actuator work:

```cpp
if (currentMode == MODE_OFF || pumpLeakDetected) {
  forceAllOff();   // heater, both pumps, stirrer forced to 0
}
```

Heater logic when the system is not in `MODE_OFF` and no pump leak is detected:

1. `overTempCeiling` (`waterTemp >= MAX_WATER_BATH_TEMP`, `50.0 °C`), the water level being at or below `OUT_PUMP_STOP_ADC` (`400`), or `heaterOverride == 2` → heater off. These three win over every other case, including `HEATER_ON`.
2. `heaterOverride == 1` → heater on, subject to the block above.
3. `heaterOverride == 0` (AUTO) → off if the bath sensor is invalid; otherwise on while the bath is below the `50.0 °C` ceiling. There is no lower bath target: the heater does not read bottle temperature, `targetBottleTemp`, or `bottleHysteresis`.

`targetBottleTemp` (`20.0`–`50.0` °C) and `bottleHysteresis` (`0.1`–`5.0` °C) are still set by `TEMP:` and reported in status, but do not affect heater operation. AUTO fails off on an invalid bath sensor reading. The low-water, system-off, leak, and manual override gates still apply.

## 7. Unified 3-Channel MOSFET PWM Ramping and `0–255` Debit Control

All 3 MOSFET channels (`pump1Mosfet` on `D3`, `pump2Mosfet` on `D7`, and `stirMosfet` on `D4`) use the `MosfetChannel` struct and:

```cpp
void slewMosfet(MosfetChannel &ch, bool wantOn, unsigned long now,
                uint8_t pwmLimit = 255U, uint8_t onPwm = 255U);
```

- Target selection: `desiredPwm = ch.maxPwm`, then clamped by `pwmLimit`, then by `onPwm`; the target is `desiredPwm` when `wantOn`, else `0`.
- `STIR_RAMP_MS = 3000UL`: 3-second soft-start and soft-stop linear ramp between the current PWM and the target.
- Whenever the target changes, `slewMosfet` records `ch.rampOriginPwm = ch.currentPwm` and `ch.rampOriginMs = now`, then interpolates toward the target over `3000 ms`. A target change mid-ramp restarts the ramp from the instantaneous PWM.
- For the stirrer (`D4`), `slewMosfet` is used across all operating modes:
  - `stirMode == 0` (`OFF`): 3-second ramp down to `0`.
  - `stirMode == 1` (`CONSTANT ON`): 3-second ramp up to `stirMosfet.maxPwm`.
  - `stirMode == 2` (`PULSE INTERVAL`): 3-second ramp up at the start of each pulse window and 3-second ramp down before the pulse window ends.
  - `stirMode == 3` (`TIMED RUN`): 3-second ramp up, continuous hold at `stirMosfet.maxPwm`, and 3-second ramp down at the end of the timed duration, after which `stirMode` returns to `0`.
- For Pump 1 (`D3`) and Pump 2 (`D7`), `slewMosfet` ramps each pump over 3 seconds to the target described in section 5 and ramps down to `0` over 3 seconds when the interlock or override switches it off.

## 8. Raspberry Pi USB Serial Protocol

Commands are newline-terminated ASCII strings received on `Serial` (115200 baud):

- `STATUS`: Emits a `{"type":"master_status", ...}` JSON line containing:
  - `water_temp`, `bottle_temp_avg`, `target_temp`, `hysteresis`, `system_mode`
  - `heater`, `pump` (combined `pump1 || pump2`), `pump1`, `pump2`, `stirrer`
  - `heater_mode`, `pump_mode`, `pump1_mode`, `pump2_mode`, `stir_mode`, `stir_int_min`, `stir_pulse_sec`, `stir_timed_min`, `stir_rem_sec`
  - `pwm_d3`, `pwm_d7`, `pwm_d4` (configured `0`–`255` target PWM ceilings for `D3`, `D7`, `D4`)
  - `out_d3`, `out_d7`, `out_d4` (instantaneous `0`–`255` PWM output values for `D3`, `D7`, `D4`)
  - `water_low`, `water_adc`, `water_thr` (water level sensor on `A9`; `water_low` is `waterDepleted`, `water_thr` is `WATER_DRY_THRESHOLD = 500`)
  - `leak_enabled`, `pump_leak`, `leak_adc`, `leak_thr` (optional pump leak sensor on `A8`; `leak_enabled = 0`, `pump_leak = 0`, `leak_adc = -1` when disabled)
- `MODE_AUTO` / `MODE_OFF`: Switches master system mode. `MODE_AUTO` does not change any override, so the pumps and heater stay in `FORCE OFF` until the matching `*_AUTO` command arrives. `MODE_OFF` calls `forceAllOff()`.
- `HEATER_AUTO` / `HEATER_ON` / `HEATER_OFF`: Sets heater override mode (`0`, `1`, `2`). `HEATER_OFF` also drives the heater low immediately.
- `PUMP1_AUTO` / `PUMP1_ON` / `PUMP1_OFF`: Sets Pump 1 (`Pin D3`) override mode (`0`, `1`, `2`).
- `PUMP2_AUTO` / `PUMP2_ON` / `PUMP2_OFF`: Sets Pump 2 (`Pin D7`) override mode (`0`, `1`, `2`).
- `PUMP_AUTO` / `PUMP_ON` / `PUMP_OFF`: Sets both Pump 1 and Pump 2 override modes simultaneously.
- `PWM:<D3|D7|D4>:<0-255>`: Sets the target PWM ceiling (`0`–`255`) for `Pin D3` (Pump 1), `Pin D7` (Pump 2), or `Pin D4` (Stirrer).
- `STIR:<mode>:<mins>:<secs>`: Configures stirrer mode (`0`–`3`), interval/timed minutes, and pulse duration seconds. Also resets `stirCycleStart`.
- `TEMP:<target>:<hysteresis>`: Updates `targetBottleTemp` (`20.0`–`50.0` °C) and `bottleHysteresis` (`0.1`–`5.0` °C).

## 9. Known Gaps

- **No hysteresis on the pump or heater cutoffs.** `waterRaw < 550` and `waterRaw > 400` flip on a single ADC count. Because `slewMosfet` restarts its 3-second ramp on every target change, a reading hovering at a threshold keeps restarting the ramp and the pump never settles at its target PWM. Only `waterDepleted` (the `500` Pump 2 throttle) has debounce. The heater's `<= 400` cutoff is raw like the OUT pump, so it can toggle around that threshold.
- **AUTO heating has no lower bath target.** It heats continuously while the bath sensor is valid and below `50 °C`; only the bath ceiling, low-water/system/leak gates, and heater override can stop it. Bottle telemetry may be absent without changing this behavior.
- **The `D6` switch cannot restore `AUTO`.** See section 3.
- **The pump interlock thresholds are not telemetered.** `STATUS` reports only `water_thr = 500`; `IN_PUMP_STOP_ADC` and `OUT_PUMP_STOP_ADC` are visible only in the source.
- **`pump_mode` mirrors Pump 1 only.** Use `pump1_mode` and `pump2_mode`; `pump_mode` duplicates `pump1_mode`.

---
Content generated by `gpt-6-luna` in `harness`.
