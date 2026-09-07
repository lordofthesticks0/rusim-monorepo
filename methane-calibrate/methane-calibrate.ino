// TGS2611 Methane Logger — simplified
// Output: Human readable. ADJUST FOR AUTOMATION LATER
// IMPORTANT: Wait for sensor to stabilize before calibration (TAKES A WHILE)

const int SENSOR_PIN = A0;

// --- Calibration constants ---
const float RL     = 2970.0;   // POTENTIOMETER. MIGHT
const float RL1    = 2665.0;
const float R0_AIR = 2897.0;   // Rs measured in known-clean air — RECALIBRATE per sensor.
const float CAL_FACTOR = 6.0;  // ppm scaling factor — FOR CALIBRATION
// Datasheet log-log curve fit: log10(ppm) = (log10(ratio) - B) / M
const float CURVE_M = -0.529;
const float CURVE_B =  0.957;

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println("TGS2611 Methane Logger");
  Serial.println("time_s, Rs_ohm, ratio, ppm");
}

float readRs() {
  int   raw     = analogRead(SENSOR_PIN);
  float voltage = raw * (5.0 / 1023.0);
  if (voltage <= 0.0) return -1.0;          // sensor not responding / disconnected
  return ((5.0 / voltage) - 1.0) * RL;
}

float rsToPpm(float rs) {
  float ratio = rs / R0_AIR;
  return pow(10.0, (log10(ratio) - CURVE_B) / CURVE_M);
}

void loop() {
  float rs = readRs();

  if (rs > 0) {
    float ratio = rs / R0_AIR;
    float ppm   = rsToPpm(rs); // multiply by calibration factor

    Serial.print(millis() / 1000);
    Serial.print(" || ");
    Serial.print(rs, 1);
    Serial.print(" || ");
    Serial.print(ratio, 4);
    Serial.print(" || ");
    Serial.println(ppm, 1);
  } else {
    Serial.println("ERR,sensor read <= 0V");
  }

  delay(1000); // 1 reading/sec
}
