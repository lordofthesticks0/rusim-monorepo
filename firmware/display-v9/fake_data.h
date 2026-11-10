#pragma once

// Display model fed by real WT32 SENS frames. Legacy file/type names are kept
// for callers; no sensor readings or history are generated locally.

#include <Arduino.h>

#include "ui_config.h"

enum SensorId : uint8_t {
  S_CO2 = 0,  // ppm
  S_CH4,      // ppm
  S_PRESS,    // kPa
  S_PH,       // pH
  S_TEMP,     // degC
  S_COUNT = UI_SENSORS_PER_BOTTLE,
};

extern const char *const SENSOR_NAMES[S_COUNT];
extern const char *const SENSOR_UNITS[S_COUNT];

struct BottleFake {
  bool present;  // reported in the latest live sensor frame
  float sensor[S_COUNT];
  bool sensorNull[S_COUNT];  // true = WT32 failed to fetch this field this cycle
  float trend[UI_TREND_PTS];  // measured bottle temperatures; NAN = no point
};

struct ClusterFake {
  bool online;
  BottleFake bottle[UI_BOTTLES_PER_CLUSTER];
};

struct AppState {
  bool loggedIn;  // portal auth succeeded; gates /control etc. before any run
  bool experimentRunning;
  uint32_t experimentStartMs;  // 0 when not running (used by home progress arc)
  int expNum;
  int durationH;
  float tempSetpointC;
  bool stirrerOn;
  bool postEnabled;
  float chamberTempC;  // NAN until a separate chamber measurement is available
  ClusterFake cluster[UI_CLUSTERS];
  char ipAddr[24];
  char lastHttp[72];
  char storageNote[72];
};

extern AppState g;

void fake_init();  // initializes controls; all measured values start unavailable
// Applies new SENS frames to node 0. Missing/invalid values and frames older
// than 90 s are unavailable, never replaced with synthetic values.
// Returns true when labels should refresh.
bool sens_apply_real();
// True while the applied WT32 sensor feed is live.
bool sens_real_active();
int fake_null_count();
bool fake_any_null();
const char *fake_sensor_text(int c, int b, int s, char *out, size_t n);
