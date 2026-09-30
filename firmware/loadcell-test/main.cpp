#include <Arduino.h>
#include <HX711.h>

const byte HX711_DOUT = 13;
const byte HX711_SCK = 12;
const unsigned long SERIAL_BAUD = 115200;
const unsigned long HX711_READY_TIMEOUT_MS = 5000;
const unsigned long HX711_READY_POLL_MS = 10;

// Raw mode: no conversion. Scale of 1.0 keeps get_units() as raw units
// (tare-corrected, unscaled), same call chain as pressure-calibrate.
const float CALIBRATION_FACTOR = 1.0f;

HX711 scale;
bool sensorInitialized = false;

bool initializeSensor() {
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

  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();
  return true;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println("Loadcell test starting...");
  Serial.flush();

  Serial.println("Waiting for HX711...");
  Serial.flush();

  sensorInitialized = initializeSensor();
  if (!sensorInitialized) {
    Serial.println("ERROR: HX711 not found / not ready.");
    Serial.println("Check VCC, GND, DOUT, SCK, and the selected pins.");
    return;
  }

  Serial.println("Loadcell initialized.");
  Serial.println("Raw reading:");
}

void loop() {
  if (!sensorInitialized) {
    // Allow recovery if the HX711 was powered after the microcontroller.
    sensorInitialized = initializeSensor();
    if (!sensorInitialized) {
      delay(1000);
      return;
    }
    Serial.println("Loadcell initialized.");
    Serial.println("Raw reading:");
  }

  if (scale.wait_ready_timeout(HX711_READY_TIMEOUT_MS, HX711_READY_POLL_MS)) {
    float rawValue = scale.get_units(10);
    Serial.print("Raw reading: ");
    Serial.println(rawValue, 0);
  } else {
    Serial.println("HX711 not ready; retrying...");
  }
}
