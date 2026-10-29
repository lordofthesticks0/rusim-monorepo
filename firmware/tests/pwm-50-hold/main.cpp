#include <Arduino.h>

// Standalone PWM test for the Arduino Mega 2560.
//
// Sequence: wait 3 s, ramp D9 from 0 to 50% duty, then hold 50% forever.
// D9 is a PWM-capable pin (~490 Hz on the Mega) and is the same pin the
// motor-driver sketch uses for the L298N ENA input.
constexpr uint8_t PWM_PIN = 9;

constexpr uint16_t START_DELAY_MS = 3000;

// 50% of the 8-bit duty range. 255 * 50 / 100 = 127.5, rounded up to 128.
constexpr uint8_t TARGET_DUTY = 128;

// 128 steps of 1 reach 128. 128 * 25 ms = ms, about 3 s.
constexpr uint8_t PWM_STEP = 1;
constexpr uint16_t RAMP_STEP_MS = 25;

void setup() {
  pinMode(PWM_PIN, OUTPUT);
  analogWrite(PWM_PIN, 0);

  Serial.begin(115200);
  Serial.println("PWM 50% hold test on D9");
  Serial.print("Waiting ");
  Serial.print(START_DELAY_MS);
  Serial.println(" ms before ramp");
  delay(START_DELAY_MS);

  for (uint16_t duty = 0; duty <= TARGET_DUTY; duty += PWM_STEP) {
    analogWrite(PWM_PIN, (uint8_t)duty);
    delay(RAMP_STEP_MS);
  }
  // Land exactly on the target in case the loop overshoots.
  analogWrite(PWM_PIN, TARGET_DUTY);
  Serial.print("Holding at duty ");
  Serial.print(TARGET_DUTY);
  Serial.println(" / 255");
}

void loop() {
  // Nothing to do: the duty set in setup() stays active.
  delay(1000);
}
