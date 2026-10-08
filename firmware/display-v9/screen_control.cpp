#include "screen_control.h"

#include "fake_data.h"
#include "link_modbus.h"
#include "ui_config.h"
#include "ui_shell.h"

// Hold-to-confirm (500 ms, locked): commit the switch only if the press that
// caused VALUE_CHANGED lasted >= UI_HOLD_TO_CONFIRM_MS, else revert + hint.
struct HoldCtx {
  lv_obj_t *sw = nullptr;
  lv_obj_t *bar = nullptr;
  lv_obj_t *hint = nullptr;
  lv_timer_t *timer = nullptr;
  uint32_t t0 = 0;
  bool armed = false;
  bool reverting = false;
  bool *flag = nullptr;
  const char *name = "";
  const char *wt32cmd = "";
  void (*after)() = nullptr;  // optional hook run after a confirmed toggle
};

static void hold_progress(lv_timer_t *t) {
  HoldCtx *ctx = (HoldCtx *)lv_timer_get_user_data(t);
  if (ctx == nullptr || !ctx->armed) return;
  uint32_t el = millis() - ctx->t0;
  int pct = el >= UI_HOLD_TO_CONFIRM_MS ? 100 : (int)(el * 100 / UI_HOLD_TO_CONFIRM_MS);
  lv_bar_set_value(ctx->bar, pct, LV_ANIM_OFF);
}

static void hold_reset(HoldCtx *ctx) {
  ctx->armed = false;
  if (ctx->timer != nullptr) {
    lv_timer_delete(ctx->timer);
    ctx->timer = nullptr;
  }
  lv_bar_set_value(ctx->bar, 0, LV_ANIM_OFF);
}

static void on_hold_event(lv_event_t *e) {
  HoldCtx *ctx = (HoldCtx *)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    ctx->t0 = millis();
    ctx->armed = true;
    if (ctx->timer == nullptr) ctx->timer = lv_timer_create(hold_progress, 50, ctx);
    lv_label_set_text(ctx->hint, "hold 500ms to confirm...");
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    // VALUE_CHANGED (if any) is evaluated separately; a press with no toggle
    // just disarms.
    if (ctx->armed && ctx->timer != nullptr) {
      // Let VALUE_CHANGED decide; disarm happens there. If no value change
      // occurred, disarm now.
      lv_timer_t *tm = ctx->timer;
      (void)tm;
    }
  } else if (code == LV_EVENT_VALUE_CHANGED) {
    if (ctx->reverting) return;
    bool nowOn = lv_obj_is_checked(ctx->sw);
    uint32_t el = millis() - ctx->t0;
    if (ctx->armed && el >= UI_HOLD_TO_CONFIRM_MS) {
      *(ctx->flag) = nowOn;
      Serial.printf("[FAKE->WT32] %s %s\n", ctx->wt32cmd, nowOn ? "ON" : "OFF");
      if (ctx->after != nullptr) ctx->after();
      lv_label_set_text(ctx->hint, "sent to WT32 (fake).");
      hold_reset(ctx);
      ui_update_bell();
    } else {
      // Too short: revert to committed state.
      ctx->reverting = true;
      if (*(ctx->flag)) {
        lv_obj_set_checked(ctx->sw, true);
      } else {
        lv_obj_set_checked(ctx->sw, false);
      }
      ctx->reverting = false;
      lv_label_set_text(ctx->hint, "too short: hold 500ms.");
      hold_reset(ctx);
    }
  }
}

static void hold_attach(HoldCtx &ctx, lv_obj_t *parent, int y, const char *label, bool *flag,
                        const char *wt32cmd) {
  ctx.flag = flag;
  ctx.wt32cmd = wt32cmd;
  ctx.name = label;
  ui_mk_label(parent, label, 16, y, 200, UI_COL_TEXT, &lv_font_montserrat_14);
  ctx.sw = lv_switch_create(parent);
  lv_obj_set_pos(ctx.sw, 220, y - 4);
  lv_obj_set_size(ctx.sw, 72, 36);
  if (*flag) lv_obj_set_checked(ctx.sw, true);
  ctx.bar = lv_bar_create(parent);
  lv_obj_set_pos(ctx.bar, 310, y + 4);
  lv_obj_set_size(ctx.bar, 160, 16);
  lv_bar_set_range(ctx.bar, 0, 100);
  lv_bar_set_value(ctx.bar, 0, LV_ANIM_OFF);
  ctx.hint = ui_mk_label(parent, "", 490, y, 280, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_add_event_cb(ctx.sw, on_hold_event, LV_EVENT_PRESSED, &ctx);
  lv_obj_add_event_cb(ctx.sw, on_hold_event, LV_EVENT_RELEASED, &ctx);
  lv_obj_add_event_cb(ctx.sw, on_hold_event, LV_EVENT_PRESS_LOST, &ctx);
  lv_obj_add_event_cb(ctx.sw, on_hold_event, LV_EVENT_VALUE_CHANGED, &ctx);
}

static lv_obj_t *s_setpointLabel = nullptr;

static void setpoint_show() {
  char t[32];
  snprintf(t, sizeof(t), "%.1f C", g.tempSetpointC);
  lv_label_set_text(s_setpointLabel, t);
}

static void on_temp_minus(lv_event_t *e) {
  (void)e;
  g.tempSetpointC -= UI_TEMP_STEP_C;
  if (g.tempSetpointC < UI_TEMP_MIN_C) g.tempSetpointC = UI_TEMP_MIN_C;
  setpoint_show();
  Serial.printf("[FAKE->WT32] SETPOINT %.1f\n", g.tempSetpointC);
}
static void on_temp_plus(lv_event_t *e) {
  (void)e;
  g.tempSetpointC += UI_TEMP_STEP_C;
  if (g.tempSetpointC > UI_TEMP_MAX_C) g.tempSetpointC = UI_TEMP_MAX_C;
  setpoint_show();
  Serial.printf("[FAKE->WT32] SETPOINT %.1f\n", g.tempSetpointC);
}
static void on_back(lv_event_t *e) {
  (void)e;
  ui_show_home();
}

static HoldCtx s_stirrerCtx;
static HoldCtx s_postCtx;
static HoldCtx s_runCtx;

static lv_obj_t *s_durSpin = nullptr;
static lv_obj_t *s_expSpin = nullptr;
static lv_obj_t *s_dbWarn = nullptr;
static bool s_dbSuggested = false;

static void exp_mirror_link() {
  link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
}

// DB latest -> next suggestion + mismatch warn (backlog #9).
// Called on show, on edit, and from screen_control_poll() in the main loop
// so a late master sync still updates the open screen.
static void db_refresh_warn() {
  if (s_dbWarn == nullptr) return;
  int latest = link_modbus_get_db_latest();
  if (latest == -2) {
    lv_label_set_text(s_dbWarn, "DB: waiting for master sync...");
    return;
  }
  if (!s_dbSuggested) {
    s_dbSuggested = true;
    int next = latest + 1;
    if (next < UI_EXP_NUM_MIN) next = UI_EXP_NUM_MIN;
    if (next > UI_EXP_NUM_MAX) next = UI_EXP_NUM_MAX;
    if (g.expNum != next && s_expSpin != nullptr) {
      g.expNum = next;
      lv_spinbox_set_value(s_expSpin, next);
      exp_mirror_link();
      ui_update_bell();
      Serial.printf("[DB] auto-suggest exp=%d (latest=%d)\n", next, latest);
    }
  }
  if (g.expNum <= latest) {
    char t[96];
    snprintf(t, sizeof(t), "WARN: exp %d already in DB (latest %d). Use %d.",
             g.expNum, latest, latest + 1);
    lv_label_set_text(s_dbWarn, t);
    lv_obj_set_style_text_color(s_dbWarn, lv_color_hex(UI_COL_WARN), LV_PART_MAIN);
    Serial.printf("[DB] mismatch: local=%d latest=%d\n", g.expNum, latest);
  } else {
    char t[64];
    snprintf(t, sizeof(t), "DB latest %d, next %d OK.", latest, latest + 1);
    lv_label_set_text(s_dbWarn, t);
    lv_obj_set_style_text_color(s_dbWarn, lv_color_hex(UI_COL_OK), LV_PART_MAIN);
  }
}

void screen_control_poll() { db_refresh_warn(); }

// RUN switch confirmed ON: each stopped->running edge starts a new
// experiment number. Mirror everything to the link either way.
static void on_run_toggled() {
  if (g.experimentRunning) g.expNum++;
  exp_mirror_link();
}

static void on_dur_changed(lv_event_t *e) {
  (void)e;
  int32_t v = lv_spinbox_get_value(s_durSpin);
  if (v < UI_DURATION_MIN_H) v = UI_DURATION_MIN_H;
  if (v > UI_DURATION_MAX_H) v = UI_DURATION_MAX_H;
  g.durationH = (int)v;
  exp_mirror_link();
  ui_update_bell();
}

static void on_dur_plus(lv_event_t *e) {
  (void)e;
  lv_spinbox_increment(s_durSpin);
}
static void on_dur_minus(lv_event_t *e) {
  (void)e;
  lv_spinbox_decrement(s_durSpin);
}

static void on_exp_changed(lv_event_t *e) {
  (void)e;
  int32_t v = lv_spinbox_get_value(s_expSpin);
  if (v < UI_EXP_NUM_MIN) v = UI_EXP_NUM_MIN;
  if (v > UI_EXP_NUM_MAX) v = UI_EXP_NUM_MAX;
  g.expNum = (int)v;
  exp_mirror_link();
  ui_update_bell();
  db_refresh_warn();
}

static void on_exp_plus(lv_event_t *e) {
  (void)e;
  lv_spinbox_increment(s_expSpin);
}
static void on_exp_minus(lv_event_t *e) {
  (void)e;
  lv_spinbox_decrement(s_expSpin);
}

static lv_obj_t *mk_spin(lv_obj_t *parent, int x, int y, int min, int max,
                          int digits, int value) {
  lv_obj_t *s = lv_spinbox_create(parent);
  lv_obj_set_pos(s, x, y);
  lv_obj_set_size(s, 120, 42);
  lv_spinbox_set_range(s, min, max);
  lv_spinbox_set_digit_format(s, digits, 0);
  lv_spinbox_set_step(s, 1);
  lv_spinbox_set_value(s, value >= min && value <= max ? value : min);
  lv_obj_set_style_bg_color(s, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(s, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_color(s, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_border_width(s, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(s, 4, LV_PART_MAIN);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, LV_PART_MAIN);
  return s;
}

// Poll-request line to the WT32 (Display IO11 -> WT32 IO14). link_modbus owns
// the pin and pulses it from link_modbus_set_credentials(), which is what makes
// a corrected password take effect on the next poll instead of up to 5 s later.
// This button stays so the line can be exercised by hand from the bench.
static lv_obj_t *s_intTestHint = nullptr;

static void on_int_test(lv_event_t *e) {
  (void)e;
  link_notify_pulse();
  Serial.println("[INT] poll request -> WT32 IO14 (watch the WT32 serial)");
  if (s_intTestHint != nullptr) {
    lv_label_set_text(s_intTestHint, "asked WT32 to poll now");
  }
}

void screen_control_show(lv_obj_t *parent) {
  ui_mk_label(parent, "/control (cmds -> WT32, fake)", 16, 8, 500, UI_COL_TEXT,
              &lv_font_montserrat_20);
  lv_obj_t *back = ui_mk_button(parent, "< Home", 648, 4, 140, 36, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);

  ui_mk_label(parent, "Temp setpoint 35.0-40.0C, 0.1 steps:", 16, 52, 420, UI_COL_TEXT,
              &lv_font_montserrat_14);
  lv_obj_t *minus = ui_mk_button(parent, "-", 16, 84, 64, 48, UI_COL_CARD);
  lv_obj_add_event_cb(minus, on_temp_minus, LV_EVENT_CLICKED, nullptr);
  s_setpointLabel = ui_mk_label(parent, "", 96, 92, 200, UI_COL_TEXT, &lv_font_montserrat_20);
  setpoint_show();
  lv_obj_t *plus = ui_mk_button(parent, "+", 300, 84, 64, 48, UI_COL_CARD);
  lv_obj_add_event_cb(plus, on_temp_plus, LV_EVENT_CLICKED, nullptr);
  ui_mk_label(parent, "each step logs [FAKE->WT32] SETPOINT", 390, 96, 380, UI_COL_DIM,
              &lv_font_montserrat_14);

  hold_attach(s_stirrerCtx, parent, 160, "Stirrer (hold 500ms)", &g.stirrerOn,
              "STIRRER");
  hold_attach(s_postCtx, parent, 206, "POST enable (hold 500ms)", &g.postEnabled, "POST");
  s_runCtx.after = on_run_toggled;
  hold_attach(s_runCtx, parent, 252, "Run experiment (hold 500ms)", &g.experimentRunning,
              "RUN");

  ui_mk_label(parent, "Duration:", 16, 316, 100, UI_COL_TEXT,
              &lv_font_montserrat_14);
  s_durSpin = mk_spin(parent, 120, 310, UI_DURATION_MIN_H, UI_DURATION_MAX_H, 2,
                      g.durationH);
  lv_obj_add_event_cb(s_durSpin, on_dur_changed, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_t *dMinus = ui_mk_button(parent, "-", 250, 310, 48, 42, UI_COL_CARD);
  lv_obj_add_event_cb(dMinus, on_dur_minus, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *dPlus = ui_mk_button(parent, "+", 306, 310, 48, 42, UI_COL_CARD);
  lv_obj_add_event_cb(dPlus, on_dur_plus, LV_EVENT_CLICKED, nullptr);
  ui_mk_label(parent, "hours 1-99", 366, 318, 120, UI_COL_DIM,
              &lv_font_montserrat_14);

  ui_mk_label(parent, "Exp number:", 16, 378, 100, UI_COL_TEXT,
              &lv_font_montserrat_14);
  s_expSpin = mk_spin(parent, 120, 372, UI_EXP_NUM_MIN, UI_EXP_NUM_MAX, 4,
                      g.expNum);
  lv_obj_add_event_cb(s_expSpin, on_exp_changed, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_t *eMinus = ui_mk_button(parent, "-", 250, 372, 48, 42, UI_COL_CARD);
  lv_obj_add_event_cb(eMinus, on_exp_minus, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ePlus = ui_mk_button(parent, "+", 306, 372, 48, 42, UI_COL_CARD);
  lv_obj_add_event_cb(ePlus, on_exp_plus, LV_EVENT_CLICKED, nullptr);
  s_dbWarn = ui_mk_label(parent, "DB: waiting for master sync...", 366, 372, 420,
                         UI_COL_DIM, &lv_font_montserrat_14);
  db_refresh_warn();

  ui_mk_label(parent, "INT test (IO11 -> WT32 IO14):", 16, 424, 260, UI_COL_TEXT,
              &lv_font_montserrat_14);
  lv_obj_t *tBtn = ui_mk_button(parent, "PULSE INT", 280, 418, 140, 42, UI_COL_CARD);
  lv_obj_add_event_cb(tBtn, on_int_test, LV_EVENT_CLICKED, nullptr);
  s_intTestHint = ui_mk_label(parent, "", 430, 428, 350, UI_COL_DIM,
                              &lv_font_montserrat_14);
}
