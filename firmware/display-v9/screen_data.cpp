#include "screen_data.h"

#include "fake_data.h"
#include "ui_config.h"
#include "ui_shell.h"

#include <math.h>

// Which sensor the trend card is charting. Temperature is the default; tapping
// any tile switches to that sensor. The route re-renders every 5 s when new
// SENS frames land, so the choice is kept here and only reset when a different
// bottle is opened.
static int s_chartSensor = S_TEMP;
static int s_chartC = -1;
static int s_chartB = -1;

static void on_pick_cluster(lv_event_t *e) {
  intptr_t c = (intptr_t)lv_event_get_user_data(e);
  ui_show_cluster((int)c);
}

static void on_pick_sensor(lv_event_t *e) {
  intptr_t v = (intptr_t)lv_event_get_user_data(e);
  s_chartSensor = (int)(v & 0xF);
  ui_show_bottle((int)((v >> 8) & 0xFF), (int)((v >> 4) & 0xF));
}

static void on_pick_bottle(lv_event_t *e) {
  intptr_t v = (intptr_t)lv_event_get_user_data(e);
  ui_show_bottle((int)(v >> 8), (int)(v & 0xFF));
}

static void on_back_picker(lv_event_t *e) {
  (void)e;
  ui_show_home();
}

static void on_back_cluster(lv_event_t *e) {
  intptr_t c = (intptr_t)lv_event_get_user_data(e);
  (void)c;
  ui_show_data_picker();
}

static void on_back_bottle(lv_event_t *e) {
  intptr_t c = (intptr_t)lv_event_get_user_data(e);
  s_chartSensor = S_TEMP;  // leaving the screen drops back to the default chart
  ui_show_cluster((int)c);
}

void screen_data_picker_show(lv_obj_t *parent) {
  lv_obj_t *title = ui_mk_label(parent, "NODES", 0, 8, 800, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  for (int c = 0; c < UI_CLUSTERS; c++) {
    int row = c / 3, col = c % 3;
    int x = 68 + col * 232, y = 56 + row * 160;
    (void)x;
    (void)y;
    // Recompute exact per spec: 3*200 + 2*32 = 664, start (800-664)/2=68.
    x = 68 + col * 232;
    y = 56 + row * 160;
    uint32_t bg = g.cluster[c].online ? UI_COL_CARD : UI_COL_CARD_OFF;
    lv_obj_t *b = ui_mk_button(parent, "", x, y, 200, 140, bg);
    lv_obj_t *num = lv_label_create(b);
    char t[8];
    snprintf(t, sizeof(t), "%d", c);
    lv_label_set_text(num, t);
    lv_obj_set_style_text_font(num, &lv_font_montserrat_28, LV_PART_MAIN);
    uint32_t colr = g.cluster[c].online ? UI_COL_TEXT : UI_COL_DIM;
    lv_obj_set_style_text_color(num, lv_color_hex(colr), LV_PART_MAIN);
    lv_obj_align(num, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(num, 180);
    lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_event_cb(b, on_pick_cluster, LV_EVENT_CLICKED, (void *)(intptr_t)c);
  }
  lv_obj_t *back = ui_mk_button(parent, LV_SYMBOL_NEW_LINE, 364, 372, 72, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back_picker, LV_EVENT_CLICKED, nullptr);
}

// One square per bottle, built like the /info tiles: the measured value on top
// in the large compressed font, a dim caption underneath. Only the bottle
// temperature is shown here; the tap opens the full sensor list.
static lv_obj_t *mk_temp_tile(lv_obj_t *parent, int c, int b) {
  const bool nodeOn = g.cluster[c].online;
  const BottleFake &bt = g.cluster[c].bottle[b];
  const bool haveTemp = nodeOn && bt.present && !bt.sensorNull[S_TEMP];
  const bool tempNull = nodeOn && bt.present && bt.sensorNull[S_TEMP];

  lv_obj_t *tile = ui_mk_card(parent, 0, 0, 180, 180);
  // Flex positions the children of the grid, so the x/y above are placeholders.
  if (!nodeOn) {
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COL_CARD_OFF), LV_PART_MAIN);
  }
  lv_obj_set_clickable(tile, true);  // the card itself is the tap target
  lv_obj_add_event_cb(tile, on_pick_bottle, LV_EVENT_CLICKED,
                      (void *)(intptr_t)((c << 8) | b));

  char val[24];
  if (haveTemp) {
    snprintf(val, sizeof(val), "%.1f C", bt.sensor[S_TEMP]);
  } else {
    snprintf(val, sizeof(val), "--");
  }
  lv_obj_t *v = lv_label_create(tile);
  lv_label_set_text(v, val);
  lv_obj_set_width(v, 164);
  lv_label_set_long_mode(v, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_28_compressed, LV_PART_MAIN);
  // A field the WT32 could not fetch is flagged, not silently shown as a value.
  lv_obj_set_style_text_color(
      v, lv_color_hex(tempNull ? UI_COL_ALARM : (haveTemp ? UI_COL_TEXT : UI_COL_DIM)),
      LV_PART_MAIN);
  lv_obj_align(v, LV_ALIGN_TOP_MID, 0, 38);

  char cap[24];
  snprintf(cap, sizeof(cap), "Bottle %d", b);
  lv_obj_t *cl = lv_label_create(tile);
  lv_label_set_text(cl, cap);
  lv_obj_set_width(cl, 164);
  lv_obj_set_style_text_align(cl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(cl, lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
  lv_obj_align(cl, LV_ALIGN_BOTTOM_MID, 0, -16);
  return tile;
}

void screen_cluster_show(lv_obj_t *parent, int c) {
  char title[48];
  snprintf(title, sizeof(title), "NODE %d", c);
  lv_obj_t *ctitle = ui_mk_label(parent, title, 0, 8, 800, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(ctitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // Same tile grid as /info: 180x180 squares, wrapped and centred in the area
  // above the back button.
  lv_obj_t *grid = lv_obj_create(parent);
  lv_obj_set_pos(grid, 0, 40);
  lv_obj_set_size(grid, 800, 316);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(grid, 16, LV_PART_MAIN);
  lv_obj_set_style_pad_row(grid, 16, LV_PART_MAIN);
  lv_obj_set_style_pad_column(grid, 16, LV_PART_MAIN);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scrollable(grid, false);
  for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
    mk_temp_tile(grid, c, b);
  }

  lv_obj_t *back = ui_mk_button(parent, LV_SYMBOL_NEW_LINE, 364, 372, 72, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back_picker, LV_EVENT_CLICKED, nullptr);
}

// Captions for the chamber detail tiles. The unit lives here, not on the big
// value line, so every number stays the same size and the eye can compare them.
// All five fit one 124 px line: a wrapped caption would sit higher than its
// neighbours and break the row.
static const char *const SENSOR_CAPTION[S_COUNT] = {
    "CO2 (ppm)", "CH4 (ppm)", "Pressure (kPa)", "pH", "Temp (\xC2\xB0" "C)"};

// Short names for the chart heading.
static const char *const SENSOR_CHART_NAME[S_COUNT] = {"CO2", "CH4", "Pressure", "pH",
                                                       "Temperature"};

// Fixed Y axis per sensor: CO2 0-5000 ppm, CH4 0-10000 ppm, pressure 0-10 kPa,
// pH 5-9, temperature 25-45 °C. lv_chart only accepts int32 values, so kPa and
// pH are carried in hundredths and temperature in tenths.
static const float kAxisLo[S_COUNT] = {0.0f, 0.0f, 0.0f, 5.0f, 25.0f};
static const float kAxisHi[S_COUNT] = {5000.0f, 10000.0f, 10.0f, 9.0f, 45.0f};
static const float kAxisGain[S_COUNT] = {1.0f, 1.0f, 100.0f, 100.0f, 10.0f};

// Prints the axis in the caption, derived from the constants above so the label
// cannot drift away from the scale the chart is actually drawn on.
static void axis_caption(int s, char *out, size_t n) {
  switch (s) {
    case S_CO2:
    case S_CH4:
      snprintf(out, n, "scale %.0f-%.0f ppm", kAxisLo[s], kAxisHi[s]);
      break;
    case S_PRESS:
      snprintf(out, n, "scale %.0f-%.0f kPa", kAxisLo[s], kAxisHi[s]);
      break;
    case S_PH:
      snprintf(out, n, "scale %.0f-%.0f", kAxisLo[s], kAxisHi[s]);
      break;
    default:
      snprintf(out, n, "scale %.0f-%.0f \xC2\xB0" "C", kAxisLo[s], kAxisHi[s]);
      break;
  }
}

// One sensor reading, built like the /info tiles: value in the compressed 28 px
// face on top, dim caption underneath, colour carrying the field state. The
// selected tile is filled with the accent colour, the same language the status
// bar uses for the active tab, and it is also the tap target that charts it.
static lv_obj_t *mk_sensor_tile(lv_obj_t *parent, int c, int b, int s, int x, int y) {
  const bool nodeOn = g.cluster[c].online;
  const BottleFake &bt = g.cluster[c].bottle[b];
  const bool have = nodeOn && bt.present && !bt.sensorNull[s];
  const bool failed = nodeOn && bt.present && bt.sensorNull[s];
  const bool sel = (s == s_chartSensor);

  lv_obj_t *tile = ui_mk_card(parent, x, y, 144, 144);
  if (!nodeOn) {
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COL_CARD_OFF), LV_PART_MAIN);
  }
  if (sel) {
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);
  } else {
    // Touch feedback before the rebuild replaces the screen (no theme is on).
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COL_BAR), LV_STATE_PRESSED);
  }
  lv_obj_set_clickable(tile, true);
  lv_obj_add_event_cb(tile, on_pick_sensor, LV_EVENT_CLICKED,
                      (void *)(intptr_t)((c << 8) | (b << 4) | s));

  char val[16];
  fake_sensor_value_text(c, b, s, val, sizeof(val));
  lv_obj_t *v = lv_label_create(tile);
  lv_label_set_text(v, val);
  lv_obj_set_width(v, 124);
  lv_label_set_long_mode(v, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_28_compressed, LV_PART_MAIN);
  // A field the WT32 could not fetch is flagged, not silently shown as a value.
  // On the accent-filled tile white is the only legible choice.
  lv_obj_set_style_text_color(
      v,
      sel ? lv_color_white()
          : lv_color_hex(failed ? UI_COL_ALARM : (have ? UI_COL_TEXT : UI_COL_DIM)),
      LV_PART_MAIN);
  lv_obj_align(v, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t *cap = lv_label_create(tile);
  lv_label_set_text(cap, SENSOR_CAPTION[s]);
  lv_obj_set_width(cap, 124);
  lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(
      cap, sel ? lv_color_white() : lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
  lv_obj_align(cap, LV_ALIGN_BOTTOM_MID, 0, -10);
  return tile;
}

// Trend for the selected sensor. The card carries the series name and its fixed
// scale, because LVGL v9 charts draw no axis tick labels.
static void build_trend_card(lv_obj_t *parent, int c, int b, int s) {
  const BottleFake &bt = g.cluster[c].bottle[b];
  lv_obj_t *card = ui_mk_card(parent, 16, 196, 768, 160);

  // The card's border and padding leave 748x140 of content.
  char title[64];
  snprintf(title, sizeof(title), "%s trend, last 24 polls", SENSOR_CHART_NAME[s]);
  ui_mk_label(card, title, 0, 0, 748, UI_COL_DIM, &lv_font_montserrat_14);
  char axis[40];
  axis_caption(s, axis, sizeof(axis));
  lv_obj_t *scale = ui_mk_label(card, axis, 0, 0, 748, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(scale, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  bool haveTrend = false;
  for (int i = 0; i < UI_TREND_PTS; i++) {
    if (isfinite(bt.trend[s][i])) haveTrend = true;
  }
  if (!haveTrend) {
    lv_obj_t *empty =
        ui_mk_label(card, "No samples yet.\nValues appear after the next WT32 poll.",
                    0, 20, 748, UI_COL_DIM, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, 16);
    return;
  }

  lv_obj_t *chart = lv_chart_create(card);
  lv_obj_set_pos(chart, 0, 20);
  lv_obj_set_size(chart, 748, 120);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, UI_TREND_PTS);
  lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, (int32_t)(kAxisLo[s] * kAxisGain[s]),
                          (int32_t)(kAxisHi[s] * kAxisGain[s]));
  lv_chart_set_div_line_count(chart, 3, 0);
  // The card is the surface, and no theme is enabled: the line width and the
  // point background opacity both default to "nothing" and must be set here.
  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(chart, 4, LV_PART_MAIN);
  lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
  lv_obj_set_style_line_color(chart, lv_color_hex(UI_COL_BAR), LV_PART_MAIN);
  lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(chart, true, LV_PART_ITEMS);
  lv_obj_set_style_width(chart, 6, LV_PART_INDICATOR);
  lv_obj_set_style_height(chart, 6, LV_PART_INDICATOR);
  lv_obj_set_style_radius(chart, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_border_width(chart, 0, LV_PART_INDICATOR);

  lv_chart_series_t *ser =
      lv_chart_add_series(chart, lv_color_hex(UI_COL_ACCENT), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < UI_TREND_PTS; i++) {
    const float v = bt.trend[s][i];
    lv_chart_set_next_value(chart, ser,
                            isfinite(v) ? (int32_t)lroundf(v * kAxisGain[s])
                                        : LV_CHART_POINT_NONE);
  }
}

void screen_bottle_show(lv_obj_t *parent, int c, int b) {
  if (c != s_chartC || b != s_chartB) {
    s_chartC = c;
    s_chartB = b;
    s_chartSensor = S_TEMP;  // temperature is the chart every bottle opens on
  }

  // Header: ownership in words instead of a route path. The node reads as quiet
  // secondary text on the same baseline.
  char t[24];
  snprintf(t, sizeof(t), "BOTTLE %d", b);
  ui_mk_label(parent, t, 16, 6, 220, UI_COL_TEXT, &lv_font_montserrat_20);
  snprintf(t, sizeof(t), "NODE %d", c);
  ui_mk_label(parent, t, 132, 12, 200, UI_COL_DIM, &lv_font_montserrat_14);

  if (!g.cluster[c].bottle[b].present) {
    lv_obj_t *note = ui_mk_label(parent, "No live data for this bottle.", 436, 12, 348,
                                 UI_COL_ALARM, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  }

  // Five tiles on one line: 5*144 + 4*12 = 768, the same 16 px edges as /info.
  for (int s = 0; s < S_COUNT; s++) {
    mk_sensor_tile(parent, c, b, s, 16 + s * 156, 40);
  }

  build_trend_card(parent, c, b, s_chartSensor);

  // Discoverability for the tile taps; the selected tile is visually distinct,
  // but a first-time operator has nothing else to go on.
  ui_mk_label(parent, "Tap a tile to chart that sensor.", 16, 386, 340, UI_COL_DIM,
              &lv_font_montserrat_14);

  lv_obj_t *back = ui_mk_button(parent, LV_SYMBOL_NEW_LINE, 364, 372, 72, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back_bottle, LV_EVENT_CLICKED, (void *)(intptr_t)c);
}