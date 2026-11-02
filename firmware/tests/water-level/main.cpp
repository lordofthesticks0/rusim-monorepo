#include <Arduino.h>

// Print raw analog readings from a water level sensor connected to A0.
constexpr uint8_t WATER_LEVEL_PIN = A0;
constexpr uint16_t SAMPLE_INTERVAL_MS = 500;

void setup() {
  Serial.begin(115200);
  Serial.println("Water level sensor readings (A0, raw ADC 0-1023)");
}

void loop() {
  Serial.println(analogRead(WATER_LEVEL_PIN));
  delay(SAMPLE_INTERVAL_MS);
}
