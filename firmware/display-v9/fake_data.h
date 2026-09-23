#pragma once

// Fake in-memory model of what the WT32 will push over UART every 5 minutes.
// Single cycle only, no history (PSRAM is spent on frame buffers).
// Every value here is settable at runtime over the USB serial line protocol
// (see serial_cmd.h). There is no real Modbus/UART/WiFi traffic in this pass.

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
  float sensor[S_COUNT];
  bool sensorNull[S_COUNT];  // true = WT32 failed to fetch this field this cycle
  float trend[UI_TREND_PTS];  // fake chamber-temp ring for the bottle chart
};

struct ClusterFake {
  bool online;
  BottleFake bottle[UI_BOTTLES_PER_CLUSTER];
};

struct AppState {
  bool experimentRunning;
  int durationH;
  float tempSetpointC;
  bool stirrerOn;
  bool postEnabled;
  float chamberTempC;  // drives the 41/45 overheat UI; settable via serial
  ClusterFake cluster[UI_CLUSTERS];
  char ipAddr[24];
  char lastHttp[72];
  char storageNote[72];
};

extern AppState g;

void fake_init();
void fake_wt32_push();  // jitter online values, shift trends (5-min timer + PUSH cmd)
int fake_null_count();
bool fake_any_null();
const char *fake_sensor_text(int c, int b, int s, char *out, size_t n);
