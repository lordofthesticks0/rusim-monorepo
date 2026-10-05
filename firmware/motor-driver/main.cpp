#include <Arduino.h>

// L298N wiring: IN1 -> D7, IN2 -> D8, ENA -> D9 (PWM)
// IMPORTANT: Remove the ENA jumper, otherwise PWM speed control will not work.
// Motor on OUT1 / OUT2. Common GND between Nano and L298N required.
constexpr uint8_t IN1 = 7;  // D7
constexpr uint8_t IN2 = 8;  // D8
constexpr uint8_t ENA = 9;  // D9, PWM-capable (~D9)

// How fast to ramp: step size + delay per step
constexpr uint8_t PWM_STEP = 5;         // 0, 5, 10, ... 255
constexpr uint16_t RAMP_DELAY_MS = 30;  // ~1.5 s from 0 to full speed
constexpr uint16_t HOLD_MS = 2000;      // hold at full speed
constexpr uint16_t STOP_MS = 1000;      // pause between directions

// Gradually ramp speed from 0 up to 255
void rampUp() {
  for (uint16_t speed = 0; speed <= 255; speed += PWM_STEP) {
    analogWrite(ENA, (uint8_t)speed);
    delay(RAMP_DELAY_MS);
  }
  analogWrite(ENA, 255);  // make sure we reach full speed
}

void stopMotor() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  stopMotor();

  Serial.begin(115200);
}

void loop() {
  // Forward with soft start: IN1 HIGH, IN2 LOW, ramp 0 -> full
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  Serial.println("FORWARD - ramping up");
  rampUp();
  Serial.println("FORWARD - full speed");
  delay(HOLD_MS);

  // Stop for 1 s
  stopMotor();
  Serial.println("STOP");
  delay(STOP_MS);

  // Reverse with soft start: IN1 LOW, IN2 HIGH, ramp 0 -> full
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  Serial.println("REVERSE - ramping up");
  rampUp();
  Serial.println("REVERSE - full speed");
  delay(HOLD_MS);

  // Stop for 1 s
  stopMotor();
  Serial.println("STOP");
  delay(STOP_MS);
}
