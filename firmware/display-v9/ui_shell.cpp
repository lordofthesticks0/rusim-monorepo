#include "ui_shell.h"

#include "fake_data.h"
#include "screen_control.h"
#include "screen_data.h"
#include "screen_home.h"
#include "screen_info.h"
#include "screen_setup.h"
#include "link_modbus.h"
#include "ui_config.h"

// Layout: status bar 800x44 on top, content 800x436 below.
static lv_obj_t *s_content = nullptr;
static lv_obj_t *s_tabHome = nullptr;
static lv_obj_t *s_tabData = nullptr;
static lv_obj_t *s_tabControl = nullptr;
static lv_obj_t *s_tabInfo = nullptr;
static lv_obj_t *s_bellBtn = nullptr;
static lv_obj_t *s_bellLabel = nullptr;
static lv_obj_t *s_clock = nullptr;

// Modal state.
static lv_obj_t *s_modal = nullptr;
static UiAckFn s_modalAck = nullptr;

// Overheat latches (fake): re-arm only after cooling below the threshold.
static bool s_warn41Shown = false;
static bool s_trip45Shown = false;

// Current route for in-place refresh.
static int s_route = 0;  // 0 home, 1 picker, 2 cluster, 3 bottle, 4 control, 5 info, 6 setup
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
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
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
  // The light theme fills a plain button with UI_COL_CARD (white), so a border
  // is what separates it from the background.
  lv_obj_set_style_border_color(b, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_center(l);
  lv_obj_set_style_text_color(l, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
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
          snprintf(line, sizeof(line), "N%d/C%d %s NULL\n", c, b, SENSOR_NAMES[s]);
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

static void on_tab_clicked(lv_event_t *e) {
  // Locked during network login: nav tabs are disabled and greyed out until
  // the WT32 reports RESULT=success (g.loggedIn). Guard here too so a stray
  // event can never route away from /setup.
  if (!g.loggedIn) return;
  intptr_t id = (intptr_t)lv_event_get_user_data(e);
  switch (id) {
    case 0:
      ui_show_home();
      break;
    case 1:
      ui_show_data_picker();
      break;
    case 2:
      ui_show_control();
      break;
    case 3:
      ui_show_info();
      break;
    default:
      break;
  }
}

// Swipe left/right to switch top-level tabs: home <-> data <-> control <->
// info. Cluster/bottle drill-downs count as the data tab. Stops at the ends
// (no wrap). Ignored during login, on /setup, and while a modal is up.
// LVGL delivers one gesture per press after a 50 px flick, and every widget
// bubbles gestures to the screen, so a single screen-level handler catches
// swipes starting anywhere. Rebuilding the route here also deletes the
// pressed widget before release, so no tap/click fires alongside the swipe.
static void on_swipe_gesture(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_GESTURE_LEFT && code != LV_EVENT_GESTURE_RIGHT) return;
  if (!g.loggedIn) return;
  if (ui_modal_visible()) return;
  if (s_route == 6) return;  // /setup: swipes disabled
  int tab;
  if (s_route >= 1 && s_route <= 3)
    tab = 1;  // picker / cluster / bottle
  else if (s_route == 4)
    tab = 2;  // control
  else if (s_route == 5)
    tab = 3;  // info
  else
    tab = 0;  // home
  if (code == LV_EVENT_GESTURE_LEFT && tab < 3)
    tab++;
  else if (code == LV_EVENT_GESTURE_RIGHT && tab > 0)
    tab--;
  else
    return;  // at the end: stay put
  switch (tab) {
    case 0:
      ui_show_home();
      break;
    case 1:
      ui_show_data_picker();
      break;
    case 2:
      ui_show_control();
      break;
    case 3:
      ui_show_info();
      break;
    default:
      break;
  }
}

static void set_tab_state(lv_obj_t *tab, bool active) {
  if (tab == nullptr) return;
  lv_obj_t *label = lv_obj_get_child(tab, 0);
  if (active) {
    lv_obj_set_style_bg_color(tab, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, LV_PART_MAIN);
    if (label != nullptr)
      lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_color(tab, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, LV_PART_MAIN);
    if (label != nullptr)
      lv_obj_set_style_text_color(label, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  }
}

// Greyed-out locked look used before login. Disabled state blocks input;
// explicit grey fill + dim label signals it (no theme is enabled).
static void set_tab_locked(lv_obj_t *tab, bool locked) {
  if (tab == nullptr) return;
  if (locked) {
    lv_obj_add_state(tab, LV_STATE_DISABLED);
  } else {
    lv_obj_remove_state(tab, LV_STATE_DISABLED);
  }
  if (locked) {
    lv_obj_set_style_bg_color(tab, lv_color_hex(UI_COL_CARD_OFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(tab, lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
    lv_obj_t *label = lv_obj_get_child(tab, 0);
    if (label != nullptr)
      lv_obj_set_style_text_color(label, lv_color_hex(UI_COL_DIM), LV_PART_MAIN);
  }
}

static void build_status_bar(lv_obj_t *scr) {
  lv_obj_t *bar = lv_obj_create(scr);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, 800, 44);
  style_plain(bar, UI_COL_BAR);
  lv_obj_set_scrollable(bar, false);

  // Square 32x32 icon tabs. Left cluster (home/data/control), clock centred,
  // right cluster (info/bell).
  s_tabHome = ui_mk_button(bar, LV_SYMBOL_HOME, 6, 6, 32, 32, UI_COL_CARD);
  lv_obj_add_event_cb(s_tabHome, on_tab_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)0);
  s_tabData = ui_mk_button(bar, LV_SYMBOL_LIST, 46, 6, 32, 32, UI_COL_CARD);
  lv_obj_add_event_cb(s_tabData, on_tab_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)1);
  s_tabControl = ui_mk_button(bar, LV_SYMBOL_SETTINGS, 86, 6, 32, 32, UI_COL_CARD);
  lv_obj_add_event_cb(s_tabControl, on_tab_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)2);
  s_clock = ui_mk_label(bar, "--:--", 300, 10, 200, UI_COL_TEXT, &lv_font_montserrat_20);
  // center text
  lv_obj_set_style_text_align(s_clock, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_tabInfo = ui_mk_button(bar, LV_SYMBOL_FILE, 722, 6, 32, 32, UI_COL_CARD);
  lv_obj_add_event_cb(s_tabInfo, on_tab_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)3);

  s_bellBtn = ui_mk_button(bar, LV_SYMBOL_BELL, 762, 6, 32, 32, UI_COL_CARD);
  s_bellLabel = lv_obj_get_child(s_bellBtn, 0);
  lv_label_set_text(s_bellLabel, LV_SYMBOL_BELL);
  lv_obj_set_style_text_color(s_bellLabel, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  lv_obj_add_event_cb(s_bellBtn, on_bell_clicked, LV_EVENT_CLICKED, nullptr);
}

void ui_update_bell() {
  if (s_bellBtn == nullptr) return;
  int n = fake_null_count();
  if (s_bellLabel == nullptr) s_bellLabel = lv_obj_get_child(s_bellBtn, 0);
  if (n > 0) {
    lv_label_set_text(s_bellLabel, LV_SYMBOL_WARNING);
    lv_obj_set_style_bg_color(s_bellBtn, lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bellBtn, 0, LV_PART_MAIN);
    if (s_bellLabel != nullptr)
      lv_obj_set_style_text_color(s_bellLabel, lv_color_white(), LV_PART_MAIN);
  } else {
    lv_label_set_text(s_bellLabel, LV_SYMBOL_BELL);
    lv_obj_set_style_bg_color(s_bellBtn, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_bellBtn, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bellBtn, 1, LV_PART_MAIN);
    if (s_bellLabel != nullptr)
      lv_obj_set_style_text_color(s_bellLabel, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  }
}

void ui_update_nav() {
  // Before login the nav tabs are unusable: grey them out and disable input.
  // Active-route highlight only applies once logged in.
  if (!g.loggedIn) {
    set_tab_locked(s_tabHome, true);
    set_tab_locked(s_tabData, true);
    set_tab_locked(s_tabControl, true);
    set_tab_locked(s_tabInfo, true);
    return;
  }
  set_tab_locked(s_tabHome, false);
  set_tab_locked(s_tabData, false);
  set_tab_locked(s_tabControl, false);
  set_tab_locked(s_tabInfo, false);
  bool homeActive = (s_route == 0);
  bool dataActive = (s_route >= 1 && s_route <= 3);
  bool controlActive = (s_route == 4);
  bool infoActive = (s_route == 5);
  set_tab_state(s_tabHome, homeActive);
  set_tab_state(s_tabData, dataActive);
  set_tab_state(s_tabControl, controlActive);
  set_tab_state(s_tabInfo, infoActive);
}

void ui_update_clock() {
  if (s_clock != nullptr) lv_label_set_text(s_clock, "--:--");
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
  lv_obj_set_scrollable(s_modal, false);

  lv_obj_t *card = ui_mk_card(s_modal, 150, 70, 500, 340);
  ui_mk_label(card, title, 12, 8, 460, UI_COL_TEXT, &lv_font_montserrat_20);
  ui_mk_label(card, body, 12, 48, 460, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_t *ack = ui_mk_button(card, ackLabel, 150, 250, 180, 48, UI_COL_ACCENT);
  // The ack button is filled with the darker UI_COL_ACCENT, so it keeps a light
  // label instead of the dark one ui_mk_button gives a white button.
  lv_obj_set_style_text_color(lv_obj_get_child(ack, 0), lv_color_white(), LV_PART_MAIN);
  s_modalAck = onAck;
  lv_obj_add_event_cb(ack, on_modal_ack, LV_EVENT_CLICKED, nullptr);
}

void ui_modal_hide() {
  if (s_modal != nullptr) {
    lv_obj_delete(s_modal);
    s_modal = nullptr;
    s_modalAck = nullptr;
  }
}

bool ui_modal_visible() { return s_modal != nullptr; }

// --- router ---

static void render_current() {
  if (s_content == nullptr) return;
  // Per-route teardown before lv_obj_clean() frees the widgets. Without this
  // the /control statics (s_dbWarn, hold bars) dangle and the poll/timer
  // callbacks spam "No screen found" on the deleted objects.
  screen_control_hide();
  screen_home_hide();
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
    case 6:
      screen_setup_show(s_content);
      break;
    default:
      screen_home_show(s_content);
      break;
  }
  ui_update_nav();
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
void ui_show_setup() {
  s_route = 6;
  render_current();
}
bool ui_is_setup() { return s_route == 6; }
bool ui_is_control() { return s_route == 4; }
bool ui_is_info() { return s_route == 5; }

void ui_refresh_current() {
  if (!g.loggedIn) return;  // setup still up
  if (s_route == 6) return;          // never wipe typed login text
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
  if (!g.loggedIn) return;
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

void ui_experiment_toggle() {
  if (g.experimentRunning) {
    g.experimentRunning = false;
    g.experimentStartMs = 0;
  } else {
    g.experimentRunning = true;
    g.experimentStartMs = millis();
    g.expNum++;
  }
  link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  Serial.printf("[FAKE->WT32] RUN %s\n", g.experimentRunning ? "ON" : "OFF");
  ui_update_bell();
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

  // Swipe navigation. Registered for the direction codes (not the generic
  // GESTURE, which would double-fire since the indev sends both).
  lv_obj_add_event_cb(scr, on_swipe_gesture, LV_EVENT_GESTURE_LEFT, nullptr);
  lv_obj_add_event_cb(scr, on_swipe_gesture, LV_EVENT_GESTURE_RIGHT, nullptr);

  build_status_bar(scr);

  s_content = lv_obj_create(scr);
  lv_obj_set_pos(s_content, 0, 44);
  lv_obj_set_size(s_content, 800, 436);
  style_plain(s_content, UI_COL_BG);

  ui_show_setup();  // first route until the WT32 reports RESULT=success
  ui_update_bell();
  ui_update_nav();
  ui_update_clock();
  lv_timer_create(on_push_timer, UI_PUSH_PERIOD_MS, nullptr);
}
