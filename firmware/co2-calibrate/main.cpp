// MH-Z19C 4-Sensor CO2 Logger (ATmega2560)
// Reads the PWM output of four MH-Z19C sensors and the UART output of sensor 0
// and prints one JSON line per
// sweep, mirroring methane-calibrate's format so the same capture workflow
// can be reused:
//   {"time_s":5,"raw_0":812.0,"ppm_0":804.5,"raw_1":null,...}
//
// Calibration is done in FIRMWARE, not on the sensor: the MH-Z19C only
// accepts internal calibration via UART commands or its HD pin, which this
// rig does not use. Instead each sensor gets a two-point linear correction
// (see CORR_SLOPE/CORR_OFFSET below), the same approach as
// firmware/methane-calibrate/main.cpp.
//
// How to calibrate (per sensor, PPM-space, no raw signal needed):
//   1. Flash with SLOPE=1.0, OFFSET=0.0 and log readings ("raw_i").
//      Let the sensors stabilize at least 20 minutes before capturing.
//   2. Point 1 (air): expose to clean outdoor air (~420 ppm) and note the
//      stable raw reading, READ_air.
//   3. Point 2 (span): expose to certified CO2 span gas of known ppm
//      (e.g. a 2000 ppm bottle) and note READ_span.
//      TRUE_air = 420.0 (or your reference), TRUE_span = bottle label.
//   4. SLOPE  = (TRUE_span - TRUE_air) / (READ_span - READ_air)
//      OFFSET = TRUE_air - SLOPE * READ_air
//      Example: READ_air=450, READ_span=2400, TRUE_span=2000
//        SLOPE  = (2000-420)/(2400-450) = 0.810
//        OFFSET = 420 - 0.810*450 = 55.5
//   5. Flash again with the fitted values; "ppm_i" is then corrected.
//      "raw_i" always logs the uncorrected PWM reading.
//
// NOTE on ABC (Automatic Baseline Correction): MH-Z19 sensors ship with ABC
// enabled, which re-adjusts the internal baseline every 24 h assuming the
// lowest CO2 seen equals ~400 ppm. The UART control command can disable it, but
// this logger only reads UART data, so a
// fitted correction can drift over long sessions. Recalibrate at the start
// of each session, and keep readings below the ABC influence by finishing
// captures promptly.
//
// PWM decode (per MH-Z19C datasheet):
//   ppm = 5000 * (highMs - 2.0) / (periodMs - 4.0)
// The period sits near 1004 ms, so each sensor is read sequentially (one full
// PWM cycle per sensor); a complete 4-sensor sweep takes roughly 4-5 s.
#include <Arduino.h>

namespace {
const int NUM_SENSORS = 4;
const uint8_t SENSOR_PINS[NUM_SENSORS] = {6, 3, 4, 7};

constexpr unsigned long kWaitTimeoutUs = 2000000UL;
constexpr unsigned long kPeriodMinUs = 950000UL;
constexpr unsigned long kPeriodMaxUs = 1060000UL;
constexpr unsigned long kHighMinUs = 1000UL;
constexpr unsigned long kHighMaxUs = 1002000UL;
constexpr unsigned long kSweepDelayMs = 1000;
constexpr float kPwmRangePpm = 5000.0f;
constexpr unsigned long kUartTimeoutMs = 250;
constexpr uint8_t kUartReadCommand[9] = {0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79};
}  // namespace

// --- FIELD RECALIBRATION (PPM-space, no raw signal needed) ---
// Output: ppm = CORR_SLOPE[i] * ppm_raw + CORR_OFFSET[i]
// Fit the values per sensor as described in the header comment; keep at
// 1.0 / 0.0 until you have captured both calibration points.
const float CORR_SLOPE[NUM_SENSORS] = {1.0, 1.0, 1.0, 1.0};
const float CORR_OFFSET[NUM_SENSORS] = {0.0, 0.0, 0.0, 0.0};

// Poll until the pin reaches `level`, giving up after kWaitTimeoutUs.
static bool waitForLevel(uint8_t pin, uint8_t level) {
  const unsigned long start = micros();
  while (digitalRead(pin) != level) {
    if (micros() - start > kWaitTimeoutUs) {
      return false;
    }
  }
  return true;
}

static bool readCo2Ppm(uint8_t pin, float& ppmOut) {
  if (!waitForLevel(pin, LOW)) {
    return false;
  }
  if (!waitForLevel(pin, HIGH)) {
    return false;
  }
  const unsigned long tRise = micros();
  if (!waitForLevel(pin, LOW)) {
    return false;
  }
  const unsigned long tFall = micros();
  if (!waitForLevel(pin, HIGH)) {
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
  if (!isfinite(ppmOut) || ppmOut < 0.0f) {
    return false;
  }
  return true;
}

// MH-Z19C UART uses 9600 baud, 8 data bits, no parity, 1 stop bit. The
// response is FF 86 ppm-high ppm-low ... checksum. Only one sensor should be
// connected to this UART because these sensors share the same address.
static bool readCo2Uart(uint16_t& ppmOut) {
  while (Serial1.available() > 0) (void)Serial1.read();
  Serial1.write(kUartReadCommand, sizeof(kUartReadCommand));
  Serial1.flush();

  uint8_t response[9];
  const unsigned long start = millis();
  size_t count = 0;
  while (count < sizeof(response) && millis() - start < kUartTimeoutMs) {
    if (Serial1.available() > 0) response[count++] = (uint8_t)Serial1.read();
  }
  if (count != sizeof(response) || response[0] != 0xFF || response[1] != 0x86) {
    return false;
  }
  uint8_t sum = 0;
  for (size_t i = 1; i < 8; ++i) sum += response[i];
  if ((uint8_t)(0xFF - sum + 1) != response[8]) return false;
  ppmOut = ((uint16_t)response[2] << 8) | response[3];
  return true;
}

void setup() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(SENSOR_PINS[i], INPUT_PULLUP);
  }
  Serial.begin(115200);
  // Mega 2560 Serial1: RX=D19, TX=D18. Cross-connect sensor TX to D19 and
  // sensor RX to D18; connect grounds. The sensor's UART pins are TTL level.
  Serial1.begin(9600, SERIAL_8N1);
  delay(100);
  Serial.println(F("MH-Z19C 4-sensor PWM CO2 logger started"));
}

void loop() {
  Serial.print("{\"time_s\":");
  Serial.print(millis() / 1000);

  for (int i = 0; i < NUM_SENSORS; i++) {
    float ppm = 0.0f;
    const bool ok = readCo2Ppm(SENSOR_PINS[i], ppm);

    Serial.print(",\"raw_");
    Serial.print(i);
    Serial.print("\":");
    if (ok) {
      Serial.print(ppm, 1);
    } else {
      Serial.print(F("null"));
    }

    Serial.print(",\"ppm_");
    Serial.print(i);
    Serial.print("\":");
    if (ok) {
      float corr = CORR_SLOPE[i] * ppm + CORR_OFFSET[i];
      if (!isfinite(corr) || corr < 0.0f) corr = 0.0f;
      Serial.print(corr, 1);
    } else {
      Serial.print(F("null"));
    }
  }

  uint16_t uartPpm = 0;
  const bool uartOk = readCo2Uart(uartPpm);
  Serial.print(",\"uart_0\":");
  if (uartOk) Serial.print(uartPpm);
  else Serial.print(F("null"));

  Serial.println("}");
  delay(kSweepDelayMs);
}
