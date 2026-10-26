#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define TEMP_SENSOR_PIN   2     // DS18B20 Water bath sensor
#define PUMP_PIN          3     // Circulation pump MOSFET (Active-HIGH, R385 12VDC diaphragm pump)
#define STIRRER_PIN       4     // Stirrer MOSFET (Formerly FAN_PIN, Active-HIGH, PWM OC0B)
#define HEATER_PIN        5     // 300W Heater SSR (Active-HIGH trigger, 0V = OFF)
#define SYSTEM_SWITCH_PIN 6     // Physical AUTO / OFF toggle
#define WATER_SENSOR_PIN  A7    // Analog water level probe

// ==========================================
// CONTROL PARAMETERS & DYNAMIC TARGETS
// ==========================================
float targetBottleTemp  = 39.0; // Adjustable target bottle average temp (°C)
float bottleHysteresis  = 0.3;  // Adjustable hysteresis band (°C)
#define MAX_WATER_BATH_TEMP  41.0   // Hard safety bath ceiling clamp (°C)
#define WATER_THRESHOLD      200    // Minimum dry-run ADC threshold (~0.98V on 5V ADC)
#define WATER_FAULT_LIMIT    5      // 5 consecutive 500ms checks (2.5s) to trip/clear
#define WATER_CHECK_MS       500UL  // Time-gated water level sampling interval (ms)
#define WATER_ADC_SAMPLES    16     // Number of ADC samples averaged per check
#define SLAVE_TIMEOUT_MS     12000  // Failsafe timeout if Slave stops responding
#define TEMP_INTERVAL_MS     750    // Non-blocking DS18B20 cadence
#define SLAVE_POLL_INTERVAL  3500   // Acquisition cadence matching Slave cycle time
#define STIR_RAMP_MS         3000UL // 3-second soft-start / soft-stop PWM ramp
#define STIR_MAX_PWM         255U   // Full speed PWM duty cycle (0-255)

enum SystemMode { MODE_OFF, MODE_AUTO };
SystemMode currentMode = MODE_AUTO;

// Per-Actuator Override Modes: 0 = AUTO, 1 = FORCE ON, 2 = FORCE OFF
// Boot default: heater and pump start FORCE OFF. The Pi or web UI must
// explicitly select AUTO / FORCE ON after power-up.
uint8_t heaterOverride = 2;
uint8_t pumpOverride   = 2;

// Stirrer Modes: 0 = OFF, 1 = CONSTANT ON, 2 = PULSE INTERVAL, 3 = TIMED RUN
uint8_t stirMode = 0;
unsigned long stirIntervalMin = 10;   // Default: Pulse every 10 minutes
unsigned long stirPulseSec    = 30;   // Default: 30-second pulse duration
unsigned long stirTimedMin    = 15;   // Default: 15-minute timed continuous run
unsigned long stirCycleStart  = 0;    // Timestamp tracker for Pulse / Timed modes

// Stirrer non-blocking slew-rate ramp state
bool stirTargetOn = false;
uint8_t stirRampOriginPwm = 0;
unsigned long stirRampOriginMs = 0;

// Pump non-blocking slew-rate ramp state
bool pumpTargetOn = false;
uint8_t pumpRampOriginPwm = 0;
unsigned long pumpRampOriginMs = 0;

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature bathSensor(&oneWire);

// State Variables
float waterTemp = -999.0;
float currentBottleTemp = -999.0;
bool heaterState = false;
bool pumpState = false;
uint8_t pumpPwm = 0;
bool stirrerState = false;
uint8_t stirrerPwm = 0;
bool bottleTelemetryValid = false;
int waterRaw = 0;
int waterFaultCounter = 0;
bool waterDepleted = false;

unsigned long lastTempRequest = 0;
unsigned long lastWaterCheck = 0;
unsigned long lastSlavePollTime = 0;
unsigned long lastSlavePacketTime = 0;
String slaveRxBuffer = "";

// Forward declaration
void sendTelemetryStatus(int waterRaw, unsigned long now);

// ==========================================
// ACTUATOR DRIVERS
// ==========================================
void drivePumpPwm(uint8_t pwm) {
  pinMode(PUMP_PIN, OUTPUT);
  analogWrite(PUMP_PIN, pwm);
  pumpPwm = pwm;
  pumpState = (pwm > 0);
}

void driveHeater(bool state) {
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, state ? HIGH : LOW);
  heaterState = state;
}

void driveStirrerPwm(uint8_t pwm) {
  pinMode(STIRRER_PIN, OUTPUT);
  analogWrite(STIRRER_PIN, pwm);
  stirrerPwm = pwm;
  stirrerState = (pwm > 0);
}

void driveStirrer(bool state) {
  driveStirrerPwm(state ? STIR_MAX_PWM : 0);
}

void slewStirrer(bool wantOn, unsigned long now) {
  if (wantOn != stirTargetOn) {
    stirTargetOn = wantOn;
    stirRampOriginPwm = stirrerPwm;
    stirRampOriginMs = now;
  }

  unsigned long elapsed = now - stirRampOriginMs;
  unsigned long delta = (elapsed * STIR_MAX_PWM) / STIR_RAMP_MS;

  uint8_t nextPwm = 0;
  if (stirTargetOn) {
    unsigned long target = (unsigned long)stirRampOriginPwm + delta;
    if (target > STIR_MAX_PWM) target = STIR_MAX_PWM;
    if (target == 0 && STIR_MAX_PWM > 0) target = 1;
    nextPwm = (uint8_t)target;
  } else {
    if (delta >= stirRampOriginPwm) {
      nextPwm = 0;
    } else {
      nextPwm = (uint8_t)(stirRampOriginPwm - delta);
    }
  }

  driveStirrerPwm(nextPwm);
}

void slewPump(bool wantOn, unsigned long now) {
  if (wantOn != pumpTargetOn) {
    pumpTargetOn = wantOn;
    pumpRampOriginPwm = pumpPwm;
    pumpRampOriginMs = now;
  }

  unsigned long elapsed = now - pumpRampOriginMs;
  unsigned long delta = (elapsed * STIR_MAX_PWM) / STIR_RAMP_MS;

  uint8_t nextPwm = 0;
  if (pumpTargetOn) {
    unsigned long target = (unsigned long)pumpRampOriginPwm + delta;
    if (target > STIR_MAX_PWM) target = STIR_MAX_PWM;
    if (target == 0 && STIR_MAX_PWM > 0) target = 1;
    nextPwm = (uint8_t)target;
  } else {
    if (delta >= pumpRampOriginPwm) {
      nextPwm = 0;
    } else {
      nextPwm = (uint8_t)(pumpRampOriginPwm - delta);
    }
  }

  drivePumpPwm(nextPwm);
}

void forceAllOff() {
  driveHeater(false);
  pumpTargetOn = false;
  pumpRampOriginPwm = 0;
  drivePumpPwm(0);
  stirTargetOn = false;
  stirRampOriginPwm = 0;
  driveStirrerPwm(0);
}

int readWaterSensorAdc() {
  analogRead(WATER_SENSOR_PIN); // Discard first mux switch reading to settle S/H capacitor
  long sum = 0;
  for (int i = 0; i < WATER_ADC_SAMPLES; i++) {
    sum += analogRead(WATER_SENSOR_PIN);
  }
  return (int)(sum / WATER_ADC_SAMPLES);
}

// ==========================================
// DIRECT UART POLLER (Serial1 RX/TX)
// ==========================================
void sendSlavePing(byte id) {
  while (Serial1.available()) Serial1.read();
  Serial1.print("#");
  Serial1.print(id);
  Serial1.print(":PING*");
}

// ==========================================
// STIRRER STATE MACHINE
// ==========================================
void updateStirrer(unsigned long now) {
  if (currentMode == MODE_OFF) {
    stirTargetOn = false;
    stirRampOriginPwm = 0;
    driveStirrerPwm(0);
    return;
  }

  if (stirMode == 0) {
    // Manual OFF (3s soft-stop ramp down to 0)
    slewStirrer(false, now);
  } 
  else if (stirMode == 1) {
    // CONSTANT ON (3s soft-start ramp up to 255)
    slewStirrer(true, now);
  } 
  else if (stirMode == 2) {
    // PULSE INTERVAL (run for stirPulseSec every stirIntervalMin with 3s soft-start + 3s soft-stop)
    unsigned long periodMs = stirIntervalMin * 60000UL;
    unsigned long onWindowMs = stirPulseSec * 1000UL;
    if (periodMs == 0) periodMs = 60000UL;
    if (onWindowMs > periodMs) onWindowMs = periodMs;

    unsigned long elapsed = (now - stirCycleStart) % periodMs;
    unsigned long activeDriveMs = (onWindowMs > STIR_RAMP_MS)
                                    ? (onWindowMs - STIR_RAMP_MS)
                                    : (onWindowMs / 2);
    slewStirrer(elapsed < activeDriveMs, now);
  } 
  else if (stirMode == 3) {
    // TIMED RUN (run continuously for stirTimedMin with 3s soft-start + 3s soft-stop, then switch to OFF)
    unsigned long durationMs = stirTimedMin * 60000UL;
    unsigned long elapsed = now - stirCycleStart;
    if (elapsed < durationMs) {
      unsigned long activeDriveMs = (durationMs > STIR_RAMP_MS)
                                      ? (durationMs - STIR_RAMP_MS)
                                      : (durationMs / 2);
      slewStirrer(elapsed < activeDriveMs, now);
    } else {
      stirMode = 0;
      slewStirrer(false, now);
    }
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);  // USB to Raspberry Pi
  Serial1.begin(9600);   // Direct UART to Slave Mega Pro

  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(SYSTEM_SWITCH_PIN, INPUT_PULLUP);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(STIRRER_PIN, OUTPUT);
  digitalWrite(STIRRER_PIN, LOW);
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, LOW);

  forceAllOff();

  bathSensor.begin();
  bathSensor.setWaitForConversion(false);
  bathSensor.requestTemperatures();
  unsigned long bootNow = millis();
  lastTempRequest = bootNow;
  stirCycleStart = bootNow;

  waterRaw = readWaterSensorAdc();
  waterDepleted = (waterRaw < WATER_THRESHOLD);
  waterFaultCounter = waterDepleted ? WATER_FAULT_LIMIT : 0;
  lastWaterCheck = bootNow;
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  unsigned long now = millis();

  // 1. Non-Blocking Bath Temperature Read
  if (now - lastTempRequest >= TEMP_INTERVAL_MS) {
    float readVal = bathSensor.getTempCByIndex(0);
    if (readVal > -50.0 && readVal < 85.0 && readVal != 85.0) {
      waterTemp = readVal;
    } else {
      waterTemp = -999.0;
    }
    bathSensor.requestTemperatures();
    lastTempRequest = now;
  }

  // 2. Poll Slave Mega Pro (#1)
  if (now - lastSlavePollTime >= SLAVE_POLL_INTERVAL) {
    sendSlavePing(1);
    lastSlavePollTime = now;
  }

  // 3. Ingest, Clean, and Forward Slave Telemetry
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    if (c == '\n') {
      slaveRxBuffer.trim();
      if (slaveRxBuffer.length() > 0) {
        int jsonStart = slaveRxBuffer.indexOf('{');
        int jsonEnd = slaveRxBuffer.lastIndexOf('}');

        if (jsonStart >= 0 && jsonEnd > jsonStart) {
          String cleanJson = slaveRxBuffer.substring(jsonStart, jsonEnd + 1);
          Serial.println(cleanJson);

          StaticJsonDocument<1536> doc;
          DeserializationError err = deserializeJson(doc, cleanJson);

          if (!err) {
            float sumTemp = 0.0;
            int validCount = 0;

            if (doc.containsKey("bottles")) {
              JsonArray bottles = doc["bottles"];
              for (JsonObject b : bottles) {
                float t = b["temp"];
                if (t > 15.0 && t < 60.0) {
                  sumTemp += t;
                  validCount++;
                }
              }
            } else if (doc.containsKey("temp")) {
              float t = doc["temp"];
              if (t > 15.0 && t < 60.0) {
                sumTemp += t;
                validCount++;
              }
            }

            if (validCount > 0) {
              currentBottleTemp = sumTemp / validCount;
              bottleTelemetryValid = true;
              lastSlavePacketTime = now;
            }
          }
        }
      }
      slaveRxBuffer = "";
    } else {
      if (slaveRxBuffer.length() < 512) {
        slaveRxBuffer += c;
      } else {
        slaveRxBuffer = "";
      }
    }
  }

  // 4. Physical Switch Verification (Pin 6 pulled LOW forces OFF)
  if (digitalRead(SYSTEM_SWITCH_PIN) == LOW) {
    currentMode = MODE_OFF;
  }

  // 5. Time-Gated Debounced Water Level Verification (500ms cadence, 16x ADC avg)
  if (now - lastWaterCheck >= WATER_CHECK_MS) {
    waterRaw = readWaterSensorAdc();
    if (!waterDepleted) {
      if (waterRaw < WATER_THRESHOLD) {
        waterFaultCounter++;
        if (waterFaultCounter >= WATER_FAULT_LIMIT) {
          waterFaultCounter = WATER_FAULT_LIMIT;
          waterDepleted = true;
        }
      } else {
        waterFaultCounter = 0;
      }
    } else {
      if (waterRaw >= WATER_THRESHOLD) {
        if (waterFaultCounter > 0) waterFaultCounter--;
        if (waterFaultCounter == 0) {
          waterDepleted = false;
        }
      } else {
        waterFaultCounter = WATER_FAULT_LIMIT;
      }
    }
    lastWaterCheck = now;
  }

  bool bathSensorOk = (waterTemp > 0.0 && waterTemp < 85.0);
  bool slaveActive = ((now - lastSlavePacketTime) < SLAVE_TIMEOUT_MS);

  // 6. Master Mode, Overrides & Hysteresis Control
  if (currentMode == MODE_OFF || waterDepleted) {
    forceAllOff();
  } else {
    // --- PUMP LOGIC ---
    if (pumpOverride == 1) {
      slewPump(true, now);
    } else if (pumpOverride == 2) {
      slewPump(false, now);
    } else {
      // AUTO Pump: runs when bath & slave sensors are healthy
      slewPump(bathSensorOk && slaveActive, now);
    }

    // --- HEATER LOGIC ---
    bool overTempCeiling = (bathSensorOk && waterTemp >= MAX_WATER_BATH_TEMP);
    if (overTempCeiling || heaterOverride == 2) {
      driveHeater(false);
    } else if (heaterOverride == 1) {
      driveHeater(true);
    } else {
      // AUTO Heater: Hysteresis around targetBottleTemp
      if (!bathSensorOk || !slaveActive) {
        driveHeater(false);
      } else if (!heaterState && currentBottleTemp < (targetBottleTemp - bottleHysteresis)) {
        driveHeater(true);
      } else if (heaterState && currentBottleTemp >= targetBottleTemp) {
        driveHeater(false);
      }
    }

    // --- STIRRER LOGIC ---
    updateStirrer(now);
  }

  // 7. Pi USB Interface Commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "STATUS") {
      sendTelemetryStatus(waterRaw, now);
    } 
    else if (cmd == "MODE_AUTO") {
      if (currentMode == MODE_OFF) {
        stirCycleStart = now;
      }
      currentMode = MODE_AUTO;
    } 
    else if (cmd == "MODE_OFF") {
      currentMode = MODE_OFF;
      forceAllOff();
    }
    else if (cmd == "HEATER_AUTO") { heaterOverride = 0; }
    else if (cmd == "HEATER_ON")   { heaterOverride = 1; }
    else if (cmd == "HEATER_OFF")  { heaterOverride = 2; driveHeater(false); }
    else if (cmd == "PUMP_AUTO")   { pumpOverride = 0; }
    else if (cmd == "PUMP_ON")     { pumpOverride = 1; }
    else if (cmd == "PUMP_OFF")    { pumpOverride = 2; }
    else if (cmd.startsWith("STIR:")) {
      // Format: STIR:<mode_int>:<interval_or_timed_min>:<pulse_sec>
      int p1 = cmd.indexOf(':');
      int p2 = cmd.indexOf(':', p1 + 1);
      int p3 = cmd.indexOf(':', p2 + 1);
      if (p1 > 0 && p2 > p1 && p3 > p2) {
        int m = cmd.substring(p1 + 1, p2).toInt();
        long mins = cmd.substring(p2 + 1, p3).toInt();
        long secs = cmd.substring(p3 + 1).toInt();
        stirMode = constrain(m, 0, 3);
        if (mins > 0) {
          if (stirMode == 2) stirIntervalMin = mins;
          if (stirMode == 3) stirTimedMin = mins;
        }
        if (secs > 0) stirPulseSec = secs;
        stirCycleStart = now;
        updateStirrer(now);
      }
    }
    else if (cmd.startsWith("TEMP:")) {
      // Format: TEMP:<target_float>:<hyst_float>
      int p1 = cmd.indexOf(':');
      int p2 = cmd.indexOf(':', p1 + 1);
      if (p1 > 0 && p2 > p1) {
        float t = cmd.substring(p1 + 1, p2).toFloat();
        float h = cmd.substring(p2 + 1).toFloat();
        if (t >= 20.0 && t <= 50.0) targetBottleTemp = t;
        if (h >= 0.1 && h <= 5.0)   bottleHysteresis = h;
      }
    }
  }
}

void sendTelemetryStatus(int waterRaw, unsigned long now) {
  StaticJsonDocument<448> doc;
  doc["type"] = "master_status";
  doc["water_temp"] = (waterTemp > -50.0) ? serialized(String(waterTemp, 2)) : serialized(String(-999.0));
  doc["bottle_temp_avg"] = bottleTelemetryValid ? serialized(String(currentBottleTemp, 2)) : serialized(String(-999.0));
  doc["heater"] = heaterState ? 1 : 0;
  doc["pump"] = pumpState ? 1 : 0;
  doc["stirrer"] = stirrerState ? 1 : 0;
  doc["heater_mode"] = (heaterOverride == 1) ? "ON" : ((heaterOverride == 2) ? "OFF" : "AUTO");
  doc["pump_mode"] = (pumpOverride == 1) ? "ON" : ((pumpOverride == 2) ? "OFF" : "AUTO");

  const char* sModes[] = {"OFF", "ON", "PULSE", "TIMED"};
  doc["stir_mode"] = sModes[stirMode];
  doc["stir_int_min"] = stirIntervalMin;
  doc["stir_pulse_sec"] = stirPulseSec;
  doc["stir_timed_min"] = stirTimedMin;

  // Remaining seconds for Timed Run mode
  long remSec = 0;
  if (stirMode == 3) {
    unsigned long durMs = stirTimedMin * 60000UL;
    unsigned long el = now - stirCycleStart;
    if (el < durMs) remSec = (durMs - el) / 1000UL;
  }
  doc["stir_rem_sec"] = remSec;

  doc["target_temp"] = serialized(String(targetBottleTemp, 1));
  doc["hysteresis"] = serialized(String(bottleHysteresis, 1));
  doc["water_low"] = waterDepleted ? 1 : 0;
  doc["water_adc"] = waterRaw;
  doc["water_thr"] = WATER_THRESHOLD;
  doc["system_mode"] = (currentMode == MODE_AUTO) ? "AUTO" : "OFF";

  serializeJson(doc, Serial);
  Serial.println();
}
