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
  ui_show_data_picker();
}
static void on_back_home(lv_event_t *e) {
  (void)e;
  ui_show_home();
}
static void on_back_cluster(lv_event_t *e) {
  intptr_t c = (intptr_t)lv_event_get_user_data(e);
  ui_show_cluster((int)c);
}

void screen_data_picker_show(lv_obj_t *parent) {
  ui_mk_label(parent, "/data clusters (fake)", 16, 8, 400, UI_COL_WHITE,
              &lv_font_montserrat_20);
  for (int c = 0; c < UI_CLUSTERS; c++) {
    int row = c / 3, col = c % 3;
    int x = 16 + col * 256, y = 52 + row * 150;
    char t[64];
    if (g.cluster[c].online) {
      snprintf(t, sizeof(t), "Cluster %d\nONLINE", c);
    } else {
      snprintf(t, sizeof(t), "Cluster %d\nNOT CONNECTED", c);
    }
    uint32_t bg = g.cluster[c].online ? UI_COL_CARD : 0x2A2F3A;
    lv_obj_t *b = ui_mk_button(parent, t, x, y, 240, 130, bg);
    lv_obj_add_event_cb(b, on_pick_cluster, LV_EVENT_CLICKED, (void *)(intptr_t)c);
  }
  lv_obj_t *back = ui_mk_button(parent, "< Home", 16, 360, 140, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back_home, LV_EVENT_CLICKED, nullptr);
}

void screen_cluster_show(lv_obj_t *parent, int c) {
  char title[48];
  snprintf(title, sizeof(title), "/data/%d bottles (fake)", c);
  ui_mk_label(parent, title, 16, 8, 500, UI_COL_WHITE, &lv_font_montserrat_20);
  if (!g.cluster[c].online) {
    ui_mk_label(parent, "node offline (fake NULL fields)", 16, 40, 500, UI_COL_ALARM,
                &lv_font_montserrat_14);
  }
  for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
    int x = 16 + (b % 2) * 384, y = 76 + (b / 2) * 140;
    char t[96];
    if (!g.cluster[c].online) {
      snprintf(t, sizeof(t), "Bottle %d\nnode offline", b);
    } else {
      char tmp[32];
      fake_sensor_text(c, b, S_TEMP, tmp, sizeof(tmp));
      snprintf(t, sizeof(t), "Bottle %d\n%s (fake)", b, tmp);
    }
    lv_obj_t *btn = ui_mk_button(parent, t, x, y, 368, 124, UI_COL_CARD);
    lv_obj_add_event_cb(btn, on_pick_bottle, LV_EVENT_CLICKED,
                        (void *)(intptr_t)((c << 8) | b));
  }
  lv_obj_t *back = ui_mk_button(parent, "< Clusters", 16, 360, 160, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back_picker, LV_EVENT_CLICKED, nullptr);
}

void screen_bottle_show(lv_obj_t *parent, int c, int b) {
  char title[48];
  snprintf(title, sizeof(title), "/data/%d/bottle_%d (fake)", c, b);
  ui_mk_label(parent, title, 16, 8, 500, UI_COL_WHITE, &lv_font_montserrat_20);

  if (!g.cluster[c].online) {
    ui_mk_label(parent, "node offline: values are NULL this cycle, not stale.", 16, 40, 500,
                UI_COL_ALARM, &lv_font_montserrat_14);
  }

  // 5 sensor rows.
  for (int s = 0; s < S_COUNT; s++) {
    char row[96], val[32];
    fake_sensor_text(c, b, s, val, sizeof(val));
    snprintf(row, sizeof(row), "%-6s  %s", SENSOR_NAMES[s], val);
    uint32_t col =
        g.cluster[c].bottle[b].sensorNull[s] ? UI_COL_ALARM : UI_COL_WHITE;
    ui_mk_label(parent, row, 24, 72 + s * 30, 320, col, &lv_font_montserrat_14);
  }

  // Fake temp trend chart (ring buffer, not history storage).
  lv_obj_t *chart = lv_chart_create(parent);
  lv_obj_set_pos(chart, 360, 72);
  lv_obj_set_size(chart, 400, 220);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, UI_TREND_PTS);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 300, 450);  // x10 degC
  lv_obj_set_style_bg_color(chart, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
  lv_chart_series_t *ser = lv_chart_add_series(chart, lv_color_hex(UI_COL_ACCENT),
                                              LV_CHART_AXIS_PRIMARY_Y);
  const BottleFake &bt = g.cluster[c].bottle[b];
  for (int i = 0; i < UI_TREND_PTS; i++) {
    lv_chart_set_next_value(chart, ser, (int32_t)(bt.trend[i] * 10.0f));
  }
  ui_mk_label(parent, "Temp trend x10C (fake ring)", 360, 300, 400, UI_COL_DIM,
              &lv_font_montserrat_14);

  lv_obj_t *back = ui_mk_button(parent, "< Bottles", 16, 360, 160, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back_cluster, LV_EVENT_CLICKED, (void *)(intptr_t)c);
}
