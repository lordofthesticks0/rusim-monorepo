#include "screen_info.h"

#include <string.h>

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_shell.h"

// Eight 180 x 172 information tiles (4 x 2, 16 px gaps). Each tile: value on
// top (20 px for words, 28 px for numbers), a dim detail line, and a caption at
// the bottom. Tiles are not tappable, so they keep the grey card outline.

enum {
  T_IP = 0,
  T_NET,
  T_FW,
  T_DB,
  T_RUN,
  T_QUEUE,
  T_FEED,
  T_COUNT
};
static lv_obj_t *s_val[T_COUNT] = {};
static lv_obj_t *s_sub[T_COUNT] = {};
static lv_obj_t *s_cpuArc = nullptr;
static lv_obj_t *s_cpuVal = nullptr;
static uint32_t s_lastPollMs = 0;

static void tile_pos(int i, int *x, int *y) {
  *x = UI_MARGIN + (i % 4) * 196;
  *y = UI_PAGE_TOP + (i / 4) * 188;
}

static void mk_tile(lv_obj_t *parent, int i, const char *caption, const lv_font_t *font) {
  int x, y;
  tile_pos(i, &x, &y);
  lv_obj_t *card = ui_mk_card(parent, x, y, 180, 172);  // inner 154 x 146
  s_val[i] = ui_mk_label(card, "", 0, 8, 154, UI_COL_TEXT, font);
  lv_obj_set_style_text_align(s_val[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_sub[i] = ui_mk_label(card, "", 0, 56, 154, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_sub[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_t *cap = ui_mk_caption(card, caption, 0, 126, 154);
  lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static void set_tile(int i, const char *value, uint32_t col, const char *sub) {
  if (s_val[i] == nullptr) return;
  if (strcmp(lv_label_get_text(s_val[i]), value) != 0) lv_label_set_text(s_val[i], value);
  lv_obj_set_style_text_color(s_val[i], lv_color_hex(col), LV_PART_MAIN);
  if (strcmp(lv_label_get_text(s_sub[i]), sub) != 0) lv_label_set_text(s_sub[i], sub);
}

// "12 s", "4 min", "3 h".
static void fmt_age(uint32_t ms, bool never, char *out, size_t n) {
  if (never) {
    snprintf(out, n, "never");
  } else if (ms < 60000UL) {
    snprintf(out, n, "%lu s", (unsigned long)(ms / 1000));
  } else if (ms < 3600000UL) {
    snprintf(out, n, "%lu min", (unsigned long)(ms / 60000UL));
  } else {
    snprintf(out, n, "%lu h", (unsigned long)(ms / 3600000UL));
  }
}

static void render() {
  char v[32];
  char sub[64];
  char a[40];
  char b[40];

  char ip[LINK_IP_MAX + 1];
  link_serial_get_ip(ip, sizeof(ip));
  set_tile(T_IP, ip[0] != '\0' ? ip : "--", ip[0] != '\0' ? UI_COL_TEXT : UI_COL_DIM,
           "WT32 Ethernet");

  // Network: same priority as the status-bar chip.
  const uint32_t rx = link_serial_rx_age_ms();
  const int ping = link_serial_get_ping_ms();
  if (ping >= 0) {
    snprintf(sub, sizeof(sub), "Ping %d ms", ping);
  } else {
    snprintf(sub, sizeof(sub), "No ping result");
  }
  if (rx > UI_LINK_SILENT_MS) {
    set_tile(T_NET, "No WT32", UI_COL_ALARM, "No line received");
  } else if (link_serial_login_requested()) {
    set_tile(T_NET, "Login needed", UI_COL_WARN, "Open the login screen");
  } else if (!link_serial_net_up()) {
    set_tile(T_NET, "No internet", UI_COL_WARN, sub);
  } else {
    set_tile(T_NET, "Online", UI_COL_OK, sub);
  }

  snprintf(sub, sizeof(sub), "LVGL %d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
           LVGL_VERSION_PATCH);
  set_tile(T_FW, FW_VERSION, UI_COL_TEXT, sub);

  // Latest experiment id in the database (-2 = never synced, -1 = none yet).
  const int latest = link_serial_get_db_latest();
  const int bottles = link_serial_get_db_bottles();
  if (bottles >= 0) {
    snprintf(sub, sizeof(sub), "%d bottles configured", bottles);
  } else {
    snprintf(sub, sizeof(sub), "Bottle count unknown");
  }
  if (latest == -2) {
    set_tile(T_DB, "--", UI_COL_DIM, "Not synced yet");
  } else if (latest == -1) {
    set_tile(T_DB, "None", UI_COL_TEXT, sub);
  } else {
    snprintf(v, sizeof(v), "Exp %d", latest);
    set_tile(T_DB, v, UI_COL_TEXT, sub);
  }

  // Active run as frozen on the WT32, or whether a start is possible.
  const int aexp = link_serial_get_db_active_exp();
  if (aexp >= 0) {
    snprintf(v, sizeof(v), "Exp %d", aexp);
    snprintf(sub, sizeof(sub), "%d h, %s", link_serial_get_db_active_dur(),
             link_serial_get_db_active_src() == 1 ? "from DB" : "from display");
    set_tile(T_RUN, v, UI_COL_OK, sub);
  } else if (link_serial_db_ready()) {
    set_tile(T_RUN, "None", UI_COL_TEXT, "Ready to start");
  } else {
    set_tile(T_RUN, "None", UI_COL_TEXT, "Start blocked:\nDB not loaded");
    lv_obj_set_style_text_color(s_sub[T_RUN], lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
  }
  if (aexp >= 0 || link_serial_db_ready())
    lv_obj_set_style_text_color(s_sub[T_RUN], lv_color_hex(UI_COL_DIM), LV_PART_MAIN);

  // Upload queue: rows waiting on the WT32 + ages of the last config read and
  // the last acknowledged upload.
  const uint32_t readAge = link_serial_get_db_read_age_ms();
  const uint32_t ackAge = link_serial_get_db_ack_age_ms();
  if (readAge == 0) {
    snprintf(a, sizeof(a), "never");
  } else {
    fmt_age(readAge, false, v, sizeof(v));
    snprintf(a, sizeof(a), "%s ago", v);
  }
  if (ackAge == 0) {
    snprintf(b, sizeof(b), "never");
  } else {
    fmt_age(ackAge, false, v, sizeof(v));
    snprintf(b, sizeof(b), "%s ago", v);
  }
  const int queued = link_serial_get_db_queued();
  snprintf(v, sizeof(v), "%d %s", queued, queued == 1 ? "row" : "rows");
  snprintf(sub, sizeof(sub), "Config read: %s\nUpload ack: %s", a, b);
  set_tile(T_QUEUE, v, UI_COL_TEXT, sub);

  // Sensor feed: age of the newest SENS frame.
  const int32_t sensAge = link_serial_get_sens(nullptr);
  if (sensAge < 0) {
    set_tile(T_FEED, "No data", UI_COL_DIM, "No reading since start");
  } else {
    fmt_age((uint32_t)sensAge, false, a, sizeof(a));
    snprintf(v, sizeof(v), "%s ago", a);
    const bool stale = (uint32_t)sensAge > UI_SENS_STALE_MS;
    set_tile(T_FEED, v, stale ? UI_COL_WARN : UI_COL_TEXT,
             stale ? "Too old: shown as offline" : "Newest bottle reading");
  }

  // UI-thread load from LVGL's own idle counter (render + input + timers).
  // Not whole-chip load: WiFi, other tasks and ISRs are not counted.
  if (s_cpuArc != nullptr) {
    uint32_t idle = lv_timer_get_idle();
    if (idle > 100) idle = 100;
    const int pct = 100 - (int)idle;
    lv_arc_set_value(s_cpuArc, pct);
    snprintf(v, sizeof(v), "%d%%", pct);
    lv_label_set_text(s_cpuVal, v);
  }
}

void screen_info_refresh() {
  if (s_val[T_IP] == nullptr) return;
  render();
}

void screen_info_poll() {
  if (!ui_is_info() || s_val[T_IP] == nullptr) return;
  const uint32_t now = millis();
  if (now - s_lastPollMs < 1000) return;
  s_lastPollMs = now;
  render();
}

void screen_info_hide() {
  for (int i = 0; i < T_COUNT; i++) {
    s_val[i] = nullptr;
    s_sub[i] = nullptr;
  }
  s_cpuArc = nullptr;
  s_cpuVal = nullptr;
}

void screen_info_show(lv_obj_t *parent) {
  ui_mk_page_title(parent, "System info", nullptr, nullptr);
  mk_tile(parent, T_IP, "IP ADDRESS", &lv_font_montserrat_20);
  mk_tile(parent, T_NET, "NETWORK", &lv_font_montserrat_20);
  mk_tile(parent, T_FW, "FIRMWARE", &lv_font_montserrat_20);
  mk_tile(parent, T_DB, "LATEST EXPERIMENT", &lv_font_montserrat_28);
  mk_tile(parent, T_RUN, "ACTIVE RUN", &lv_font_montserrat_28);
  mk_tile(parent, T_QUEUE, "UPLOAD QUEUE", &lv_font_montserrat_28);
  mk_tile(parent, T_FEED, "SENSOR FEED", &lv_font_montserrat_20);

  // UI-thread load gauge in the last slot.
  int x, y;
  tile_pos(7, &x, &y);
  lv_obj_t *card = ui_mk_card(parent, x, y, 180, 172);
  s_cpuArc = lv_arc_create(card);
  lv_obj_set_size(s_cpuArc, 110, 110);
  lv_obj_align(s_cpuArc, LV_ALIGN_TOP_MID, 0, 4);
  lv_arc_set_range(s_cpuArc, 0, 100);
  lv_obj_remove_style(s_cpuArc, nullptr, LV_PART_KNOB);
  lv_obj_set_clickable(s_cpuArc, false);
  lv_obj_set_style_arc_width(s_cpuArc, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_cpuArc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(s_cpuArc, lv_color_hex(UI_COL_BAR), LV_PART_MAIN);
  lv_obj_set_style_arc_color(s_cpuArc, lv_color_hex(UI_COL_ACCENT), LV_PART_INDICATOR);
  s_cpuVal = ui_mk_label(card, "", 0, 0, 80, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(s_cpuVal, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align_to(s_cpuVal, s_cpuArc, LV_ALIGN_CENTER, 0, 0);
  lv_obj_t *cap = ui_mk_caption(card, "UI THREAD LOAD", 0, 126, 154);
  lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  render();
  s_lastPollMs = millis();
}
