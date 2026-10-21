# 6. Master firmware — `water_control_arduino/water_control_arduino.ino`

Arduino sketch for the **master controller** (AVR core 1.8.6, Arduino CLI 1.3.1). Board not currently connected (no `/dev/ttyUSB*`/`/dev/ttyACM*`).

## Pins

| Pin | Function |
|---|---|
| D2 | DS18B20 bath temperature (OneWire) |
| D3 | Circulation pump relay (active-LOW: `pinMode OUTPUT` + `LOW` = ON; `INPUT` high-Z = OFF) |
| D4 | Exhaust fan |
| D5 | Heater SSR (active HIGH trigger; `HIGH` = ON) |
| D6 | System switch (input pull-up; `LOW` = OFF) |
| D8 | RS485 DE/RE control (MAX485) |
| A7 | Water-level analog probe |
| Serial (USB) | 115200 to Raspberry Pi |
| Serial1 | 9600 to slave "Mega Pro" over RS485 |

## Control parameters

- Target bottle core temp: **39.0 °C** (heat on < 38.7, off ≥ 39.0)
- Bath ceiling: **39.8 °C** (heater clamped off)
- Emergency fan: **45.0 °C**
- Water threshold: ADC **500**; **5** consecutive low cycles trip a fault
- Slave timeout: **12 s** (failsafe)
- DS18B20 cadence: 750 ms (non-blocking)
- Slave poll cadence: 3500 ms

## Main loop

1. Non-blocking DS18B20 read (every 750 ms).
2. Poll slave #1 every 3500 ms: send `#1:PING*` over RS485 (DE high, 2 ms settle, flush, 5 ms stop-bit window, DE low).
3. Ingest slave JSON from Serial1: extract `{...}` substring, parse with ArduinoJson, compute average bottle temp from `bottles[].temp` (or single `temp`), update `currentBottleTemp`/`bottleTelemetryValid`/`lastSlavePacketTime`. Forward the cleaned JSON line to the Pi over USB serial.
4. Physical switch check: D6 `LOW` → `MODE_OFF`.
5. Water-level check (see note below).
6. Actuator logic:
   - If `MODE_OFF` or water depleted or bath sensor invalid or slave timed out → `forceAllOff()`.
   - Else `MODE_AUTO`: pump ON; heater logic with bath ceiling; fan on if bath ≥ 45 °C.
7. Handle Pi commands on USB serial: `STATUS` → `sendTelemetryStatus()`; `MODE_AUTO`; `MODE_OFF`.

## Telemetry emitted to Pi (`master_status`)

```json
{"type":"master_status","water_temp":..,"bottle_temp_avg":..,"heater":0/1,"pump":0/1,"fan":0/1,"water_low":0/1,"water_adc":..,"system_mode":"AUTO"|"OFF"}
```

## ⚠️ Water-level sensing is bypassed in the live firmware

In `~/water-control-system/water_control_arduino/water_control_arduino.ino`:

```cpp
int waterRaw = 1023;
waterFaultCounter = 0;
bool waterDepleted = false;
```

So `water_low` is always 0 and dry-run protection is **disabled**. The Desktop copies of the sketch retain the real logic (`analogRead(WATER_SENSOR_PIN)`, threshold 500, 5-cycle debounce). This is a safety regression in the live copy.

## Libraries

`OneWire`, `DallasTemperature` (under `~/Arduino/libraries`). `ArduinoJson` is also required (provided by the installed core/package).
