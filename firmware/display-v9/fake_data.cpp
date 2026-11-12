#include "fake_data.h"

#include "link_serial.h"

AppState g;

const char *const SENSOR_NAMES[S_COUNT] = {"CO2", "CH4", "Press", "pH", "Temp"};
const char *const SENSOR_UNITS[S_COUNT] = {"ppm", "ppm", "kPa", "", "C"};

static void clear_bottle(BottleFake &b) {
  b.present = false;
  for (int s = 0; s < S_COUNT; s++) {
    b.sensor[s] = NAN;
    b.sensorNull[s] = true;
    for (int i = 0; i < UI_TREND_PTS; i++) b.trend[s][i] = NAN;
  }
}

// The legacy Mega rig reports four bottles on node 0. All other slots stay
// unavailable; the configured database bottle count is not sensor presence.
static bool s_realSeen = false;
static bool s_staleShown = true;
static uint32_t s_lastSeq = 0;

bool sens_apply_real() {
  LinkSensFrame f;
  int32_t ageMs = link_serial_get_sens(&f);
  ClusterFake &cl = g.cluster[UI_REAL_CLUSTER];

  if (ageMs < 0 || (uint32_t)ageMs > UI_SENS_STALE_MS) {
    if (s_staleShown) return false;
    s_staleShown = true;
    cl.online = false;
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) clear_bottle(cl.bottle[b]);
    Serial.println("[SENS] unavailable: node 0 values cleared");
    return true;
  }

  if (!s_staleShown && s_realSeen && s_lastSeq == f.seq) return false;
  s_staleShown = false;
  s_lastSeq = f.seq;

  bool anyPresent = false;
  for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
    BottleFake &bt = cl.bottle[b];
    if (b >= LINK_SENS_BOTTLES || !f.present[b]) {
      clear_bottle(bt);
      continue;
    }
    bt.present = true;
    anyPresent = true;
    for (int s = 0; s < S_COUNT; s++) {
      float v = f.value[b][s];
      bt.sensorNull[s] = !isfinite(v);
      bt.sensor[s] = bt.sensorNull[s] ? NAN : v;
      // Every sensor keeps its own trend, so the detail screen can chart any of
      // them. A NULL field moves in as a gap, never as an interpolated value.
      for (int i = 0; i < UI_TREND_PTS - 1; i++) bt.trend[s][i] = bt.trend[s][i + 1];
      bt.trend[s][UI_TREND_PTS - 1] = bt.sensor[s];
    }
  }
  cl.online = anyPresent;
  if (!s_realSeen) Serial.println("[SENS] real WT32 feed active");
  s_realSeen = true;
  return true;
}

void fake_init() {
  s_realSeen = false;
  s_staleShown = true;
  s_lastSeq = 0;
  g.loggedIn = false;
  g.experimentRunning = false;
  g.experimentStartMs = 0;
  g.expNum = 0;
  g.durationH = UI_DURATION_DEFAULT_H;
  g.tempSetpointC = UI_TEMP_DEFAULT_C;
  g.stirrerOn = false;
  g.postEnabled = true;
  // SENS contains bottle temperatures, not a separate chamber measurement.
  g.chamberTempC = NAN;
  snprintf(g.ipAddr, sizeof(g.ipAddr), "--");
  snprintf(g.lastHttp, sizeof(g.lastHttp), "No upload acknowledged");
  snprintf(g.storageNote, sizeof(g.storageNote), "No local log; dashboard only");

  for (int c = 0; c < UI_CLUSTERS; c++) {
    ClusterFake &cl = g.cluster[c];
    cl.online = false;
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) clear_bottle(cl.bottle[b]);
  }
}

bool sens_real_active() { return s_realSeen && !s_staleShown; }

int fake_null_count() {
  int n = 0;
  for (int c = 0; c < UI_CLUSTERS; c++)
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++)
      for (int s = 0; s < S_COUNT; s++)
        if (g.cluster[c].bottle[b].sensorNull[s]) n++;
  return n;
}

bool fake_any_null() { return fake_null_count() > 0; }

const char *fake_sensor_value_text(int c, int b, int s, char *out, size_t n) {
  const BottleFake &bt = g.cluster[c].bottle[b];
  if (bt.sensorNull[s] || !isfinite(bt.sensor[s])) {
    snprintf(out, n, "--");
    return out;
  }
  const float v = bt.sensor[s];
  switch (s) {
    case S_CO2:
    case S_CH4:
      snprintf(out, n, "%.0f", v);
      break;
    case S_PRESS:
      snprintf(out, n, "%+.2f", v);
      break;
    case S_PH:
      snprintf(out, n, "%.2f", v);
      break;
    default:
      snprintf(out, n, "%.1f", v);
      break;
  }
  return out;
}

const char *fake_sensor_text(int c, int b, int s, char *out, size_t n) {
  if (g.cluster[c].bottle[b].sensorNull[s]) {
    snprintf(out, n, "-- NULL");
    return out;
  }
  char v[16];
  fake_sensor_value_text(c, b, s, v, sizeof(v));
  // pH carries no unit, so no trailing space is appended for it.
  if (SENSOR_UNITS[s][0] == '\0') {
    snprintf(out, n, "%s", v);
  } else {
    snprintf(out, n, "%s %s", v, SENSOR_UNITS[s]);
  }
  return out;
}
