// TGS2611 4-Sensor Rumen Logger (ATmega2560)
// Uses Two-Point Logarithmic Calibration per sensor

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

const float PPM_AIR = 0.0;   // Background methane
const float PPM_SPAN = 300.0; // Your calibration gas

// Arrays to hold calculated math constants
float m_slope[NUM_SENSORS];
float c_intercept[NUM_SENSORS];

void setup() {
  Serial.begin(115200); // Use higher baud rate for Mega
  delay(100);
  Serial.println("\nTGS2611 4-Sensor Methane Logger");
  Serial.println("Time_s | Rs_0 | ppm_0 | Rs_1 | ppm_1 | Rs_2 | ppm_2 | Rs_3 | ppm_3");

  // Calculate custom curve constants for EACH sensor individually
  for (int i = 0; i < NUM_SENSORS; i++) {
    float log_rs_air = log10(RS_AIR[i]);
    float log_rs_span = log10(RS_SPAN[i]);
    float log_ppm_air = log10(PPM_AIR);
    float log_ppm_span = log10(PPM_SPAN);

    m_slope[i] = (log_rs_air - log_rs_span) / (log_ppm_air - log_ppm_span);
    c_intercept[i] = log_rs_air - (m_slope[i] * log_ppm_air);
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

float rsToPpm(float rs, int sensor_idx) {
  if (rs <= 0) return -1.0;
  float log_rs = log10(rs);
  float log_ppm = (log_rs - c_intercept[sensor_idx]) / m_slope[sensor_idx];
  return pow(10.0, log_ppm);
}

void loop() {
  Serial.print(millis() / 1000); // Timestamp

  for (int i = 0; i < NUM_SENSORS; i++) {
    float rs = readRs(SENSOR_PINS[i], RL[i]);
    float ppm = rsToPpm(rs, i);

    Serial.print(" | ");
    Serial.print(rs, 1);
    Serial.print(" | ");

    if (ppm > 0) Serial.print(ppm, 1);
    else Serial.print("ERR");
  }

  Serial.println(); // End of row
  delay(1000);      // 1 reading/sec
}
