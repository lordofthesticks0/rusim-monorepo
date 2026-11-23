#include "screen_control.h"

#include <string.h>

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_hold_button.h"
#include "ui_shell.h"

// Layout (content coordinates, 16 px grid):
//   title row "Control" (+ debug PUSH EXP on the right, kept for development)
//   left top    (16, 56, 376 x 140): temperature setpoint
//   left bottom (16, 212, 376 x 204): experiment number + duration + status
//   right       (408, 56, 376 x 360): run, then equipment (stirrers, pump)
// Widgets are built once in show(); refresh() only changes text and colours.
//
// Setpoint, stirrers and water pump have no hardware transport yet (backlog):
// they change display state only and say so on screen.

static lv_obj_t *s_setpoint = nullptr;
static lv_obj_t *s_durValue = nullptr;
static lv_obj_t *s_expValue = nullptr;
static lv_obj_t *s_expDesc = nullptr;
static lv_obj_t *s_btnRun = nullptr;
static lv_obj_t *s_runState = nullptr;
static lv_obj_t *s_runHint = nullptr;
static lv_obj_t *s_btnStirrer = nullptr;
static lv_obj_t *s_stirrerState = nullptr;
static lv_obj_t *s_btnPump = nullptr;
static lv_obj_t *s_pumpState = nullptr;
static lv_obj_t *s_intHint = nullptr;

static void render() {
  if (s_setpoint == nullptr) return;
  char t[64];
  snprintf(t, sizeof(t), "%.1f " UI_DEG_C, g.tempSetpointC);
  lv_label_set_text(s_setpoint, t);
  snprintf(t, sizeof(t), "%d", g.expNum);
  lv_label_set_text(s_expValue, t);
  snprintf(t, sizeof(t), "%d h", g.durationH);
  lv_label_set_text(s_durValue, t);

  const uint32_t col = ui_exp_status_text(t, sizeof(t));
  lv_label_set_text(s_expDesc, t);
  lv_obj_set_style_text_color(s_expDesc, lv_color_hex(col), LV_PART_MAIN);

  ui_hold_button_set_symbol(s_btnRun, g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY);
  ui_hold_button_set_state(s_btnRun, g.experimentRunning);
  lv_label_set_text(s_runState, g.experimentRunning ? "Running" : "Idle");
  lv_obj_set_style_text_color(s_runState,
                              lv_color_hex(g.experimentRunning ? UI_COL_OK : UI_COL_TEXT),
                              LV_PART_MAIN);
  lv_label_set_text(s_runHint, g.experimentRunning ? "Hold 2 s to stop" : "Hold 2 s to start");

  ui_hold_button_set_state(s_btnStirrer, g.stirrerOn);
  lv_label_set_text(s_stirrerState, g.stirrerOn ? "Stirrers: On" : "Stirrers: Off");
  ui_hold_button_set_state(s_btnPump, g.pumpOn);
  lv_label_set_text(s_pumpState, g.pumpOn ? "Water pump: On" : "Water pump: Off");
}

void screen_control_refresh() { render(); }

void screen_control_poll() {
  if (!ui_is_control()) return;
  // DB state arrives from the WT32 at any time; keep the status line current.
  if (s_expDesc == nullptr) return;
  char t[64];
  const uint32_t col = ui_exp_status_text(t, sizeof(t));
  if (strcmp(lv_label_get_text(s_expDesc), t) != 0) {
    lv_label_set_text(s_expDesc, t);
    lv_obj_set_style_text_color(s_expDesc, lv_color_hex(col), LV_PART_MAIN);
  }
}

void screen_control_hide() {
  s_setpoint = nullptr;
  s_durValue = nullptr;
  s_expValue = nullptr;
  s_expDesc = nullptr;
  s_btnRun = nullptr;
  s_runState = nullptr;
  s_runHint = nullptr;
  s_btnStirrer = nullptr;
  s_stirrerState = nullptr;
  s_btnPump = nullptr;
  s_pumpState = nullptr;
  s_intHint = nullptr;
}

static void on_temp(lv_event_t *e) {
  const int dir = (int)(intptr_t)lv_event_get_user_data(e);
  g.tempSetpointC += dir * UI_TEMP_STEP_C;
  if (g.tempSetpointC < UI_TEMP_MIN_C) g.tempSetpointC = UI_TEMP_MIN_C;
  if (g.tempSetpointC > UI_TEMP_MAX_C) g.tempSetpointC = UI_TEMP_MAX_C;
  Serial.printf("[FAKE->WT32] SETPOINT %.1f\n", g.tempSetpointC);
  render();
}

static void on_dur(lv_event_t *e) {
  const int dir = (int)(intptr_t)lv_event_get_user_data(e);
  g.durationH += dir;
  if (g.durationH < UI_DURATION_MIN_H) g.durationH = UI_DURATION_MIN_H;
  if (g.durationH > UI_DURATION_MAX_H) g.durationH = UI_DURATION_MAX_H;
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  render();
}

static void on_exp(lv_event_t *e) {
  const int dir = (int)(intptr_t)lv_event_get_user_data(e);
  // Accelerates to steps of 10 after ~2 s of auto-repeat (range 0-9999).
  ui_exp_set_user(g.expNum + dir * ui_stepper_mult(e));
  render();
}

static void on_run_commit(void *user) {
  (void)user;
  ui_experiment_toggle();  // refreshes this route in place
  render();
}

static void on_stirrer_commit(void *user) {
  (void)user;
  g.stirrerOn = !g.stirrerOn;
  Serial.printf("[FAKE->WT32] STIRRER %s (no transport yet)\n", g.stirrerOn ? "ON" : "OFF");
  render();
}

static void on_pump_commit(void *user) {
  (void)user;
  g.pumpOn = !g.pumpOn;
  Serial.printf("[FAKE->WT32] PUMP %s (no transport yet)\n", g.pumpOn ? "ON" : "OFF");
  render();
}

static void on_int_test(lv_event_t *e) {
  (void)e;
  link_serial_emit_experiment();
  Serial.println("[LINK] EXPERIMENT pushed to the WT32 now");
  if (s_intHint != nullptr) lv_label_set_text(s_intHint, "EXPERIMENT sent");
}

// "Label  [-]  value  [+]" stepper row inside a card.
static lv_obj_t *mk_stepper_row(lv_obj_t *card, const char *label, int y, int bw, int bh,
                                lv_event_cb_t cb, const lv_font_t *valueFont) {
  ui_mk_label(card, label, 0, y + bh / 2 - 9, 84, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_t *minus = ui_mk_button(card, LV_SYMBOL_MINUS, 88, y, bw, bh, UI_COL_CARD);
  ui_stepper_events(minus, cb, (void *)(intptr_t)-1);
  const int vx = 88 + bw + 4;
  const int vw = 236 - 2 * bw;
  lv_obj_t *value =
      ui_mk_label(card, "", vx, y + bh / 2 - (valueFont == &lv_font_montserrat_28 ? 17 : 12), vw,
                  UI_COL_TEXT, valueFont);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_t *plus = ui_mk_button(card, LV_SYMBOL_PLUS, vx + vw + 4, y, bw, bh, UI_COL_CARD);
  ui_stepper_events(plus, cb, (void *)(intptr_t)1);
  return value;
}

// One equipment hold button with its name/state line and the "not connected"
// note, centred in a 175 px column of the right card.
static lv_obj_t *mk_equipment(lv_obj_t *card, int x0, const char *symbol, bool on,
                              UiHoldFn cb, lv_obj_t **stateOut) {
  lv_obj_t *btn = ui_hold_button_create(card, x0 + 47, 194, 80, symbol, on, cb, nullptr);
  *stateOut = ui_mk_label(card, "", x0, 290, 175, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(*stateOut, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_t *note = ui_mk_label(card, "Not connected yet", x0, 310, 175, UI_COL_DIM,
                               &lv_font_montserrat_14);
  lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  return btn;
}

void screen_control_show(lv_obj_t *parent) {
  ui_mk_page_title(parent, "Control", nullptr, nullptr);

  // Development tool (kept for now, audit 3.10): forces one EXPERIMENT line.
  s_intHint = ui_mk_label(parent, "", 380, 18, 228, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_intHint, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_t *push = ui_mk_button(parent, "Push EXPERIMENT", 616, 6, 168, 40, UI_COL_CARD);
  lv_obj_add_event_cb(push, on_int_test, LV_EVENT_CLICKED, nullptr);

  // --- Temperature setpoint (inner 350 x 114) ---
  lv_obj_t *temp = ui_mk_card(parent, UI_MARGIN, UI_PAGE_TOP, 376, 140);
  ui_mk_caption(temp, "TEMPERATURE SETPOINT", 0, 0, 350);
  lv_obj_t *minus = ui_mk_button(temp, LV_SYMBOL_MINUS, 0, 24, 64, 56, UI_COL_CARD);
  ui_stepper_events(minus, on_temp, (void *)(intptr_t)-1);
  s_setpoint = ui_mk_label(temp, "", 68, 36, 150, UI_COL_TEXT, &lv_font_montserrat_28);
  lv_obj_set_style_text_align(s_setpoint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_t *plus = ui_mk_button(temp, LV_SYMBOL_PLUS, 222, 24, 64, 56, UI_COL_CARD);
  ui_stepper_events(plus, on_temp, (void *)(intptr_t)1);
  ui_mk_label(temp, "0.1 " UI_DEG_C " steps. Display only, not sent to the rig yet.", 0, 90,
              350, UI_COL_DIM, &lv_font_montserrat_14);

  // --- Experiment (inner 350 x 178) ---
  lv_obj_t *exp = ui_mk_card(parent, UI_MARGIN, 212, 376, 204);
  ui_mk_caption(exp, "EXPERIMENT", 0, 0, 350);
  s_expValue = mk_stepper_row(exp, "Number", 24, 56, 52, on_exp, &lv_font_montserrat_20);
  s_durValue = mk_stepper_row(exp, "Duration", 86, 56, 52, on_dur, &lv_font_montserrat_20);
  s_expDesc = ui_mk_label(exp, "", 0, 154, 350, UI_COL_DIM, &lv_font_montserrat_14);

  // --- Run + equipment (inner 350 x 334) ---
  lv_obj_t *right = ui_mk_card(parent, 408, UI_PAGE_TOP, 376, 360);
  ui_mk_caption(right, "RUN", 0, 0, 350);
  s_btnRun = ui_hold_button_create(right, 8, 30, 96,
                                   g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY,
                                   g.experimentRunning, on_run_commit, nullptr);
  ui_mk_label(right, "Experiment", 128, 34, 222, UI_COL_DIM, &lv_font_montserrat_14);
  s_runState = ui_mk_label(right, "", 128, 54, 222, UI_COL_TEXT, &lv_font_montserrat_20);
  s_runHint = ui_mk_label(right, "", 128, 84, 222, UI_COL_DIM, &lv_font_montserrat_14);

  // Divider between the run and the equipment (1 px, flat).
  lv_obj_t *div = lv_obj_create(right);
  lv_obj_set_pos(div, 0, 152);
  lv_obj_set_size(div, 350, 1);
  lv_obj_set_style_bg_color(div, lv_color_hex(UI_COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(div, 0, LV_PART_MAIN);

  ui_mk_caption(right, "EQUIPMENT  (HOLD 2 S TO SWITCH)", 0, 164, 350);
  s_btnStirrer = mk_equipment(right, 0, LV_SYMBOL_REFRESH, g.stirrerOn, on_stirrer_commit,
                              &s_stirrerState);
  s_btnPump = mk_equipment(right, 175, LV_SYMBOL_TINT, g.pumpOn, on_pump_commit, &s_pumpState);

  render();
}
