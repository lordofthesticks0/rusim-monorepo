#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define TEMP_SENSOR_PIN   2     // DS18B20 Water bath sensor
#define PUMP1_PIN         3     // Circulation Pump 1 MOSFET (Active-HIGH, PWM Timer 3 OC3C)
#define PUMP_PIN          3     // Alias for Pump 1 pin
#define STIRRER_PIN       4     // Stirrer MOSFET (Active-HIGH, PWM Timer 0 OC0B)
#define HEATER_PIN        5     // 300W Heater SSR (Active-HIGH trigger, 0V = OFF)
#define SYSTEM_SWITCH_PIN 6     // Physical AUTO / OFF toggle
#define PUMP2_PIN         7     // Circulation Pump 2 MOSFET (Active-HIGH, PWM Timer 4 OC4B)
#define WATER_SENSOR_PIN  A7    // Analog water bath level probe (Dry fault when < 200)
#define LEAK_SENSOR_PIN   A8    // Analog pump leak detection probe (Wet fault when >= 200)

// ==========================================
// CONTROL PARAMETERS & DYNAMIC TARGETS
// ==========================================
float targetBottleTemp  = 39.0; // Adjustable target bottle average temp (°C)
float bottleHysteresis  = 0.3;  // Adjustable hysteresis band (°C)
#define MAX_WATER_BATH_TEMP  41.0   // Hard safety bath ceiling clamp (°C)
#define WATER_THRESHOLD      200    // Minimum bath water ADC threshold (~0.98V on 5V ADC)
#define ENABLE_LEAK_SENSOR   0      // 0 = disabled (only A7 water presence sensor installed); 1 = enable A8 leak probe
#define LEAK_THRESHOLD       200    // Pump leak wet-detection ADC threshold (~0.98V on 5V ADC)
#define WATER_FAULT_LIMIT    5      // 5 consecutive 500ms checks (2.5s) to trip/clear
#define WATER_CHECK_MS       500UL  // Time-gated water & leak sampling interval (ms)
#define WATER_ADC_SAMPLES    16     // Number of ADC samples averaged per check
#define SLAVE_TIMEOUT_MS     12000  // Failsafe timeout if Slave stops responding
#define TEMP_INTERVAL_MS     750    // Non-blocking DS18B20 cadence
#define SLAVE_POLL_INTERVAL  3500   // Acquisition cadence matching Slave cycle time
#define STIR_RAMP_MS         3000UL // 3-second soft-start / soft-stop PWM ramp
#define DEFAULT_MAX_PWM      255U   // Default full-scale PWM ceiling (0-255)

enum SystemMode { MODE_OFF, MODE_AUTO };
SystemMode currentMode = MODE_AUTO;

// Per-Actuator Override Modes: 0 = AUTO, 1 = FORCE ON, 2 = FORCE OFF
// Boot default: heater and both pumps start FORCE OFF.
uint8_t heaterOverride = 2;
uint8_t pump1Override  = 2;
uint8_t pump2Override  = 2;

// Stirrer Modes: 0 = OFF, 1 = CONSTANT ON, 2 = PULSE INTERVAL, 3 = TIMED RUN
uint8_t stirMode = 0;
unsigned long stirIntervalMin = 10;   // Default: Pulse every 10 minutes
unsigned long stirPulseSec    = 30;   // Default: 30-second pulse duration
unsigned long stirTimedMin    = 15;   // Default: 15-minute timed continuous run
unsigned long stirCycleStart  = 0;    // Timestamp tracker for Pulse / Timed modes

// Unified non-blocking PWM slew-rate state for all 3 MOSFET channels
struct MosfetChannel {
  uint8_t pin;
  uint8_t maxPwm;         // Configurable 0-255 target ceiling for debit testing
  uint8_t currentPwm;     // Instantaneous output PWM (0-255)
  uint8_t targetPwm;      // Active slewing target PWM (0 or maxPwm)
  uint8_t rampOriginPwm;  // PWM value when current ramp transition started
  unsigned long rampOriginMs;
  bool state;             // true when currentPwm > 0
};

MosfetChannel pump1Mosfet = { PUMP1_PIN,   DEFAULT_MAX_PWM, 0, 0, 0, 0, false };
MosfetChannel pump2Mosfet = { PUMP2_PIN,   DEFAULT_MAX_PWM, 0, 0, 0, 0, false };
MosfetChannel stirMosfet  = { STIRRER_PIN, DEFAULT_MAX_PWM, 0, 0, 0, 0, false };

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature bathSensor(&oneWire);

// State Variables
float waterTemp = -999.0;
float currentBottleTemp = -999.0;
bool heaterState = false;
bool bottleTelemetryValid = false;

int waterRaw = 0;
int waterFaultCounter = 0;
bool waterDepleted = false;

int leakRaw = -1;
int leakFaultCounter = 0;
bool pumpLeakDetected = false;

unsigned long lastTempRequest = 0;
unsigned long lastWaterCheck = 0;
unsigned long lastSlavePollTime = 0;
unsigned long lastSlavePacketTime = 0;
String slaveRxBuffer = "";

// Forward declaration
void sendTelemetryStatus(unsigned long now);

// ==========================================
// ACTUATOR DRIVERS
// ==========================================
void driveMosfetPwm(MosfetChannel &ch, uint8_t pwm) {
  pinMode(ch.pin, OUTPUT);
  analogWrite(ch.pin, pwm);
  ch.currentPwm = pwm;
  ch.state = (pwm > 0);
}

void resetMosfetOff(MosfetChannel &ch) {
  ch.targetPwm = 0;
  ch.rampOriginPwm = 0;
  driveMosfetPwm(ch, 0);
}

void slewMosfet(MosfetChannel &ch, bool wantOn, unsigned long now) {
  uint8_t desiredTarget = wantOn ? ch.maxPwm : 0;
  if (desiredTarget != ch.targetPwm) {
    ch.targetPwm = desiredTarget;
    ch.rampOriginPwm = ch.currentPwm;
    ch.rampOriginMs = now;
  }

  uint8_t refSpan = ch.maxPwm;
  if (ch.rampOriginPwm > refSpan) {
    refSpan = ch.rampOriginPwm;
  }
  if (refSpan == 0) {
    driveMosfetPwm(ch, 0);
    return;
  }

  unsigned long elapsed = now - ch.rampOriginMs;
  unsigned long delta = (elapsed * (unsigned long)refSpan) / STIR_RAMP_MS;

  uint8_t nextPwm = 0;
  if (ch.targetPwm >= ch.rampOriginPwm) {
    unsigned long stepped = (unsigned long)ch.rampOriginPwm + delta;
    if (stepped > ch.targetPwm) stepped = ch.targetPwm;
    if (stepped == 0 && ch.targetPwm > 0) stepped = 1;
    nextPwm = (uint8_t)stepped;
  } else {
    uint8_t diff = ch.rampOriginPwm - ch.targetPwm;
    if (delta >= diff) {
      nextPwm = ch.targetPwm;
    } else {
      nextPwm = (uint8_t)(ch.rampOriginPwm - delta);
    }
  }

  driveMosfetPwm(ch, nextPwm);
}

void driveHeater(bool state) {
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, state ? HIGH : LOW);
  heaterState = state;
}

void forceAllOff() {
  driveHeater(false);
  resetMosfetOff(pump1Mosfet);
  resetMosfetOff(pump2Mosfet);
  resetMosfetOff(stirMosfet);
}

int readAdcAveraged(uint8_t pin) {
  analogRead(pin); // Discard first mux switch reading to settle S/H capacitor
  long sum = 0;
  for (int i = 0; i < WATER_ADC_SAMPLES; i++) {
    sum += analogRead(pin);
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
    resetMosfetOff(stirMosfet);
    return;
  }

  if (stirMode == 0) {
    // Manual OFF (3s soft-stop ramp down to 0)
    slewMosfet(stirMosfet, false, now);
  } 
  else if (stirMode == 1) {
    // CONSTANT ON (3s soft-start ramp up to stirMosfet.maxPwm)
    slewMosfet(stirMosfet, true, now);
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
    slewMosfet(stirMosfet, elapsed < activeDriveMs, now);
  } 
  else if (stirMode == 3) {
    // TIMED RUN (run continuously for stirTimedMin with 3s soft-start + 3s soft-stop, then switch to OFF)
    unsigned long durationMs = stirTimedMin * 60000UL;
    unsigned long elapsed = now - stirCycleStart;
    if (elapsed < durationMs) {
      unsigned long activeDriveMs = (durationMs > STIR_RAMP_MS)
                                      ? (durationMs - STIR_RAMP_MS)
                                      : (durationMs / 2);
      slewMosfet(stirMosfet, elapsed < activeDriveMs, now);
    } else {
      stirMode = 0;
      slewMosfet(stirMosfet, false, now);
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
#if ENABLE_LEAK_SENSOR
  pinMode(LEAK_SENSOR_PIN, INPUT);
#endif
  pinMode(SYSTEM_SWITCH_PIN, INPUT_PULLUP);
  pinMode(PUMP1_PIN, OUTPUT);
  digitalWrite(PUMP1_PIN, LOW);
  pinMode(PUMP2_PIN, OUTPUT);
  digitalWrite(PUMP2_PIN, LOW);
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

  waterRaw = readAdcAveraged(WATER_SENSOR_PIN);
  waterDepleted = (waterRaw < WATER_THRESHOLD);
  waterFaultCounter = waterDepleted ? WATER_FAULT_LIMIT : 0;

#if ENABLE_LEAK_SENSOR
  leakRaw = readAdcAveraged(LEAK_SENSOR_PIN);
  pumpLeakDetected = (leakRaw >= LEAK_THRESHOLD);
  leakFaultCounter = pumpLeakDetected ? WATER_FAULT_LIMIT : 0;
#else
  leakRaw = -1;
  pumpLeakDetected = false;
  leakFaultCounter = 0;
#endif

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

  // 5. Time-Gated Debounced Water Level (A7) & Pump Leak (A8) Verification (500ms cadence, 16x ADC avg)
  if (now - lastWaterCheck >= WATER_CHECK_MS) {
    // 5a. Bath Water Level Probe (A7: dry fault when ADC < WATER_THRESHOLD)
    waterRaw = readAdcAveraged(WATER_SENSOR_PIN);
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

    // 5b. Pump Leak Detection Sensor (A8: wet fault when ADC >= LEAK_THRESHOLD, disabled when ENABLE_LEAK_SENSOR == 0)
#if ENABLE_LEAK_SENSOR
    leakRaw = readAdcAveraged(LEAK_SENSOR_PIN);
    if (!pumpLeakDetected) {
      if (leakRaw >= LEAK_THRESHOLD) {
        leakFaultCounter++;
        if (leakFaultCounter >= WATER_FAULT_LIMIT) {
          leakFaultCounter = WATER_FAULT_LIMIT;
          pumpLeakDetected = true;
        }
      } else {
        leakFaultCounter = 0;
      }
    } else {
      if (leakRaw < LEAK_THRESHOLD) {
        if (leakFaultCounter > 0) leakFaultCounter--;
        if (leakFaultCounter == 0) {
          pumpLeakDetected = false;
        }
      } else {
        leakFaultCounter = WATER_FAULT_LIMIT;
      }
    }
#else
    leakRaw = -1;
    pumpLeakDetected = false;
    leakFaultCounter = 0;
#endif

    lastWaterCheck = now;
  }

  bool bathSensorOk = (waterTemp > 0.0 && waterTemp < 85.0);
  bool slaveActive = ((now - lastSlavePacketTime) < SLAVE_TIMEOUT_MS);

  // 6. Master Mode, Safety Cutoffs (Bath Low or Pump Leak), Overrides & Hysteresis Control
  if (currentMode == MODE_OFF || waterDepleted || pumpLeakDetected) {
    forceAllOff();
  } else {
    // --- PUMP 1 LOGIC (Pin D3) ---
    if (pump1Override == 1) {
      slewMosfet(pump1Mosfet, true, now);
    } else if (pump1Override == 2) {
      slewMosfet(pump1Mosfet, false, now);
    } else {
      slewMosfet(pump1Mosfet, bathSensorOk && slaveActive, now);
    }

    // --- PUMP 2 LOGIC (Pin D7) ---
    if (pump2Override == 1) {
      slewMosfet(pump2Mosfet, true, now);
    } else if (pump2Override == 2) {
      slewMosfet(pump2Mosfet, false, now);
    } else {
      slewMosfet(pump2Mosfet, bathSensorOk && slaveActive, now);
    }

    // --- HEATER LOGIC (Pin D5) ---
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

    // --- STIRRER LOGIC (Pin D4) ---
    updateStirrer(now);
  }

  // 7. Pi USB Interface Commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "STATUS") {
      sendTelemetryStatus(now);
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
    else if (cmd == "PUMP_AUTO")   { pump1Override = 0; pump2Override = 0; }
    else if (cmd == "PUMP_ON")     { pump1Override = 1; pump2Override = 1; }
    else if (cmd == "PUMP_OFF")    { pump1Override = 2; pump2Override = 2; }
    else if (cmd == "PUMP1_AUTO")  { pump1Override = 0; }
    else if (cmd == "PUMP1_ON")    { pump1Override = 1; }
    else if (cmd == "PUMP1_OFF")   { pump1Override = 2; }
    else if (cmd == "PUMP2_AUTO")  { pump2Override = 0; }
    else if (cmd == "PUMP2_ON")    { pump2Override = 1; }
    else if (cmd == "PUMP2_OFF")   { pump2Override = 2; }
    else if (cmd.startsWith("PWM:")) {
      // Format: PWM:<D3|D7|D4|PUMP1|PUMP2|STIR>:<0-255>
      int p1 = cmd.indexOf(':');
      int p2 = cmd.indexOf(':', p1 + 1);
      if (p1 > 0 && p2 > p1) {
        String chName = cmd.substring(p1 + 1, p2);
        chName.toUpperCase();
        int val = constrain(cmd.substring(p2 + 1).toInt(), 0, 255);
        if (chName == "D3" || chName == "3" || chName == "PUMP1") {
          pump1Mosfet.maxPwm = (uint8_t)val;
        } else if (chName == "D7" || chName == "7" || chName == "PUMP2") {
          pump2Mosfet.maxPwm = (uint8_t)val;
        } else if (chName == "D4" || chName == "4" || chName == "STIR" || chName == "STIRRER") {
          stirMosfet.maxPwm = (uint8_t)val;
        }
      }
    }
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

void sendTelemetryStatus(unsigned long now) {
  StaticJsonDocument<640> doc;
  doc["type"] = "master_status";
  doc["water_temp"] = (waterTemp > -50.0) ? serialized(String(waterTemp, 2)) : serialized(String(-999.0));
  doc["bottle_temp_avg"] = bottleTelemetryValid ? serialized(String(currentBottleTemp, 2)) : serialized(String(-999.0));
  doc["heater"] = heaterState ? 1 : 0;
  doc["pump"] = (pump1Mosfet.state || pump2Mosfet.state) ? 1 : 0;
  doc["pump1"] = pump1Mosfet.state ? 1 : 0;
  doc["pump2"] = pump2Mosfet.state ? 1 : 0;
  doc["stirrer"] = stirMosfet.state ? 1 : 0;
  doc["heater_mode"] = (heaterOverride == 1) ? "ON" : ((heaterOverride == 2) ? "OFF" : "AUTO");
  doc["pump_mode"] = (pump1Override == 1) ? "ON" : ((pump1Override == 2) ? "OFF" : "AUTO");
  doc["pump1_mode"] = (pump1Override == 1) ? "ON" : ((pump1Override == 2) ? "OFF" : "AUTO");
  doc["pump2_mode"] = (pump2Override == 1) ? "ON" : ((pump2Override == 2) ? "OFF" : "AUTO");

  doc["pwm_d3"] = pump1Mosfet.maxPwm;
  doc["pwm_d7"] = pump2Mosfet.maxPwm;
  doc["pwm_d4"] = stirMosfet.maxPwm;
  doc["out_d3"] = pump1Mosfet.currentPwm;
  doc["out_d7"] = pump2Mosfet.currentPwm;
  doc["out_d4"] = stirMosfet.currentPwm;

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
  doc["leak_enabled"] = ENABLE_LEAK_SENSOR;
  doc["pump_leak"] = pumpLeakDetected ? 1 : 0;
  doc["leak_adc"] = leakRaw;
  doc["leak_thr"] = LEAK_THRESHOLD;
  doc["system_mode"] = (currentMode == MODE_AUTO) ? "AUTO" : "OFF";

  serializeJson(doc, Serial);
  Serial.println();
}
