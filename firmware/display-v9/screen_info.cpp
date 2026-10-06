#include "screen_info.h"

#include "fake_data.h"
#include "link_modbus.h"
#include "ui_config.h"
#include "ui_shell.h"

static void on_back(lv_event_t *e) {
  (void)e;
  ui_show_home();
}

void screen_info_show(lv_obj_t *parent) {
  // Not cached: rebuilt fresh on every open (locked decision). Network rows
  // are read live from the Modbus slave state the WT32 pushes each cycle.
  ui_mk_label(parent, "/info (fresh on open)", 16, 8, 500, UI_COL_WHITE,
              &lv_font_montserrat_20);

  char row[128];
  snprintf(row, sizeof(row), "Storage limits: %s", g.storageNote);
  ui_mk_label(parent, row, 16, 60, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  char ip[LINK_IP_MAX + 1];
  link_modbus_get_ip(ip, sizeof(ip));
  if (ip[0] != '\0') {
    snprintf(row, sizeof(row), "IP address: %s (real, WT32)", ip);
  } else {
    snprintf(row, sizeof(row), "IP address: waiting for WT32...");
  }
  ui_mk_label(parent, row, 16, 100, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  int pingMs = link_modbus_get_ping_ms();
  if (pingMs >= 0) {
    snprintf(row, sizeof(row), "Ping 1.1.1.1: %d ms (real, WT32)", pingMs);
  } else if (link_modbus_net_up()) {
    snprintf(row, sizeof(row), "Ping 1.1.1.1: no reply yet (real, WT32)");
  } else {
    snprintf(row, sizeof(row), "Ping 1.1.1.1: no data yet (link down?)");
  }
  ui_mk_label(parent, row, 16, 140, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Latest HTTP reply: %s", g.lastHttp);
  ui_mk_label(parent, row, 16, 180, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  int dbLatest = link_modbus_get_db_latest();
  if (dbLatest == -2) {
    snprintf(row, sizeof(row), "DB latest exp: waiting for master sync...");
  } else {
    snprintf(row, sizeof(row), "DB latest exp: %d (next %d)%s", dbLatest, dbLatest + 1,
             link_modbus_time_ok() ? " NTP OK" : " NTP no sync");
  }
  ui_mk_label(parent, row, 16, 200, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Firmware version: %s", FW_VERSION);
  ui_mk_label(parent, row, 16, 220, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  // Real MCU readings, labeled as real to distinguish from fake rows.
  snprintf(row, sizeof(row), "Heap free: %u B (real) | PSRAM: %u B (real)", ESP.getFreeHeap(),
           ESP.getPsramSize());
  ui_mk_label(parent, row, 16, 260, 760, UI_COL_DIM, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Fetched at %lu ms (fake stamp)", millis());
  ui_mk_label(parent, row, 16, 300, 760, UI_COL_DIM, &lv_font_montserrat_14);

  lv_obj_t *back = ui_mk_button(parent, "< Home", 16, 360, 140, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);
}
