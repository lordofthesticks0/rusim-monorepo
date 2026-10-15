#include "screen_home.h"

#include "fake_data.h"
#include "link_modbus.h"
#include "ui_config.h"
#include "ui_hold_button.h"
#include "ui_shell.h"

static lv_obj_t *s_curTemp = nullptr;
static lv_obj_t *s_target = nullptr;
static lv_obj_t *s_btnTargetMinus = nullptr;
static lv_obj_t *s_btnTargetPlus = nullptr;
static lv_obj_t *s_bottleCount = nullptr;
static lv_obj_t *s_nodeCount = nullptr;
static lv_obj_t *s_expNum = nullptr;
static lv_obj_t *s_invalidCount = nullptr;
static lv_obj_t *s_stirrerReadout = nullptr;
static lv_obj_t *s_arc = nullptr;
static lv_obj_t *s_arcReadout = nullptr;
static lv_obj_t *s_btnDurMinus = nullptr;
static lv_obj_t *s_btnDurPlus = nullptr;
static lv_obj_t *s_btnRun = nullptr;
static uint32_t s_lastPollMs = 0;

static int bottles_online() {
  int n = 0;
  for (int c = 0; c < UI_CLUSTERS; c++) {
    if (g.cluster[c].online) n += UI_BOTTLES_PER_CLUSTER;
  }
  return n;
}

static int nodes_online() {
  int n = 0;
  for (int c = 0; c < UI_CLUSTERS; c++) {
    if (g.cluster[c].online) n++;
  }
  return n;
}

static void home_render_status() {
  if (s_curTemp != nullptr) {
    char t[32];
    snprintf(t, sizeof(t), "%.1f C", g.chamberTempC);
    lv_label_set_text(s_curTemp, t);
  }
  if (s_target != nullptr) {
    char t[32];
    snprintf(t, sizeof(t), "%.1f C", g.tempSetpointC);
    lv_label_set_text(s_target, t);
  }
  if (s_bottleCount != nullptr) {
    char t[32];
    snprintf(t, sizeof(t), "%d / %d", bottles_online(),
             UI_CLUSTERS * UI_BOTTLES_PER_CLUSTER);
    lv_label_set_text(s_bottleCount, t);
  }
  if (s_nodeCount != nullptr) {
    char t[32];
    snprintf(t, sizeof(t), "%d / %d nodes online", nodes_online(), UI_CLUSTERS);
    lv_label_set_text(s_nodeCount, t);
  }
  if (s_expNum != nullptr) {
    char t[32];
    snprintf(t, sizeof(t), "Experiment #%d", g.expNum);
    lv_label_set_text(s_expNum, t);
  }
  if (s_invalidCount != nullptr) {
    int n = fake_null_count();
    if (n > 0) {
      char t[32];
      snprintf(t, sizeof(t), LV_SYMBOL_WARNING " %d invalid", n);
      lv_label_set_text(s_invalidCount, t);
      lv_obj_set_style_text_color(s_invalidCount, lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
    } else {
      lv_label_set_text(s_invalidCount, LV_SYMBOL_OK " All valid");
      lv_obj_set_style_text_color(s_invalidCount, lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
    }
  }
  if (s_stirrerReadout != nullptr) {
    if (g.stirrerOn) {
      lv_label_set_text(s_stirrerReadout, "Stirrer " LV_SYMBOL_OK " ON");
    } else {
      lv_label_set_text(s_stirrerReadout, "Stirrer " LV_SYMBOL_CLOSE " OFF");
    }
  }
}

static float home_elapsed_hours() {
  if (!g.experimentRunning || g.experimentStartMs == 0) return 0.0f;
  uint32_t el = millis() - g.experimentStartMs;
  float h = el / 3600000.0f;
  if (h > (float)g.durationH) h = (float)g.durationH;
  if (h < 0.0f) h = 0.0f;
  return h;
}

static void home_render_run() {
  if (g.experimentRunning && g.experimentStartMs == 0) {
    g.experimentStartMs = millis();
  }
  const char *sym = g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY;
  if (s_btnRun != nullptr) {
    ui_hold_button_set_symbol(s_btnRun, sym);
    ui_hold_button_set_state(s_btnRun, g.experimentRunning);
  }
  int pct = 0;
  if (g.experimentRunning && g.durationH > 0) {
    float elh = home_elapsed_hours();
    pct = (int)(elh / (float)g.durationH * 100.0f);
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
  }
  if (s_arc != nullptr) lv_arc_set_value(s_arc, pct);
  if (s_arcReadout != nullptr) {
    char t[64];
    if (g.experimentRunning) {
      snprintf(t, sizeof(t), "%.1f / %d h\n%d%%", home_elapsed_hours(), g.durationH, pct);
    } else {
      snprintf(t, sizeof(t), "Idle\n%d h set", g.durationH);
    }
    lv_label_set_text(s_arcReadout, t);
  }
}

static void on_target_minus(lv_event_t *e) {
  (void)e;
  g.tempSetpointC -= UI_TEMP_STEP_C;
  if (g.tempSetpointC < UI_TEMP_MIN_C) g.tempSetpointC = UI_TEMP_MIN_C;
  home_render_status();
  Serial.printf("[FAKE->WT32] SETPOINT %.1f\n", g.tempSetpointC);
  ui_update_bell();
}

static void on_target_plus(lv_event_t *e) {
  (void)e;
  g.tempSetpointC += UI_TEMP_STEP_C;
  if (g.tempSetpointC > UI_TEMP_MAX_C) g.tempSetpointC = UI_TEMP_MAX_C;
  home_render_status();
  Serial.printf("[FAKE->WT32] SETPOINT %.1f\n", g.tempSetpointC);
  ui_update_bell();
}

static void on_duration_down(lv_event_t *e) {
  (void)e;
  g.durationH--;
  if (g.durationH < UI_DURATION_MIN_H) g.durationH = UI_DURATION_MIN_H;
  Serial.printf("[HOME] duration %dh\n", g.durationH);
  link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  ui_update_bell();
  home_render_run();
}

static void on_duration_up(lv_event_t *e) {
  (void)e;
  g.durationH++;
  if (g.durationH > UI_DURATION_MAX_H) g.durationH = UI_DURATION_MAX_H;
  Serial.printf("[HOME] duration %dh\n", g.durationH);
  link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  ui_update_bell();
  home_render_run();
}

static void on_run_commit(void *user) {
  (void)user;
  ui_experiment_toggle();
  home_render_run();
  home_render_status();
}

void screen_home_show(lv_obj_t *parent) {
  // Card A: chamber temp + steppable target.
  lv_obj_t *cardA = ui_mk_card(parent, 16, 8, 376, 160);
  ui_mk_label(cardA, "CHAMBER TEMP", 12, 8, 352, UI_COL_DIM, &lv_font_montserrat_14);
  s_curTemp = ui_mk_label(cardA, "", 12, 28, 352, UI_COL_TEXT, &lv_font_montserrat_20);

  ui_mk_label(cardA, "Target setpoint", 12, 58, 352, UI_COL_DIM, &lv_font_montserrat_14);
  s_btnTargetMinus = ui_mk_button(cardA, LV_SYMBOL_DOWN, 12, 80, 44, 36, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnTargetMinus, on_target_minus, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnTargetMinus, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  s_target = ui_mk_label(cardA, "", 68, 84, 100, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(s_target, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_btnTargetPlus = ui_mk_button(cardA, LV_SYMBOL_UP, 180, 80, 44, 36, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnTargetPlus, on_target_plus, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnTargetPlus, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  ui_mk_label(cardA, "0.1 C steps", 236, 88, 128, UI_COL_DIM, &lv_font_montserrat_14);
  s_stirrerReadout = ui_mk_label(cardA, "", 12, 124, 352, UI_COL_TEXT, &lv_font_montserrat_14);

  // Card B: bottles online. Not scrollable: all rows fit, so no scroll
  // affordance should appear. Experiment number and the validity warning
  // share one row to use the horizontal space.
  lv_obj_t *cardB = ui_mk_card(parent, 408, 8, 376, 160);
  lv_obj_set_scrollable(cardB, false);
  ui_mk_label(cardB, "BOTTLES ONLINE", 12, 8, 352, UI_COL_DIM, &lv_font_montserrat_14);
  s_bottleCount = ui_mk_label(cardB, "", 12, 28, 352, UI_COL_TEXT, &lv_font_montserrat_20);
  s_nodeCount = ui_mk_label(cardB, "", 12, 62, 352, UI_COL_TEXT, &lv_font_montserrat_14);
  s_expNum = ui_mk_label(cardB, "", 12, 92, 176, UI_COL_DIM, &lv_font_montserrat_14);
  s_invalidCount = ui_mk_label(cardB, "", 196, 92, 156, UI_COL_ALARM, &lv_font_montserrat_14);

  // Progress zone.
  ui_mk_label(parent, "Experiment runtime", 240, 172, 320, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(
      lv_obj_get_child(parent, lv_obj_get_child_cnt(parent) - 1), LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_arc = lv_arc_create(parent);
  lv_obj_set_pos(s_arc, 340, 194);
  lv_obj_set_size(s_arc, 120, 120);
  lv_arc_set_range(s_arc, 0, 100);
  lv_arc_set_value(s_arc, 0);
  lv_obj_remove_style(s_arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(s_arc, lv_color_hex(UI_COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(s_arc, lv_color_hex(UI_COL_BAR), LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_arc, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_arc, 10, LV_PART_INDICATOR);

  s_arcReadout = ui_mk_label(parent, "", 340, 238, 120, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_arcReadout, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // Controls row.
  s_btnDurMinus = ui_mk_button(parent, LV_SYMBOL_DOWN " 1h", 176, 350, 104, 44, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnDurMinus, on_duration_down, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnDurMinus, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);

  s_btnRun = ui_hold_button_create(parent, 352, 322, 96,
                                   g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY,
                                   g.experimentRunning, on_run_commit, nullptr);

  s_btnDurPlus = ui_mk_button(parent, LV_SYMBOL_UP " 1h", 520, 350, 104, 44, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnDurPlus, on_duration_up, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnDurPlus, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);

  home_render_status();
  home_render_run();
  s_lastPollMs = millis();
}

void screen_home_hide() {
  s_curTemp = nullptr;
  s_target = nullptr;
  s_btnTargetMinus = nullptr;
  s_btnTargetPlus = nullptr;
  s_bottleCount = nullptr;
  s_nodeCount = nullptr;
  s_expNum = nullptr;
  s_invalidCount = nullptr;
  s_stirrerReadout = nullptr;
  s_arc = nullptr;
  s_arcReadout = nullptr;
  s_btnDurMinus = nullptr;
  s_btnDurPlus = nullptr;
  s_btnRun = nullptr;
  s_lastPollMs = 0;
}

void screen_home_poll() {
  if (s_arc == nullptr) return;
  uint32_t now = millis();
  if (now - s_lastPollMs < 1000) return;
  s_lastPollMs = now;
  home_render_status();
  home_render_run();
}
