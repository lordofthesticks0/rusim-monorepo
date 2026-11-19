// pH 4-Probe Calibration Logger (ATmega2560)
// Reads four PH-402C / PH-4502C analog outputs on A0-A3 and prints one JSON
// line per sweep:
//   {"time_s":5,"raw_0":512,"v_0":2.500,"ph_0":7.00,...}
//
// Every reading is reported twice:
//   raw_i  averaged 10-bit ADC count (0-1023)
//   v_i    that count converted to volts (0-5 V)
//   ph_i   pH from the linear map below
//
// Linear map (temporary, assumed linear for now):
//   pH 7.00 at 512 counts (2.500 V).
//   Slope: the full 0-5 V span (1023 counts) covers 14 pH, as hardware.md
//   section 5.5 states for the PO output.
//   Sign: -1, so a higher voltage gives a lower pH. This matches the polarity
//   of the measured per-probe table kept in the comment below. Flip
//   PH_SIGN to +1 if a probe reads the other way.
//
// The probe output is not linear over the full 0-14 range. The previous
// three-point piecewise fit is kept in the comment below for reference.
//
// How to check (per probe):
//   1. Flash and log "raw_i"/"v_i".
//   2. Immerse the probe in a pH 7 buffer and wait for a stable reading. The
//      raw count should be close to 512 and "ph_i" close to 7.00. Record the
//      offset if not.
//   3. Re-check in pH 4.01 and 9.18 buffers. Rinse the probe with distilled
//      water and blot it dry between buffers.
//   4. If a buffer gives a clearly wrong value, re-measure it rather than
//      averaging it into the result.
//
// Wiring per probe:
//   module VCC -> 5V
//   module GND -> GND
//   module PO  -> A0/A1/A2/A3 (one probe per pin, in PROBE_PINS order)
//   BNC shield -> the module's dedicated probe GND
//
// A floating or disconnected input is reported as null, never as 0. The
// conversion rejects 0 V and the 5 V rail, the same guard the legacy slave uses.
#include <Arduino.h>

namespace {
constexpr uint8_t NUM_PROBES = 4;
const uint8_t PROBE_PINS[NUM_PROBES] = {A0, A1, A2, A3};

constexpr uint8_t ADC_SAMPLES = 16;
constexpr unsigned long ADC_SAMPLE_GAP_MS = 1;
constexpr unsigned long SWEEP_INTERVAL_MS = 1000;
constexpr float ADC_REF_V = 5.0f;
constexpr float ADC_MAX_COUNT = 1023.0f;

// --- LINEAR pH MAP (shared by all probes) ---
// The ADC count that corresponds to pH 7.00 (512 counts = 2.500 V).
constexpr float PH_ANCHOR_COUNT = 512.0f;
constexpr float PH_ANCHOR_VALUE = 7.0f;
// pH change per ADC count: the full 0-5 V span (1023 counts) covers 14 pH.
constexpr float PH_PER_COUNT = 14.0f / ADC_MAX_COUNT;
// -1: a higher voltage gives a lower pH (measured polarity). Use +1 to invert.
constexpr float PH_SIGN = -1.0f;
}  // namespace

// Previous per-probe three-point table, kept for reference. Not used by the
// linear map above. Measured module output voltage at each buffer.
//   PH_V_401 = {4.120, 4.160, 4.160, 4.160}   (pH 4.01)
//   PH_V_686 = {3.833, 3.833, 3.833, 3.833}   (pH 6.86)
//   PH_V_918 = {3.620, 3.630, 3.630, 3.630}   (pH 9.18)

// Average several ADC samples to reduce noise on the high-impedance probe
// output. Returns the unrounded mean count.
static float readAverageCount(uint8_t pin) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
    if (i + 1 < ADC_SAMPLES) delay(ADC_SAMPLE_GAP_MS);
  }
  return static_cast<float>(sum) / static_cast<float>(ADC_SAMPLES);
}

// Linear conversion from the averaged count to pH:
//   pH = 7.00 + PH_SIGN * (count - 512) * (14 / 1023)
// 0 V and the 5 V rail are invalid. A NaN input fails the same test.
// The result is clamped to 0-14 and rounded to two decimals.
static bool convertCountToPh(float count, float& phOut) {
  if (!(count > 0.0f) || count >= ADC_MAX_COUNT) {
    return false;
  }

  float ph = PH_ANCHOR_VALUE + PH_SIGN * (count - PH_ANCHOR_COUNT) * PH_PER_COUNT;

  if (!isfinite(ph)) {
    return false;
  }
  if (ph < 0.0f) ph = 0.0f;
  if (ph > 14.0f) ph = 14.0f;
  phOut = roundf(ph * 100.0f) / 100.0f;
  return true;
}

void setup() {
  for (uint8_t i = 0; i < NUM_PROBES; i++) {
    pinMode(PROBE_PINS[i], INPUT);
  }
  Serial.begin(115200);
  delay(100);
  Serial.println(F("PH-402C 4-probe pH calibration logger started"));
}

void loop() {
  Serial.print(F("{\"time_s\":"));
  Serial.print(millis() / 1000);

  for (uint8_t i = 0; i < NUM_PROBES; i++) {
    const float count = readAverageCount(PROBE_PINS[i]);
    const float volts = count * (ADC_REF_V / ADC_MAX_COUNT);

    float ph = 0.0f;
    const bool ok = convertCountToPh(count, ph);

    Serial.print(F(",\"raw_"));
    Serial.print(i);
    Serial.print(F("\":"));
    Serial.print(count, 0);

    Serial.print(F(",\"v_"));
    Serial.print(i);
    Serial.print(F("\":"));
    if (ok) {
      Serial.print(volts, 3);
    } else {
      Serial.print(F("null"));
    }

    Serial.print(F(",\"ph_"));
    Serial.print(i);
    Serial.print(F("\":"));
    if (ok) {
      Serial.print(ph, 2);
    } else {
      Serial.print(F("null"));
    }
  }

  Serial.println('}');
  delay(SWEEP_INTERVAL_MS);
}
