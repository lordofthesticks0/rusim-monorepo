#include "ui_shell.h"

#include "fake_data.h"
#include "screen_boot.h"
#include "screen_control.h"
#include "screen_data.h"
#include "screen_home.h"
#include "screen_info.h"
#include "ui_config.h"

// Layout: status bar 800x44 on top, content 800x436 below.
static lv_obj_t *s_content = nullptr;
static lv_obj_t *s_bellBtn = nullptr;
static lv_obj_t *s_bellLabel = nullptr;
static lv_obj_t *s_postLabel = nullptr;
static lv_obj_t *s_expLabel = nullptr;

// Modal state.
static lv_obj_t *s_modal = nullptr;
static UiAckFn s_modalAck = nullptr;

// Overheat latches (fake): re-arm only after cooling below the threshold.
static bool s_warn41Shown = false;
static bool s_trip45Shown = false;

// Current route for in-place refresh.
static int s_route = 0;  // 0 home, 1 picker, 2 cluster, 3 bottle, 4 control, 5 info
static int s_argC = 0;
static int s_argB = 0;

static void style_plain(lv_obj_t *o, uint32_t bg) {
  lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
}

lv_obj_t *ui_mk_label(lv_obj_t *parent, const char *text, int x, int y, int w,
                      uint32_t color, const lv_font_t *font) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_size(l, w, LV_SIZE_CONTENT);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
  if (font != nullptr) lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  return l;
}

lv_obj_t *ui_mk_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       uint32_t bg) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_center(l);
  lv_obj_set_style_text_color(l, lv_color_white(), LV_PART_MAIN);
  return b;
}

lv_obj_t *ui_mk_card(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(c, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 8, LV_PART_MAIN);
  return c;
}

// --- status bar ---

static void on_bell_clicked(lv_event_t *e) {
  (void)e;
  // Live list of currently-invalid fields. No history (locked decision).
  static char body[1024];
  int n = 0;
  body[0] = '\0';
  int shown = 0;
  for (int c = 0; c < UI_CLUSTERS && shown < 10; c++) {
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER && shown < 10; b++) {
      for (int s = 0; s < S_COUNT && shown < 10; s++) {
        if (g.cluster[c].bottle[b].sensorNull[s]) {
          char line[64];
          snprintf(line, sizeof(line), "C%d/B%d %s NULL\n", c, b, SENSOR_NAMES[s]);
          strncat(body, line, sizeof(body) - strlen(body) - 1);
          shown++;
          n++;
        }
      }
    }
  }
  int total = fake_null_count();
  if (total == 0) {
    snprintf(body, sizeof(body), "All fields valid this cycle (fake).");
  } else if (total > shown) {
    char more[48];
    snprintf(more, sizeof(more), "... +%d more (fake)", total - shown);
    strncat(body, more, sizeof(body) - strlen(body) - 1);
  }
  ui_modal_show("Invalid fields (live, fake)", body, "Close", nullptr);
}

static void build_status_bar(lv_obj_t *scr) {
  lv_obj_t *bar = lv_obj_create(scr);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, 800, 44);
  style_plain(bar, 0x101828);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  ui_mk_label(bar, "NET OK(fake)", 10, 12, 150, UI_COL_OK, &lv_font_montserrat_14);
  s_postLabel = ui_mk_label(bar, "POST ON", 170, 12, 110, UI_COL_WHITE, &lv_font_montserrat_14);
  s_expLabel =
      ui_mk_label(bar, "exp --h (fake)", 290, 12, 220, UI_COL_DIM, &lv_font_montserrat_14);

  s_bellBtn = ui_mk_button(bar, "", 690, 6, 100, 32, UI_COL_DIM);
  s_bellLabel = lv_label_create(s_bellBtn);
  lv_label_set_text(s_bellLabel, "OK");
  lv_obj_center(s_bellLabel);
  lv_obj_set_style_text_color(s_bellLabel, lv_color_white(), LV_PART_MAIN);
  lv_obj_add_event_cb(s_bellBtn, on_bell_clicked, LV_EVENT_CLICKED, nullptr);
}

void ui_update_bell() {
  if (s_bellBtn == nullptr) return;
  int n = fake_null_count();
  char exp[48];
  if (g.experimentRunning) {
    snprintf(exp, sizeof(exp), "exp %dh (fake)", g.durationH);
  } else {
    snprintf(exp, sizeof(exp), "exp --h (fake)");
  }
  lv_label_set_text(s_expLabel, exp);
  lv_label_set_text(s_postLabel, g.postEnabled ? "POST ON" : "POST OFF");
  if (n > 0) {
    char t[32];
    snprintf(t, sizeof(t), "ALERT(%d)", n);
    lv_label_set_text(s_bellLabel, t);
    lv_obj_set_style_bg_color(s_bellBtn, lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
  } else {
    lv_label_set_text(s_bellLabel, "OK");
    lv_obj_set_style_bg_color(s_bellBtn, lv_color_hex(UI_COL_OK), LV_PART_MAIN);
  }
}

// --- modal ---

static void on_modal_ack(lv_event_t *e) {
  (void)e;
  UiAckFn fn = s_modalAck;
  ui_modal_hide();
  if (fn != nullptr) fn();
}

void ui_modal_show(const char *title, const char *body, const char *ackLabel, UiAckFn onAck) {
  ui_modal_hide();
  lv_obj_t *scr = lv_screen_active();
  s_modal = lv_obj_create(scr);
  lv_obj_set_pos(s_modal, 0, 0);
  lv_obj_set_size(s_modal, 800, 480);
  lv_obj_set_style_bg_color(s_modal, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_modal, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_modal, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(s_modal, 0, LV_PART_MAIN);
  lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *card = ui_mk_card(s_modal, 150, 70, 500, 340);
  ui_mk_label(card, title, 12, 8, 460, UI_COL_WHITE, &lv_font_montserrat_20);
  ui_mk_label(card, body, 12, 48, 460, UI_COL_WHITE, &lv_font_montserrat_14);
  lv_obj_t *ack = ui_mk_button(card, ackLabel, 150, 250, 180, 48, UI_COL_ACCENT);
  s_modalAck = onAck;
  lv_obj_add_event_cb(ack, on_modal_ack, LV_EVENT_CLICKED, nullptr);
}

void ui_modal_hide() {
  if (s_modal != nullptr) {
    lv_obj_del(s_modal);
    s_modal = nullptr;
    s_modalAck = nullptr;
  }
}

bool ui_modal_visible() { return s_modal != nullptr; }

// --- router ---

static void render_current() {
  if (s_content == nullptr) return;
  lv_obj_clean(s_content);
  switch (s_route) {
    case 1:
      screen_data_picker_show(s_content);
      break;
    case 2:
      screen_cluster_show(s_content, s_argC);
      break;
    case 3:
      screen_bottle_show(s_content, s_argC, s_argB);
      break;
    case 4:
      screen_control_show(s_content);
      break;
    case 5:
      screen_info_show(s_content);
      break;
    default:
      screen_home_show(s_content);
      break;
  }
}

void ui_show_home() {
  s_route = 0;
  render_current();
}
void ui_show_data_picker() {
  s_route = 1;
  render_current();
}
void ui_show_cluster(int c) {
  s_route = 2;
  s_argC = c;
  render_current();
}
void ui_show_bottle(int c, int b) {
  s_route = 3;
  s_argC = c;
  s_argB = b;
  render_current();
}
void ui_show_control() {
  s_route = 4;
  render_current();
}
void ui_show_info() {
  s_route = 5;
  render_current();
}

void ui_refresh_current() {
  if (!g.experimentRunning) return;  // boot gate still up
  render_current();
  ui_update_bell();
}

void ui_shell_on_data() {
  ui_update_bell();
  ui_refresh_current();
}

static void on_warn41_ack() {}
static void on_trip45_ack() {}

void ui_eval_overheat() {
  if (!g.experimentRunning) return;
  float t = g.chamberTempC;
  if (t >= UI_OVERHEAT_TRIP_C) {
    if (!s_trip45Shown) {
      s_trip45Shown = true;
      g.stirrerOn = false;  // fake: breaker cut everything (display stays for demo)
      ui_modal_show("45C BREAKER TRIP (fake)",
                    "Chamber >= 45C.\nAll power cut (fake).\nStirrer forced OFF.\nSet TEMP < 45 via serial to re-arm.",
                    "Acknowledge", on_trip45_ack);
      ui_refresh_current();
    }
    return;
  }
  s_trip45Shown = false;
  if (t >= UI_OVERHEAT_WARN_C) {
    if (!s_warn41Shown) {
      s_warn41Shown = true;
      ui_modal_show("41C WARNING (fake)",
                    "Chamber >= 41C.\nHeater keeps running (fake).\nSet TEMP < 41 via serial to re-arm.",
                    "Acknowledge", on_warn41_ack);
    }
    return;
  }
  s_warn41Shown = false;
}

// --- init ---

static void on_push_timer(lv_timer_t *t) {
  (void)t;
  fake_wt32_push();
  ui_shell_on_data();
}

void ui_init() {
  lv_obj_t *scr = lv_screen_active();
  style_plain(scr, UI_COL_BG);

  build_status_bar(scr);

  s_content = lv_obj_create(scr);
  lv_obj_set_pos(s_content, 0, 44);
  lv_obj_set_size(s_content, 800, 436);
  style_plain(s_content, UI_COL_BG);

  ui_show_home();
  ui_update_bell();
  screen_boot_show_gate();  // blocking gate on top until confirmed
  lv_timer_create(on_push_timer, UI_PUSH_PERIOD_MS, nullptr);
}
