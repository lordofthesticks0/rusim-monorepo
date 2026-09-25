#include <Arduino.h>

// --- Dual MAX485 loopback test (Mega 2560, single board, two transceivers) ---
// Wire the RS-485 bus together: A-to-A, B-to-B, plus common GND.
// Add 120R termination if the cable is long.
//
// Module A:
//   RO -> RX3 (pin 15), DI -> TX3 (pin 14)
//   DE + RE tied together, driven by D2 AND D3 (both always equal)
// Module B:
//   RO -> RX2 (pin 17), DI -> TX2 (pin 16)
//   DE + RE tied together, driven by D4 AND D5 (both always equal)
//
// IMPORTANT: DE and RE are treated as ONE direction signal (single-pin control).
//   HIGH (both pins HIGH) = transmit (driver on, receiver off)
//   LOW  (both pins LOW)  = receive  (driver off, receiver on)
// The two MCU pins per module are ALWAYS driven to the SAME value, because on
// this hardware DE and RE are tied together. Driving them oppositely would
// short one MCU pin HIGH against the other LOW. Manual direction control is
// used instead of the RS485 library for exactly this reason.
//
// Test: A sends to B, then B sends to A. Results logged over USB Serial.
// A->B PASS proves A-TX + B-RX work. B->A PASS proves B-TX + A-RX work.
//
// NOTE: with single-pin control the receiver is OFF while transmitting, so a
// module never hears its own echo. That is expected, not a fault.

#if defined(ARDUINO_AVR_MEGA2560)

// Both pins per module carry the SAME direction signal. Never split them.
#define PIN_RS485A_DIR1 2
#define PIN_RS485A_DIR2 3

#define PIN_RS485B_DIR1 4
#define PIN_RS485B_DIR2 5

#define RS485_BAUD 9600
#define RECV_TIMEOUT_MS 500
#define CYCLE_DELAY_MS 2000
#define DIR_SETTLE_MS 2

static unsigned long cycleCount = 0;
static unsigned long passAtoB = 0, totalAtoB = 0;
static unsigned long passBtoA = 0, totalBtoA = 0;

struct Rs485Port {
  HardwareSerial *serial;
  int dirPin1;
  int dirPin2;
};

static Rs485Port portA = {&Serial3, PIN_RS485A_DIR1, PIN_RS485A_DIR2};
static Rs485Port portB = {&Serial2, PIN_RS485B_DIR1, PIN_RS485B_DIR2};

// Single-pin direction control: both pins always identical.
static void rs485Dir(Rs485Port &port, bool transmit) {
  int level = transmit ? HIGH : LOW;
  digitalWrite(port.dirPin1, level);
  digitalWrite(port.dirPin2, level);
}

static void rs485InitPort(Rs485Port &port) {
  pinMode(port.dirPin1, OUTPUT);
  pinMode(port.dirPin2, OUTPUT);
  // Receive mode first, before the UART starts, so the bus is never jammed.
  rs485Dir(port, false);
  port.serial->begin(RS485_BAUD);
}

static void drainPort(Rs485Port &port) {
  while (port.serial->available() > 0) {
    port.serial->read();
  }
}

// Send a newline-terminated message. Returns bytes written.
static size_t sendMessage(Rs485Port &txPort, const String &msg) {
  rs485Dir(txPort, true); // both direction pins HIGH together
  delay(DIR_SETTLE_MS);   // let the transceiver + bus settle
  size_t n = txPort.serial->print(msg + "\n");
  txPort.serial->flush(); // wait until the last stop bit is on the wire
  delay(DIR_SETTLE_MS);   // turnaround guard before releasing the bus
  rs485Dir(txPort, false); // both direction pins LOW together
  return n;
}

// Block up to timeoutMs waiting for a newline-terminated line.
// Returns the line (without CR/LF), sets gotLine true if a line arrived.
static String receiveMessage(Rs485Port &rxPort, unsigned long timeoutMs, bool &gotLine) {
  String s;
  s.reserve(64);
  unsigned long start = millis();
  gotLine = false;
  while (millis() - start < timeoutMs) {
    while (rxPort.serial->available() > 0) {
      char c = (char)rxPort.serial->read();
      if (c == '\n') {
        gotLine = true;
        s.trim();
        return s;
      } else if (c != '\r') {
        s += c;
      }
    }
  }
  s.trim();
  gotLine = s.length() > 0;
  return s;
}

// One direction of the test. Logs TX, RX, and PASS/FAIL. Returns true on match.
static bool testDirection(const char *label, Rs485Port &txPort, Rs485Port &rxPort,
                          const String &payload) {
  drainPort(txPort);
  drainPort(rxPort);
  delay(20);

  Serial.print(F("[SEND "));
  Serial.print(label);
  Serial.print(F("] TX: \""));
  Serial.print(payload);
  Serial.println(F("\""));

  size_t n = sendMessage(txPort, payload);
  Serial.print(F("        (bytes written: "));
  Serial.print(n);
  Serial.println(F(")"));

  bool gotLine = false;
  String rx = receiveMessage(rxPort, RECV_TIMEOUT_MS, gotLine);

  bool pass = gotLine && (rx == payload);
  Serial.print(F("[RECV "));
  Serial.print(label);
  if (gotLine) {
    Serial.print(F("] RX: \""));
    Serial.print(rx);
    Serial.print(F("\" | "));
    Serial.println(pass ? F("PASS") : F("FAIL (mismatch)"));
  } else {
    Serial.print(F("] RX: <timeout after "));
    Serial.print(RECV_TIMEOUT_MS);
    Serial.println(F("ms> | FAIL"));
  }
  return pass;
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for USB serial
  }

  rs485InitPort(portA);
  rs485InitPort(portB);

  Serial.println(F("=== MAX485 A<->B loopback test (Mega2560) ==="));
  Serial.println(F("Module A: Serial3 (TX3=14,RX3=15) DIR=D2+D3 (tied)"));
  Serial.println(F("Module B: Serial2 (TX2=16,RX2=17) DIR=D4+D5 (tied)"));
  Serial.println(F("Bus wiring: A-A, B-B, GND-GND"));
  Serial.println(F("Direction mode: single-pin (HIGH=TX, LOW=RX), pins never split"));
  Serial.println(F("Starting A->B then B->A cycles...\n"));
}

void loop() {
  cycleCount++;

  Serial.print(F("----- Cycle "));
  Serial.print(cycleCount);
  Serial.println(F(" -----"));

  String msgA = "Hello from A #" + String(cycleCount);
  totalAtoB++;
  if (testDirection("A->B", portA, portB, msgA)) {
    passAtoB++;
  }

  delay(200);

  String msgB = "Hello from B #" + String(cycleCount);
  totalBtoA++;
  if (testDirection("B->A", portB, portA, msgB)) {
    passBtoA++;
  }

  Serial.print(F("Summary: A->B "));
  Serial.print(passAtoB);
  Serial.print(F("/"));
  Serial.print(totalAtoB);
  Serial.print(F(" pass | B->A "));
  Serial.print(passBtoA);
  Serial.print(F("/"));
  Serial.print(totalBtoA);
  Serial.println(F(" pass"));
  Serial.println();

  delay(CYCLE_DELAY_MS);
}

#else

// Fallback so the shared source still compiles for envs without Serial2/Serial3
// (e.g. max485-test2 on Nano). The dual-transceiver test needs a Mega 2560.
void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;
  }
  Serial.println(F("SKIP: dual MAX485 A<->B test requires a Mega2560 (Serial2 + Serial3)."));
}

void loop() {
  delay(1000);
}

#endif
