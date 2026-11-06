"""Native regression for the actual slave/logger methane code (stdlib + C++11).

Run from the repository root:
    python3 firmware/tests/test_methane_calibration.py -v

Extract only methane declarations, setup calibration, and conversion functions;
no Arduino libraries or unrelated sensor stubs are needed. All generated files
live in TemporaryDirectory. Native floating point is not an AVR/hardware test.
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


def braced(source, marker, *, last=False):
    """Extract one known C++ block, failing loudly if its source shape changes."""
    start = source.rindex(marker) if last else source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for end in range(opening, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[start : end + 1]
    raise ValueError(f"Unclosed C++ block: {marker}")


def declarations(source, names):
    result = []
    for name in names.split():
        match = re.search(
            rf"^(?:const\s+)?(?:int|float|bool)\s+{name}\b[^;]*;",
            source,
            re.MULTILINE,
        )
        if match is None:
            raise ValueError(f"Missing C++ declaration: {name}")
        result.append(match.group())
    return "\n".join(result)


def native_source():
    slave = (ROOT / "legacy/slave/new-slave.cpp").read_text()
    logger = (ROOT / "firmware/methane-calibrate/main.cpp").read_text()
    update = braced(slave, "void updateAllMethane()")
    sensor_loop = braced(
        update, "for (int i = 0; i < NUM_BOTTLES; i++)", last=True
    )
    # Preserve the exact post-ADC conversion, including its continue statements.
    conversion = sensor_loop[
        sensor_loop.index("if (!isfinite(rs)") : sensor_loop.rindex("}")
    ]
    setup_loop = braced(
        braced(slave, "void setup()"),
        "for (int i = 0; i < NUM_BOTTLES; i++)",
    )
    return "\n".join(
        [
            PRELUDE,
            "namespace slave {",
            declarations(
                slave,
                "NUM_BOTTLES PIN_METHANE PIN_PH PIN_CO2_PWM RL ADC_SAMPLES "
                "ADC_SCALE PPM_MIN_TYPICAL PPM_MAX RS_AIR RS_SPAN PPM_AIR "
                "PPM_SPAN curve_exp rs_at_max ch4_cal_ok cachedMethane",
            ),
            ADC_STUB,
            braced(slave, "bool fitExponent("),
            update,
            "void setup() {" + setup_loop + "}",
            "void fromResistance(int i, float rs) { do {" + conversion + "} while (false); }",
            "} namespace logger {",
            declarations(
                logger,
                "NUM_SENSORS RS_AIR RS_SPAN PPM_AIR PPM_SPAN "
                "CORR_SLOPE CORR_OFFSET curve_exp",
            ),
            braced(logger, "void setup()"),
            braced(logger, "float rsToPpmRaw("),
            braced(logger, "float rsToPpm("),
            "}",
            CHECKS,
        ]
    )


PRELUDE = r"""
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
using std::isfinite;
enum { A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, INPUT };
void delay(int) {}
void pinMode(int, int) {}
struct SerialStub { void begin(int) {} } Serial;
"""

ADC_STUB = r"""
std::array<std::array<int, ADC_SAMPLES>, NUM_BOTTLES> adc{};
std::array<int, NUM_BOTTLES> reads{};
int ping_calls = 0;
void serviceMasterPing() { ++ping_calls; }
int analogRead(int pin) {
  for (int i = 0; i < NUM_BOTTLES; ++i) {
    if (pin != PIN_METHANE[i]) continue;
    int call = reads.at(i)++;
    // A clipped mux-switch reading must be discarded, not accumulated.
    return call % 2 == 0 ? 0 : adc.at(i).at(call / 2);
  }
  std::abort();
}
"""

CHECKS = r"""
int checks = 0;
void check(bool ok, const char* label, int sensor = -1) {
  ++checks;
  if (!ok) {
    std::cerr << label << " (sensor " << sensor << ")\n";
    std::exit(1);
  }
}
void near(float actual, float expected, const char* label, int sensor,
          float relative = 2e-5f, float absolute = 2e-5f) {
  if (!isfinite(actual) ||
      std::fabs(actual - expected) > std::max(absolute, relative * std::fabs(expected))) {
    std::cerr << "actual=" << actual << ", expected=" << expected << ": ";
    check(false, label, sensor);
  }
  check(true, label, sensor);
}
float slavePpm(float rs, int i) {
  slave::fromResistance(i, rs);
  return slave::cachedMethane[i];
}
void compare(float rs, int i) {
  float raw = logger::rsToPpmRaw(rs, i);
  check(isfinite(raw) && raw > 0, "positive finite curve", i);
  near(slavePpm(rs, i), raw, "slave/logger agreement", i);
  check(logger::rsToPpm(rs, i) == raw, "logger identity correction", i);
}
void readAdc() {
  slave::reads.fill(0);
  slave::ping_calls = 0;
  slave::updateAllMethane();
  check(slave::ping_calls == slave::ADC_SAMPLES, "ADC sampling services master");
  for (int i = 0; i < 4; ++i)
    check(slave::reads[i] == 2 * slave::ADC_SAMPLES, "mux discard/read count", i);
}
void normalAdc() {
  for (int i = 0; i < 4; ++i) {
    float raw = slave::ADC_SCALE / (1 + slave::RS_SPAN[i] / slave::RL[i]);
    for (int j = 0; j < slave::ADC_SAMPLES; ++j)
      slave::adc[i][j] = static_cast<int>(std::floor(raw)) + j % 2;
  }
}
void checkAdcValues() {
  for (int i = 0; i < 4; ++i) {
    double sum = 0;
    for (int raw : slave::adc[i]) sum += raw;
    float rs = slave::RL[i] * (1024.0 / (sum / slave::ADC_SAMPLES) - 1.0);
    near(slave::cachedMethane[i], logger::rsToPpm(rs, i), "ADC conversion", i, 5e-5f);
  }
}
int main() {
  const float air[] = {6160.3f, 5370.7f, 8592.1f, 7950.2f};
  const float span[] = {259.8083f, 370.7250f, 382.4533f, 300.8875f};
  const float ppm[] = {432, 450, 387, 499};
  const float exponent[] = {1.6978478f, 2.0260336f, 1.6919317f, 1.6857346f};
  const std::vector<std::vector<float>> samples = {
    {260.2f,259.8f,259.9f,259.8f,259.9f,259.8f,259.8f,259.8f,259.7f,259.8f,259.5f,259.7f},
    {373.1f,371.8f,373.5f,373.9f,371.6f,371.7f,370.3f,369.1f,370.5f,369.7f,369.4f,370.6f,369.8f,368.7f,368.3f,369.6f},
    {375.8f,378.1f,378.0f,380.2f,383.8f,387.4f,387.5f,384.1f,384.1f,381.2f,382.2f,383.6f,383.8f,383.4f,383.6f},
    {300.8f,300.6f,301.0f,301.1f,301.0f,301.1f,300.9f,301.0f,300.9f,300.6f,300.9f,301.0f,300.6f,300.8f,301.1f,300.8f}
  };
  check(slave::NUM_BOTTLES == 4 && logger::NUM_SENSORS == 4, "four sensors");
  slave::setup();
  logger::setup();
  near(slave::PPM_AIR, 2, "slave background", -1);
  near(logger::PPM_AIR, 2, "logger background", -1);
  near(slave::PPM_MAX, 10000, "maximum concentration", -1);
  for (int i = 0; i < 4; ++i) {
    near(slave::RS_AIR[i], air[i], "slave air data", i);
    near(logger::RS_AIR[i], air[i], "logger air data", i);
    near(slave::RS_SPAN[i], span[i], "slave span data", i);
    near(logger::RS_SPAN[i], span[i], "logger span data", i);
    near(slave::PPM_SPAN[i], ppm[i], "slave reference data", i);
    near(logger::PPM_SPAN[i], ppm[i], "logger reference data", i);
    near(logger::CORR_SLOPE[i], 1, "correction slope", i);
    near(logger::CORR_OFFSET[i], 0, "correction offset", i);
    check(slave::ch4_cal_ok[i], "valid calibration", i);
    near(slave::curve_exp[i], exponent[i], "slave fitted exponent", i, 2e-6f);
    near(logger::curve_exp[i], exponent[i], "logger fitted exponent", i, 2e-6f);
    near(slavePpm(air[i], i), 2, "slave air endpoint", i);
    near(logger::rsToPpm(air[i], i), 2, "logger air endpoint", i);
    near(slavePpm(span[i], i), ppm[i], "slave span endpoint", i);
    near(logger::rsToPpm(span[i], i), ppm[i], "logger span endpoint", i);

    float threshold = slave::rs_at_max[i];
    check(isfinite(threshold) && threshold > 0 && threshold < span[i], "valid threshold", i);
    near(logger::rsToPpm(threshold, i), 10000, "threshold inversion", i);
    near(slavePpm(threshold, i), 10000, "inclusive exact maximum", i);
    check(slavePpm(std::nextafter(threshold, 0.0f), i) == -1, "overrange resistance", i);
    compare(threshold * 1.001f, i);
    for (float bad : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()})
      check(slavePpm(bad, i) == -1, "invalid resistance", i);
    check(logger::rsToPpm(0, i) == -1, "logger invalid resistance", i);

    float previous = 0;
    for (int j = 0; j <= 100; ++j) {
      float high = 2 * air[i], low = threshold * 1.001f;
      float rs = high * std::pow(low / high, j / 100.0f);
      compare(rs, i);
      float current = slave::cachedMethane[i];
      check(current > previous, "ppm increases as resistance decreases", i);
      previous = current;
    }
    double sum = 0;
    for (float rs : samples[i]) { sum += rs; compare(rs, i); }
    float mean = sum / samples[i].size();
    near(mean, span[i], "reference sample mean", i, 0, 1e-4f);
    near(slavePpm(mean, i), ppm[i], "sample mean reference ppm", i);
  }
  std::cout << "PASS: calibration data, endpoints, curves, sample vectors, identity correction\n";

  normalAdc();
  readAdc();
  checkAdcValues();
  // Also exercise complete ADC conversion near the air anchors.
  for (int i = 0; i < 4; ++i) {
    float raw = slave::ADC_SCALE / (1 + air[i] / slave::RL[i]);
    for (int j = 0; j < slave::ADC_SAMPLES; ++j)
      slave::adc[i][j] = static_cast<int>(std::floor(raw)) + j % 2;
  }
  readAdc();
  checkAdcValues();
  for (int i = 0; i < 4; ++i) {
    float raw = slave::ADC_SCALE / (1 + slave::rs_at_max[i] / slave::RL[i]);
    slave::adc[i].fill(static_cast<int>(std::floor(raw)));
  }
  readAdc();
  checkAdcValues();
  for (int i = 0; i < 4; ++i) {
    check(slave::cachedMethane[i] > 0 && slave::cachedMethane[i] <= 10000,
          "ADC bin below maximum", i);
    for (int& raw : slave::adc[i]) ++raw;
  }
  readAdc();
  for (int i = 0; i < 4; ++i)
    check(slave::cachedMethane[i] == -1, "ADC bin above maximum", i);

  for (int sensor = 0; sensor < 4; ++sensor) {
    for (int rail : {0, 1023}) {
      normalAdc();
      readAdc();
      slave::adc[sensor][slave::ADC_SAMPLES / 2] = rail;
      readAdc();
      for (int i = 0; i < 4; ++i)
        check(i == sensor ? slave::cachedMethane[i] == -1 : slave::cachedMethane[i] > 0,
              "one clipped sample invalidates only its channel", i);
    }
    normalAdc();
    slave::ch4_cal_ok[sensor] = false;
    readAdc();
    for (int i = 0; i < 4; ++i)
      check(i == sensor ? slave::cachedMethane[i] == -1 : slave::cachedMethane[i] > 0,
            "invalid calibration isolates channel", i);
    slave::ch4_cal_ok[sensor] = true;
  }
  const float invalid_fits[][4] = {
    {0,2,300,400}, {6000,2,0,400}, {300,2,300,400}, {200,2,300,400},
    {6000,0,300,400}, {6000,2,300,2}, {6000,2,300,1}, {6000,2,300,10001},
    {std::numeric_limits<float>::quiet_NaN(),2,300,400},
    {6000,2,std::numeric_limits<float>::infinity(),400}
  };
  for (const auto& values : invalid_fits) {
    float fitted = 0;
    check(!slave::fitExponent(values[0], values[1], values[2], values[3], fitted),
          "invalid calibration fit rejected");
  }
  std::cout << "PASS: full ADC sampling, maximum/overrange, clipping, invalid calibration\n"
            << checks << " native checks passed\n";
}
"""


class MethaneCalibrationRegression(unittest.TestCase):
    def test_actual_native_methane_sections(self):
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        self.assertIsNotNone(compiler, "A native C++11 compiler is required")
        with tempfile.TemporaryDirectory(prefix="methane-regression-") as temporary:
            source = Path(temporary) / "methane_regression.cpp"
            binary = Path(temporary) / "methane_regression"
            source.write_text(native_source())
            build = subprocess.run(
                [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                 "-O0", str(source), "-o", str(binary)],
                capture_output=True, text=True, timeout=60,
            )
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run(
                [str(binary)], capture_output=True, text=True, timeout=15
            )
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            print(run.stdout, end="")


if __name__ == "__main__":
    unittest.main()
