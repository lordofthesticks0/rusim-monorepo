#include "screen_info.h"

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_shell.h"

static lv_obj_t *s_tileIp = nullptr;
static lv_obj_t *s_valIp = nullptr;
static lv_obj_t *s_tileNet = nullptr;
static lv_obj_t *s_valNet = nullptr;
static lv_obj_t *s_tileFw = nullptr;
static lv_obj_t *s_valFw = nullptr;
static lv_obj_t *s_tileDb = nullptr;
static lv_obj_t *s_valDb = nullptr;
static lv_obj_t *s_tileCfg = nullptr;
static lv_obj_t *s_valCfg = nullptr;
static lv_obj_t *s_tileQueue = nullptr;
static lv_obj_t *s_valQueue = nullptr;

static lv_obj_t *s_gaugeStorage = nullptr;
static lv_obj_t *s_ctrStorage = nullptr;
static lv_obj_t *s_gaugeCpu = nullptr;
static lv_obj_t *s_ctrCpu = nullptr;

static uint32_t s_lastPollMs = 0;

static lv_obj_t *mk_tile(lv_obj_t *parent, const char *caption, lv_obj_t **valueOut) {
  lv_obj_t *card = ui_mk_card(parent, 0, 0, 180, 180);
  *valueOut = lv_label_create(card);
  lv_label_set_text(*valueOut, "");
  lv_obj_set_width(*valueOut, 164);
  lv_label_set_long_mode(*valueOut, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(*valueOut, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(*valueOut, &lv_font_montserrat_28_compressed, LV_PART_MAIN);
  lv_obj_set_style_text_color(*valueOut, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
  lv_obj_align(*valueOut, LV_ALIGN_TOP_MID, 0, 34);

  lv_obj_t *cap = ui_mk_label(card, caption, 8, 142, 164, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  (void)cap;
  return card;
}

static lv_obj_t *mk_gauge(lv_obj_t *parent, const char *caption, lv_obj_t **centreOut,
                          lv_obj_t **arcOut) {
  lv_obj_t *card = ui_mk_card(parent, 0, 0, 180, 180);
  lv_obj_t *arc = lv_arc_create(card);
  lv_obj_set_size(arc, 132, 132);
  lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 10);
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, 0);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(UI_COL_BAR), LV_PART_MAIN);
  if (arcOut != nullptr) *arcOut = arc;

  *centreOut = lv_label_create(card);
  lv_label_set_text(*centreOut, "");
  lv_obj_set_width(*centreOut, 92);
  lv_label_set_long_mode(*centreOut, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(*centreOut, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(*centreOut, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(*centreOut, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
  lv_obj_align(*centreOut, LV_ALIGN_TOP_MID, 0, 58);

  lv_obj_t *cap = ui_mk_label(card, caption, 8, 142, 164, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  (void)cap;
  return card;
}


static void gauge_set(lv_obj_t *arc, lv_obj_t *centre, int pct, const char *centreText) {
  if (arc != nullptr) {
    lv_arc_set_value(arc, pct);
    lv_color_t lo = lv_color_hex(UI_COL_OK);
    lv_color_t hi = lv_color_hex(UI_COL_ALARM);
    // mix=255 yields c1, mix=0 yields c2: full load reads red, idle green.
    lv_color_t c = lv_color_mix(hi, lo, (uint8_t)(pct * 255 / 100));
    lv_obj_set_style_arc_color(arc, c, LV_PART_INDICATOR);
  }
  if (centre != nullptr && centreText != nullptr) {
    lv_label_set_text(centre, centreText);
  }
}

static void info_render() {
  // IP tile
  char ip[LINK_IP_MAX + 1];
  link_serial_get_ip(ip, sizeof(ip));
  if (s_valIp != nullptr) {
    lv_label_set_text(s_valIp, ip[0] != '\0' ? ip : "--");
  }

  // Network tile
  if (s_valNet != nullptr) {
    if (!g.loggedIn) {
      lv_label_set_text(s_valNet, "Blocked");
      lv_obj_set_style_text_color(s_valNet, lv_color_hex(UI_COL_WARN), LV_PART_MAIN);
    } else if (ip[0] == '\0') {
      lv_label_set_text(s_valNet, "Offline");
      lv_obj_set_style_text_color(s_valNet, lv_color_hex(UI_COL_ALARM), LV_PART_MAIN);
    } else {
      lv_label_set_text(s_valNet, "Online");
      lv_obj_set_style_text_color(s_valNet, lv_color_hex(UI_COL_OK), LV_PART_MAIN);
    }
  }

  // Firmware tile
  if (s_valFw != nullptr) {
    lv_label_set_text(s_valFw, FW_VERSION);
  }

  // DB tile: latest experiment id plus the bottle_count from authenticate()
  // ("12 (24bt)"). "--" while never synced ("latest=-" on the wire).
  if (s_valDb != nullptr) {
    int db = link_serial_get_db_latest();
    if (db == -2) {
      lv_label_set_text(s_valDb, "--");
    } else {
      int bottles = link_serial_get_db_bottles();
      char t[32];
      if (bottles < 0) snprintf(t, sizeof(t), "%d", db);
      else snprintf(t, sizeof(t), "%d (%dbt)", db, bottles);
      lv_label_set_text(s_valDb, t);
    }
  }

  // Run configuration tile: the FROZEN snapshot while running (experiment,
  // duration, source), otherwise whether a start is allowed yet. Pending edits
  // on /control are not shown here until they are frozen by the next start.
  if (s_valCfg != nullptr) {
    const int aexp = link_serial_get_db_active_exp();
    if (aexp < 0) {
      const bool ready = link_serial_db_ready();
      lv_label_set_text(s_valCfg, ready ? "ready" : "waiting");
      lv_obj_set_style_text_color(
          s_valCfg, lv_color_hex(ready ? UI_COL_OK : UI_COL_ALARM), LV_PART_MAIN);
    } else {
      char t[32];
      snprintf(t, sizeof(t), "%d/%ldh/%s", aexp, (long)link_serial_get_db_active_dur(),
               link_serial_get_db_active_src() == 1 ? "db" : "loc");
      lv_label_set_text(s_valCfg, t);
      lv_obj_set_style_text_color(s_valCfg, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    }
  }

  // Queue tile: rows waiting on the WT32 plus the age of the last successful
  // configuration read and acknowledged upload. "--" means never.
  if (s_valQueue != nullptr) {
    char rb[12];
    char ab[12];
    const uint32_t readAge = link_serial_get_db_read_age_ms();
    const uint32_t ackAge = link_serial_get_db_ack_age_ms();
    if (readAge == 0) {
      snprintf(rb, sizeof(rb), "--");
    } else {
      snprintf(rb, sizeof(rb), "%lus", (unsigned long)(readAge / 1000));
    }
    if (ackAge == 0) {
      snprintf(ab, sizeof(ab), "--");
    } else {
      snprintf(ab, sizeof(ab), "%lus", (unsigned long)(ackAge / 1000));
    }
    char t[40];
    snprintf(t, sizeof(t), "%d\n%s/%s", link_serial_get_db_queued(), rb, ab);
    lv_label_set_text(s_valQueue, t);
  }

  // Storage usage is not provided by the WT32 protocol.
  gauge_set(s_gaugeStorage, s_ctrStorage, 0, "--");

  // CPU gauge. lv_timer_get_idle() is LVGL's own busy measurement (render +
  // input + timers), maintained as O(1) counters inside lv_timer_handler():
  // no heap walk, no FreeRTOS dependency. 100 - idle = UI-thread load %.
  // This is not whole-chip load: the WiFi stack, other tasks and ISRs are
  // not counted. (FreeRTOS run-time stats would cover those but
  // CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS is off in this Arduino core,
  // and the prebuilt lib has no stats functions to link against.)
  {
    uint32_t idle = lv_timer_get_idle();
    if (idle > 100) idle = 100;
    int pct = 100 - (int)idle;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    gauge_set(s_gaugeCpu, s_ctrCpu, pct, buf);
  }
}

void screen_info_poll() {
  if (!ui_is_info()) return;
  if (s_gaugeStorage == nullptr) return;
  uint32_t now = millis();
  if (now - s_lastPollMs < 1000) return;
  s_lastPollMs = now;
  info_render();
}

void screen_info_show(lv_obj_t *parent) {
  lv_obj_t *cont = lv_obj_create(parent);
  lv_obj_set_pos(cont, 0, 0);
  lv_obj_set_size(cont, 800, 436);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(cont, 16, LV_PART_MAIN);
  lv_obj_set_style_pad_row(cont, 16, LV_PART_MAIN);
  lv_obj_set_style_pad_column(cont, 16, LV_PART_MAIN);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_scrollable(cont, false);

  s_tileIp = mk_tile(cont, "IP Address", &s_valIp);
  s_tileNet = mk_tile(cont, "Network", &s_valNet);
  s_tileFw = mk_tile(cont, "Firmware", &s_valFw);
  s_tileDb = mk_tile(cont, "DB experiment", &s_valDb);
  s_tileCfg = mk_tile(cont, "Run config", &s_valCfg);
  s_tileQueue = mk_tile(cont, "Queue: rows, read/ack", &s_valQueue);

  {
    lv_obj_t *card = mk_gauge(cont, "Storage", &s_ctrStorage, &s_gaugeStorage);
    (void)card;
  }
  {
    lv_obj_t *card = mk_gauge(cont, "CPU", &s_ctrCpu, &s_gaugeCpu);
    (void)card;
  }

  info_render();
  s_lastPollMs = millis();
}