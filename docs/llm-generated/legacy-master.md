# Legacy Master Firmware (`legacy/master/master.cpp`)

## 1. Overview

[`legacy/master/master.cpp`](../../legacy/master/master.cpp) is the ATmega2560 Mega Mini firmware for the water-bath master controller (PlatformIO environment `[env:legacy-master]` in [`platformio.ini`](../../platformio.ini)). It manages:

- Non-blocking water-bath temperature acquisition via a DS18B20 sensor.
- Polling the 4-bottle sensor slave (`#1:PING*` over `Serial1` at 9600 baud) and parsing the returned JSON telemetry to compute average bottle temperature.
- Hysteresis control of the 300 W heater SSR and R385 12 V DC diaphragm circulation pump MOSFET, with per-actuator manual overrides.
- Non-blocking MOSFET PWM ramping (`analogWrite` on Pin D4 / Timer 0 `OC0B`) for the stirrer motor across all transitions (`CONSTANT ON`, `PULSE`, `TIMED RUN`, and manual `OFF`) to suppress startup inrush current and smooth motor spin-down.
- Non-blocking MOSFET PWM ramping (`analogWrite` on Pin D3 / Timer 3 `OC3C`) for the pump, using the same 3-second linear ramp on AUTO and override transitions.
- Bidirectional USB serial (`Serial` at 115200 baud) command and status telemetry with the Raspberry Pi Flask backend ([`web_control.py`](../../legacy/pi/water-control-system/web_control.py)).

## 2. Pin Assignment

| Constant | Arduino Pin | ATmega2560 Port | Direction / Type | Function |
|---|---|---|---|---|
| `TEMP_SENSOR_PIN` | `D2` | `PE4` | 1-Wire Digital | DS18B20 water bath temperature sensor |
| `PUMP_PIN` | `D3` | `PE5` (`OC3C`) | 8-bit PWM Output | Circulation pump MOSFET gate driver (Active-HIGH PWM, `0`–`255`; drives R385 12 V DC diaphragm pump) |
| `STIRRER_PIN` | `D4` | `PG5` (`OC0B`) | 8-bit PWM Output | Stirrer MOSFET gate driver (Active-HIGH PWM, `0`–`255`) |
| `HEATER_PIN` | `D5` | `PE3` | Digital Output | 300 W Heater SSR trigger (Active-HIGH) |
| `SYSTEM_SWITCH_PIN` | `D6` | `PH3` | `INPUT_PULLUP` | Physical `AUTO` / `OFF` safety switch (`LOW` forces `MODE_OFF`) |
| `WATER_SENSOR_PIN` | `A7` | `PF7` (`ADC7`) | Analog Input | Analog water level probe |
| `Serial` | `D0`/`D1` | `PE0`/`PE1` | UART0 (115200) | USB serial link to Raspberry Pi |
| `Serial1` | `D19`/`D18` | `PD2`/`PD3` | UART1 (9600) | Direct UART link to Slave Mega (`#1`) |

## 3. Stirrer MOSFET PWM Ramping

To prevent high startup inrush current from overloading the power rail and provide consistent mechanical acceleration and deceleration, `STIRRER_PIN` (`D4`) is driven via hardware PWM (`analogWrite`) using [`driveStirrerPwm`](../../legacy/master/master.cpp#L85-L90) and [`slewStirrer`](../../legacy/master/master.cpp#L96-L120).

### Parameters

- `STIR_RAMP_MS = 3000UL`: 3000 ms (3 seconds) full-scale linear ramp duration between `0` and `STIR_MAX_PWM`.
- `STIR_MAX_PWM = 255U`: 100% duty cycle at full speed.

### Slew-Rate State Tracking

`slewStirrer(bool wantOn, unsigned long now)` tracks `stirTargetOn`, `stirRampOriginPwm`, and `stirRampOriginMs`. Whenever the desired state (`wantOn`) changes, it captures the current `stirrerPwm` and `now`, then ramps linearly from `stirRampOriginPwm` toward `255` (when `wantOn == true`) or `0` (when `wantOn == false`) at a constant rate of `255 / 3000 ms`.

### Mode Behavior

| `stirMode` / State | Name | Ramp Profile |
|---|---|---|
| `0` | Manual `OFF` | Soft-stop: ramps linearly from current `stirrerPwm` down to `0` over up to `3000 ms`. |
| `1` | `CONSTANT ON` | Soft-start: ramps linearly from current `stirrerPwm` up to `255` over up to `3000 ms`, then holds `255`. |
| `2` | `PULSE INTERVAL` | Soft-start + soft-stop inside each `onWindowMs = stirPulseSec * 1000UL` window of `periodMs = stirIntervalMin * 60000UL`: ramps up over the first `3000 ms`, holds `255`, and begins ramping down `3000 ms` before the end of the pulse (`activeDriveMs = onWindowMs - STIR_RAMP_MS`, or `onWindowMs / 2` for pulses `< 6 s`). |
| `3` | `TIMED RUN` | Soft-start + soft-stop over `durationMs = stirTimedMin * 60000UL`: ramps up over the first `3000 ms`, holds `255`, ramps down over the final `3000 ms`, and transitions `stirMode` to `0` (`OFF`) when the timer expires. |
| `MODE_OFF` / `forceAllOff()` | System / Emergency Stop | Immediate cut to `0` (`analogWrite(STIRRER_PIN, 0)`) and resets `stirTargetOn = false` and `stirRampOriginPwm = 0`. |

## 4. Pump MOSFET PWM Ramping

`PUMP_PIN` (`D3`) is driven with active-HIGH PWM through `drivePumpPwm` and `slewPump`. The pump follows the same 3-second linear ramp as the stirrer when AUTO or Pi override logic changes its requested state. `PUMP_ON` requests a ramp to full duty; `PUMP_OFF` requests a ramp down to zero. `forceAllOff()` immediately writes zero duty, matching its immediate stirrer shutdown behavior.

## 5. Raspberry Pi USB Serial Protocol

Commands are newline-terminated ASCII strings received on `Serial` (115200 baud):

- `STATUS`: Emits a `{"type":"master_status", ...}` JSON line with bath temperature, average bottle temperature, actuator states (`heater`, `pump`, `stirrer`), override modes, stirrer timing parameters, and remaining timed-run seconds.
- `MODE_AUTO` / `MODE_OFF`: Switches master system mode.
- `HEATER_AUTO` / `HEATER_ON` / `HEATER_OFF`: Sets heater override mode (`0`, `1`, `2`).
- `PUMP_AUTO` / `PUMP_ON` / `PUMP_OFF`: Sets circulation pump override mode (`0`, `1`, `2`).
- `STIR:<mode>:<mins>:<secs>`: Configures stirrer mode (`0`–`3`), interval/timed minutes, and pulse duration seconds, resetting `stirCycleStart` to `now`.
- `TEMP:<target>:<hysteresis>`: Updates `targetBottleTemp` (`20.0`–`50.0` °C) and `bottleHysteresis` (`0.1`–`5.0` °C).

## 6. Build and Deployment

PlatformIO builds the sketch from the monorepo root:

```bash
pio run -e legacy-master
```

The live Pi deploys it with [`sync_pi.sh`](../../sync_pi.sh). When
`legacy/master/master.cpp` changes, the script compiles it on the Pi with
Arduino CLI (`arduino:avr:mega`) and uploads it to the master Arduino over USB.
The last successful source hash is kept in
`~/.cache/water-control-system/master-flash.sha256`. PM2 app `water-ui` is
stopped briefly to release the serial port, then restarted. See
[pi-sync-script.md](pi-sync-script.md#master-firmware-flashing).

The Pi's `water_control_arduino/water_control_arduino.ino` is overwritten from
this file during sync. The historical sketch in the repository is not what runs
on the board.

The water-level probe is bypassed in this source: `waterRaw` is fixed at 1023,
`waterFaultCounter` stays zero, and `waterDepleted` stays `false`. Flashing does
not add dry-run protection.

---
Content generated by `openai/gpt-6` in `harness`.
