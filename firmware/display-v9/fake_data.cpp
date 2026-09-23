#include "fake_data.h"

AppState g;

const char *const SENSOR_NAMES[S_COUNT] = {"CO2", "CH4", "Press", "pH", "Temp"};
const char *const SENSOR_UNITS[S_COUNT] = {"ppm", "ppm", "kPa", "", "C"};

static float frand(float lo, float hi) {
  return lo + (hi - lo) * (random(0, 1000) / 1000.0f);
}

static void fake_bottle_seed(BottleFake &b, float tempBase) {
  b.sensor[S_CO2] = frand(800.0f, 1200.0f);
  b.sensor[S_CH4] = frand(1.8f, 2.4f);
  b.sensor[S_PRESS] = frand(-2.0f, 2.0f);
  b.sensor[S_PH] = frand(6.8f, 7.4f);
  b.sensor[S_TEMP] = tempBase + frand(-0.6f, 0.6f);
  for (int i = 0; i < S_COUNT; i++) b.sensorNull[i] = false;
  for (int i = 0; i < UI_TREND_PTS; i++) {
    b.trend[i] = tempBase + sinf(i * 0.5f) * 0.4f + frand(-0.1f, 0.1f);
  }
}

void fake_init() {
  randomSeed(analogRead(0) + millis());
  g.experimentRunning = false;
  g.durationH = UI_DURATION_DEFAULT_H;
  g.tempSetpointC = UI_TEMP_DEFAULT_C;
  g.stirrerOn = false;
  g.postEnabled = true;
  g.chamberTempC = 37.0f;
  snprintf(g.ipAddr, sizeof(g.ipAddr), "192.168.1.50 (fake)");
  snprintf(g.lastHttp, sizeof(g.lastHttp), "200 OK /api/push (fake)");
  snprintf(g.storageNote, sizeof(g.storageNote), "No local log; dashboard only (fake)");

  for (int c = 0; c < UI_CLUSTERS; c++) {
    ClusterFake &cl = g.cluster[c];
    cl.online = (c < UI_ONLINE_CLUSTERS);
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
      fake_bottle_seed(cl.bottle[b], 37.0f + c * 0.2f);
      if (!cl.online) {
        // Not connected: every field reads NULL, shown as "node offline".
        for (int s = 0; s < S_COUNT; s++) cl.bottle[b].sensorNull[s] = true;
      }
    }
  }
}

void fake_wt32_push() {
  // Pretends a WT32 UART push just arrived: jitter online nodes, shift trends.
  for (int c = 0; c < UI_CLUSTERS; c++) {
    ClusterFake &cl = g.cluster[c];
    if (!cl.online) continue;
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
      BottleFake &bt = cl.bottle[b];
      if (!bt.sensorNull[S_CO2]) bt.sensor[S_CO2] *= frand(0.98f, 1.02f);
      if (!bt.sensorNull[S_CH4]) bt.sensor[S_CH4] *= frand(0.98f, 1.02f);
      if (!bt.sensorNull[S_PRESS]) bt.sensor[S_PRESS] += frand(-0.1f, 0.1f);
      if (!bt.sensorNull[S_PH]) bt.sensor[S_PH] += frand(-0.05f, 0.05f);
      if (!bt.sensorNull[S_TEMP]) bt.sensor[S_TEMP] += frand(-0.15f, 0.15f);
      // Shift temp trend ring left, append newest temp.
      for (int i = 0; i < UI_TREND_PTS - 1; i++) bt.trend[i] = bt.trend[i + 1];
      bt.trend[UI_TREND_PTS - 1] = bt.sensor[S_TEMP];
    }
  }
  Serial.println("[FAKE WT32 push] values updated");
}

int fake_null_count() {
  int n = 0;
  for (int c = 0; c < UI_CLUSTERS; c++)
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++)
      for (int s = 0; s < S_COUNT; s++)
        if (g.cluster[c].bottle[b].sensorNull[s]) n++;
  return n;
}

bool fake_any_null() { return fake_null_count() > 0; }

const char *fake_sensor_text(int c, int b, int s, char *out, size_t n) {
  const BottleFake &bt = g.cluster[c].bottle[b];
  if (bt.sensorNull[s]) {
    snprintf(out, n, "-- NULL");
    return out;
  }
  float v = bt.sensor[s];
  switch (s) {
    case S_CO2:
    case S_CH4:
      snprintf(out, n, "%.0f %s", v, SENSOR_UNITS[s]);
      break;
    case S_PRESS:
      snprintf(out, n, "%+.2f %s", v, SENSOR_UNITS[s]);
      break;
    case S_PH:
      snprintf(out, n, "%.2f", v);
      break;
    default:
      snprintf(out, n, "%.1f %s", v, SENSOR_UNITS[s]);
      break;
  }
  return out;
}
