# Legacy Master Firmware (`legacy/master/master.cpp`)

## 1. Overview

[`legacy/master/master.cpp`](../../legacy/master/master.cpp) is the ATmega2560 Mega Mini firmware for the water-bath master controller (PlatformIO environment `[env:legacy-master]` in [`platformio.ini`](../../platformio.ini)). It manages:

- Non-blocking water-bath temperature acquisition via a DS18B20 sensor (`D2`).
- Polling the 4-bottle sensor slave (`#1:PING*` over `Serial1` at 9600 baud), draining `Serial1` frequently via `pollSlaveSerial(now)` to prevent 64-byte AVR UART RX ring-buffer overflows, and validating each returned JSON packet and bottle schema (`id` in `1..4`, no duplicates, required `temp`, `ph`, `methane`, `co2`, `press` fields) via `isValidBottleObject()` before forwarding to the Raspberry Pi or updating average bottle temperature.
- Hardware watchdog protection (`wdt_enable(WDTO_4S)` in `setup()`, `wdt_reset()` every `loop()` iteration) and non-blocking USB serial command reception (`usbRxBuffer` + `handleUsbCommand()`).
- Time-gated interquartile-mean analog water-level sensing on `A9` (`250 ms` cadence, middle 8 of 16 sorted samples) driving a 4-zone safety state machine with trip-only thresholds and asymmetric fast-trip (`1.0 s`) / slow-recovery (`5.0 s`) timers. Optional pump leak detection sensing on `A8` (wet fault at or above `leakThreshold`, boot default `200`) is guarded by `#define ENABLE_LEAK_SENSOR 0`. Thresholds are RAM-only variables set by the Pi via `THRESH:...` (§4.1, §9); the master forgets them on reboot.
- Debounced pump and heater interlocks driven by `A9`: Pump 1 (`D3`, pumps into the reservoir) hard-stops on high water (`inletWaterFull`: trips at or above `inPumpStopAdc`), Pump 2 (`D7`, pumps out of the reservoir) throttles on low water (`waterDepleted`: trips at or below `waterDryThreshold`) and hard-stops alongside the heater on critical dry (`waterCriticalLow`: trips at or below `outPumpStopAdc`). Boot defaults `550`, `460`, `400`. See sections 4 and 5. Pump `FORCE ON` (§5) is debug-only and ignores these interlocks.
- Hysteresis control of the 300 W heater SSR (`D5`), with per-actuator manual overrides (`AUTO`, `FORCE ON`, `OFF`).
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
| `WATER_SENSOR_PIN` | `A9` | `PK1` (`ADC9`) | Analog Input | Water bath level probe. ADC rises as the probe submerges: critical dry at or below `outPumpStopAdc` (default `400`), low at or below `waterDryThreshold` (default `460`), full at or above `inPumpStopAdc` (default `550`) |
| `LEAK_SENSOR_PIN` | `A8` | `PK0` (`ADC8`) | Analog Input (Disabled by default) | Optional pump leak detection probe (wet fault at or above `leakThreshold`, default `200`; guarded by `ENABLE_LEAK_SENSOR = 0`) |
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
| `outPumpStopAdc` etc. (§4.1) | `400`/`460`/`550`/`200` | RAM-only thresholds; forgotten on reboot until the Pi re-sends `THRESH:...`. |

`setup()` calls `forceAllOff()` and drives `D3`, `D7`, `D4`, and `D5` `LOW` before the first loop iteration. `web_control.py` also sends `HEATER_OFF`, `PUMP1_OFF`, `PUMP2_OFF`, and `STIR:0:10:30` on its first serial connection (`BOOT_DEFAULT_COMMANDS`), then its `THRESH:...` constants so the master adopts the Pi values.

Two consequences of these defaults:

- `MODE_AUTO` alone does not start the pumps. It sets `currentMode` but leaves the three override variables at `2`, so the pumps stay in `FORCE OFF` until `PUMP_AUTO`, `PUMP1_AUTO`, `PUMP2_AUTO`, or the `*_ON` variants arrive.
- The `D6` switch is one-way. Only `digitalRead(SYSTEM_SWITCH_PIN) == LOW` is acted on (`currentMode = MODE_OFF`); releasing the switch does not restore `MODE_AUTO`. Returning to `AUTO` requires a serial `MODE_AUTO`.

## 4. Water-Level (`A9`) and Optional Pump Leak (`A8`) Sensing

Analog water sensing runs every `WATER_CHECK_MS = 250UL` (4 Hz) using `readAdcAveraged(uint8_t pin)`, which discards one initial `analogRead(pin)` to settle the AVR ADC multiplexer sample-and-hold capacitor, collects `WATER_ADC_SAMPLES = 16` readings, sorts them via insertion sort, and returns the interquartile mean (average of the middle 8 samples). This rejects up to 4 low spikes and 4 high spikes per burst.

### 4.1 Trip thresholds

Thresholds are RAM-only variables (`outPumpStopAdc`,
`waterDryThreshold`, `inPumpStopAdc`, `leakThreshold`) so the Pi can change
them at runtime via `THRESH:...` (see §9). Trip-only: no recover deadbands
(the water fills too fast for a deadband). They are not stored in EEPROM:
on reboot the master forgets them and returns to the boot defaults below.
The Pi re-sends its constants on every serial connect and whenever the
master's reported values drift (see `04-backend-web-control.md`).

| Variable | Boot default | Meaning |
|---|---|---|
| `outPumpStopAdc` | `400` | Trips `waterCriticalLow` (`<= 400`): hard-stops Pump 2 (`D7`) and cuts Heater (`D5`) |
| `waterDryThreshold` | `460` | Trips `waterDepleted` (`<= 460`): throttles Pump 2 to `DRY_OUT_PUMP_PWM` (`100`) |
| `inPumpStopAdc` | `550` | Trips `inletWaterFull` (`>= 550`): hard-stops Pump 1 (`D3`) to prevent overflow |
| `leakThreshold` | `200` | Wet-fault level for the optional `A8` leak probe |

### 4.2 Asymmetric Fast-Trip / Slow-Recovery Debouncing

All three water-level safety flags (`waterCriticalLow`, `waterDepleted`, `inletWaterFull`) are updated via `updateLowTripFlag()` and `updateHighTripFlag()` with a single trip threshold per flag:

- **Fast Trip (`WATER_TRIP_LIMIT = 4`, `1.0 s`)**: 4 checks at `250 ms` beyond the trip threshold set the flag `true` and preload its counter to `WATER_RECOVER_LIMIT` (`20`). While untripped, a non-fault reading decrements the counter by 1 rather than zeroing it immediately so noisy fault signals still trip reliably.
- **Slow Recovery (`WATER_RECOVER_LIMIT = 20`, `5.0 s`)**: While tripped, the reading must remain continuously past the same trip threshold for 20 consecutive checks (`5.0 s`) before the flag clears to `false`. Any reading that fails to clear resets the counter back to `20`, preventing momentary false readings from dry-firing the heater or restarting the inlet pump into a full reservoir.
- **Boot**: `setup()` takes one interquartile-mean reading and initializes `waterCriticalLow`, `waterDepleted`, and `inletWaterFull` immediately, preloading any tripped flag's recovery counter to `WATER_RECOVER_LIMIT`.
- **Opposite-Extreme Mutual Exclusion**: To prevent both circulation pumps from locking each other out simultaneously when the water level swings between `inPumpStopAdc` (`>= 550`) and `waterDryThreshold` (`<= 460`) or `outPumpStopAdc` (`<= 400`) faster than the 5.0-second recovery window, an active `waterCriticalLow` or `waterDepleted` flag immediately clears `inletWaterFull` (`inletFullCounter = 0`) when `inPumpStopAdc > waterDryThreshold`, and an active `inletWaterFull` flag immediately clears `waterCriticalLow` (`waterCritCounter = 0`) and `waterDepleted` (`waterFaultCounter = 0`).

### 4.3 Optional Pump Leak Detection Probe (`A8`)

- Controlled at compile time by `#define ENABLE_LEAK_SENSOR 0`.
- When `ENABLE_LEAK_SENSOR == 0` (default when only the single `A9` water presence sensor is connected), `A8` is not sampled (`leakRaw = -1`, `pumpLeakDetected = false`, `leakFaultCounter = 0`). Sampling an unconnected `A8` pin immediately after `A9` would otherwise retain sample-and-hold charge and report a false leak.
- When `ENABLE_LEAK_SENSOR == 1`, 5 consecutive `500 ms` checks at or above `LEAK_THRESHOLD` (`200`) set `pumpLeakDetected = true`, and 5 consecutive checks below `200` clear it.

## 5. Automatic Pump Interlocks

`loop()` derives the interlock booleans from the debounced state flags:

```cpp
bool inletLevelOk   = !inletWaterFull;   // trips >= inPumpStopAdc
bool outletLevelOk  = !waterCriticalLow; // trips <= outPumpStopAdc
bool heaterWaterLow = waterCriticalLow;
```

When `!inletLevelOk`, Pump 1 (`D3`) ramps down via `slewMosfet(..., false)` (3-second soft stop). Likewise, when `!outletLevelOk`, Pump 2 (`D7`) ramps down. Pump `FORCE OFF` instead cuts immediately via `resetMosfetOff()`. `MODE_OFF`/leak still call `forceAllOff()` (immediate).

AUTO behaviour with `currentMode == MODE_AUTO` and no leak (boot defaults shown; thresholds are Pi-settable, see §4.1; each flag uses one trip threshold):

| Debounced State | ADC Trip | Pump 1 (`D3`, IN) | Pump 2 (`D7`, OUT) | Heater (`D5`) |
|---|---|---|---|---|
| `inletWaterFull` | Trips `>= 550` | **ramped off (`0` PWM over 3 s)** | on, `200` PWM | allowed |
| Normal band | Between thresholds | on, `200` PWM | on, `200` PWM | allowed |
| `waterDepleted` | Trips `<= 460` | on, `200` PWM | throttled, `100` PWM (`DRY_OUT_PUMP_PWM`) | allowed |
| `waterCriticalLow` | Trips `<= 400` | on, `200` PWM | **ramped off (`0` PWM over 3 s)** | **hard off** |

Override interaction:

| Override | Pump 1 (`D3`) | Pump 2 (`D7`) |
|---|---|---|
| `0` AUTO | `inletLevelOk`, PWM `<= 200`; interlock stop ramps down | `outletLevelOk`, PWM `200` or `100`; interlock stop ramps down |
| `1` FORCE ON | naive on, PWM `<= 255` (`maxPwm`), ignores water levels | naive on, PWM `<= 255` (`maxPwm`), ignores water levels |
| `2` FORCE OFF | immediate `0` (`resetMosfetOff`) | immediate `0` (`resetMosfetOff`) |

`FORCE ON` is debug-only: naively on/off, it ignores `inletWaterFull`,
`waterCriticalLow` and the `waterDepleted` throttle so the pump can be
tested regardless of the water level. It still bypasses the `200` PWM
cap (runs to `maxPwm`). It does not bypass the master kills in §6
(`MODE_OFF` or a detected leak still call `forceAllOff()`). For
production use `AUTO` or `OFF`; do not rely on `FORCE ON` for safety.

## 6. Mode and Safety Cutoffs

`loop()` evaluates one gate before any actuator work:

```cpp
if (currentMode == MODE_OFF || pumpLeakDetected) {
  forceAllOff();   // heater, both pumps, stirrer forced to 0
}
```

Heater logic inside `MODE_AUTO`:

1. `overTempCeiling` (`waterTemp >= MAX_WATER_BATH_TEMP`, `50.0 °C`), `heaterWaterLow` (`waterCriticalLow == true`, tripped at `<= outPumpStopAdc` and held for `5.0 s` past the threshold), or `heaterOverride == 2` → heater off. These three win over every other case, including `HEATER_ON`.
2. `heaterOverride == 1` → heater on, subject to the block above.
3. `heaterOverride == 0` (AUTO) → off while `bathSensorOk` or `slaveActive` is false; otherwise hysteresis on the average bottle temperature: on below `targetBottleTemp - bottleHysteresis`, off at or above `targetBottleTemp`.

`targetBottleTemp` (`20.0`–`50.0` °C) and `bottleHysteresis` (`0.1`–`5.0` °C) are set by the `TEMP:` command. The heater is driven from the *bottle* average reported by the slave; the bath sensor supplies only the `50 °C` ceiling and the health gate. The heater is the only actuator that reads `bathSensorOk` or `slaveActive`.

## 7. Unified 3-Channel MOSFET PWM Ramping and `0–255` Debit Control

All 3 MOSFET channels (`pump1Mosfet` on `D3`, `pump2Mosfet` on `D7`, and `stirMosfet` on `D4`) use the `MosfetChannel` struct and:

```cpp
void slewMosfet(MosfetChannel &ch, bool wantOn, unsigned long now,
                uint8_t pwmLimit = 255U, uint8_t onPwm = 255U);
```

- Target selection: `desiredPwm = ch.maxPwm`, then clamped by `pwmLimit`, then by `onPwm`; the target is `desiredPwm` when `wantOn`, else `0`.
- `STIR_RAMP_MS = 3000UL`: 3-second soft-start and soft-stop linear ramp between the current PWM and the target.
- Whenever the target changes, `slewMosfet` records `ch.rampOriginPwm = ch.currentPwm` and `ch.rampOriginMs = now`, then interpolates toward the target over `3000 ms`. Once `elapsed >= STIR_RAMP_MS`, `slewMosfet` clamps directly to `ch.targetPwm` and returns early so `elapsed * refSpan` never overflows 32-bit `unsigned long` during multi-hour steady holds. A target change mid-ramp restarts the ramp from the instantaneous PWM.
- For the stirrer (`D4`), `slewMosfet` is used across all operating modes:
  - `stirMode == 0` (`OFF`): 3-second ramp down to `0`.
  - `stirMode == 1` (`CONSTANT ON`): 3-second ramp up to `stirMosfet.maxPwm`.
  - `stirMode == 2` (`PULSE INTERVAL`): 3-second ramp up at the start of each pulse window and 3-second ramp down before the pulse window ends.
  - `stirMode == 3` (`TIMED RUN`): 3-second ramp up, continuous hold at `stirMosfet.maxPwm`, and 3-second ramp down at the end of the timed duration, after which `stirMode` returns to `0`.
- For Pump 1 (`D3`) and Pump 2 (`D7`), `slewMosfet` ramps each pump over 3 seconds to the target described in section 5 and ramps down to `0` over 3 seconds when an interlock switches it off. `FORCE OFF` instead cuts immediately via `resetMosfetOff()`.

## 8. Slave UART Polling, Validation, and Watchdog (`Serial1`)

A 4-bottle slave JSON line is ~340 bytes at 9600 baud (~350 ms on the wire), while the AVR `HardwareSerial` RX ring buffer is 64 bytes (~66 ms at 9600 baud). To prevent `Serial1` buffer overflows during DS18B20 1-Wire conversions, 16-sample ADC bursts, or USB status serializations:

- **Frequent `Serial1` Draining (`pollSlaveSerial(now)`)**: `loop()` calls `pollSlaveSerial(now)` at the start of the loop, immediately after `sensors.getTempCByIndex(0)`, after `readAdcAveraged()`, after `sendTelemetryStatus()`, and at the end of `loop()`. `slaveRxBuffer.reserve(512)` is pre-allocated in `setup()` to prevent AVR heap fragmentation.
- **Pre-Forwarding JSON & Schema Validation (`isValidBottleObject`)**: When a newline arrives on `Serial1`, `pollSlaveSerial` extracts the `{...}` substring and runs `deserializeJson(doc, cleanJson)` **before** forwarding anything to the Raspberry Pi. If `doc["bottles"]` is present, every element must pass `isValidBottleObject()` (`id` integer in `1..4`, no duplicate IDs in the array, and non-null `temp`, `ph`, `methane`, `co2`, and `press` fields). Truncated or corrupted packets (such as UART byte-drop splices producing invalid IDs or missing fields) are discarded instead of forwarded to the Pi.
- **Non-Blocking USB Command Reception (`usbRxBuffer`)**: `Serial` is drained character-by-character into `usbRxBuffer` (reserved to 128 bytes, capped at `USB_RX_MAX_LEN = 96`) and dispatched via `handleUsbCommand(usbRxBuffer, now)` on `\n`, avoiding `Serial.readStringUntil('\n')` blocking stalls.
- **Hardware Watchdog (`WDTO_4S`)**: `setup()` disables any residual watchdog early (`wdt_disable()`) and enables a 4-second hardware watchdog (`wdt_enable(WDTO_4S)`) at the end of initialization. `loop()` feeds the watchdog (`wdt_reset()`) on every iteration so any EMI/brownout lockup when switching inductive loads automatically resets the MCU into safe boot defaults (`forceAllOff()`).

## 9. Raspberry Pi USB Serial Protocol

Commands are newline-terminated ASCII strings received on `Serial` (115200 baud):

- `STATUS`: Emits a `{"type":"master_status", ...}` JSON line containing:
  - `water_temp`, `bottle_temp_avg`, `target_temp`, `hysteresis`, `system_mode`
  - `heater`, `pump` (combined `pump1 || pump2`), `pump1`, `pump2`, `stirrer`
  - `heater_mode`, `pump_mode`, `pump1_mode`, `pump2_mode`, `stir_mode`, `stir_int_min`, `stir_pulse_sec`, `stir_timed_min`, `stir_rem_sec`
  - `pwm_d3`, `pwm_d7`, `pwm_d4` (configured `0`–`255` target PWM ceilings for `D3`, `D7`, `D4`)
  - `out_d3`, `out_d7`, `out_d4` (instantaneous `0`–`255` PWM output values for `D3`, `D7`, `D4`)
  - `water_low`, `water_crit`, `water_full`, `water_adc`, `water_thr`, `out_stop_thr`, `in_stop_thr` (live thresholds; boot defaults `460`, `400`, `550`)
  - `leak_enabled`, `pump_leak`, `leak_adc`, `leak_thr` (optional pump leak sensor on `A8`; `leak_enabled = 0`, `pump_leak = 0`, `leak_adc = -1` when disabled)
- `MODE_AUTO` / `MODE_OFF`: Switches master system mode. `MODE_AUTO` does not change any override, so the pumps and heater stay in `FORCE OFF` until the matching `*_AUTO` command arrives. `MODE_OFF` calls `forceAllOff()`.
- `HEATER_AUTO` / `HEATER_ON` / `HEATER_OFF`: Sets heater override mode (`0`, `1`, `2`). `HEATER_OFF` also drives the heater low immediately.
- `PUMP1_AUTO` / `PUMP1_ON` / `PUMP1_OFF`: Sets Pump 1 (`Pin D3`) override mode (`0`, `1`, `2`).
- `PUMP2_AUTO` / `PUMP2_ON` / `PUMP2_OFF`: Sets Pump 2 (`Pin D7`) override mode (`0`, `1`, `2`).
- `PUMP_AUTO` / `PUMP_ON` / `PUMP_OFF`: Sets both Pump 1 and Pump 2 override modes simultaneously.
- `PWM:<D3|D7|D4>:<0-255>`: Sets the target PWM ceiling (`0`–`255`) for `Pin D3` (Pump 1), `Pin D7` (Pump 2), or `Pin D4` (Stirrer).
- `STIR:<mode>:<mins>:<secs>`: Configures stirrer mode (`0`–`3`), interval/timed minutes, and pulse duration seconds. Also resets `stirCycleStart`.
- `TEMP:<target>:<hysteresis>`: Updates `targetBottleTemp` (`20.0`–`50.0` °C) and `bottleHysteresis` (`0.1`–`5.0` °C).
- `THRESH:<out_stop>:<dry>:<in_stop>:<leak>` (example `THRESH:400:460:550:200`): Updates the four RAM-only water/leak thresholds (§4.1). All four values must be `0`–`1023`; otherwise the whole line is ignored. Applied atomically and effective on the next 250 ms sample. Never written to EEPROM — the master forgets them on reboot. The Pi sends this on every serial connect from its module constants (or live `thresholds` dict) in `web_control.py`.

## 10. Known Gaps

- **A silent bottle slave stops AUTO heating.** `slaveActive` is false when no packet with a bottle temperature between 15 and 60 °C has arrived in the last 12 s, which leaves `bottle_temp_avg` at `-999` and the heater off in AUTO. Circulation is unaffected.
- **The `D6` switch cannot restore `AUTO`.** See section 3.
- **`pump_mode` mirrors Pump 1 only.** Use `pump1_mode` and `pump2_mode`; `pump_mode` duplicates `pump1_mode`.

---
Content generated by LLM.
