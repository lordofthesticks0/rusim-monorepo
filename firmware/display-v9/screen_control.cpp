#include "screen_control.h"

#include "fake_data.h"
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
    lv_timer_del(ctx->timer);
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
    bool nowOn = lv_obj_has_state(ctx->sw, LV_STATE_CHECKED);
    uint32_t el = millis() - ctx->t0;
    if (ctx->armed && el >= UI_HOLD_TO_CONFIRM_MS) {
      *(ctx->flag) = nowOn;
      Serial.printf("[FAKE->WT32] %s %s\n", ctx->wt32cmd, nowOn ? "ON" : "OFF");
      lv_label_set_text(ctx->hint, "sent to WT32 (fake).");
      hold_reset(ctx);
      ui_update_bell();
    } else {
      // Too short: revert to committed state.
      ctx->reverting = true;
      if (*(ctx->flag)) {
        lv_obj_add_state(ctx->sw, LV_STATE_CHECKED);
      } else {
        lv_obj_remove_state(ctx->sw, LV_STATE_CHECKED);
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
  ui_mk_label(parent, label, 16, y, 200, UI_COL_WHITE, &lv_font_montserrat_14);
  ctx.sw = lv_switch_create(parent);
  lv_obj_set_pos(ctx.sw, 220, y - 4);
  lv_obj_set_size(ctx.sw, 72, 36);
  if (*flag) lv_obj_add_state(ctx.sw, LV_STATE_CHECKED);
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

void screen_control_show(lv_obj_t *parent) {
  ui_mk_label(parent, "/control (cmds -> WT32, fake)", 16, 8, 500, UI_COL_WHITE,
              &lv_font_montserrat_20);

  ui_mk_label(parent, "Temp setpoint 35.0-40.0C, 0.1 steps:", 16, 56, 420, UI_COL_WHITE,
              &lv_font_montserrat_14);
  lv_obj_t *minus = ui_mk_button(parent, "-", 16, 88, 64, 56, UI_COL_CARD);
  lv_obj_add_event_cb(minus, on_temp_minus, LV_EVENT_CLICKED, nullptr);
  s_setpointLabel = ui_mk_label(parent, "", 96, 96, 200, UI_COL_WHITE, &lv_font_montserrat_20);
  setpoint_show();
  lv_obj_t *plus = ui_mk_button(parent, "+", 300, 88, 64, 56, UI_COL_CARD);
  lv_obj_add_event_cb(plus, on_temp_plus, LV_EVENT_CLICKED, nullptr);
  ui_mk_label(parent, "each step logs [FAKE->WT32] SETPOINT", 390, 100, 380, UI_COL_DIM,
              &lv_font_montserrat_14);

  hold_attach(s_stirrerCtx, parent, 180, "Stirrer (hold 500ms)", &g.stirrerOn,
              "STIRRER");
  hold_attach(s_postCtx, parent, 240, "POST enable (hold 500ms)", &g.postEnabled, "POST");

  char ch[64];
  snprintf(ch, sizeof(ch), "Chamber now %.1fC (fake, set via serial TEMP x)", g.chamberTempC);
  ui_mk_label(parent, ch, 16, 300, 700, UI_COL_DIM, &lv_font_montserrat_14);

  lv_obj_t *back = ui_mk_button(parent, "< Home", 16, 360, 140, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);
}
