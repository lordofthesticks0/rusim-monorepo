#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define TEMP_SENSOR_PIN   2     // DS18B20 Water bath sensor
#define PUMP_PIN          3     // Circulation pump MOSFET (Active-HIGH, R385 12VDC diaphragm pump)
#define FAN_PIN           4     // Exhaust fan
#define HEATER_PIN        5     // 300W Heater SSR (Active HIGH trigger, 0V = OFF)
#define SYSTEM_SWITCH_PIN 6     // Optional physical AUTO / OFF toggle
#define RS485_CTRL_PIN    8     // MAX485 DE/RE control pin
#define WATER_SENSOR_PIN  A7    // Analog water level probe

// ==========================================
// CONTROL PARAMETERS
// ==========================================
#define TARGET_BOTTLE_TEMP   39.0   // Core liquid target (°C)
#define BOTTLE_HYSTERESIS    0.3    // Heat on < 38.7°C, off >= 39.0°C
#define MAX_WATER_BATH_TEMP  39.8   // Hard bath ceiling clamp (°C)
#define FAN_EMERGENCY_TEMP   45.0   // Overheat exhaust threshold (°C)
#define WATER_THRESHOLD      500    // Minimum dry-run ADC threshold
#define WATER_FAULT_LIMIT    5      // 5 consecutive cycles to trip
#define SLAVE_TIMEOUT_MS     12000  // Failsafe timeout if Slave stops responding
#define TEMP_INTERVAL_MS     750    // Non-blocking DS18B20 cadence
#define SLAVE_POLL_INTERVAL  3500   // Acquisition cadence matching Slave cycle time

enum SystemMode { MODE_OFF, MODE_AUTO };
SystemMode currentMode = MODE_AUTO;

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature bathSensor(&oneWire);

// State Variables
float waterTemp = -999.0;
float currentBottleTemp = -999.0;
bool heaterState = false;
bool pumpState = false;
bool bottleTelemetryValid = false;
int waterFaultCounter = 0;

unsigned long lastTempRequest = 0;
unsigned long lastSlavePollTime = 0;
unsigned long lastSlavePacketTime = 0;
String slaveRxBuffer = "";

// ==========================================
// ACTUATOR DRIVERS
// ==========================================
void drivePump(bool state) {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, state ? HIGH : LOW);
  pumpState = state;
}

void driveHeater(bool state) {
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, state ? HIGH : LOW);
  heaterState = state;
}

void forceAllOff() {
  driveHeater(false);
  drivePump(false);
  digitalWrite(FAN_PIN, LOW);
}

// ==========================================
// RS485 POLLER (PROVEN HARDWARE TIMING)
// ==========================================
void sendSlavePing(byte id) {
  while (Serial1.available()) Serial1.read();

  digitalWrite(RS485_CTRL_PIN, HIGH);
  delay(2);

  Serial1.print("#");
  Serial1.print(id);
  Serial1.print(":PING*");

  Serial1.flush();
  delay(5); // Stop-bit clearance window

  digitalWrite(RS485_CTRL_PIN, LOW);
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);  // USB to Raspberry Pi
  Serial1.begin(9600);   // RS485 to Slave Mega Pro

  pinMode(RS485_CTRL_PIN, OUTPUT);
  digitalWrite(RS485_CTRL_PIN, LOW);

  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(SYSTEM_SWITCH_PIN, INPUT_PULLUP);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  forceAllOff();

  bathSensor.begin();
  bathSensor.setWaitForConversion(false);
  bathSensor.requestTemperatures();
  lastTempRequest = millis();
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

  // 2. Poll Single Slave Mega Pro (#1)
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

// 5. Debounced Water Level Verification (Virtual Bypass)
  int waterRaw = 1023;
  waterFaultCounter = 0;
  bool waterDepleted = false;

  bool bathSensorOk = (waterTemp > 0.0 && waterTemp < 85.0);
  bool slaveActive = ((now - lastSlavePacketTime) < SLAVE_TIMEOUT_MS);

  // 6. Actuator Logic & Emergency Cutoffs
  if (currentMode == MODE_OFF || waterDepleted || !bathSensorOk || !slaveActive) {
    forceAllOff();
  } 
  else if (currentMode == MODE_AUTO) {
    drivePump(true);

    if (waterTemp >= MAX_WATER_BATH_TEMP) {
      driveHeater(false);
    } 
    else if (!heaterState && currentBottleTemp < (TARGET_BOTTLE_TEMP - BOTTLE_HYSTERESIS)) {
      driveHeater(true);
    } 
    else if (heaterState && currentBottleTemp >= TARGET_BOTTLE_TEMP) {
      driveHeater(false);
    }

    if (waterTemp >= FAN_EMERGENCY_TEMP) {
      digitalWrite(FAN_PIN, HIGH);
    } else {
      digitalWrite(FAN_PIN, LOW);
    }
  }

  // 7. Pi USB Interface Commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "STATUS") {
      sendTelemetryStatus(waterRaw);
    } 
    else if (cmd == "MODE_AUTO") {
      currentMode = MODE_AUTO;
    } 
    else if (cmd == "MODE_OFF") {
      currentMode = MODE_OFF;
      forceAllOff();
    }
  }
}

void sendTelemetryStatus(int waterRaw) {
  StaticJsonDocument<256> doc;
  doc["type"] = "master_status";
  doc["water_temp"] = (waterTemp > -50.0) ? serialized(String(waterTemp, 2)) : serialized(String(-999.0));
  doc["bottle_temp_avg"] = bottleTelemetryValid ? serialized(String(currentBottleTemp, 2)) : serialized(String(-999.0));
  doc["heater"] = heaterState ? 1 : 0;
  doc["pump"] = pumpState ? 1 : 0;
  doc["fan"] = digitalRead(FAN_PIN);
  doc["water_low"] = (waterFaultCounter >= WATER_FAULT_LIMIT) ? 1 : 0;
  doc["water_adc"] = waterRaw;
  doc["system_mode"] = (currentMode == MODE_AUTO) ? "AUTO" : "OFF";

  serializeJson(doc, Serial);
  Serial.println();
}
