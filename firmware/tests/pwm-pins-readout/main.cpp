// PWM readout for digital pins D4-D7 (Arduino Mega 2560), JSON output
//
// This board has faulty PWM pins, so only D4-D7 are sampled. Every 2 s the
// sketch prints one JSON object per sample on USB serial (115200 baud, one
// line per sample). Example, wrapped here for readability:
//
//   {"uptime_ms":12000,"range_ppm":5000,"pins":[
//     {"pin":4,"signal":true,"high_ms":1002.0,"period_ms":1004.0,
//      "duty_pct":99.8,"ppm":5000},
//     {"pin":5,"signal":false,"high_ms":null,"period_ms":null,
//      "duty_pct":null,"ppm":null},
//     {"pin":6,...},{"pin":7,...}]}
//
// A pin with no valid cycle in the last 3 s has "signal":false and null
// measurements. The sketch prints nothing but JSON, so the stream can be
// piped straight into a parser or a file.
//
// Why polling instead of interrupts: D4-D7 have no hardware external
// interrupts on the Mega 2560 (only D2, D3 and D18-D21 do). One software
// sampler is uniform and simple. A sweep over four pins takes a few tens of
// microseconds with digitalRead(). The MH-Z19C cycle is ~1004 ms and its
// shortest pulse is 2 ms, so polling is accurate enough for this test.
//
// PWM decode (MH-Z19C datasheet):
//   ppm = Range * (Th - 2 ms) / (Th + Tl - 4 ms)
// Th is the HIGH time, Tl the LOW time. The first 2 ms of every cycle is
// always HIGH (start marker) and the last 2 ms is always LOW, hence the
// 2 ms and 4 ms terms. This sketch assumes the 5000 ppm range, which is
// the sensor's default. Change kRangePpm to 2000 or 10000 if the sensor
// was configured differently (the range cannot be read without UART).
//
// The timing math and the JSON formatting use integers only, so no float
// rounding is involved. There is no String, no dynamic allocation and no
// input parsing: the sketch only reads pins and prints.
#include <Arduino.h>

namespace {

constexpr uint8_t kFirstPin = 4;
constexpr uint8_t kLastPin = 7;
constexpr uint8_t kPinCount = kLastPin - kFirstPin + 1;

constexpr uint32_t kRangePpm = 5000;  // range used by the PWM math

// ---- sanity limits --------------------------------------------------------
constexpr uint32_t kPeriodMinUs = 950000UL;
constexpr uint32_t kPeriodMaxUs = 1060000UL;
constexpr uint32_t kStartHighUs = 2000UL;  // fixed HIGH marker per cycle
constexpr uint32_t kEndLowUs = 2000UL;     // fixed LOW marker per cycle
constexpr unsigned long kStaleMs = 3000;   // no new cycle for this long = idle

constexpr unsigned long kPrintEveryMs = 2000;

// One PWM channel. All fields are written and read by loop() only, so no
// ISR-safe copy is needed.
struct Channel {
  uint8_t pin;
  bool level;            // level seen at the previous sample
  bool haveRise;         // riseUs holds a rising edge
  bool haveReading;      // a plausible complete cycle was measured
  uint32_t riseUs;       // time of the latest rising edge
  uint32_t periodUs;     // latest rise-to-rise time
  uint32_t readHighUs;   // Th of the last plausible cycle
  uint32_t readPeriodUs; // period of the last plausible cycle
  unsigned long readMs;  // when that cycle was measured
};

Channel g_channels[kPinCount];

void initChannels() {
  for (uint8_t i = 0; i < kPinCount; i++) {
    Channel& c = g_channels[i];
    c = Channel{};  // zero every field
    c.pin = kFirstPin + i;
    // Pull-ups keep unconnected pins at a stable HIGH instead of floating.
    pinMode(c.pin, INPUT_PULLUP);
    c.level = digitalRead(c.pin) == HIGH;
  }
}

// Record one observed transition. A rise starts a cycle; a fall closes the
// HIGH part of it and completes the pair (Th and period) that we decode.
void sampleChannel(Channel& c, uint32_t nowUs, unsigned long nowMs) {
  const bool level = digitalRead(c.pin) == HIGH;
  if (level == c.level) return;
  c.level = level;

  if (level) {
    // Rising edge: the previous rise (if any) gives the cycle period.
    // Unsigned subtraction stays correct across the micros() wrap.
    if (c.haveRise) c.periodUs = nowUs - c.riseUs;
    c.riseUs = nowUs;
    c.haveRise = true;
  } else if (c.haveRise) {
    const uint32_t highUs = nowUs - c.riseUs;
    // Latch the measurement only if this cycle looks like a real MH-Z19C
    // cycle. A missed edge (for example while Serial was busy) produces an
    // implausible pair, which is rejected here instead of being printed.
    if (c.periodUs >= kPeriodMinUs && c.periodUs <= kPeriodMaxUs &&
        highUs > kStartHighUs && highUs < c.periodUs) {
      c.readHighUs = highUs;
      c.readPeriodUs = c.periodUs;
      c.readMs = nowMs;
      c.haveReading = true;
    }
  }
}

// One pass over every pin. Called as often as possible; it never blocks.
void sampleAll() {
  const unsigned long nowMs = millis();
  for (uint8_t i = 0; i < kPinCount; i++) {
    sampleChannel(g_channels[i], micros(), nowMs);
  }
}

bool hasSignal(const Channel& c) {
  return c.haveReading && millis() - c.readMs < kStaleMs;
}

uint32_t ppmFrom(const Channel& c) {
  // ppm = Range * (Th - 2 ms) / (Th + Tl - 4 ms). Both times are converted
  // to 0.1 ms units so "Range * numerator" stays inside 32 bits.
  const uint32_t th10 = (c.readHighUs - kStartHighUs) / 100UL;
  const uint32_t window10 =
      (c.readPeriodUs - kStartHighUs - kEndLowUs) / 100UL;
  return (kRangePpm * th10) / window10;
}

// Print a value in tenths as JSON, e.g. 10020 -> "1002.0". Keeps the output
// free of floats and of AVR float formatting.
void printTenths(uint32_t tenths) {
  Serial.print(tenths / 10UL);
  Serial.print('.');
  Serial.print(tenths % 10UL);
}

void printPinJson(const Channel& c) {
  Serial.print(F("{\"pin\":"));
  Serial.print(c.pin);
  Serial.print(F(",\"signal\":"));

  if (!hasSignal(c)) {
    Serial.print(F("false,\"high_ms\":null,\"period_ms\":null,"));
    Serial.print(F("\"duty_pct\":null,\"ppm\":null}"));
    return;
  }

  // 0.1 ms units: microseconds / 100, rounded to the nearest tenth.
  const uint32_t highTenths = (c.readHighUs + 50UL) / 100UL;
  const uint32_t periodTenths = (c.readPeriodUs + 50UL) / 100UL;
  // Duty in per-mille, which is the same as tenths of a percent, rounded.
  // highUs <= periodUs <= 1.06e6, so *1000 fits in 32 bits.
  const uint32_t dutyTenths =
      (c.readHighUs * 1000UL + c.readPeriodUs / 2UL) / c.readPeriodUs;

  Serial.print(F("true,\"high_ms\":"));
  printTenths(highTenths);
  Serial.print(F(",\"period_ms\":"));
  printTenths(periodTenths);
  Serial.print(F(",\"duty_pct\":"));
  printTenths(dutyTenths);
  Serial.print(F(",\"ppm\":"));
  Serial.print(ppmFrom(c));
  Serial.print('}');
}

void printSample() {
  Serial.print(F("{\"uptime_ms\":"));
  Serial.print(millis());
  Serial.print(F(",\"range_ppm\":"));
  Serial.print(kRangePpm);
  Serial.print(F(",\"pins\":["));

  for (uint8_t i = 0; i < kPinCount; i++) {
    if (i != 0) Serial.print(',');
    printPinJson(g_channels[i]);
  }

  Serial.println(F("]}"));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  initChannels();
}

void loop() {
  // Sampling and printing share the same core: keep sampling tight and
  // print in one burst every 2 s. No banner text is printed, so every line
  // on the serial port is valid JSON.
  sampleAll();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= kPrintEveryMs) {
    lastPrint = millis();
    printSample();
  }
}
