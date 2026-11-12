// Behavioral integration tests: bytes enter Serial1 and the actual production
// link_serial.cpp parses them before fake_data.cpp updates the model. Neither
// sensor/link/USB functions nor model state are replaced by simulator stubs.
// Only UI delegation and unused ESP dependencies are stubbed.
#include "fake_data.h"
#include "link_serial.h"
#include "serial_cmd.h"
#include "ui_shell.h"

#include <array>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace host {
uint32_t now_ms = 0;
int toggle_calls = 0;
int data_calls = 0;
int overheat_calls = 0;
int observed_exp = -1;
int observed_duration = -1;
bool observed_running = false;
bool allow_toggle = false;
}  // namespace host
HostSerial Serial;
HostSerial Serial1;
HostESP ESP;

// A controllable UI spy, not the production gate: a denied toggle must leave
// RUN unable to start a run itself. An allowed toggle changes only run state,
// so an experiment-number increment in serial_cmd.cpp cannot be hidden here.
void ui_experiment_toggle() {
  ++host::toggle_calls;
  host::observed_exp = g.expNum;
  host::observed_duration = g.durationH;
  host::observed_running = g.experimentRunning;
  if (host::allow_toggle) g.experimentRunning = !g.experimentRunning;
}
void ui_shell_on_data() { ++host::data_calls; }
void ui_eval_overheat() { ++host::overheat_calls; }

namespace {

void check(bool condition, const char *expression, int line,
           const std::string &context = std::string()) {
  if (condition) return;
  std::ostringstream message;
  message << "line " << line << ": " << expression;
  if (!context.empty()) message << " (" << context << ')';
  throw std::runtime_error(message.str());
}

#define CHECK(expression) check((expression), #expression, __LINE__)
#define CHECK_AT(expression, context) \
  check((expression), #expression, __LINE__, (context))

std::string position(int cluster, int bottle, int field = -1) {
  std::ostringstream text;
  text << "node " << cluster << ", bottle " << bottle;
  if (field >= 0) text << ", field/point " << field;
  return text.str();
}

void near(float actual, float expected) {
  std::ostringstream text;
  text << "actual=" << actual << ", expected=" << expected;
  CHECK_AT(std::isfinite(actual) && std::fabs(actual - expected) < 0.0001f,
           text.str());
}

void receive(const std::string &bytes) {
  Serial1.feed(bytes);
  link_serial_poll();
  CHECK(Serial1.available() == 0);
}

void reset_group() {
  Serial.clear();
  Serial1.clear();
  host::advance_ms(1000);
  // HELLO is the public link-cache reset. fake_init must independently reset
  // model bookkeeping; tests deliberately run in one process to catch leaks.
  receive("HELLO\n");
  fake_init();
}

void reset_command_group() {
  reset_group();
  link_serial_set_exp_state(false, 0, 24);
  // Re-anchor the production heartbeat without reaching into its statics.
  link_serial_init();
  Serial.clear();
  Serial1.clear();
  host::toggle_calls = 0;
  host::data_calls = 0;
  host::overheat_calls = 0;
  host::observed_exp = -1;
  host::observed_duration = -1;
  host::observed_running = false;
  host::allow_toggle = false;
}

void usb(const std::string &bytes) {
  Serial.feed(bytes);
  serial_cmd_poll();
  CHECK(Serial.available() == 0);
}

void expect_uart(const std::string &expected) {
  CHECK_AT(Serial1.output() == expected, "actual UART output: " + Serial1.output());
  Serial1.clear_output();
}

void same_float(float actual, float expected) {
  CHECK((std::isnan(actual) && std::isnan(expected)) || actual == expected);
}

void model_unchanged(const AppState &before) {
  // Compare members rather than raw struct bytes: padding and NAN encodings
  // are not a portable equality contract for AppState.
  CHECK(g.loggedIn == before.loggedIn);
  CHECK(g.experimentRunning == before.experimentRunning);
  CHECK(g.experimentStartMs == before.experimentStartMs);
  CHECK(g.expNum == before.expNum);
  CHECK(g.durationH == before.durationH);
  same_float(g.tempSetpointC, before.tempSetpointC);
  CHECK(g.stirrerOn == before.stirrerOn);
  CHECK(g.postEnabled == before.postEnabled);
  same_float(g.chamberTempC, before.chamberTempC);
  CHECK(std::strcmp(g.ipAddr, before.ipAddr) == 0);
  CHECK(std::strcmp(g.lastHttp, before.lastHttp) == 0);
  CHECK(std::strcmp(g.storageNote, before.storageNote) == 0);
  for (int cluster = 0; cluster < UI_CLUSTERS; ++cluster) {
    CHECK(g.cluster[cluster].online == before.cluster[cluster].online);
    for (int bottle = 0; bottle < UI_BOTTLES_PER_CLUSTER; ++bottle) {
      const BottleFake &actual = g.cluster[cluster].bottle[bottle];
      const BottleFake &expected = before.cluster[cluster].bottle[bottle];
      CHECK(actual.present == expected.present);
      for (int sensor = 0; sensor < S_COUNT; ++sensor) {
        CHECK(actual.sensorNull[sensor] == expected.sensorNull[sensor]);
        same_float(actual.sensor[sensor], expected.sensor[sensor]);
      }
      for (int sensor = 0; sensor < S_COUNT; ++sensor) {
        for (int point = 0; point < UI_TREND_PTS; ++point) {
          same_float(actual.trend[sensor][point], expected.trend[sensor][point]);
        }
      }
    }
  }
}

void unavailable_bottle(int cluster, int bottle) {
  const BottleFake &value = g.cluster[cluster].bottle[bottle];
  CHECK_AT(!value.present, position(cluster, bottle));
  for (int sensor = 0; sensor < S_COUNT; ++sensor) {
    CHECK_AT(value.sensorNull[sensor], position(cluster, bottle, sensor));
    CHECK_AT(std::isnan(value.sensor[sensor]), position(cluster, bottle, sensor));
  }
  for (int sensor = 0; sensor < S_COUNT; ++sensor) {
    for (int point = 0; point < UI_TREND_PTS; ++point) {
      CHECK_AT(std::isnan(value.trend[sensor][point]), position(cluster, bottle, point));
    }
  }
}

void unavailable_model() {
  for (int cluster = 0; cluster < UI_CLUSTERS; ++cluster) {
    CHECK_AT(!g.cluster[cluster].online, position(cluster, -1));
    for (int bottle = 0; bottle < UI_BOTTLES_PER_CLUSTER; ++bottle) {
      unavailable_bottle(cluster, bottle);
    }
  }
  CHECK(std::isnan(g.chamberTempC));
  CHECK(!sens_real_active());
  CHECK(fake_any_null());
  CHECK(fake_null_count() == UI_CLUSTERS * UI_BOTTLES_PER_CLUSTER * S_COUNT);
}

void only_bottle_zero_present() {
  CHECK(g.cluster[0].online);
  CHECK(g.cluster[0].bottle[0].present);
  for (int cluster = 0; cluster < UI_CLUSTERS; ++cluster) {
    if (cluster != 0) CHECK_AT(!g.cluster[cluster].online, position(cluster, -1));
    for (int bottle = 0; bottle < UI_BOTTLES_PER_CLUSTER; ++bottle) {
      if (cluster == 0 && bottle == 0) continue;
      unavailable_bottle(cluster, bottle);
    }
  }
}

void full_frame(float temp = 37.5f) {
  std::ostringstream line;
  line << "SENS b1 ph=7.1 ch4=2.3 t=" << temp << " co2=900 p=1.2\n";
  receive(line.str());
}

void check_full_bottle(float temp = 37.5f) {
  const BottleFake &bottle = g.cluster[0].bottle[0];
  for (int sensor = 0; sensor < S_COUNT; ++sensor) CHECK(!bottle.sensorNull[sensor]);
  near(bottle.sensor[S_CO2], 900.0f);
  near(bottle.sensor[S_CH4], 2.3f);
  near(bottle.sensor[S_PRESS], 1.2f);
  near(bottle.sensor[S_PH], 7.1f);
  near(bottle.sensor[S_TEMP], temp);
}

// Every sensor keeps its own history, and each history holds exactly one sample
// after the first applied frame.
void one_trend_point(float temp) {
  const BottleFake &bottle = g.cluster[0].bottle[0];
  for (int sensor = 0; sensor < S_COUNT; ++sensor) {
    for (int point = 0; point < UI_TREND_PTS - 1; ++point) {
      CHECK_AT(std::isnan(bottle.trend[sensor][point]), position(0, 0, point));
    }
  }
  near(bottle.trend[S_CO2][UI_TREND_PTS - 1], 900.0f);
  near(bottle.trend[S_CH4][UI_TREND_PTS - 1], 2.3f);
  near(bottle.trend[S_PRESS][UI_TREND_PTS - 1], 1.2f);
  near(bottle.trend[S_PH][UI_TREND_PTS - 1], 7.1f);
  near(bottle.trend[S_TEMP][UI_TREND_PTS - 1], temp);
}

void test_initial_state_and_no_synthetic_data() {
  reset_group();
  unavailable_model();
  CHECK(link_serial_get_sens(nullptr) == -1);
  CHECK(!sens_apply_real());
  unavailable_model();
  host::advance_ms(UI_SENS_STALE_MS + 1);
  link_serial_poll();
  CHECK(!sens_apply_real());
  CHECK(!sens_apply_real());
  unavailable_model();
}

void test_real_serial_parsing_and_node_isolation() {
  reset_group();
  receive("SENS b1 ph=7.1 ch4=2.3");
  CHECK(link_serial_get_sens(nullptr) == -1);
  CHECK(!sens_apply_real());
  receive(" t=37.5 co2=900 p=1.2\r\n");

  LinkSensFrame frame;
  CHECK(link_serial_get_sens(&frame) == 0);
  CHECK(frame.seq != 0);
  CHECK(frame.present[0]);
  for (int bottle = 1; bottle < LINK_SENS_BOTTLES; ++bottle) {
    CHECK(!frame.present[bottle]);
  }
  near(frame.value[0][LINK_SENS_CO2], 900.0f);
  near(frame.value[0][LINK_SENS_CH4], 2.3f);
  near(frame.value[0][LINK_SENS_PRESS], 1.2f);
  near(frame.value[0][LINK_SENS_PH], 7.1f);
  near(frame.value[0][LINK_SENS_TEMP], 37.5f);
  CHECK(sens_apply_real());
  CHECK(sens_real_active());
  check_full_bottle();
  only_bottle_zero_present();
  CHECK(fake_null_count() == UI_CLUSTERS * UI_BOTTLES_PER_CLUSTER * S_COUNT - S_COUNT);
}

void test_partial_frame_does_not_keep_or_invent_values() {
  reset_group();
  full_frame();
  CHECK(sens_apply_real());
  receive("SENS b1 ph=6.9\n");
  LinkSensFrame frame;
  CHECK(link_serial_get_sens(&frame) == 0);
  CHECK(frame.present[0]);
  near(frame.value[0][LINK_SENS_PH], 6.9f);
  for (int bottle = 0; bottle < LINK_SENS_BOTTLES; ++bottle) {
    for (int sensor = 0; sensor < LINK_SENS_VALS; ++sensor) {
      if (bottle == 0 && sensor == LINK_SENS_PH) continue;
      CHECK_AT(std::isnan(frame.value[bottle][sensor]), position(0, bottle, sensor));
    }
  }
  CHECK(sens_apply_real());
  const BottleFake &bottle = g.cluster[0].bottle[0];
  near(bottle.sensor[S_PH], 6.9f);
  CHECK(!bottle.sensorNull[S_PH]);
  for (int sensor = 0; sensor < S_COUNT; ++sensor) {
    if (sensor == S_PH) continue;
    CHECK(bottle.sensorNull[sensor]);
    CHECK(std::isnan(bottle.sensor[sensor]));
  }
  only_bottle_zero_present();
}

void test_malformed_numeric_suffixes_are_not_accepted() {
  reset_group();
  const char *invalid[] = {"37oops", "37.5C", "1e+", "oops37", "", "nanSuffix", "infOops"};
  for (const char *token : invalid) {
    receive(std::string("SENS b1 ph=7.1 t=") + token + "\n");
    LinkSensFrame frame;
    CHECK(link_serial_get_sens(&frame) == 0);
    CHECK_AT(std::isnan(frame.value[0][LINK_SENS_TEMP]), token);
    CHECK(sens_apply_real());
    CHECK_AT(g.cluster[0].bottle[0].sensorNull[S_TEMP], token);
    CHECK_AT(std::isnan(g.cluster[0].bottle[0].sensor[S_TEMP]), token);
    // The invalid temperature never enters its history; the valid pH field in
    // the same frame still records its own sample.
    for (int point = 0; point < UI_TREND_PTS; ++point) {
      CHECK_AT(std::isnan(g.cluster[0].bottle[0].trend[S_TEMP][point]), token);
    }
    near(g.cluster[0].bottle[0].trend[S_PH][UI_TREND_PTS - 1], 7.1f);
  }
  receive("SENS b1 ph=7.1bad ch4=2.3bad t=37oops co2=900bad p=1.2bad\n");
  LinkSensFrame frame;
  CHECK(link_serial_get_sens(&frame) == 0);
  for (int sensor = 0; sensor < LINK_SENS_VALS; ++sensor) {
    CHECK(std::isnan(frame.value[0][sensor]));
  }
  CHECK(sens_apply_real());
  for (int sensor = 0; sensor < S_COUNT; ++sensor) {
    CHECK(g.cluster[0].bottle[0].sensorNull[sensor]);
    CHECK(std::isnan(g.cluster[0].bottle[0].sensor[sensor]));
  }
}

void test_nonfinite_fields_become_model_nulls() {
  reset_group();
  // Replace old finite readings too: setting only sensorNull would leave
  // stale numeric values available to other model consumers.
  const char *frames[] = {
      "SENS b1 ph=nan ch4=inf t=-inf co2=nan p=inf\n",
      "SENS b1 ph=inf ch4=nan t=nan co2=-inf p=nan\n"};
  for (const char *line : frames) {
    full_frame();
    CHECK(sens_apply_real());
    receive(line);
    LinkSensFrame frame;
    CHECK(link_serial_get_sens(&frame) == 0);
    for (int sensor = 0; sensor < LINK_SENS_VALS; ++sensor) {
      CHECK(!std::isfinite(frame.value[0][sensor]));
    }
    CHECK(sens_apply_real());
    CHECK(sens_real_active());
    only_bottle_zero_present();
    for (int sensor = 0; sensor < S_COUNT; ++sensor) {
      CHECK(g.cluster[0].bottle[0].sensorNull[sensor]);
      CHECK(std::isnan(g.cluster[0].bottle[0].sensor[sensor]));
    }
    char text[32];
    CHECK(std::string(fake_sensor_text(0, 0, S_TEMP, text, sizeof(text))) == "-- NULL");
  }
}

void test_trend_contains_only_new_real_samples() {
  reset_group();
  unavailable_model();
  full_frame();
  CHECK(sens_apply_real());
  one_trend_point(37.5f);
  LinkSensFrame first;
  CHECK(link_serial_get_sens(&first) == 0);
  CHECK(first.seq != 0);
  std::array<std::array<float, UI_TREND_PTS>, S_COUNT> before;
  for (int sensor = 0; sensor < S_COUNT; ++sensor) {
    for (int point = 0; point < UI_TREND_PTS; ++point) {
      before[sensor][point] = g.cluster[0].bottle[0].trend[sensor][point];
    }
  }
  for (int repeat = 0; repeat < 3; ++repeat) {
    host::advance_ms(UI_SENS_APPLY_MS);
    CHECK(!sens_apply_real());
    for (int sensor = 0; sensor < S_COUNT; ++sensor) {
      for (int point = 0; point < UI_TREND_PTS; ++point) {
        const float after = g.cluster[0].bottle[0].trend[sensor][point];
        CHECK((std::isnan(before[sensor][point]) && std::isnan(after)) ||
              before[sensor][point] == after);
      }
    }
  }
  LinkSensFrame duplicate;
  CHECK(link_serial_get_sens(&duplicate) == 3 * UI_SENS_APPLY_MS);
  CHECK(duplicate.seq == first.seq);
  // Identical readings in a NEW frame still represent one real new sample.
  full_frame();
  LinkSensFrame second;
  CHECK(link_serial_get_sens(&second) == 0);
  CHECK(second.seq == first.seq + 1);
  CHECK(sens_apply_real());
  for (int point = 0; point < UI_TREND_PTS - 2; ++point) {
    CHECK(std::isnan(g.cluster[0].bottle[0].trend[S_TEMP][point]));
  }
  near(g.cluster[0].bottle[0].trend[S_TEMP][UI_TREND_PTS - 2], 37.5f);
  near(g.cluster[0].bottle[0].trend[S_TEMP][UI_TREND_PTS - 1], 37.5f);
  CHECK(!sens_apply_real());
  full_frame(38.1f);
  CHECK(sens_apply_real());
  for (int point = 0; point < UI_TREND_PTS - 3; ++point) {
    CHECK(std::isnan(g.cluster[0].bottle[0].trend[S_TEMP][point]));
  }
  near(g.cluster[0].bottle[0].trend[S_TEMP][UI_TREND_PTS - 3], 37.5f);
  near(g.cluster[0].bottle[0].trend[S_TEMP][UI_TREND_PTS - 2], 37.5f);
  near(g.cluster[0].bottle[0].trend[S_TEMP][UI_TREND_PTS - 1], 38.1f);
}

void test_disappearing_bottles_are_cleared() {
  reset_group();
  receive("SENS b1 t=37.5 b2 t=38.0 ph=7.0 b4 co2=950 t=36.5\n");
  CHECK(sens_apply_real());
  CHECK(g.cluster[0].bottle[0].present);
  CHECK(g.cluster[0].bottle[1].present);
  unavailable_bottle(0, 2);
  CHECK(g.cluster[0].bottle[3].present);
  near(g.cluster[0].bottle[1].sensor[S_TEMP], 38.0f);
  near(g.cluster[0].bottle[3].sensor[S_CO2], 950.0f);
  receive("SENS b1 t=37.6\n");
  CHECK(sens_apply_real());
  only_bottle_zero_present();
}

void test_staleness_boundary_single_refresh_and_recovery() {
  reset_group();
  full_frame();
  CHECK(sens_apply_real());
  host::advance_ms(UI_SENS_STALE_MS);
  CHECK(link_serial_get_sens(nullptr) == static_cast<int32_t>(UI_SENS_STALE_MS));
  CHECK(!sens_apply_real());
  CHECK(sens_real_active());
  check_full_bottle();
  only_bottle_zero_present();
  one_trend_point(37.5f);

  host::advance_ms(1);
  CHECK(sens_apply_real());
  unavailable_model();
  CHECK(!sens_apply_real());
  host::advance_ms(UI_SENS_APPLY_MS);
  CHECK(!sens_apply_real());
  unavailable_model();

  full_frame(38.2f);
  CHECK(sens_apply_real());
  CHECK(sens_real_active());
  check_full_bottle(38.2f);
  only_bottle_zero_present();
  one_trend_point(38.2f);
  CHECK(!sens_apply_real());
}

void test_hello_invalidates_cached_frame_and_model() {
  reset_group();
  full_frame();
  CHECK(sens_apply_real());
  CHECK(sens_real_active());
  receive("HELLO\n");
  LinkSensFrame frame;
  CHECK(link_serial_get_sens(&frame) == -1);
  CHECK(link_serial_get_sens(nullptr) == -1);
  CHECK(frame.seq == 0);
  for (int bottle = 0; bottle < LINK_SENS_BOTTLES; ++bottle) CHECK(!frame.present[bottle]);
  CHECK(sens_apply_real());
  unavailable_model();
  CHECK(!sens_apply_real());
  host::advance_ms(UI_SENS_STALE_MS + 1);
  CHECK(!sens_apply_real());
  unavailable_model();
  full_frame(36.8f);
  CHECK(sens_apply_real());
  CHECK(sens_real_active());
  only_bottle_zero_present();
  check_full_bottle(36.8f);
  one_trend_point(36.8f);
}

void test_fake_init_resets_internal_tracking() {
  reset_group();
  full_frame();
  CHECK(sens_apply_real());
  CHECK(!sens_apply_real());
  fake_init();
  unavailable_model();
  // Keep the SAME cached frame: forgetting to reset last-sequence tracking
  // would incorrectly suppress the first apply after reinitialization.
  CHECK(link_serial_get_sens(nullptr) == 0);
  CHECK(sens_apply_real());
  CHECK(sens_real_active());
  check_full_bottle();
  one_trend_point(37.5f);
  CHECK(!sens_apply_real());

  host::advance_ms(UI_SENS_STALE_MS + 1);
  CHECK(sens_apply_real());
  unavailable_model();
  fake_init();
  fake_init();
  unavailable_model();
  full_frame(39.0f);
  CHECK(sens_apply_real());
  CHECK(sens_real_active());
  check_full_bottle(39.0f);
  one_trend_point(39.0f);
  CHECK(!sens_apply_real());
}

void test_experiment_wire_format_and_no_implicit_increment() {
  reset_command_group();
  link_serial_set_exp_state(false, 42, 18);
  expect_uart("EXPERIMENT run=0 num=42 dur=18\n");
  link_serial_set_exp_state(false, 42, 18);
  expect_uart("");
  link_serial_set_exp_state(true, 42, 18);
  expect_uart("EXPERIMENT run=1 num=42 dur=18\n");
  link_serial_set_exp_state(true, 42, 18);
  expect_uart("");
  link_serial_set_exp_state(false, 42, 18);
  expect_uart("EXPERIMENT run=0 num=42 dur=18\n");
  link_serial_set_exp_state(true, 42, 18);
  expect_uart("EXPERIMENT run=1 num=42 dur=18\n");
  link_serial_emit_experiment();
  expect_uart("EXPERIMENT run=1 num=42 dur=18\n");
}

void test_experiment_clamps_and_normalized_duplicate() {
  reset_command_group();
  link_serial_set_exp_state(false, -12, -4);
  expect_uart("EXPERIMENT run=0 num=0 dur=1\n");
  link_serial_set_exp_state(false, -1, 0);
  expect_uart("");
  link_serial_set_exp_state(true, 70000, 100);
  expect_uart("EXPERIMENT run=1 num=65535 dur=99\n");
  link_serial_set_exp_state(true, 999999, 1000);
  expect_uart("");
  link_serial_set_exp_state(false, 65535, 99);
  expect_uart("EXPERIMENT run=0 num=65535 dur=99\n");
  link_serial_set_exp_state(true, 0, 1);
  expect_uart("EXPERIMENT run=1 num=0 dur=1\n");
  link_serial_set_exp_state(false, 50000, 42);
  expect_uart("EXPERIMENT run=0 num=50000 dur=42\n");
}

void test_experiment_heartbeat_replays_exact_state() {
  reset_command_group();
  link_serial_set_exp_state(true, 73, 17);
  expect_uart("EXPERIMENT run=1 num=73 dur=17\n");
  link_serial_poll();
  expect_uart("");
  host::advance_ms(4999);
  link_serial_poll();
  expect_uart("");
  host::advance_ms(1);
  link_serial_poll();
  expect_uart("EXPERIMENT run=1 num=73 dur=17\n");
  link_serial_poll();
  expect_uart("");
  host::advance_ms(5000);
  link_serial_poll();
  expect_uart("EXPERIMENT run=1 num=73 dur=17\n");
  link_serial_set_exp_state(false, 73, 17);
  expect_uart("EXPERIMENT run=0 num=73 dur=17\n");
  host::advance_ms(5000);
  link_serial_poll();
  expect_uart("EXPERIMENT run=0 num=73 dur=17\n");
}


void test_usb_run_delegates_without_incrementing_experiment() {
  reset_command_group();
  g.expNum = 27;
  g.durationH = 36;
  usb("RUN ON\n");
  CHECK(host::toggle_calls == 1);
  CHECK(host::observed_exp == 27);
  CHECK(host::observed_duration == 36);
  CHECK(!host::observed_running);
  CHECK(!g.experimentRunning);
  CHECK(g.expNum == 27);
  CHECK(host::data_calls == 0);
  CHECK(host::overheat_calls == 0);

  host::allow_toggle = true;
  usb("RUN ON\n");
  CHECK(host::toggle_calls == 2);
  CHECK(host::observed_exp == 27);
  CHECK(!host::observed_running);
  CHECK(g.experimentRunning);
  CHECK(g.expNum == 27);
  CHECK(host::data_calls == 1);
  CHECK(host::overheat_calls == 1);
  usb("RUN ON\n");
  CHECK(host::toggle_calls == 2);
  CHECK(host::data_calls == 1);
  CHECK(host::overheat_calls == 1);
  usb("RUN OFF\n");
  CHECK(host::toggle_calls == 3);
  CHECK(host::observed_exp == 27);
  CHECK(host::observed_running);
  CHECK(!g.experimentRunning);
  CHECK(g.expNum == 27);
  CHECK(host::data_calls == 2);
  CHECK(host::overheat_calls == 2);
  usb("RUN OFF\n");
  CHECK(host::toggle_calls == 3);
  usb("RUN ON\nRUN OFF\n");
  CHECK(host::toggle_calls == 5);
  CHECK(host::observed_exp == 27);
  CHECK(g.expNum == 27);
  CHECK(g.durationH == 36);
  CHECK(!g.experimentRunning);
}

void test_usb_push_requests_hello_without_mutating_model() {
  reset_command_group();
  full_frame();
  CHECK(sens_apply_real());
  Serial1.clear_output();
  const AppState before = g;
  usb("PUSH\n");
  expect_uart("HELLO\n");
  model_unchanged(before);
  CHECK(sens_real_active());
  CHECK(link_serial_get_sens(nullptr) == 0);
  CHECK(host::toggle_calls == 0);
  CHECK(host::data_calls == 0);
  CHECK(host::overheat_calls == 0);
}

void test_usb_synthetic_commands_leave_model_unchanged() {
  reset_command_group();
  const char *commands[] = {
      "TEMP 45\n", "NODE 0 OFF\n", "NODE 1 ON\n",
      "NULL 0 0 PH\n", "UNNULL 0 0 PH\n"};
  // Test both offline startup and a real live frame: old commands must neither
  // invent readings nor erase/alter readings that came from the sensor link.
  for (int live = 0; live < 2; ++live) {
    if (live != 0) {
      full_frame();
      CHECK(sens_apply_real());
    }
    const AppState before = g;
    for (const char *command : commands) {
      Serial.clear_output();
      Serial1.clear_output();
      usb(command);
      CHECK(Serial.output().find("Synthetic sensor commands disabled") != std::string::npos);
      model_unchanged(before);
      CHECK(sens_real_active() == (live != 0));
      expect_uart("");
      CHECK(host::toggle_calls == 0);
      CHECK(host::data_calls == 0);
      CHECK(host::overheat_calls == 0);
    }
  }
}


struct TestCase {
  const char *name;
  void (*run)();
};

}  // namespace

int main() {
  link_serial_init();
  const TestCase tests[] = {
      {"initial state / no synthetic data", test_initial_state_and_no_synthetic_data},
      {"real serial parsing / node isolation", test_real_serial_parsing_and_node_isolation},
      {"partial fields / old values discarded", test_partial_frame_does_not_keep_or_invent_values},
      {"malformed numeric suffixes", test_malformed_numeric_suffixes_are_not_accepted},
      {"nan and inf become model NULL", test_nonfinite_fields_become_model_nulls},
      {"trend / one point per new frame", test_trend_contains_only_new_real_samples},
      {"disappearing bottles cleared", test_disappearing_bottles_are_cleared},
      {"staleness boundary / refresh / recovery", test_staleness_boundary_single_refresh_and_recovery},
      {"HELLO invalidates cache and model", test_hello_invalidates_cached_frame_and_model},
      {"fake_init resets internal tracking", test_fake_init_resets_internal_tracking},
      {"EXPERIMENT format / no implicit increment", test_experiment_wire_format_and_no_implicit_increment},
      {"EXPERIMENT clamps / normalized duplicates", test_experiment_clamps_and_normalized_duplicate},
      {"EXPERIMENT heartbeat replay", test_experiment_heartbeat_replays_exact_state},

      {"USB RUN delegates / no experiment increment", test_usb_run_delegates_without_incrementing_experiment},
      {"USB PUSH requests HELLO only", test_usb_push_requests_hello_without_mutating_model},
      {"USB synthetic commands leave model unchanged", test_usb_synthetic_commands_leave_model_unchanged},

  };
  int passed = 0;
  int failed = 0;
  for (const TestCase &test : tests) {
    try {
      test.run();
      ++passed;
      std::cout << "PASS " << test.name << '\n';
    } catch (const std::exception &error) {
      ++failed;
      std::cout << "FAIL " << test.name << ": " << error.what() << '\n';
    }
  }
  std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
