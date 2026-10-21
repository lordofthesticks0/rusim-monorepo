/*
 * temp-probe: read one DS18B20 on D22 and print it over USB serial.
 *
 * Wiring:
 *   DS18B20 VDD -> 5V
 *   DS18B20 GND -> GND
 *   DS18B20 DQ  -> D22
 *   4.7k resistor from DQ to 5V
 *
 * Build:  pio run -e temp-probe
 * Flash:  pio run -e temp-probe -t upload
 * Watch:  pio device monitor -e temp-probe
 *
 * This is a bring-up aid, not part of the slave firmware. It prints plain text,
 * not JSON, and does not touch Serial1/RS485.
 */

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

constexpr uint8_t TEMP_PIN = 22;
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned long REPORT_INTERVAL_MS = 1000;
constexpr unsigned long RESCAN_INTERVAL_MS = 2000;

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

unsigned long lastReportMs = 0;
unsigned long lastRescanMs = 0;
uint8_t readFailures = 0;

static void printAddress(const uint8_t *addr) {
  for (uint8_t i = 0; i < 8; i++) {
    if (addr[i] < 0x10) Serial.print('0');
    Serial.print(addr[i], HEX);
    if (i < 7) Serial.print(':');
  }
}

static void checkIdleLine() {
  // The external 4.7k is the real pull-up. This check only proves whether the
  // line is shorted to GND or loaded by something like the D13 LED circuit.
  pinMode(TEMP_PIN, INPUT_PULLUP);
  delay(2);
  const int level = digitalRead(TEMP_PIN);
  pinMode(TEMP_PIN, INPUT);

  Serial.print(F("Idle D22 with internal pull-up: "));
  Serial.println(level ? F("HIGH") : F("LOW (shorted or loaded)"));
}

static uint8_t scanBus() {
  oneWire.reset_search();
  uint8_t addr[8];
  uint8_t found = 0;

  while (oneWire.search(addr)) {
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println(F("  ROM CRC mismatch, skipped"));
      continue;
    }
    Serial.print(F("  ROM #"));
    Serial.print(found);
    Serial.print(F(": "));
    printAddress(addr);
    Serial.print(F(" family=0x"));
    Serial.println(addr[0], HEX);
    found++;
  }

  if (found == 0) {
    Serial.println(F("  (no ROM codes)"));
  }
  return found;
}

static void printBusDiagnostics() {
  checkIdleLine();

  Serial.print(F("OneWire reset presence: "));
  Serial.println(oneWire.reset() ? F("yes") : F("no"));

  Serial.println(F("Searching the bus:"));
  const uint8_t found = scanBus();
  Serial.print(F("ROM codes found: "));
  Serial.println(found);

  sensors.begin();
  Serial.print(F("DallasTemperature device count: "));
  Serial.println(sensors.getDeviceCount());
}

static bool readAndReport() {
  if (sensors.getDeviceCount() == 0) {
    Serial.println(F("[----] no device on D22"));
    return false;
  }

  const float tempC = sensors.getTempCByIndex(0);

  Serial.print('[');
  Serial.print(millis());
  Serial.print(F(" ms] "));

  if (tempC <= DEVICE_DISCONNECTED_C) {
    Serial.print(F("read failed ("));
    Serial.print(tempC, 3);
    Serial.println(F(" C)"));
    return false;
  }

  Serial.print(F("temp = "));
  Serial.print(tempC, 3);
  Serial.println(F(" C"));
  return true;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  Serial.println();
  Serial.println(F("temp-probe: one DS18B20 on D22"));
  Serial.println(F("expect VDD=5V, GND=GND, DQ=D22, 4.7k DQ to 5V"));

  printBusDiagnostics();

  if (sensors.getDeviceCount() == 0) {
    Serial.println(F("No sensor found. Check the 4.7k pull-up and the D22 wire."));
  }

  lastRescanMs = millis();
}

void loop() {
  if (sensors.getDeviceCount() == 0) {
    if (millis() - lastRescanMs >= RESCAN_INTERVAL_MS) {
      lastRescanMs = millis();
      Serial.println(F("Rescanning D22..."));
      printBusDiagnostics();
    }
    delay(100);
    return;
  }

  if (millis() - lastReportMs < REPORT_INTERVAL_MS) {
    delay(10);
    return;
  }
  lastReportMs = millis();

  // Blocking conversion, 750 ms at the DS18B20's default 12-bit resolution.
  sensors.requestTemperatures();

  if (readAndReport()) {
    readFailures = 0;
    return;
  }

  if (++readFailures >= 3) {
    readFailures = 0;
    Serial.println(F("Three failed reads, re-initialising the bus..."));
    sensors.begin();
  }
}
