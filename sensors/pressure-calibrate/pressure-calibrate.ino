#include <HX711.h>

const byte HX711_DOUT = 2;
const byte HX711_SCK = 3;
const unsigned long SERIAL_BAUD = 115200;
const unsigned long HX711_STARTUP_TIMEOUT_MS = 3000;
const float CALIBRATION_FACTOR = 2280.0f;
const float PA_PER_UNIT = 100.0f;

HX711 scale;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println("Pressure calibration starting...");
  Serial.flush();

  scale.begin(HX711_DOUT, HX711_SCK);
  Serial.println("Waiting for HX711...");
  Serial.flush();

  const unsigned long start = millis();
  while (!scale.is_ready() && millis() - start < HX711_STARTUP_TIMEOUT_MS) {
    delay(10);
  }

  if (!scale.is_ready()) {
    Serial.println("ERROR: HX711 not found / not ready.");
    Serial.println("Check VCC, GND, DOUT, SCK, and the selected pins.");
    return;
  }

  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();

  Serial.println("Pressure sensor initialized.");
  Serial.println("Pressure (kPa):");
}

void loop() {
  if (scale.is_ready()) {
    float sensorValue = scale.get_units(10);
    float pressureKPa = sensorValue * PA_PER_UNIT / 1000.0f;
    Serial.print("Pressure: ");
    Serial.print(pressureKPa, 2);
    Serial.println(" kPa");
  } else {
    Serial.println("HX711 not found / not ready.");
  }
}
