#include "ui_hold_button.h"

#include <Arduino.h>
#include <new>

#include "ui_config.h"

struct HoldBtnCtx {
  lv_obj_t *btn = nullptr;
  lv_obj_t *ring = nullptr;
  lv_obj_t *halo = nullptr;
  lv_obj_t *label = nullptr;
  lv_timer_t *timer = nullptr;
  uint32_t start = 0;
  bool state = false;
  bool completed = false;
  UiHoldFn onCommit = nullptr;
  void *user = nullptr;
};

// The progress ring lives in a wrap the same size as the ring itself, so it
// is never clipped by the button bounds (an oversized child gets cut to the
// parent's rectangle, which read as a square). At rest both halo and ring
// are hidden, so nothing is painted behind the button. While holding, a
// slightly larger solid circle (halo) shows behind the button as the
// circular backdrop and the blue arc sweeps over its rim.
static void ring_hide(HoldBtnCtx *ctx) {
  if (ctx == nullptr) return;
  if (ctx->timer != nullptr) {
    lv_timer_delete(ctx->timer);
    ctx->timer = nullptr;
  }
  if (ctx->ring != nullptr) {
    lv_arc_set_value(ctx->ring, 0);
    lv_obj_set_hidden(ctx->ring, true);
  }
  if (ctx->halo != nullptr) lv_obj_set_hidden(ctx->halo, true);
}

static void ring_show(HoldBtnCtx *ctx) {
  if (ctx == nullptr) return;
  if (ctx->halo != nullptr) lv_obj_set_hidden(ctx->halo, false);
  if (ctx->ring != nullptr) {
    lv_arc_set_value(ctx->ring, 0);
    lv_obj_set_hidden(ctx->ring, false);
  }
}

static void hold_commit(HoldBtnCtx *ctx) {
  if (ctx == nullptr || ctx->completed) return;
  ctx->completed = true;
  ctx->state = !ctx->state;
  if (ctx->btn != nullptr) {
    lv_obj_set_style_bg_color(ctx->btn, lv_color_hex(ctx->state ? UI_COL_OK : UI_COL_ALARM),
                              LV_PART_MAIN);
  }
  if (ctx->timer != nullptr) {
    lv_timer_delete(ctx->timer);
    ctx->timer = nullptr;
  }
  if (ctx->ring != nullptr) lv_arc_set_value(ctx->ring, 100);
  if (ctx->onCommit != nullptr) ctx->onCommit(ctx->user);
}

static void hold_tick(lv_timer_t *t) {
  HoldBtnCtx *ctx = (HoldBtnCtx *)lv_timer_get_user_data(t);
  if (ctx == nullptr) return;
  uint32_t elapsed = lv_tick_elaps(ctx->start);
  if (elapsed >= UI_HOLD_TO_CONFIRM_MS) {
    if (ctx->ring != nullptr) lv_arc_set_value(ctx->ring, 100);
    hold_commit(ctx);
    return;
  }
  if (ctx->ring != nullptr) {
    lv_arc_set_value(ctx->ring, (int32_t)((uint64_t)elapsed * 100 / UI_HOLD_TO_CONFIRM_MS));
  }
}

static void on_pressed(lv_event_t *e) {
  HoldBtnCtx *ctx = (HoldBtnCtx *)lv_event_get_user_data(e);
  if (ctx == nullptr) return;
  if (ctx->timer != nullptr) ring_hide(ctx);
  ctx->completed = false;
  ctx->start = lv_tick_get();
  ring_show(ctx);
  ctx->timer = lv_timer_create(hold_tick, 20, ctx);
}

static void on_released(lv_event_t *e) {
  HoldBtnCtx *ctx = (HoldBtnCtx *)lv_event_get_user_data(e);
  if (ctx == nullptr) return;
  if (ctx->timer != nullptr) {
    // Released early: cancel, state unchanged, callback not called.
    ring_hide(ctx);
  } else if (ctx->completed) {
    // Already fired from hold_tick the moment the duration elapsed;
    // release only clears the ring.
    ctx->completed = false;
    ring_hide(ctx);
  }
}

static void on_delete(lv_event_t *e) {
  HoldBtnCtx *ctx = (HoldBtnCtx *)lv_event_get_user_data(e);
  if (ctx == nullptr) return;
  if (ctx->timer != nullptr) {
    lv_timer_delete(ctx->timer);
    ctx->timer = nullptr;
  }
  lv_obj_set_user_data((lv_obj_t *)lv_event_get_target(e), nullptr);
  delete ctx;
}

lv_obj_t *ui_hold_button_create(lv_obj_t *parent, int x, int y, int size,
                                const char *symbol, bool initial,
                                UiHoldFn onCommit, void *user) {
  // Wrap is exactly the ring diameter so the arc is never clipped by the
  // button bounds (an oversized child gets cut to the parent's rectangle,
  // which read as a square). Wrap center == button center.
  const int pad = 8;
  const int wrapSize = size + pad * 2;
  lv_obj_t *wrap = lv_obj_create(parent);
  lv_obj_set_pos(wrap, x - pad, y - pad);
  lv_obj_set_size(wrap, wrapSize, wrapSize);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(wrap, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(wrap, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(wrap, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_scrollable(wrap, false);
  lv_obj_set_clickable(wrap, false);

  // Slightly larger solid circle behind the button: the circular backdrop
  // the ring sweeps over. Hidden at rest.
  lv_obj_t *halo = lv_obj_create(wrap);
  lv_obj_set_size(halo, wrapSize, wrapSize);
  lv_obj_center(halo);
  lv_obj_set_style_bg_color(halo, lv_color_hex(UI_COL_BAR), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(halo, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(halo, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_scrollable(halo, false);
  lv_obj_set_clickable(halo, false);
  lv_obj_set_hidden(halo, true);

  // Progress ring: same center, same diameter, a sibling of the button (not
  // a child) so its rim is never cut to the button rectangle.
  lv_obj_t *ring = lv_arc_create(wrap);
  lv_obj_set_size(ring, wrapSize, wrapSize);
  lv_obj_center(ring);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(ring, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(ring, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
  lv_obj_set_clickable(ring, false);
  lv_arc_set_range(ring, 0, 100);
  lv_arc_set_value(ring, 0);
  // Track is transparent: the halo disc is the backdrop. Only the blue
  // sweep is drawn.
  lv_obj_set_style_arc_width(ring, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ring, lv_color_hex(UI_COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(ring, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_hidden(ring, true);

  lv_obj_t *btn = lv_button_create(wrap);
  lv_obj_set_size(btn, size, size);
  lv_obj_center(btn);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, initial ? lv_color_hex(UI_COL_OK) : lv_color_hex(UI_COL_ALARM),
                           LV_PART_MAIN);

  HoldBtnCtx *ctx = new HoldBtnCtx();
  ctx->btn = btn;
  ctx->ring = ring;
  ctx->halo = halo;
  ctx->state = initial;
  ctx->completed = false;
  ctx->onCommit = onCommit;
  ctx->user = user;

  ctx->label = lv_label_create(btn);
  lv_label_set_text(ctx->label, symbol != nullptr ? symbol : "");
  lv_obj_set_style_text_color(ctx->label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(ctx->label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(ctx->label);

  // (halo + ring already built above as siblings of btn inside wrap.)

  lv_obj_set_user_data(btn, ctx);

  lv_obj_add_event_cb(btn, on_pressed, LV_EVENT_PRESSED, ctx);
  lv_obj_add_event_cb(btn, on_released, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(btn, on_released, LV_EVENT_PRESS_LOST, ctx);
  lv_obj_add_event_cb(btn, on_delete, LV_EVENT_DELETE, ctx);

  return btn;
}

void ui_hold_button_set_state(lv_obj_t *btn, bool on) {
  if (btn == nullptr) return;
  HoldBtnCtx *ctx = (HoldBtnCtx *)lv_obj_get_user_data(btn);
  if (ctx != nullptr) ctx->state = on;
  lv_obj_set_style_bg_color(btn, on ? lv_color_hex(UI_COL_OK) : lv_color_hex(UI_COL_ALARM),
                           LV_PART_MAIN);
}

void ui_hold_button_set_symbol(lv_obj_t *btn, const char *symbol) {
  if (btn == nullptr) return;
  HoldBtnCtx *ctx = (HoldBtnCtx *)lv_obj_get_user_data(btn);
  if (ctx == nullptr || ctx->label == nullptr) return;
  lv_label_set_text(ctx->label, symbol != nullptr ? symbol : "");
}

bool ui_hold_button_state(lv_obj_t *btn) {
  if (btn == nullptr) return false;
  HoldBtnCtx *ctx = (HoldBtnCtx *)lv_obj_get_user_data(btn);
  return ctx != nullptr ? ctx->state : false;
}