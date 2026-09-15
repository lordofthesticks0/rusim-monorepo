#include <Arduino.h>

namespace {
constexpr uint8_t kPwmPin = 2;
constexpr unsigned long kWaitTimeoutUs = 2000000UL;
constexpr unsigned long kPeriodMinUs = 950000UL;
constexpr unsigned long kPeriodMaxUs = 1060000UL;
constexpr unsigned long kHighMinUs = 1000UL;
constexpr unsigned long kHighMaxUs = 1002000UL;
constexpr unsigned long kMeasureIntervalMs = 2000;
constexpr float kPwmRangePpm = 5000.0f;
}

static bool waitForLevel(uint8_t level) {
  const unsigned long start = micros();
  while (digitalRead(kPwmPin) != level) {
    if (micros() - start > kWaitTimeoutUs) {
      return false;
    }
  }
  return true;
}

static bool readCo2Ppm(float& ppmOut) {
  if (!waitForLevel(LOW)) {
    return false;
  }
  if (!waitForLevel(HIGH)) {
    return false;
  }
  const unsigned long tRise = micros();
  if (!waitForLevel(LOW)) {
    return false;
  }
  const unsigned long tFall = micros();
  if (!waitForLevel(HIGH)) {
    return false;
  }
  const unsigned long tRise2 = micros();

  const unsigned long highUs = tFall - tRise;
  const unsigned long lowUs = tRise2 - tFall;
  const unsigned long periodUs = highUs + lowUs;

  if (periodUs < kPeriodMinUs || periodUs > kPeriodMaxUs) {
    return false;
  }
  if (highUs < kHighMinUs || highUs > kHighMaxUs) {
    return false;
  }

  const float periodMs = static_cast<float>(periodUs) / 1000.0f;
  const float highMs = static_cast<float>(highUs) / 1000.0f;
  ppmOut = kPwmRangePpm * (highMs - 2.0f) / (periodMs - 4.0f);
  return true;
}

void setup() {
  pinMode(kPwmPin, INPUT);
  Serial.begin(9600);
  Serial.println(F("MH-Z19C PWM CO2 reader started"));
}

void loop() {
  static unsigned long lastMeasureMs = 0;
  const unsigned long now = millis();

  if (now - lastMeasureMs < kMeasureIntervalMs) {
    return;
  }
  lastMeasureMs = now;

  float ppm = 0.0f;
  if (readCo2Ppm(ppm)) {
    Serial.print(F("CO2: "));
    Serial.print(ppm, 0);
    Serial.println(F(" ppm"));
  } else {
    Serial.println(F("CO2: ERROR - no valid PWM signal"));
  }
}