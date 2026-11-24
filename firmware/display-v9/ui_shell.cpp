#include "ui_shell.h"

#include <string.h>

#include "fake_data.h"
#include "link_serial.h"
#include "screen_control.h"
#include "screen_data.h"
#include "screen_home.h"
#include "screen_info.h"
#include "screen_setup.h"
#include "screen_sleep.h"
#include "ui_config.h"

// Layout: status bar 800 x UI_BAR_H on top, content 800 x UI_CONTENT_H below.
static lv_obj_t *s_content = nullptr;
static lv_obj_t *s_tabs[4] = {nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *s_bellBtn = nullptr;
static lv_obj_t *s_bellLabel = nullptr;
static lv_obj_t *s_chip = nullptr;
static lv_obj_t *s_chipDot = nullptr;
static lv_obj_t *s_chipLabel = nullptr;
static bool s_chipOpensSetup = false;

// Modal state.
static lv_obj_t *s_modal = nullptr;
static UiAckFn s_modalAck = nullptr;
static UiSeverity s_modalSev = UI_SEV_INFO;

// Overheat latches: re-arm only after cooling below the threshold.
static bool s_warn41Shown = false;
static bool s_trip45Shown = false;

enum Route { R_HOME = 0, R_PICKER, R_CLUSTER, R_BOTTLE, R_CONTROL, R_INFO, R_SETUP };
static int s_route = R_HOME;
static int s_argC = 0;
static int s_argB = 0;
// Route open before /setup, restored after a re-login.
static int s_prevRoute = R_HOME;
static int s_prevC = 0;
static int s_prevB = 0;

// --- widget helpers --------------------------------------------------------

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
  lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
  // Pressed: a flat colour change, no animation. White buttons darken to the
  // bar grey; filled buttons darken by 25 %.
  lv_color_t pressed = (bg == UI_COL_CARD) ? lv_color_hex(UI_COL_BAR)
                                           : lv_color_darken(lv_color_hex(bg), LV_OPA_20);
  lv_obj_set_style_bg_color(b, pressed, LV_STATE_PRESSED);
  // A white button needs an outline to read as tappable on a white card; the
  // accent outline is reserved for tappable things (cards use a grey one).
  const bool white = (bg == UI_COL_CARD);
  lv_obj_set_style_border_width(b, white ? 1 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_color(b, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_text_color(b, white ? lv_color_hex(UI_COL_ACCENT) : lv_color_white(),
                              LV_PART_MAIN);
  // Disabled: grey fill, grey outline, faint text.
  lv_obj_set_style_bg_color(b, lv_color_hex(UI_COL_CARD_OFF), LV_STATE_DISABLED);
  lv_obj_set_style_border_color(b, lv_color_hex(UI_COL_LINE), LV_STATE_DISABLED);
  lv_obj_set_style_text_color(b, lv_color_hex(UI_COL_DISABLED),
                              LV_STATE_DISABLED);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_center(l);
  return b;
}

lv_obj_t *ui_mk_card(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(c, lv_color_hex(UI_COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 12, LV_PART_MAIN);
  lv_obj_set_scrollable(c, false);
  return c;
}

lv_obj_t *ui_mk_caption(lv_obj_t *parent, const char *text, int x, int y, int w) {
  return ui_mk_label(parent, text, x, y, w, UI_COL_DIM, &lv_font_montserrat_14);
}

lv_obj_t *ui_mk_page_title(lv_obj_t *parent, const char *title, lv_event_cb_t backCb,
                           void *user) {
  int x = UI_MARGIN;
  if (backCb != nullptr) {
    lv_obj_t *back = ui_mk_button(parent, LV_SYMBOL_LEFT, UI_MARGIN, 6, 56, 40, UI_COL_CARD);
    lv_obj_set_ext_click_area(back, 6);
    lv_obj_add_event_cb(back, backCb, LV_EVENT_CLICKED, user);
    x = UI_MARGIN + 56 + 12;
  }
  lv_obj_t *l = ui_mk_label(parent, title, x, 15, 300, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_width(l, LV_SIZE_CONTENT);
  return l;
}

static uint16_t s_stepRepeats = 0;

static void on_stepper_pressed(lv_event_t *e) {
  (void)e;
  s_stepRepeats = 0;
}

void ui_stepper_events(lv_obj_t *btn, lv_event_cb_t cb, void *user) {
  lv_obj_set_ext_click_area(btn, 4);
  lv_obj_add_event_cb(btn, on_stepper_pressed, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_SHORT_CLICKED, user);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_LONG_PRESSED, user);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_LONG_PRESSED_REPEAT, user);
}

int ui_stepper_mult(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED_REPEAT) return 1;
  if (s_stepRepeats < 0xFFFF) s_stepRepeats++;
  return s_stepRepeats > 20 ? 10 : 1;
}

// --- experiment helpers ----------------------------------------------------

int ui_run_duration_h() {
  if (g.experimentRunning) {
    const int a = link_serial_get_db_active_exp();
    const int d = link_serial_get_db_active_dur();
    if (a >= 0 && d > 0) return d;
    return g.durationH;
  }
  const int pd = link_serial_get_db_pending_dur();
  if (pd > 0 && link_serial_get_db_pending_exp() == g.expNum &&
      g.expNum > link_serial_get_db_latest())
    return pd;
  return g.durationH;
}

uint32_t ui_exp_status_text(char *out, size_t n) {
  if (g.experimentRunning) {
    const int aexp = link_serial_get_db_active_exp();
    if (aexp < 0) {
      snprintf(out, n, "Running: configuration not reported yet");
      return UI_COL_WARN;
    }
    const int src = link_serial_get_db_active_src();
    snprintf(out, n, "Running exp %d: %d h, %s", aexp, link_serial_get_db_active_dur(),
             src == 1 ? "from DB" : src == 2 ? "from display" : "source unknown");
    return UI_COL_OK;
  }
  if (!link_serial_db_ready()) {
    snprintf(out, n, "Start blocked: DB configuration not loaded");
    return UI_COL_ALARM;
  }
  const int latest = link_serial_get_db_latest();
  if (latest == -2) {
    snprintf(out, n, "Waiting for DB sync");
    return UI_COL_DIM;
  }
  if (g.expNum <= latest) {
    snprintf(out, n, "Exp %d has readings; next free is %d", g.expNum, latest + 1);
    return UI_COL_WARN;
  }
  const int pendingDur = link_serial_get_db_pending_dur();
  if (pendingDur > 0 && link_serial_get_db_pending_exp() == g.expNum) {
    snprintf(out, n, "Exp %d: %d h from DB", g.expNum, pendingDur);
  } else {
    snprintf(out, n, "Exp %d: %d h from display", g.expNum, g.durationH);
  }
  return UI_COL_OK;
}

void ui_exp_set_user(int expNum) {
  if (expNum < UI_EXP_NUM_MIN) expNum = UI_EXP_NUM_MIN;
  if (expNum > UI_EXP_NUM_MAX) expNum = UI_EXP_NUM_MAX;
  g.expNum = expNum;
  g.expNumUserSet = true;
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
}

// Suggests the next free experiment id (max(latest, configured) + 1) while
// idle. Runs from ui_poll() so it does not depend on which route is open.
// It applies only when the DB answer changes and never after the operator
// picked a number, so a deliberate choice (e.g. 0 = testing) sticks.
static void exp_suggest_poll() {
  static int s_lastBase = -1000;
  if (g.experimentRunning || g.expNumUserSet) return;
  if (!link_serial_db_ready()) return;
  const int latest = link_serial_get_db_latest();
  if (latest == -2) return;
  const int cfgMax = link_serial_get_db_cfg_max();
  const int base = latest > cfgMax ? latest : cfgMax;
  if (base == s_lastBase) return;
  s_lastBase = base;
  int next = base + 1;
  if (next < UI_EXP_NUM_MIN) next = UI_EXP_NUM_MIN;
  if (next > UI_EXP_NUM_MAX) next = UI_EXP_NUM_MAX;
  if (g.expNum == next) return;
  g.expNum = next;
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  Serial.printf("[DB] suggest exp=%d (latest=%d cfgmax=%d)\n", next, latest, cfgMax);
  ui_refresh_current();
}

// After a display reboot the WT32 may be mid-run. The link holds EXPERIMENT
// back until the first DB line; an active run reported there (aexp) is
// adopted instead of being overwritten with "run=0" (audit finding 2.2).
static void boot_adopt_poll() {
  if (!link_serial_take_first_db()) return;
  const int aexp = link_serial_get_db_active_exp();
  if (aexp >= 0 && !g.experimentRunning) {
    g.experimentRunning = true;
    g.experimentStartMs = 0;
    g.runStartUnknown = true;  // the WT32 does not report the start time
    g.expNum = aexp;
    g.expNumUserSet = true;
    const int adur = link_serial_get_db_active_dur();
    if (adur >= UI_DURATION_MIN_H && adur <= UI_DURATION_MAX_H) g.durationH = adur;
    Serial.printf("[RUN] adopted active run exp=%d dur=%dh from the WT32\n", aexp,
                  g.durationH);
  }
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  link_serial_exp_release();
  ui_shell_on_data();
}

// --- status bar --------------------------------------------------------------

static void on_bell_clicked(lv_event_t *e) {
  (void)e;
  // Live list of alert fields on online nodes. No history (locked decision).
  // A bottle with every field missing is one line, not five.
  static char body[1024];
  body[0] = '\0';
  int shown = 0;
  int hidden = 0;
  for (int c = 0; c < UI_CLUSTERS; c++) {
    if (!g.cluster[c].online) continue;
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
      const int nulls = fake_bottle_null_count(c, b);
      if (nulls == 0) continue;
      if (shown >= 10) {
        hidden++;
        continue;
      }
      char line[96];
      if (nulls == S_COUNT) {
        snprintf(line, sizeof(line), "Node %d, bottle %d: no data\n", c + 1, b + 1);
      } else {
        int len = snprintf(line, sizeof(line), "Node %d, bottle %d:", c + 1, b + 1);
        for (int s = 0; s < S_COUNT && len > 0 && len < (int)sizeof(line); s++) {
          if (g.cluster[c].bottle[b].sensorNull[s])
            len += snprintf(line + len, sizeof(line) - len, " %s", SENSOR_NAMES[s]);
        }
        strncat(line, " missing\n", sizeof(line) - strlen(line) - 1);
      }
      strncat(body, line, sizeof(body) - strlen(body) - 1);
      shown++;
    }
  }
  if (shown == 0) {
    snprintf(body, sizeof(body),
             "No alerts. Every reading from connected bottles is valid.\n"
             "Nodes that are not connected are not listed.");
  } else if (hidden > 0) {
    char more[48];
    snprintf(more, sizeof(more), "... and %d more bottles", hidden);
    strncat(body, more, sizeof(body) - strlen(body) - 1);
  }
  ui_modal_show(shown == 0 ? "Sensor alerts" : "Sensor alerts: missing readings", body,
                "Close", nullptr);
}

static void on_tab_clicked(lv_event_t *e) {
  // Before the first login the tabs are disabled; guard here too so a stray
  // event can never route away from /setup.
  if (!g.loggedIn) return;
  switch ((intptr_t)lv_event_get_user_data(e)) {
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

static void on_chip_clicked(lv_event_t *e) {
  (void)e;
  if (s_chipOpensSetup) {
    ui_show_setup();
  } else if (g.loggedIn) {
    ui_show_info();
  }
}

// Swipe left/right switches top-level tabs: home <-> data <-> control <->
// info, stopping at the ends. Drill-down pages (/data/X, /data/X/bottle_Y)
// and /setup ignore swipes, so a stray flick never loses the operator's
// place. LVGL delivers one gesture per press after a 50 px flick and every
// widget bubbles gestures to the screen, so one screen-level handler is enough.
static void on_swipe_gesture(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_GESTURE_LEFT && code != LV_EVENT_GESTURE_RIGHT) return;
  if (!g.loggedIn || ui_modal_visible()) return;
  int tab;
  switch (s_route) {
    case R_HOME:
      tab = 0;
      break;
    case R_PICKER:
      tab = 1;
      break;
    case R_CONTROL:
      tab = 2;
      break;
    case R_INFO:
      tab = 3;
      break;
    default:
      return;
  }
  if (code == LV_EVENT_GESTURE_LEFT && tab < 3)
    tab++;
  else if (code == LV_EVENT_GESTURE_RIGHT && tab > 0)
    tab--;
  else
    return;
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
    default:
      ui_show_info();
      break;
  }
}

// Tab looks: active = accent fill, white text; idle = bar fill, accent text;
// locked (before the first login) = disabled, faint text. All flat fills.
static void style_tab(lv_obj_t *tab, bool active, bool locked) {
  if (tab == nullptr) return;
  if (locked) {
    lv_obj_add_state(tab, LV_STATE_DISABLED);
  } else {
    lv_obj_remove_state(tab, LV_STATE_DISABLED);
  }
  lv_obj_set_style_bg_color(tab, lv_color_hex(active ? UI_COL_ACCENT : UI_COL_BAR), LV_PART_MAIN);
  lv_obj_set_style_text_color(tab, active ? lv_color_white() : lv_color_hex(UI_COL_ACCENT),
                              LV_PART_MAIN);
}

static lv_obj_t *mk_tab(lv_obj_t *bar, const char *text, int i) {
  lv_obj_t *t = ui_mk_button(bar, text, 8 + i * 106, 4, 100, 40, UI_COL_BAR);
  lv_obj_set_style_border_width(t, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(t, lv_color_hex(UI_COL_LINE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(t, lv_color_hex(UI_COL_BAR), LV_STATE_DISABLED);
  lv_obj_set_ext_click_area(t, 4);  // hit area spans the full bar height
  lv_obj_add_event_cb(t, on_tab_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  return t;
}

static void build_status_bar(lv_obj_t *scr) {
  lv_obj_t *bar = lv_obj_create(scr);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, 800, UI_BAR_H);
  style_plain(bar, UI_COL_BAR);
  // 1 px divider under the bar: the bar fill alone is too close to UI_COL_BG.
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(bar, lv_color_hex(UI_COL_LINE), LV_PART_MAIN);
  lv_obj_set_scrollable(bar, false);

  // Nav tabs grouped on the left: icon + word, 100 x 40.
  s_tabs[0] = mk_tab(bar, LV_SYMBOL_HOME "  Home", 0);
  s_tabs[1] = mk_tab(bar, LV_SYMBOL_LIST "  Data", 1);
  s_tabs[2] = mk_tab(bar, LV_SYMBOL_SETTINGS "  Control", 2);
  s_tabs[3] = mk_tab(bar, LV_SYMBOL_FILE "  Info", 3);

  // Link chip: "is my data getting out?" at a glance. Tap = details.
  s_chip = ui_mk_button(bar, "", 552, 4, 176, 40, UI_COL_CARD);
  lv_obj_set_style_radius(s_chip, 20, LV_PART_MAIN);
  lv_obj_set_style_border_color(s_chip, lv_color_hex(UI_COL_LINE), LV_PART_MAIN);
  s_chipLabel = lv_obj_get_child(s_chip, 0);
  lv_obj_align(s_chipLabel, LV_ALIGN_LEFT_MID, 28, 0);
  lv_obj_set_style_text_color(s_chip, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
  s_chipDot = lv_obj_create(s_chip);
  lv_obj_set_size(s_chipDot, 12, 12);
  lv_obj_align(s_chipDot, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_radius(s_chipDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_chipDot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_chipDot, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_clickable(s_chipDot, false);
  lv_obj_add_event_cb(s_chip, on_chip_clicked, LV_EVENT_CLICKED, nullptr);

  s_bellBtn = ui_mk_button(bar, LV_SYMBOL_BELL, 736, 4, 56, 40, UI_COL_CARD);
  lv_obj_set_ext_click_area(s_bellBtn, 4);
  s_bellLabel = lv_obj_get_child(s_bellBtn, 0);
  lv_obj_add_event_cb(s_bellBtn, on_bell_clicked, LV_EVENT_CLICKED, nullptr);
}

void ui_update_bell() {
  if (s_bellBtn == nullptr) return;
  const int n = fake_alarm_count();
  if (n > 0) {
    char t[24];
    snprintf(t, sizeof(t), LV_SYMBOL_WARNING " %d", n > 99 ? 99 : n);
    lv_label_set_text(s_bellLabel, t);
    lv_obj_set_style_bg_color(s_bellBtn, lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bellBtn, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_bellBtn, lv_color_white(), LV_PART_MAIN);
  } else {
    lv_label_set_text(s_bellLabel, LV_SYMBOL_BELL);
    lv_obj_set_style_bg_color(s_bellBtn, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bellBtn, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_bellBtn, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
  }
}

static void update_chip() {
  if (s_chip == nullptr) return;
  const char *text = "Online";
  uint32_t col = UI_COL_OK;
  char buf[24];
  s_chipOpensSetup = false;
  const uint32_t rx = link_serial_rx_age_ms();
  if (rx > UI_LINK_SILENT_MS) {
    text = "No WT32";
    col = UI_COL_ALARM;
  } else if (link_serial_login_requested()) {
    text = "Login needed";
    col = UI_COL_WARN;
    s_chipOpensSetup = true;
  } else if (!link_serial_net_up()) {
    text = "No internet";
    col = UI_COL_WARN;
  } else if (strcmp(link_serial_db_state(), "none") != 0) {
    snprintf(buf, sizeof(buf), "DB: %s", link_serial_db_state());
    text = buf;
    col = UI_COL_WARN;
  }
  if (strcmp(lv_label_get_text(s_chipLabel), text) != 0) lv_label_set_text(s_chipLabel, text);
  lv_obj_set_style_bg_color(s_chipDot, lv_color_hex(col), LV_PART_MAIN);
}

void ui_update_status_bar() {
  update_chip();
  ui_update_bell();
}

void ui_update_nav() {
  const int active = (s_route == R_HOME)                       ? 0
                     : (s_route >= R_PICKER && s_route <= R_BOTTLE) ? 1
                     : (s_route == R_CONTROL)                  ? 2
                     : (s_route == R_INFO)                     ? 3
                                                               : -1;
  for (int i = 0; i < 4; i++) style_tab(s_tabs[i], g.loggedIn && i == active, !g.loggedIn);
}

// --- modal -------------------------------------------------------------------

static void on_modal_ack(lv_event_t *e) {
  (void)e;
  UiAckFn fn = s_modalAck;
  ui_modal_hide();
  if (fn != nullptr) fn();
}

static uint32_t sev_color(UiSeverity sev) {
  return sev == UI_SEV_ALARM ? UI_COL_ALARM : sev == UI_SEV_WARN ? UI_COL_WARN : UI_COL_ACCENT;
}

void ui_modal_show_sev(const char *title, const char *body, const char *ackLabel, UiAckFn onAck,
                       UiSeverity sev) {
  if (s_modal != nullptr && sev < s_modalSev) {
    Serial.printf("[UI] modal '%s' not shown: a more severe one is open\n", title);
    return;
  }
  ui_modal_hide();
  sleep_wake();
  lv_obj_t *scr = lv_screen_active();
  // Opaque backdrop: nothing beneath it is blended or redrawn while it is up.
  s_modal = lv_obj_create(scr);
  lv_obj_set_pos(s_modal, 0, 0);
  lv_obj_set_size(s_modal, 800, 480);
  style_plain(s_modal, UI_COL_SCRIM);
  lv_obj_set_scrollable(s_modal, false);

  // Card sized to its content. A 6 px top band carries the severity colour.
  lv_obj_t *card = lv_obj_create(s_modal);
  lv_obj_set_width(card, 520);
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card, lv_color_hex(UI_COL_CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
  lv_obj_set_style_border_side(card, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 6, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(sev_color(sev)), LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 20, LV_PART_MAIN);
  lv_obj_set_style_pad_row(card, 12, LV_PART_MAIN);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scrollable(card, false);

  lv_obj_t *t = ui_mk_label(card, title, 0, 0, 10,
                            sev == UI_SEV_INFO ? UI_COL_TEXT : sev_color(sev),
                            &lv_font_montserrat_20);
  lv_obj_set_width(t, lv_pct(100));
  lv_obj_t *bl = ui_mk_label(card, body, 0, 0, 10, UI_COL_TEXT, &lv_font_montserrat_14);
  lv_obj_set_width(bl, lv_pct(100));
  lv_obj_t *ack = ui_mk_button(card, ackLabel, 0, 0, 200, 48, UI_COL_ACCENT);
  lv_obj_set_style_margin_top(ack, 4, LV_PART_MAIN);
  lv_obj_center(card);

  s_modalAck = onAck;
  s_modalSev = sev;
  lv_obj_add_event_cb(ack, on_modal_ack, LV_EVENT_CLICKED, nullptr);
}

void ui_modal_show(const char *title, const char *body, const char *ackLabel, UiAckFn onAck) {
  ui_modal_show_sev(title, body, ackLabel, onAck, UI_SEV_INFO);
}

void ui_modal_hide() {
  if (s_modal != nullptr) {
    lv_obj_delete(s_modal);
    s_modal = nullptr;
    s_modalAck = nullptr;
    s_modalSev = UI_SEV_INFO;
  }
}

bool ui_modal_visible() { return s_modal != nullptr; }

// --- router ------------------------------------------------------------------

static void render_current() {
  if (s_content == nullptr) return;
  // Per-route teardown before lv_obj_clean() frees the widgets, so no screen
  // keeps a pointer to a deleted object.
  screen_control_hide();
  screen_home_hide();
  screen_data_hide();
  screen_info_hide();
  screen_setup_hide();
  lv_obj_clean(s_content);
  switch (s_route) {
    case R_PICKER:
      screen_data_picker_show(s_content);
      break;
    case R_CLUSTER:
      screen_cluster_show(s_content, s_argC);
      break;
    case R_BOTTLE:
      screen_bottle_show(s_content, s_argC, s_argB);
      break;
    case R_CONTROL:
      screen_control_show(s_content);
      break;
    case R_INFO:
      screen_info_show(s_content);
      break;
    case R_SETUP:
      screen_setup_show(s_content);
      break;
    default:
      screen_home_show(s_content);
      break;
  }
  ui_update_nav();
}

static void go(int route, int c, int b) {
  if (route == R_SETUP && s_route != R_SETUP) {
    s_prevRoute = s_route;
    s_prevC = s_argC;
    s_prevB = s_argB;
  }
  s_route = route;
  s_argC = c;
  s_argB = b;
  render_current();
}

void ui_show_home() { go(R_HOME, 0, 0); }
void ui_show_data_picker() { go(R_PICKER, 0, 0); }
void ui_show_cluster(int c) { go(R_CLUSTER, c, 0); }
void ui_show_bottle(int c, int b) { go(R_BOTTLE, c, b); }
void ui_show_control() { go(R_CONTROL, 0, 0); }
void ui_show_info() { go(R_INFO, 0, 0); }
void ui_show_setup() { go(R_SETUP, 0, 0); }
void ui_show_previous() {
  const int r = (s_prevRoute == R_SETUP) ? R_HOME : s_prevRoute;
  go(r, s_prevC, s_prevB);
}
bool ui_is_setup() { return s_route == R_SETUP; }
const char *ui_route_name() {
  switch (s_route) {
    case R_HOME: return "/home";
    case R_PICKER: return "/data";
    case R_CLUSTER: return "/data/node";
    case R_BOTTLE: return "/data/bottle";
    case R_CONTROL: return "/control";
    case R_INFO: return "/info";
    case R_SETUP: return "/setup";
  }
  return "?";
}
bool ui_is_control() { return s_route == R_CONTROL; }
bool ui_is_info() { return s_route == R_INFO; }

void ui_refresh_current() {
  if (!g.loggedIn) return;
  switch (s_route) {
    case R_HOME:
      screen_home_refresh();
      break;
    case R_PICKER:
    case R_CLUSTER:
    case R_BOTTLE:
      screen_data_refresh();
      break;
    case R_CONTROL:
      screen_control_refresh();
      break;
    case R_INFO:
      screen_info_refresh();
      break;
    default:
      break;  // /setup polls the link itself
  }
}

void ui_shell_on_data() {
  ui_update_bell();
  ui_refresh_current();
}

static void on_warn41_ack() {}
static void on_trip45_ack() {}

void ui_eval_overheat() {
  if (!g.loggedIn) return;
  const float t = g.chamberTempC;
  if (t >= UI_OVERHEAT_TRIP_C) {
    if (!s_trip45Shown) {
      s_trip45Shown = true;
      g.stirrerOn = false;  // breaker cut: the stirrers drop out, the display stays up
      ui_modal_show_sev("Chamber above 45 " UI_DEG_C,
                        "The chamber reached 45 " UI_DEG_C " or more.\n"
                        "The display does not cut heater power.\n"
                        "Stirrers set to OFF on this display (no hardware link yet).\n"
                        "Check the rig now.",
                        "Acknowledge", on_trip45_ack, UI_SEV_ALARM);
      ui_refresh_current();
    }
    return;
  }
  s_trip45Shown = false;
  if (t >= UI_OVERHEAT_WARN_C) {
    if (!s_warn41Shown) {
      s_warn41Shown = true;
      ui_modal_show_sev("Chamber above 41 " UI_DEG_C,
                        "The chamber reached 41 " UI_DEG_C " or more.\n"
                        "The display does not control the heater.\n"
                        "This alert shows again if the chamber cools below\n"
                        "41 " UI_DEG_C " and then rises again.",
                        "Acknowledge", on_warn41_ack, UI_SEV_WARN);
    }
    return;
  }
  s_warn41Shown = false;
}

void ui_experiment_toggle() {
  if (g.experimentRunning) {
    g.experimentRunning = false;
    g.experimentStartMs = 0;
    g.runStartUnknown = false;
  } else {
    // Decision 2026-11-10: a run may not start until the WT32 has completed at
    // least one successful configuration pull since boot. Cached or local
    // configuration is not substituted, so the operator sees this block and
    // can retry once the link is live.
    if (!link_serial_db_ready()) {
      Serial.println("[RUN] blocked: no successful DB configuration pull yet");
      ui_modal_show_sev("Cannot start: DB configuration not loaded",
                        "The WT32 has not loaded the experiment configuration from\n"
                        "the database since it started.\n\n"
                        "Check the link chip in the top bar or the Info page, then\n"
                        "hold the start button again.",
                        "Close", nullptr, UI_SEV_WARN);
      return;
    }
    g.experimentRunning = true;
    g.experimentStartMs = millis();
    g.runStartUnknown = false;
    // The selected id is used for this run; after it, the DB suggestion may
    // propose the next free id again.
    g.expNumUserSet = false;
  }
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  Serial.printf("[RUN] requested %s exp=%d\n", g.experimentRunning ? "ON" : "OFF", g.expNum);
  ui_shell_on_data();
}

// --- init / poll -------------------------------------------------------------

// Copies new real SENS frames into the model on their own cadence and updates
// the open screen in place. Runs even while no real data exists: the call
// returns fast and keeps the staleness watcher armed.
static void on_sens_timer(lv_timer_t *t) {
  (void)t;
  if (sens_apply_real()) ui_shell_on_data();
}

void ui_poll() {
  boot_adopt_poll();
  exp_suggest_poll();
  static uint32_t s_lastBarMs = 0;
  if (millis() - s_lastBarMs >= 500) {
    s_lastBarMs = millis();
    update_chip();
  }
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
  lv_obj_set_pos(s_content, 0, UI_BAR_H);
  lv_obj_set_size(s_content, 800, UI_CONTENT_H);
  style_plain(s_content, UI_COL_BG);
  lv_obj_set_scrollable(s_content, false);

  ui_show_setup();  // first route until the WT32 reports RESULT=success
  ui_update_status_bar();
  ui_update_nav();

  lv_timer_create(on_sens_timer, UI_SENS_APPLY_MS, nullptr);
}
