#include <HX711.h>

const byte HX711_DOUT = 2;
const byte HX711_SCK = 3;
const unsigned long SERIAL_BAUD = 115200;
const unsigned long HX711_READY_TIMEOUT_MS = 5000;
const unsigned long HX711_READY_POLL_MS = 10;

const int SENSORID = 3; // Select the sensor calibration: 0, 1, 2, or 3.
const float CALIBRATION_FACTORS[4] = {
  581040.0f, // Sensor 0
  70802.7743f, // Sensor 1
  82656.645503906f, // Sensor 2
  60223.70030581f  // Sensor 3
};

// Adjusted from 2280 based on a measured 50,000 Pa at a 981 Pa reference.
const float PA_PER_UNIT = 100.0f;

HX711 scale;
bool sensorInitialized = false;

bool initializeSensor() {
  if (SENSORID < 0 || SENSORID >= 4) {
    Serial.println("ERROR: SENSORID must be between 0 and 3.");
    return false;
  }

  // Do not use the library's default reset here. Its reset path performs a
  // blocking read before our readiness timeout can run.
  scale.begin(HX711_DOUT, HX711_SCK, false, false);

  const unsigned long start = millis();
  while (!scale.is_ready() && millis() - start < HX711_READY_TIMEOUT_MS) {
    delay(HX711_READY_POLL_MS);
  }

  if (!scale.is_ready()) {
    return false;
  }

  scale.set_scale(CALIBRATION_FACTORS[SENSORID]);
  scale.tare();
  return true;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println("Pressure calibration starting...");
  Serial.flush();

  Serial.println("Waiting for HX711...");
  Serial.flush();

  sensorInitialized = initializeSensor();
  if (!sensorInitialized) {
    Serial.println("ERROR: HX711 not found / not ready.");
    Serial.println("Check VCC, GND, DOUT, SCK, and the selected pins.");
    return;
  }

  Serial.println("Pressure sensor initialized.");
  Serial.println("Pressure (Pa):");
}

void loop() {
  if (!sensorInitialized) {
    // Allow recovery if the HX711 was powered after the microcontroller.
    sensorInitialized = initializeSensor();
    if (!sensorInitialized) {
      delay(1000);
      return;
    }
    Serial.println("Pressure sensor initialized.");
    Serial.println("Pressure (Pa):");
  }

  if (scale.wait_ready_timeout(HX711_READY_TIMEOUT_MS, HX711_READY_POLL_MS)) {
    float sensorValue = scale.get_units(10);
    float pressurePa = sensorValue * PA_PER_UNIT;
    Serial.print("Pressure: ");
    Serial.print(pressurePa, 0);
    Serial.println(" Pa");
  } else {
    Serial.println("HX711 not ready; retrying...");
  }
}
