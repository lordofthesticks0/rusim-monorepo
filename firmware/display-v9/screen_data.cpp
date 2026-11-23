#include "screen_data.h"

#include <math.h>

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_shell.h"

// Three pages share this file. Each builds its widgets once; screen_data_refresh()
// updates whichever page is open in place when a new frame arrives. Numbers
// shown to the operator are 1-based; c and b stay 0-based in code.

enum Page { P_NONE = 0, P_PICKER, P_CLUSTER, P_BOTTLE };
static Page s_page = P_NONE;
static int s_c = 0;
static int s_b = 0;

// Which sensor the trend card charts. Temperature by default; kept while the
// same bottle stays open, reset when another bottle is opened.
static int s_chartSensor = S_TEMP;
static int s_chartC = -1;
static int s_chartB = -1;

// --- shared state helpers ----------------------------------------------------

// Short state of one bottle slot and the colour it is shown in.
static const char *bottle_state(int c, int b, uint32_t *col) {
  const BottleFake &bt = g.cluster[c].bottle[b];
  if (!g.cluster[c].online) {
    *col = UI_COL_DIM;
    return "Offline";
  }
  if (!bt.present) {
    *col = UI_COL_ALARM;
    return "No data";
  }
  const int n = fake_bottle_null_count(c, b);
  if (n == 0) {
    *col = UI_COL_OK;
    return "All readings valid";
  }
  *col = UI_COL_ALARM;
  return n == 1 ? "1 reading missing" : "Readings missing";
}

static void set_offline_look(lv_obj_t *card, bool offline) {
  lv_obj_set_style_bg_color(card, lv_color_hex(offline ? UI_COL_CARD_OFF : UI_COL_CARD),
                            LV_PART_MAIN);
}

static void make_tappable(lv_obj_t *card) {
  lv_obj_set_clickable(card, true);
  lv_obj_set_style_bg_color(card, lv_color_hex(UI_COL_BAR), LV_STATE_PRESSED);
  // Tappable cards get the accent outline; information-only cards keep grey.
  lv_obj_set_style_border_color(card, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
}

// --- /data: node picker ------------------------------------------------------

static lv_obj_t *s_nodeCard[UI_CLUSTERS] = {};
static lv_obj_t *s_nodeTitle[UI_CLUSTERS] = {};
static lv_obj_t *s_nodeLine1[UI_CLUSTERS] = {};
static lv_obj_t *s_nodeLine2[UI_CLUSTERS] = {};
static lv_obj_t *s_nodeDot[UI_CLUSTERS] = {};

static void on_pick_cluster(lv_event_t *e) {
  ui_show_cluster((int)(intptr_t)lv_event_get_user_data(e));
}

static void picker_refresh() {
  for (int c = 0; c < UI_CLUSTERS; c++) {
    if (s_nodeCard[c] == nullptr) continue;
    const bool on = g.cluster[c].online;
    int present = 0;
    int missing = 0;
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
      if (g.cluster[c].bottle[b].present) present++;
    }
    if (on) missing = fake_alarm_count_node(c);
    char t[40];
    set_offline_look(s_nodeCard[c], !on);
    lv_obj_set_style_text_color(s_nodeTitle[c], lv_color_hex(on ? UI_COL_TEXT : UI_COL_DIM),
                                LV_PART_MAIN);
    if (on) {
      snprintf(t, sizeof(t), "%d of %d bottles reporting", present, UI_BOTTLES_PER_CLUSTER);
      lv_label_set_text(s_nodeLine1[c], t);
      if (missing > 0) {
        snprintf(t, sizeof(t), "%d readings missing", missing);
        lv_label_set_text(s_nodeLine2[c], t);
        lv_obj_set_style_text_color(s_nodeLine2[c], lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
      } else {
        lv_label_set_text(s_nodeLine2[c], "All readings valid");
        lv_obj_set_style_text_color(s_nodeLine2[c], lv_color_hex(UI_COL_OK), LV_PART_MAIN);
      }
    } else {
      lv_label_set_text(s_nodeLine1[c], "Not connected");
      lv_label_set_text(s_nodeLine2[c], "");
    }
    const uint32_t dot = !on ? UI_COL_DISABLED : (missing > 0 ? UI_COL_ALARM : UI_COL_OK);
    lv_obj_set_style_bg_color(s_nodeDot[c], lv_color_hex(dot), LV_PART_MAIN);
  }
}

void screen_data_picker_show(lv_obj_t *parent) {
  s_page = P_PICKER;
  ui_mk_page_title(parent, "Nodes", nullptr, nullptr);
  // 3 x 2 cards, 245 x 172, 16 px gaps: 3*245 + 2*16 = 767.
  for (int c = 0; c < UI_CLUSTERS; c++) {
    const int x = UI_MARGIN + (c % 3) * 261;
    const int y = UI_PAGE_TOP + (c / 3) * 188;
    lv_obj_t *card = ui_mk_card(parent, x, y, 245, 172);
    make_tappable(card);
    lv_obj_add_event_cb(card, on_pick_cluster, LV_EVENT_CLICKED, (void *)(intptr_t)c);
    char t[16];
    snprintf(t, sizeof(t), "Node %d", c + 1);
    s_nodeCard[c] = card;
    s_nodeTitle[c] = ui_mk_label(card, t, 4, 4, 180, UI_COL_TEXT, &lv_font_montserrat_28);
    s_nodeLine1[c] = ui_mk_label(card, "", 4, 64, 210, UI_COL_TEXT, &lv_font_montserrat_14);
    s_nodeLine2[c] = ui_mk_label(card, "", 4, 88, 210, UI_COL_DIM, &lv_font_montserrat_14);
    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_set_size(dot, 14, 14);
    lv_obj_set_pos(dot, 203, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_clickable(dot, false);
    s_nodeDot[c] = dot;
    lv_obj_t *open = ui_mk_label(card, "Open " LV_SYMBOL_RIGHT, 4, 124, 213, UI_COL_ACCENT,
                                 &lv_font_montserrat_14);
    lv_obj_set_style_text_align(open, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  }
  picker_refresh();
}

// --- /data/X: node page ------------------------------------------------------

static lv_obj_t *s_tile[UI_BOTTLES_PER_CLUSTER] = {};
static lv_obj_t *s_tileValue[UI_BOTTLES_PER_CLUSTER] = {};
static lv_obj_t *s_tileState[UI_BOTTLES_PER_CLUSTER] = {};
static lv_obj_t *s_clusterNote = nullptr;

static void on_back_picker(lv_event_t *e) {
  (void)e;
  ui_show_data_picker();
}

static void on_pick_bottle(lv_event_t *e) {
  intptr_t v = (intptr_t)lv_event_get_user_data(e);
  ui_show_bottle((int)(v >> 8), (int)(v & 0xFF));
}

static void cluster_refresh() {
  const int c = s_c;
  const bool on = g.cluster[c].online;
  if (s_clusterNote != nullptr)
    lv_label_set_text(s_clusterNote, on ? "Bottle temperature. Tap a bottle for all sensors."
                                        : "This node is not connected.");
  for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
    if (s_tile[b] == nullptr) continue;
    const BottleFake &bt = g.cluster[c].bottle[b];
    const bool have = on && bt.present && !bt.sensorNull[S_TEMP];
    char val[16];
    if (have) {
      snprintf(val, sizeof(val), "%.1f", bt.sensor[S_TEMP]);
    } else {
      snprintf(val, sizeof(val), "--");
    }
    lv_label_set_text(s_tileValue[b], val);
    // A temperature the WT32 could not fetch is flagged red, not hidden.
    const bool failed = on && bt.present && bt.sensorNull[S_TEMP];
    lv_obj_set_style_text_color(
        s_tileValue[b], lv_color_hex(have ? UI_COL_TEXT : failed ? UI_COL_ALARM : UI_COL_DIM),
        LV_PART_MAIN);
    uint32_t col;
    lv_label_set_text(s_tileState[b], bottle_state(c, b, &col));
    lv_obj_set_style_text_color(s_tileState[b], lv_color_hex(col), LV_PART_MAIN);
    set_offline_look(s_tile[b], !on);
  }
}

void screen_cluster_show(lv_obj_t *parent, int c) {
  s_page = P_CLUSTER;
  s_c = c;
  char t[24];
  snprintf(t, sizeof(t), "Node %d", c + 1);
  ui_mk_page_title(parent, t, on_back_picker, nullptr);
  s_clusterNote = ui_mk_label(parent, "", 400, 18, 384, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_clusterNote, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  // Four 180 x 252 tiles, 16 px gaps: 4*180 + 3*16 = 768.
  for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
    lv_obj_t *tile = ui_mk_card(parent, UI_MARGIN + b * 196, 100, 180, 252);
    make_tappable(tile);
    lv_obj_add_event_cb(tile, on_pick_bottle, LV_EVENT_CLICKED, (void *)(intptr_t)((c << 8) | b));
    snprintf(t, sizeof(t), "Bottle %d", b + 1);
    lv_obj_t *name = ui_mk_label(tile, t, 0, 4, 154, UI_COL_TEXT, &lv_font_montserrat_20);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    s_tileValue[b] = ui_mk_label(tile, "", 0, 66, 154, UI_COL_TEXT, &lv_font_montserrat_40);
    lv_obj_set_style_text_align(s_tileValue[b], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t *unit = ui_mk_label(tile, "Temperature, " UI_DEG_C, 0, 116, 154, UI_COL_DIM,
                                 &lv_font_montserrat_14);
    lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    s_tileState[b] = ui_mk_label(tile, "", 0, 186, 154, UI_COL_DIM, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(s_tileState[b], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    s_tile[b] = tile;
  }
  cluster_refresh();
}

// --- /data/X/bottle_Y: bottle page -------------------------------------------

// Captions carry the unit, so every big number stays the same size.
static const char *const SENSOR_CAPTION[S_COUNT] = {"CO2 (ppm)", "CH4 (ppm)", "Pressure (kPa)",
                                                    "pH", "Temp (" UI_DEG_C ")"};
static const char *const SENSOR_CHART_NAME[S_COUNT] = {"CO2", "CH4", "PRESSURE", "PH",
                                                       "TEMPERATURE"};
static const char *const SENSOR_UNIT_TXT[S_COUNT] = {" ppm", " ppm", " kPa", "", " " UI_DEG_C};

// lv_chart takes int32 values, so kPa and pH are charted in hundredths and
// temperature in tenths. The Y range follows the data (min..max of the shown
// window plus 10 %), but never narrower than kMinSpan, so sensor noise is not
// blown up into a dramatic-looking line.
static const float kGain[S_COUNT] = {1.0f, 1.0f, 100.0f, 100.0f, 10.0f};
static const float kMinSpan[S_COUNT] = {100.0f, 100.0f, 0.5f, 0.2f, 1.0f};
static const int kDecimals[S_COUNT] = {0, 0, 2, 2, 1};

static lv_obj_t *s_sTile[S_COUNT] = {};
static lv_obj_t *s_sValue[S_COUNT] = {};
static lv_obj_t *s_sCap[S_COUNT] = {};
static lv_obj_t *s_bottleNote = nullptr;
static lv_obj_t *s_chart = nullptr;
static lv_chart_series_t *s_ser = nullptr;
static lv_obj_t *s_chartTitle = nullptr;
static lv_obj_t *s_chartNow = nullptr;
static lv_obj_t *s_yTop = nullptr;
static lv_obj_t *s_yBottom = nullptr;
static lv_obj_t *s_chartEmpty = nullptr;

static void on_back_cluster(lv_event_t *e) {
  (void)e;
  s_chartSensor = S_TEMP;
  ui_show_cluster(s_c);
}

static void on_pick_sensor(lv_event_t *e) {
  s_chartSensor = (int)(intptr_t)lv_event_get_user_data(e);
  ui_show_bottle(s_c, s_b);
}

static void chart_refresh() {
  if (s_chart == nullptr) return;
  const int s = s_chartSensor;
  const BottleFake &bt = g.cluster[s_c].bottle[s_b];
  float lo = INFINITY;
  float hi = -INFINITY;
  for (int i = 0; i < UI_TREND_PTS; i++) {
    const float v = bt.trend[s][i];
    if (!isfinite(v)) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  const bool have = isfinite(lo);
  lv_obj_set_hidden(s_chart, !have);
  lv_obj_set_hidden(s_yTop, !have);
  lv_obj_set_hidden(s_yBottom, !have);
  lv_obj_set_hidden(s_chartEmpty, have);
  char t[48];
  if (!have) {
    lv_label_set_text(s_chartNow, "");
    return;
  }
  const float mid = (lo + hi) * 0.5f;
  float span = hi - lo;
  if (span < kMinSpan[s]) span = kMinSpan[s];
  span *= 1.2f;  // 10 % headroom above and below
  lo = mid - span * 0.5f;
  hi = mid + span * 0.5f;
  lv_chart_set_axis_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, (int32_t)lroundf(lo * kGain[s]),
                          (int32_t)lroundf(hi * kGain[s]));
  for (int i = 0; i < UI_TREND_PTS; i++) {
    const float v = bt.trend[s][i];
    lv_chart_set_series_value_by_id(s_chart, s_ser, (uint32_t)i,
                                    isfinite(v) ? (int32_t)lroundf(v * kGain[s])
                                                : LV_CHART_POINT_NONE);
  }
  lv_chart_refresh(s_chart);
  snprintf(t, sizeof(t), "%.*f", kDecimals[s], hi);
  lv_label_set_text(s_yTop, t);
  snprintf(t, sizeof(t), "%.*f", kDecimals[s], lo);
  lv_label_set_text(s_yBottom, t);
  const float now = bt.trend[s][UI_TREND_PTS - 1];
  if (isfinite(now)) {
    snprintf(t, sizeof(t), "Now %.*f%s", kDecimals[s], now, SENSOR_UNIT_TXT[s]);
  } else {
    snprintf(t, sizeof(t), "Now --");
  }
  lv_label_set_text(s_chartNow, t);
}

static void bottle_refresh() {
  const int c = s_c;
  const int b = s_b;
  const bool on = g.cluster[c].online;
  const BottleFake &bt = g.cluster[c].bottle[b];
  if (s_bottleNote != nullptr) {
    if (!on) {
      lv_label_set_text(s_bottleNote, "Node not connected");
      lv_obj_set_style_text_color(s_bottleNote, lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
    } else if (!bt.present) {
      lv_label_set_text(s_bottleNote, "No live data for this bottle");
      lv_obj_set_style_text_color(s_bottleNote, lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
    } else {
      lv_label_set_text(s_bottleNote, "Tap a tile to chart that sensor");
      lv_obj_set_style_text_color(s_bottleNote, lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
    }
  }
  for (int s = 0; s < S_COUNT; s++) {
    if (s_sTile[s] == nullptr) continue;
    const bool have = on && bt.present && !bt.sensorNull[s];
    const bool failed = on && bt.present && bt.sensorNull[s];
    char val[16];
    fake_sensor_value_text(c, b, s, val, sizeof(val));
    lv_label_set_text(s_sValue[s], val);
    // Value colour always carries the field state, selected or not.
    lv_obj_set_style_text_color(
        s_sValue[s], lv_color_hex(failed ? UI_COL_ALARM : (have ? UI_COL_TEXT : UI_COL_DIM)),
        LV_PART_MAIN);
    set_offline_look(s_sTile[s], !on);
  }
  chart_refresh();
}

void screen_bottle_show(lv_obj_t *parent, int c, int b) {
  s_page = P_BOTTLE;
  s_c = c;
  s_b = b;
  if (c != s_chartC || b != s_chartB) {
    s_chartC = c;
    s_chartB = b;
    s_chartSensor = S_TEMP;  // temperature is the chart every bottle opens on
  }

  char t[48];
  snprintf(t, sizeof(t), "Bottle %d", b + 1);
  lv_obj_t *title = ui_mk_page_title(parent, t, on_back_cluster, nullptr);
  snprintf(t, sizeof(t), "Node %d", c + 1);
  lv_obj_t *node = ui_mk_label(parent, t, 0, 0, 100, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_align_to(node, title, LV_ALIGN_OUT_RIGHT_BOTTOM, 12, -2);
  s_bottleNote = ui_mk_label(parent, "", 400, 18, 384, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_bottleNote, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  // Five 144 x 124 tiles: 5*144 + 4*12 = 768. The selected one gets a 3 px
  // accent outline and an accent caption; its value keeps its state colour.
  for (int s = 0; s < S_COUNT; s++) {
    lv_obj_t *tile = ui_mk_card(parent, UI_MARGIN + s * 156, UI_PAGE_TOP, 144, 124);
    make_tappable(tile);
    const bool sel = (s == s_chartSensor);
    if (sel) {
      lv_obj_set_style_border_width(tile, 3, LV_PART_MAIN);
      lv_obj_set_style_pad_all(tile, 10, LV_PART_MAIN);  // keep content in place
    } else {
      lv_obj_set_style_border_color(tile, lv_color_hex(UI_COL_LINE), LV_PART_MAIN);
    }
    lv_obj_add_event_cb(tile, on_pick_sensor, LV_EVENT_CLICKED, (void *)(intptr_t)s);
    s_sValue[s] = ui_mk_label(tile, "", 0, 18, 118, UI_COL_TEXT, &lv_font_montserrat_28);
    lv_obj_set_style_text_align(s_sValue[s], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    s_sCap[s] = ui_mk_label(tile, SENSOR_CAPTION[s], 0, 74, 118,
                            sel ? UI_COL_ACCENT : UI_COL_DIM, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(s_sCap[s], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    s_sTile[s] = tile;
  }

  // Trend card (inner 742 x 194): Y labels on the left, chart on the right.
  lv_obj_t *card = ui_mk_card(parent, UI_MARGIN, 196, 768, 220);
  snprintf(t, sizeof(t), "%s, LAST %lu MIN", SENSOR_CHART_NAME[s_chartSensor],
           (unsigned long)(UI_TREND_PTS * UI_TREND_STEP_MS / 60000UL));
  s_chartTitle = ui_mk_caption(card, t, 0, 0, 400);
  s_chartNow = ui_mk_label(card, "", 400, 0, 342, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_chartNow, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  s_yTop = ui_mk_label(card, "", 0, 26, 56, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_yTop, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  s_yBottom = ui_mk_label(card, "", 0, 174, 56, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_yBottom, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  s_chart = lv_chart_create(card);
  lv_obj_set_pos(s_chart, 64, 26);
  lv_obj_set_size(s_chart, 678, 166);
  lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(s_chart, UI_TREND_PTS);
  lv_chart_set_div_line_count(s_chart, 5, 0);
  // No theme: line width and point size default to "nothing" and are set here.
  lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_line_width(s_chart, 1, LV_PART_MAIN);
  lv_obj_set_style_line_color(s_chart, lv_color_hex(UI_COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_line_width(s_chart, 3, LV_PART_ITEMS);
  // No per-point dots: the line alone reads more calmly ("Now" shows the
  // latest value in the header).
  lv_obj_set_style_width(s_chart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_height(s_chart, 0, LV_PART_INDICATOR);
  s_ser = lv_chart_add_series(s_chart, lv_color_hex(UI_COL_ACCENT), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_values(s_chart, s_ser, LV_CHART_POINT_NONE);

  s_chartEmpty = ui_mk_label(card, "No samples yet. The chart starts with the next reading.",
                             0, 90, 742, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_chartEmpty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  bottle_refresh();
}

// --- shared ------------------------------------------------------------------

void screen_data_refresh() {
  switch (s_page) {
    case P_PICKER:
      picker_refresh();
      break;
    case P_CLUSTER:
      cluster_refresh();
      break;
    case P_BOTTLE:
      bottle_refresh();
      break;
    default:
      break;
  }
}

void screen_data_hide() {
  s_page = P_NONE;
  for (int c = 0; c < UI_CLUSTERS; c++) {
    s_nodeCard[c] = nullptr;
    s_nodeTitle[c] = nullptr;
    s_nodeLine1[c] = nullptr;
    s_nodeLine2[c] = nullptr;
    s_nodeDot[c] = nullptr;
  }
  for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
    s_tile[b] = nullptr;
    s_tileValue[b] = nullptr;
    s_tileState[b] = nullptr;
  }
  s_clusterNote = nullptr;
  for (int s = 0; s < S_COUNT; s++) {
    s_sTile[s] = nullptr;
    s_sValue[s] = nullptr;
    s_sCap[s] = nullptr;
  }
  s_bottleNote = nullptr;
  s_chart = nullptr;
  s_ser = nullptr;
  s_chartTitle = nullptr;
  s_chartNow = nullptr;
  s_yTop = nullptr;
  s_yBottom = nullptr;
  s_chartEmpty = nullptr;
}
