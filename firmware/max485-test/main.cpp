#include <Arduino.h>
#include <CSE_ArduinoRS485.h>

// --- MAX485 wiring (Nano, SoftwareSerial since HW Serial is USB debug) ---
// RO -> D10 (RX), DI -> D11 (TX), DE+RE tied together -> D2
// Single-pin direction control: HIGH = transmit, LOW = receive.
#define PIN_RS485_RX 10
#define PIN_RS485_TX 11
#define PIN_RS485_DE 2

SoftwareSerial rs485Serial(PIN_RS485_RX, PIN_RS485_TX);
RS485Class RS485(rs485Serial, PIN_RS485_DE);

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for USB serial (Nano: passes immediately)
  }

  RS485.begin(9600);

  if (RS485) {
    Serial.println(F("SUCCESS: MAX485 interface initialized"));
  } else {
    Serial.println(F("FAIL: MAX485 interface init failed"));
  }
}

void loop() {
}
