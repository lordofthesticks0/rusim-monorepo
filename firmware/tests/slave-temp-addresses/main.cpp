#include <Arduino.h>
#include <OneWire.h>

// Match the four independent DS18B20 buses in legacy/slave/new-slave.cpp.
constexpr uint8_t kTempPins[] = {4, 7, 10, 22};
constexpr size_t kBusCount = sizeof(kTempPins) / sizeof(kTempPins[0]);

static void printAddress(const uint8_t address[8]) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (address[i] < 0x10) Serial.print('0');
    Serial.print(address[i], HEX);
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}
  delay(500);
  Serial.println(F("DS18B20 address scan: slave temp pins 4, 7, 10, 22"));

  for (size_t bus = 0; bus < kBusCount; ++bus) {
    OneWire wire(kTempPins[bus]);
    uint8_t address[8];
    uint16_t found = 0;

    Serial.print(F("PIN "));
    Serial.println(kTempPins[bus]);
    wire.reset_search();
    while (wire.search(address)) {
      if (OneWire::crc8(address, 7) != address[7]) {
        Serial.println(F("  invalid CRC address (skipped)"));
        continue;
      }
      Serial.print(F("  "));
      printAddress(address);
      Serial.print(F("  family=0x"));
      if (address[0] < 0x10) Serial.print('0');
      Serial.println(address[0], HEX);
      ++found;
    }
    if (found == 0) Serial.println(F("  no valid devices found"));
  }
  Serial.println(F("Scan complete."));
}

void loop() {}
