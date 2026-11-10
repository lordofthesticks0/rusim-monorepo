#include "screen_data.h"

#include "fake_data.h"
#include "ui_config.h"
#include "ui_shell.h"

static void on_pick_cluster(lv_event_t *e) {
  intptr_t c = (intptr_t)lv_event_get_user_data(e);
  ui_show_cluster((int)c);
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

void screen_bottle_show(lv_obj_t *parent, int c, int b) {
  char title[48];
  snprintf(title, sizeof(title), "/data/%d/chamber_%d", c, b);
  ui_mk_label(parent, title, 16, 8, 500, UI_COL_TEXT, &lv_font_montserrat_20);

  if (!g.cluster[c].bottle[b].present) {
    ui_mk_label(parent, "No live sensor data: values are unavailable.", 16, 40, 500,
                UI_COL_ALARM, &lv_font_montserrat_14);
  }

  for (int s = 0; s < S_COUNT; s++) {
    char row[96], val[32];
    fake_sensor_text(c, b, s, val, sizeof(val));
    snprintf(row, sizeof(row), "%-6s  %s", SENSOR_NAMES[s], val);
    uint32_t col =
        g.cluster[c].bottle[b].sensorNull[s] ? UI_COL_ALARM : UI_COL_TEXT;
    ui_mk_label(parent, row, 24, 72 + s * 30, 320, col, &lv_font_montserrat_14);
  }

  lv_obj_t *chart = lv_chart_create(parent);
  lv_obj_set_pos(chart, 360, 72);
  lv_obj_set_size(chart, 400, 220);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, UI_TREND_PTS);
  lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 300, 450);
  lv_obj_set_style_bg_color(chart, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
  lv_chart_series_t *ser = lv_chart_add_series(chart, lv_color_hex(UI_COL_ACCENT),
                                               LV_CHART_AXIS_PRIMARY_Y);
  const BottleFake &bt = g.cluster[c].bottle[b];
  for (int i = 0; i < UI_TREND_PTS; i++) {
    lv_chart_set_next_value(chart, ser, isfinite(bt.trend[i])
                                          ? (int32_t)(bt.trend[i] * 10.0f)
                                          : LV_CHART_POINT_NONE);
  }
  ui_mk_label(parent, "Temp trend x10C (one pt per received poll)",
              360, 300, 400, UI_COL_DIM, &lv_font_montserrat_14);

  lv_obj_t *back = ui_mk_button(parent, LV_SYMBOL_NEW_LINE, 364, 372, 72, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back_bottle, LV_EVENT_CLICKED, (void *)(intptr_t)c);
}