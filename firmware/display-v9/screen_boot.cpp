#include "screen_boot.h"

#include "fake_data.h"
#include "ui_config.h"
#include "ui_shell.h"

static lv_obj_t *s_gate = nullptr;
static lv_obj_t *s_spin = nullptr;

static void on_confirm(lv_event_t *e) {
  (void)e;
  int32_t v = lv_spinbox_get_value(s_spin);
  if (v < UI_DURATION_MIN_H) v = UI_DURATION_MIN_H;
  if (v > UI_DURATION_MAX_H) v = UI_DURATION_MAX_H;
  g.durationH = (int)v;
  g.experimentRunning = true;
  Serial.printf("[FAKE] experiment started, duration=%dh\n", g.durationH);
  if (s_gate != nullptr) {
    lv_obj_del(s_gate);
    s_gate = nullptr;
  }
  ui_show_home();
  ui_update_bell();
}

static void on_spin_plus(lv_event_t *e) {
  (void)e;
  lv_spinbox_increment(s_spin);
}
static void on_spin_minus(lv_event_t *e) {
  (void)e;
  lv_spinbox_decrement(s_spin);
}

void screen_boot_show_gate() {
  lv_obj_t *scr = lv_screen_active();
  s_gate = lv_obj_create(scr);
  lv_obj_set_pos(s_gate, 0, 0);
  lv_obj_set_size(s_gate, 800, 480);
  lv_obj_set_style_bg_color(s_gate, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_gate, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_gate, 0, LV_PART_MAIN);
  lv_obj_remove_flag(s_gate, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *card = ui_mk_card(s_gate, 140, 60, 520, 360);
  ui_mk_label(card, "Start experiment (fake)", 12, 8, 480, UI_COL_WHITE,
              &lv_font_montserrat_28);
  ui_mk_label(card, "Pre-flight gate. Confirm to proceed.\nNo Display self-test (moved to WT32).",
              12, 52, 480, UI_COL_DIM, &lv_font_montserrat_14);
  ui_mk_label(card, "Duration (hours, 1-99):", 12, 120, 300, UI_COL_WHITE,
              &lv_font_montserrat_20);

  s_spin = lv_spinbox_create(card);
  lv_obj_set_pos(s_spin, 12, 160);
  lv_obj_set_size(s_spin, 200, 56);
  lv_spinbox_set_range(s_spin, UI_DURATION_MIN_H, UI_DURATION_MAX_H);
  lv_spinbox_set_digit_format(s_spin, 2, 0);
  lv_spinbox_set_step(s_spin, 1);
  lv_spinbox_set_value(s_spin, UI_DURATION_DEFAULT_H);
  lv_obj_set_style_text_font(s_spin, &lv_font_montserrat_20, LV_PART_MAIN);

  lv_obj_t *minus = ui_mk_button(card, "-", 230, 160, 56, 56, UI_COL_CARD);
  lv_obj_add_event_cb(minus, on_spin_minus, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *plus = ui_mk_button(card, "+", 296, 160, 56, 56, UI_COL_CARD);
  lv_obj_add_event_cb(plus, on_spin_plus, LV_EVENT_CLICKED, nullptr);

  ui_mk_label(card, "Bottles loaded (fake) | WT32 link: FAKE", 12, 232, 480, UI_COL_DIM,
              &lv_font_montserrat_14);
  lv_obj_t *ok = ui_mk_button(card, "Confirm & start", 130, 272, 240, 56, UI_COL_ACCENT);
  lv_obj_add_event_cb(ok, on_confirm, LV_EVENT_CLICKED, nullptr);
}
