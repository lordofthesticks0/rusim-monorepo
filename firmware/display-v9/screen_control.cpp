#include "screen_control.h"

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_hold_button.h"
#include "ui_shell.h"

static lv_obj_t *s_setpoint = nullptr;
static lv_obj_t *s_btnTempDown = nullptr;
static lv_obj_t *s_btnTempUp = nullptr;
static lv_obj_t *s_btnStirrer = nullptr;
static lv_obj_t *s_btnRun = nullptr;
static lv_obj_t *s_btnDurDown = nullptr;
static lv_obj_t *s_btnDurUp = nullptr;
static lv_obj_t *s_durValue = nullptr;
static lv_obj_t *s_btnExpDown = nullptr;
static lv_obj_t *s_btnExpUp = nullptr;
static lv_obj_t *s_expValue = nullptr;
static lv_obj_t *s_expDesc = nullptr;
static lv_obj_t *s_intBtn = nullptr;
static lv_obj_t *s_intHint = nullptr;

static bool s_dbSuggested = false;

static void exp_mirror_link() {
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
}

static void setpoint_show() {
  if (s_setpoint == nullptr) return;
  char t[32];
  snprintf(t, sizeof(t), "%.1f C", g.tempSetpointC);
  lv_label_set_text(s_setpoint, t);
}

static void dur_show() {
  if (s_durValue == nullptr) return;
  char t[16];
  snprintf(t, sizeof(t), "%d", g.durationH);
  lv_label_set_text(s_durValue, t);
}

static void exp_show() {
  if (s_expValue == nullptr) return;
  char t[16];
  snprintf(t, sizeof(t), "%d", g.expNum);
  lv_label_set_text(s_expValue, t);
}

static void db_refresh_warn() {
  if (!ui_is_control()) return;
  if (s_expDesc == nullptr) return;
  int latest = link_serial_get_db_latest();
  if (latest == -2) {
    lv_label_set_text(s_expDesc, "DB: waiting for sync");
    lv_obj_set_style_text_color(s_expDesc, lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
    return;
  }
  if (!s_dbSuggested) {
    s_dbSuggested = true;
    int next = latest + 1;
    if (next < UI_EXP_NUM_MIN) next = UI_EXP_NUM_MIN;
    if (next > UI_EXP_NUM_MAX) next = UI_EXP_NUM_MAX;
    if (g.expNum != next) {
      g.expNum = next;
      exp_show();
      exp_mirror_link();
      ui_update_bell();
      Serial.printf("[DB] auto-suggest exp=%d (latest=%d)\n", next, latest);
    }
  }
  if (g.expNum <= latest) {
    char t[64];
    snprintf(t, sizeof(t), "exp %d exists, use %d", g.expNum, latest + 1);
    lv_label_set_text(s_expDesc, t);
    lv_obj_set_style_text_color(s_expDesc, lv_color_hex(UI_COL_WARN), LV_PART_MAIN);
  } else {
    char t[64];
    snprintf(t, sizeof(t), "DB latest %d, use %d", latest, g.expNum);
    lv_label_set_text(s_expDesc, t);
    lv_obj_set_style_text_color(s_expDesc, lv_color_hex(UI_COL_OK), LV_PART_MAIN);
  }
}

void screen_control_poll() {
  if (!ui_is_control()) return;
  db_refresh_warn();
}

void screen_control_hide() {
  s_setpoint = nullptr;
  s_btnTempDown = nullptr;
  s_btnTempUp = nullptr;
  s_btnStirrer = nullptr;
  s_btnRun = nullptr;
  s_btnDurDown = nullptr;
  s_btnDurUp = nullptr;
  s_durValue = nullptr;
  s_btnExpDown = nullptr;
  s_btnExpUp = nullptr;
  s_expValue = nullptr;
  s_expDesc = nullptr;
  s_intBtn = nullptr;
  s_intHint = nullptr;
  s_dbSuggested = false;
}

static void on_temp_down(lv_event_t *e) {
  (void)e;
  g.tempSetpointC -= UI_TEMP_STEP_C;
  if (g.tempSetpointC < UI_TEMP_MIN_C) g.tempSetpointC = UI_TEMP_MIN_C;
  setpoint_show();
  Serial.printf("[FAKE->WT32] SETPOINT %.1f\n", g.tempSetpointC);
  ui_update_bell();
}

static void on_temp_up(lv_event_t *e) {
  (void)e;
  g.tempSetpointC += UI_TEMP_STEP_C;
  if (g.tempSetpointC > UI_TEMP_MAX_C) g.tempSetpointC = UI_TEMP_MAX_C;
  setpoint_show();
  Serial.printf("[FAKE->WT32] SETPOINT %.1f\n", g.tempSetpointC);
  ui_update_bell();
}

static void on_stirrer_commit(void *user) {
  (void)user;
  g.stirrerOn = !g.stirrerOn;
  Serial.printf("[FAKE->WT32] STIRRER %s\n", g.stirrerOn ? "ON" : "OFF");
  if (s_btnStirrer != nullptr) ui_hold_button_set_state(s_btnStirrer, g.stirrerOn);
  ui_update_bell();
}

static void on_run_commit(void *user) {
  (void)user;
  ui_experiment_toggle();
  if (s_btnRun != nullptr) {
    ui_hold_button_set_symbol(s_btnRun, g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY);
    ui_hold_button_set_state(s_btnRun, g.experimentRunning);
  }
  ui_update_bell();
}

static void on_dur_down(lv_event_t *e) {
  (void)e;
  g.durationH--;
  if (g.durationH < UI_DURATION_MIN_H) g.durationH = UI_DURATION_MIN_H;
  dur_show();
  exp_mirror_link();
  ui_update_bell();
}

static void on_dur_up(lv_event_t *e) {
  (void)e;
  g.durationH++;
  if (g.durationH > UI_DURATION_MAX_H) g.durationH = UI_DURATION_MAX_H;
  dur_show();
  exp_mirror_link();
  ui_update_bell();
}

static void on_exp_down(lv_event_t *e) {
  (void)e;
  g.expNum--;
  if (g.expNum < UI_EXP_NUM_MIN) g.expNum = UI_EXP_NUM_MIN;
  exp_show();
  exp_mirror_link();
  ui_update_bell();
  db_refresh_warn();
}

static void on_exp_up(lv_event_t *e) {
  (void)e;
  g.expNum++;
  if (g.expNum > UI_EXP_NUM_MAX) g.expNum = UI_EXP_NUM_MAX;
  exp_show();
  exp_mirror_link();
  ui_update_bell();
  db_refresh_warn();
}

static void on_int_test(lv_event_t *e) {
  (void)e;
  link_serial_emit_experiment();
  Serial.println("[LINK] EXPERIMENT pushed to the WT32 now");
  if (s_intHint != nullptr) {
    lv_label_set_text(s_intHint, "sent EXPERIMENT now");
  }
}

void screen_control_show(lv_obj_t *parent) {
  s_dbSuggested = false;

  // Setpoint row
  s_btnTempDown = ui_mk_button(parent, LV_SYMBOL_DOWN, 274, 16, 64, 52, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnTempDown, on_temp_down, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnTempDown, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  s_setpoint = ui_mk_label(parent, "", 350, 32, 100, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(s_setpoint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_btnTempUp = ui_mk_button(parent, LV_SYMBOL_UP, 462, 16, 64, 52, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnTempUp, on_temp_up, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnTempUp, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  setpoint_show();

  // Stirrer hold button
  s_btnStirrer = ui_hold_button_create(parent, 352, 76, 96, LV_SYMBOL_POWER, g.stirrerOn,
                                       on_stirrer_commit, nullptr);
  ui_mk_label(parent, "Hold 2s to turn the stirrer on or off", 200, 182, 400, UI_COL_DIM,
              &lv_font_montserrat_14);
  // center desc
  lv_obj_set_style_text_align(lv_obj_get_child(parent, lv_obj_get_child_cnt(parent) - 1),
                             LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // Run hold button
  s_btnRun = ui_hold_button_create(parent, 352, 206, 96,
                                   g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY,
                                   g.experimentRunning, on_run_commit, nullptr);
  ui_mk_label(parent, "Hold 2s to start or stop the experiment", 200, 312, 400, UI_COL_DIM,
              &lv_font_montserrat_14);
  lv_obj_set_style_text_align(lv_obj_get_child(parent, lv_obj_get_child_cnt(parent) - 1),
                             LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // Duration block
  ui_mk_label(parent, "Duration", 100, 340, 200, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(lv_obj_get_child(parent, lv_obj_get_child_cnt(parent) - 1),
                             LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  ui_mk_label(parent, "hours 1-99", 100, 358, 200, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(lv_obj_get_child(parent, lv_obj_get_child_cnt(parent) - 1),
                             LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_btnDurDown = ui_mk_button(parent, LV_SYMBOL_DOWN, 116, 376, 48, 40, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnDurDown, on_dur_down, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnDurDown, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  s_durValue = ui_mk_label(parent, "", 172, 384, 56, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(s_durValue, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_btnDurUp = ui_mk_button(parent, LV_SYMBOL_UP, 236, 376, 48, 40, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnDurUp, on_dur_up, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnDurUp, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  dur_show();

  // Experiment number block
  ui_mk_label(parent, "Experiment number", 500, 340, 200, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(lv_obj_get_child(parent, lv_obj_get_child_cnt(parent) - 1),
                             LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_expDesc = ui_mk_label(parent, "DB: waiting for sync", 500, 358, 200, UI_COL_DIM,
                          &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_expDesc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_btnExpDown = ui_mk_button(parent, LV_SYMBOL_DOWN, 516, 376, 48, 40, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnExpDown, on_exp_down, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnExpDown, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  s_expValue = ui_mk_label(parent, "", 572, 384, 56, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(s_expValue, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_btnExpUp = ui_mk_button(parent, LV_SYMBOL_UP, 636, 376, 48, 40, UI_COL_CARD);
  lv_obj_add_event_cb(s_btnExpUp, on_exp_up, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(s_btnExpUp, 0), lv_color_hex(UI_COL_ACCENT),
                             LV_PART_MAIN);
  exp_show();

  // Link push test, left column
  ui_mk_label(parent, "Link test (EXPERIMENT push)", 16, 96, 180, UI_COL_TEXT,
              &lv_font_montserrat_14);
  s_intBtn = ui_mk_button(parent, "PUSH EXP", 16, 136, 160, 42, UI_COL_CARD);
  lv_obj_add_event_cb(s_intBtn, on_int_test, LV_EVENT_CLICKED, nullptr);
  s_intHint = ui_mk_label(parent, "", 16, 184, 180, UI_COL_DIM, &lv_font_montserrat_14);

  db_refresh_warn();
}