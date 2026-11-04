# 6. Master firmware — `legacy/master/master.cpp`

Firmware for the **master controller**: ATmega2560 (Mega 2560 or Mega Mini),
reached from the Pi as a CH340 serial adapter on `/dev/ttyUSB0` at 115200 baud.
`legacy/master/master.cpp` is the source of truth. The historical
`water_control_arduino/water_control_arduino.ino` has been replaced on the Pi by
a copy of this file.

## Pins

| Pin | Function |
|---|---|
| D2 | DS18B20 bath temperature (OneWire) |
| D3 | Circulation Pump 1 MOSFET gate (active-HIGH PWM: `0`–`255`, Timer 3 `OC3C`) |
| D4 | Stirrer MOSFET gate (active-HIGH PWM: `0`–`255`, Timer 0 `OC0B`) |
| D5 | Heater SSR (active HIGH trigger; `HIGH` = ON) |
| D6 | System switch (input pull-up; `LOW` = OFF) |
| D7 | Circulation Pump 2 MOSFET gate (active-HIGH PWM: `0`–`255`, Timer 4 `OC4B`) |
| A9 | 3-pin resistive water-level analog sensor (250 ms check interval, 16-sample interquartile mean, hysteresis + asymmetric debounce) |
| A8 | Optional pump leak analog sensor (`LEAK_THRESHOLD = 200`, disabled by default via `ENABLE_LEAK_SENSOR = 0`) |
| Serial (USB) | 115200 to Raspberry Pi |
| Serial1 | 9600 to slave "Mega Pro" |

## Control parameters

- Target bottle core temperature: **39.0 °C**
- Hysteresis: **0.3 °C** (heat on below 38.7, off at or above 39.0)
- Hard bath ceiling: **50.0 °C**
- Slave timeout: **12 s** (failsafe)
- DS18B20 cadence: **750 ms**, non-blocking
- Slave poll cadence: **3500 ms**
- Stirrer & pump ramp: **3000 ms** soft-start and soft-stop, full duty **255** (AUTO pump cap **200**, dry Pump 2 throttle **100**)
- Water-level sensing (`A9`): evaluated every **250 ms** using the interquartile mean (middle 8 of 16 sorted samples), with **4** checks (**1.0 s**) to trip and **20** uninterrupted checks (**5.0 s**) past the hysteresis recovery threshold to clear:
  - `waterCriticalLow` (Heater & Pump 2 hard cutoff): trips `<= 350`, recovers `>= 420`
  - `waterDepleted` (Pump 2 throttle to `100` PWM): trips `<= 450`, recovers `>= 480`
  - `inletWaterFull` (Pump 1 hard cutoff): trips `>= 500`, recovers `<= 460`

## Stirrer modes

| Value | Mode | Behaviour |
|---|---|---|
| 0 | OFF | 3 s ramp down to PWM 0 |
| 1 | CONSTANT ON | 3 s ramp to 255, then hold |
| 2 | PULSE | 3 s ramp up, hold, 3 s ramp down inside each pulse window |
| 3 | TIMED RUN | Ramp up, hold, ramp down, then switch to OFF at the duration |

## Main loop

1. Non-blocking DS18B20 read every 750 ms.
2. Poll slave #1 every 3500 ms with `#1:PING*` on `Serial1`.
3. Ingest slave JSON, extract the `{...}` substring, forward it to the Pi, and
   compute the average bottle temperature from `bottles[].temp` or `temp`.
4. Read D6. `LOW` forces `MODE_OFF`.
5. Water-level check every 250 ms: computes the interquartile mean of 16 samples on `A9` (`readAdcAveraged()`) and updates the debounced `waterCriticalLow`, `waterDepleted`, and `inletWaterFull` flags (`1.0 s` trip, `5.0 s` recovery with hysteresis).
6. Actuator control:
   - If `currentMode == MODE_OFF` or `pumpLeakDetected`: `forceAllOff()` cuts heater, both pumps, and stirrer immediately.
   - If `inletWaterFull`: `resetMosfetOff(pump1Mosfet)` cuts Pump 1 immediately.
   - If `waterCriticalLow`: `resetMosfetOff(pump2Mosfet)` and `driveHeater(false)` cut Pump 2 and the heater immediately.
   - Otherwise in `MODE_AUTO`:
     - pump: auto when bath and slave are healthy, or forced by override;
     - heater: hysteresis around the target, hard-off at the bath ceiling, or
       forced by override;
     - stirrer: state machine above.
7. Handle newline-terminated commands on USB serial.

## Boot defaults

`heaterOverride` and `pumpOverride` start at `2` (`FORCE OFF`), `stirMode`
starts at `0` (`OFF`), and `currentMode` starts at `MODE_AUTO`. All three
actuators are therefore off after reset until the Pi or the web UI sets a mode.
`setup()` calls `forceAllOff()`, drives `D3`, `D4` and `D5` `LOW`, and samples
`A7` once to initialize `waterRaw` and `waterDepleted` immediately at boot.

## Commands

`STATUS`, `MODE_AUTO`, `MODE_OFF`, `HEATER_AUTO|ON|OFF`,
`PUMP_AUTO|ON|OFF`, `STIR:<mode>:<mins>:<secs>`, and
`TEMP:<target>:<hysteresis>`.

## Telemetry

A `STATUS` command returns one JSON line with `type: master_status`,
`water_temp`, `bottle_temp_avg`, `heater`, `pump`, `stirrer`, override modes,
stirrer mode and timing, `stir_rem_sec`, `target_temp`, `hysteresis`,
`water_low`, `water_crit`, `water_full`, `water_adc`, `water_thr`, `out_stop_thr`, `in_stop_thr`, and `system_mode`.

## Libraries

OneWire 2.3.8, DallasTemperature 4.0.x, ArduinoJson 6.x. ArduinoJson 7 is not
compatible because the source uses `StaticJsonDocument` and `serialized()`.

## Deployment

PlatformIO environment `[env:legacy-master]` builds it. The live Pi uses
[`sync_pi.sh`](../../../sync_pi.sh): when `master.cpp` changes, the script
compiles it on the Pi with Arduino CLI (`arduino:avr:mega`), stops PM2 app
`water-ui` to free `/dev/ttyUSB0`, uploads with verify, and restarts PM2. The
last successful hash is stored in
`~/.cache/water-control-system/master-flash.sha256`. See
[pi-sync-script.md](../pi-sync-script.md#master-firmware-flashing) and
[legacy-master.md](../legacy-master.md).

---
Content generated by `google/gemini-4-argon` in `antigravity`.
