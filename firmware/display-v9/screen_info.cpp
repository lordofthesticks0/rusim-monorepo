#include "screen_info.h"

#include "fake_data.h"
#include "ui_config.h"
#include "ui_shell.h"

static void on_back(lv_event_t *e) {
  (void)e;
  ui_show_home();
}

void screen_info_show(lv_obj_t *parent) {
  // Not cached: rebuilt fresh on every open (locked decision).
  ui_mk_label(parent, "/info (fresh on open, fake)", 16, 8, 500, UI_COL_WHITE,
              &lv_font_montserrat_20);

  char row[128];
  snprintf(row, sizeof(row), "Storage limits: %s", g.storageNote);
  ui_mk_label(parent, row, 16, 60, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "IP address: %s", g.ipAddr);
  ui_mk_label(parent, row, 16, 100, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Latest HTTP reply: %s", g.lastHttp);
  ui_mk_label(parent, row, 16, 140, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Firmware version: %s", FW_VERSION);
  ui_mk_label(parent, row, 16, 180, 760, UI_COL_WHITE, &lv_font_montserrat_14);

  // Real MCU readings, labeled as real to distinguish from fake rows.
  snprintf(row, sizeof(row), "Heap free: %u B (real) | PSRAM: %u B (real)", ESP.getFreeHeap(),
           ESP.getPsramSize());
  ui_mk_label(parent, row, 16, 220, 760, UI_COL_DIM, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Fetched at %lu ms (fake stamp)", millis());
  ui_mk_label(parent, row, 16, 260, 760, UI_COL_DIM, &lv_font_montserrat_14);

  lv_obj_t *back = ui_mk_button(parent, "< Home", 16, 360, 140, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);
}
