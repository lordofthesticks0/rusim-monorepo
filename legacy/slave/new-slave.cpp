/*
 * PROJECT: Bio-Array Slave (ID 1) - Calibrated Multi-Sensor Firmware
 * PROTOCOL: Responds to "#1:PING*" on Serial1 with 4-Bottle JSON Telemetry
 */

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HX711.h>
#include <ArduinoJson.h>

// ==========================================
// 1. SLAVE & PIN CONFIGURATION
// ==========================================
#define SLAVE_ID 1
const int NUM_BOTTLES = 4;

const int PIN_PH[NUM_BOTTLES]        = {A2, A5, A8, A11};
const int PIN_METHANE[NUM_BOTTLES]   = {A1, A4, A7, A10};
const int PIN_TEMP[NUM_BOTTLES]      = {4, 7, 10, 13};
const int PIN_CO2_PWM[NUM_BOTTLES]   = {14, 15, 16, 17};
const int PIN_PRESS_OUT[NUM_BOTTLES] = {2, 5, 8, 11};
const int PIN_PRESS_SCK[NUM_BOTTLES] = {3, 6, 9, 12};

// ==========================================
// 2. METHANE (TGS2611) CALIBRATION CONSTANTS
// ==========================================
const float RL[NUM_BOTTLES]      = {1000.0f, 1000.0f, 1000.0f, 1000.0f};
const int   ADC_SAMPLES          = 32;
const float ADC_SCALE            = 1024.0f;
const float PPM_MIN_TYPICAL      = 500.0f;
const float PPM_MAX              = 10000.0f;

const float RS_AIR[NUM_BOTTLES]  = {6160.3f, 5370.7f, 8592.1f, 7950.2f};
const float RS_SPAN[NUM_BOTTLES] = {259.8083f, 370.7250f, 382.4533f, 300.8875f};
const float PPM_AIR              = 2.0f;
const float PPM_SPAN             = 442.0f;

float curve_exp[NUM_BOTTLES];
float rs_at_max[NUM_BOTTLES];
bool  ch4_cal_ok[NUM_BOTTLES];

// ==========================================
// 3. CO2 (MH-Z19C PWM) CALIBRATION CONSTANTS
// ==========================================
constexpr unsigned long kWaitTimeoutUs = 2000000UL;
constexpr unsigned long kPeriodMinUs   = 950000UL;
constexpr unsigned long kPeriodMaxUs   = 1060000UL;
constexpr unsigned long kHighMinUs     = 1000UL;
constexpr unsigned long kHighMaxUs     = 1002000UL;
constexpr float         kPwmRangePpm   = 5000.0f;

// ==========================================
// 4. PRESSURE (HX711) CALIBRATION CONSTANTS
// ==========================================
const unsigned long HX711_INIT_TIMEOUT_MS = 1500;
const unsigned long HX711_READ_TIMEOUT_MS = 250;
const unsigned long HX711_POLL_MS         = 5;
const float PA_PER_UNIT                   = 100.0f;

const float CALIBRATION_FACTORS[NUM_BOTTLES] = {
  581040.0f,         // Bottle 1 (Sensor 0)
  70802.7743f,       // Bottle 2 (Sensor 1)
  82656.645503906f,  // Bottle 3 (Sensor 2)
  60223.70030581f    // Bottle 4 (Sensor 3)
};

// ==========================================
// 5. SENSOR OBJECTS & CACHED TELEMETRY
// ==========================================
OneWire oneWire1(PIN_TEMP[0]); DallasTemperature temp1(&oneWire1);
OneWire oneWire2(PIN_TEMP[1]); DallasTemperature temp2(&oneWire2);
OneWire oneWire3(PIN_TEMP[2]); DallasTemperature temp3(&oneWire3);
OneWire oneWire4(PIN_TEMP[3]); DallasTemperature temp4(&oneWire4);
DallasTemperature* tempSensors[NUM_BOTTLES] = {&temp1, &temp2, &temp3, &temp4};

HX711 press1; HX711 press2; HX711 press3; HX711 press4;
HX711* pressureSensors[NUM_BOTTLES] = {&press1, &press2, &press3, &press4};
bool pressInitialized[NUM_BOTTLES]  = {false, false, false, false};

// Latest valid readings sent immediately upon Master "#1:PING*"
float cachedPh[NUM_BOTTLES]      = {-1.0f, -1.0f, -1.0f, -1.0f};
float cachedMethane[NUM_BOTTLES] = {-1.0f, -1.0f, -1.0f, -1.0f};
float cachedTemp[NUM_BOTTLES]    = {-999.0f, -999.0f, -999.0f, -999.0f};
long  cachedCo2[NUM_BOTTLES]     = {-1, -1, -1, -1};
float cachedPress[NUM_BOTTLES]   = {-1.0f, -1.0f, -1.0f, -1.0f};

unsigned long lastTempReqMs = 0;
bool pingHandledDuringCo2   = false;

// Forward declarations
void serviceMasterPing();
void sendTelemetryJson();

// ==========================================
// METHANE FUNCTIONS
// ==========================================
bool fitExponent(float rs_low, float ppm_low, float rs_high,
                 float ppm_high, float &exponent) {
  if (!isfinite(rs_low) || !isfinite(rs_high) ||
      !isfinite(ppm_low) || !isfinite(ppm_high) ||
      rs_high <= 0.0f || rs_low <= rs_high ||
      ppm_low <= 0.0f || ppm_high <= ppm_low ||
      ppm_high > PPM_MAX) return false;

  exponent = log(ppm_high / ppm_low) / log(rs_low / rs_high);
  return isfinite(exponent) && exponent > 0.0f;
}

void updateAllMethane() {
  long sum[NUM_BOTTLES] = {0};
  bool adc_clipped[NUM_BOTTLES] = {false};

  for (int sample = 0; sample < ADC_SAMPLES; sample++) {
    for (int i = 0; i < NUM_BOTTLES; i++) {
      analogRead(PIN_METHANE[i]); // Discard first mux switch reading
      int raw = analogRead(PIN_METHANE[i]);
      if (raw <= 0 || raw >= 1023) adc_clipped[i] = true;
      sum[i] += raw;
    }
    delay(2);
    serviceMasterPing();
  }

  for (int i = 0; i < NUM_BOTTLES; i++) {
    if (adc_clipped[i] || !isfinite(RL[i]) || RL[i] <= 0.0f || !ch4_cal_ok[i]) {
      cachedMethane[i] = -1.0f;
      continue;
    }

    float raw_avg = sum[i] / float(ADC_SAMPLES);
    float rs = (ADC_SCALE / raw_avg - 1.0f) * RL[i];

    if (!isfinite(rs) || rs <= 0.0f || rs < rs_at_max[i]) {
      cachedMethane[i] = -1.0f;
      continue;
    }

    if (rs == rs_at_max[i]) {
      cachedMethane[i] = PPM_MAX;
      continue;
    }

    float ppm = PPM_SPAN * pow(RS_SPAN[i] / rs, curve_exp[i]);
    if (!isfinite(ppm) || ppm <= 0.0f || ppm > PPM_MAX) {
      cachedMethane[i] = -1.0f;
    } else {
      cachedMethane[i] = ppm;
    }
  }
}

// ==========================================
// CO2 PWM FUNCTIONS
// ==========================================
static bool waitForLevel(uint8_t pin, uint8_t level) {
  const unsigned long start = micros();
  while (digitalRead(pin) != level) {
    if (Serial1.available()) {
      serviceMasterPing();
      pingHandledDuringCo2 = true;
    }
    if (micros() - start > kWaitTimeoutUs) {
      return false;
    }
  }
  return true;
}

static bool readCo2Ppm(uint8_t pin, float &ppmOut) {
  pingHandledDuringCo2 = false;

  if (!waitForLevel(pin, LOW))  return false;
  if (!waitForLevel(pin, HIGH)) return false;
  const unsigned long tRise = micros();

  if (!waitForLevel(pin, LOW))  return false;
  const unsigned long tFall = micros();

  if (!waitForLevel(pin, HIGH)) return false;
  const unsigned long tRise2 = micros();

  const unsigned long highUs   = tFall - tRise;
  const unsigned long lowUs    = tRise2 - tFall;
  const unsigned long periodUs = highUs + lowUs;

  if (periodUs < kPeriodMinUs || periodUs > kPeriodMaxUs) return false;
  if (highUs < kHighMinUs || highUs > kHighMaxUs)         return false;

  const float periodMs = static_cast<float>(periodUs) / 1000.0f;
  const float highMs   = static_cast<float>(highUs) / 1000.0f;
  ppmOut = kPwmRangePpm * (highMs - 2.0f) / (periodMs - 4.0f);
  if (ppmOut < 0.0f) ppmOut = 0.0f;
  return true;
}

// ==========================================
// PRESSURE (HX711) FUNCTIONS
// ==========================================
bool waitHx711Ready(int idx, unsigned long timeoutMs) {
  unsigned long start = millis();
  while (!pressureSensors[idx]->is_ready()) {
    serviceMasterPing();
    if (millis() - start >= timeoutMs) return false;
    delay(HX711_POLL_MS);
  }
  return true;
}

bool initPressureSensor(int idx) {
  pressureSensors[idx]->begin(PIN_PRESS_OUT[idx], PIN_PRESS_SCK[idx]);
  if (!waitHx711Ready(idx, HX711_INIT_TIMEOUT_MS)) {
    return false;
  }
  pressureSensors[idx]->set_scale(CALIBRATION_FACTORS[idx]);
  pressureSensors[idx]->tare();
  return true;
}

void updatePressure(int idx) {
  if (!pressInitialized[idx]) {
    pressInitialized[idx] = initPressureSensor(idx);
    if (!pressInitialized[idx]) {
      cachedPress[idx] = -1.0f;
      return;
    }
  }

  if (waitHx711Ready(idx, HX711_READ_TIMEOUT_MS)) {
    // Average 5 samples while keeping RS485 responsive
    float sensorValue = pressureSensors[idx]->get_units(5);
    cachedPress[idx] = sensorValue * PA_PER_UNIT;
  }
}

// ==========================================
// TEMPERATURE & PH FUNCTIONS
// ==========================================
void updatePhAndTemp() {
  for (int i = 0; i < NUM_BOTTLES; i++) {
    int phRaw = analogRead(PIN_PH[i]);
    cachedPh[i] = (phRaw == 0) ? -1.0f : phRaw * (5.0f / 1023.0f);
  }

  unsigned long now = millis();
  if (now - lastTempReqMs >= 800) {
    for (int i = 0; i < NUM_BOTTLES; i++) {
      float tempC = tempSensors[i]->getTempCByIndex(0);
      cachedTemp[i] = (tempC < -50.0f || tempC == 85.0f) ? -999.0f : tempC;
      tempSensors[i]->requestTemperatures();
      serviceMasterPing();
    }
    lastTempReqMs = now;
  }
}

// ==========================================
// RS485 PING HANDLER & JSON SERIALIZER
// ==========================================
void serviceMasterPing() {
  if (!Serial1.available()) return;

  String msg = "";
  unsigned long start = millis();

  // 50ms window to capture complete "#1:PING*" frame
  while (millis() - start < 50) {
    if (Serial1.available()) {
      char c = Serial1.read();
      if (c == '*') break;
      msg += c;
    }
  }

  int hashPos = msg.indexOf('#');
  if (hashPos >= 0) {
    msg = msg.substring(hashPos);
    int colonPos = msg.indexOf(':');
    if (colonPos > 0) {
      byte targetID = msg.substring(1, colonPos).toInt();
      String command = msg.substring(colonPos + 1);
      command.trim();

      if (targetID == SLAVE_ID && command == "PING") {
        sendTelemetryJson();
      }
    }
  }
}

void sendTelemetryJson() {
  StaticJsonDocument<1536> doc;
  JsonArray bottles = doc.createNestedArray("bottles");

  for (int i = 0; i < NUM_BOTTLES; i++) {
    JsonObject bottle = bottles.createNestedObject();
    bottle["id"]      = i + 1;
    bottle["ph"]      = cachedPh[i];
    bottle["methane"] = cachedMethane[i];
    bottle["temp"]    = cachedTemp[i];
    bottle["co2"]     = cachedCo2[i];
    bottle["press"]   = cachedPress[i];
  }

  serializeJson(doc, Serial1);
  Serial1.println();
}

// ==========================================
// SETUP & MAIN LOOP
// ==========================================
void setup() {
  Serial.begin(9600);   // USB Debug
  Serial1.begin(9600);  // RS485 to Master (Pins 18/19)

  // 1. Initialize Methane Calibration Curves
  for (int i = 0; i < NUM_BOTTLES; i++) {
    pinMode(PIN_METHANE[i], INPUT);
    pinMode(PIN_PH[i], INPUT);
    pinMode(PIN_CO2_PWM[i], INPUT);

    ch4_cal_ok[i] = fitExponent(RS_AIR[i], PPM_AIR, RS_SPAN[i], PPM_SPAN, curve_exp[i]);
    if (ch4_cal_ok[i]) {
      rs_at_max[i] = RS_SPAN[i] * pow(PPM_SPAN / PPM_MAX, 1.0f / curve_exp[i]);
      ch4_cal_ok[i] = isfinite(rs_at_max[i]) && rs_at_max[i] > 0.0f;
    }
  }

  // 2. Initialize Non-Blocking DS18B20 Temperature Sensors
  for (int i = 0; i < NUM_BOTTLES; i++) {
    tempSensors[i]->begin();
    tempSensors[i]->setWaitForConversion(false);
    tempSensors[i]->requestTemperatures();
  }
  lastTempReqMs = millis();

  // 3. Initialize HX711 Pressure Sensors (with timeout protection)
  for (int i = 0; i < NUM_BOTTLES; i++) {
    pressInitialized[i] = initPressureSensor(i);
  }
}

void loop() {
  // Always keep RS485 responsive
  serviceMasterPing();

  // 1. Update pH, DS18B20 Temps, and 32-sample Methane array
  updatePhAndTemp();
  updateAllMethane();

  // 2. Step through each bottle's Pressure & CO2 PWM
  for (int i = 0; i < NUM_BOTTLES; i++) {
    serviceMasterPing();
    updatePressure(i);

    serviceMasterPing();
    float ppm = 0.0f;
    if (readCo2Ppm(PIN_CO2_PWM[i], ppm)) {
      cachedCo2[i] = lround(ppm);
    } else if (!pingHandledDuringCo2) {
      // Only flag -1 if it failed uninterrupted by a Serial1 ping transmit
      cachedCo2[i] = -1;
    }

    // Re-sample fast sensors between CO2 channels
    updatePhAndTemp();
    updateAllMethane();
  }
}