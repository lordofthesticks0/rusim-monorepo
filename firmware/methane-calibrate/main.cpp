// TGS2611 4-Sensor Rumen Logger (ATmega2560)
// Uses Two-Point Power-Law (log-log) Calibration per sensor:
//   ppm = PPM_AIR * (RS_AIR / rs) ^ N
// Fit N from span point: N = log(PPM_SPAN/PPM_AIR) / log(RS_AIR/RS_SPAN)
#include <Arduino.h>

const int NUM_SENSORS = 4;
const int SENSOR_PINS[NUM_SENSORS] = {A0, A1, A2, A3};

// --- HARDWARE CONSTANTS ---
// Update these if your load resistors/potentiometers are tuned differently
const float RL[NUM_SENSORS] = {1000.0, 1000.0, 1000.0, 1000.0};
const float VCC = 5; // MEASURE your Mega's 5V pin with a multimeter!

// --- CALIBRATION DATA (UPDATE THESE!) ---
// Point 1: Rs measured in PPM_AIR for each sensor
const float RS_AIR[NUM_SENSORS] = {6160.3, 5370.7, 8592.1, 7950.2};

// Point 2: Rs measured in your PPM_SPAN ppm Span Gas for each sensor
const float RS_SPAN[NUM_SENSORS] = {1450.0, 1480.0, 1550.0, 1420.0};

const float PPM_AIR = 2.0;   // Background methane
const float PPM_SPAN = 300.0; // Factory baseline gas for raw curve fit

// --- FIELD RECALIBRATION (PPM-space, no Rs needed) ---
// Output: ppm = CORR_SLOPE[i] * ppm_raw + CORR_OFFSET[i]
// where ppm_raw = PPM_AIR * (RS_AIR / rs) ^ N
//
// How to calculate per sensor from PPM readings only:
//   1. Flash with SLOPE=1.0, OFFSET=0.0, note stable readings:
//      READ_air  = ppm_ in clean air  (expect ~2)
//      READ_span = ppm_ in span gas   (e.g. 350ppm bottle)
//   2. TRUE_air = 2.0, TRUE_span = bottle label (e.g. 350.0)
//      SLOPE  = (TRUE_span - TRUE_air) / (READ_span - READ_air)
//      OFFSET = TRUE_air - SLOPE * READ_air
// Example: READ_air=5.0, READ_span=50.0, TRUE_span=350.0
//   SLOPE = (350-2)/(50-5) = 7.733, OFFSET = 2 - 7.733*5 = -36.67
const float CORR_SLOPE[NUM_SENSORS] = {1.0, 1.0, 1.0, 1.0};
const float CORR_OFFSET[NUM_SENSORS] = {0.0, 0.0, 0.0, 0.0};

// Array to hold fitted curve exponent per sensor (line slope in log-log space)
float curve_exp[NUM_SENSORS];

void setup() {
  Serial.begin(115200); // Use higher baud rate for Mega
  delay(100);

  // Fit N for EACH sensor from its two points:
  //   N[i] = log10(PPM_SPAN / PPM_AIR) / log10(RS_AIR[i] / RS_SPAN[i])
  float log_ppm_span = log10(PPM_SPAN / PPM_AIR);
  for (int i = 0; i < NUM_SENSORS; i++) {
    curve_exp[i] = log_ppm_span / log10(RS_AIR[i] / RS_SPAN[i]);
  }
}

float readRs(int pin, float rl) {
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  float raw_avg = sum / 20.0;
  float voltage = raw_avg * (VCC / 1024.0);

  if (voltage <= 0.01) return -1.0;
  return ((VCC / voltage) - 1.0) * rl;
}

float rsToPpmRaw(float rs, int sensor_idx) {
  if (rs <= 0) return -1.0;
  return PPM_AIR * pow(RS_AIR[sensor_idx] / rs, curve_exp[sensor_idx]);
}

float rsToPpm(float rs, int sensor_idx) {
  float raw = rsToPpmRaw(rs, sensor_idx);
  if (raw < 0 || !isfinite(raw)) return -1.0;
  float corr = CORR_SLOPE[sensor_idx] * raw + CORR_OFFSET[sensor_idx];
  if (!isfinite(corr)) return -1.0;
  if (corr < 0) corr = 0; // clamp small negative offset noise
  return corr;
}

void loop() {
  Serial.print("{\"time_s\":");
  Serial.print(millis() / 1000);

  for (int i = 0; i < NUM_SENSORS; i++) {
    float rs = readRs(SENSOR_PINS[i], RL[i]);
    float ppm = rsToPpm(rs, i);

    Serial.print(",\"rs_");
    Serial.print(i);
    Serial.print("\":");
    if (rs > 0 && isfinite(rs)) Serial.print(rs, 1);
    else Serial.print("null");

    Serial.print(",\"ppm_");
    Serial.print(i);
    Serial.print("\":");
    if (ppm >= 0 && isfinite(ppm)) Serial.print(ppm, 1);
    else Serial.print("null");
  }

  Serial.println("}");
  delay(1000); // 1 reading/sec
}
