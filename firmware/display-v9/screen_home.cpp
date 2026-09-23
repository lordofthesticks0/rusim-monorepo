#include "screen_home.h"

#include "fake_data.h"
#include "ui_config.h"
#include "ui_shell.h"

static void nav(lv_event_t *e) {
  intptr_t id = (intptr_t)lv_event_get_user_data(e);
  switch (id) {
    case 0:
      ui_show_data_picker();
      break;
    case 1:
      ui_show_control();
      break;
    case 2:
      ui_show_info();
      break;
    default:
      break;
  }
}

void screen_home_show(lv_obj_t *parent) {
  ui_mk_label(parent, "Home (fake data)", 16, 8, 400, UI_COL_WHITE, &lv_font_montserrat_20);

  char sub[96];
  snprintf(sub, sizeof(sub), "Run %dh (fake) | %d/%d nodes online | push every 5 min (fake)",
           g.durationH, UI_ONLINE_CLUSTERS, UI_CLUSTERS);
  ui_mk_label(parent, sub, 16, 40, 760, UI_COL_DIM, &lv_font_montserrat_14);

  lv_obj_t *b0 = ui_mk_button(parent, "DATA", 16, 90, 240, 120, UI_COL_CARD);
  lv_obj_add_event_cb(b0, nav, LV_EVENT_CLICKED, (void *)(intptr_t)0);
  lv_obj_t *b1 = ui_mk_button(parent, "CONTROL", 272, 90, 240, 120, UI_COL_CARD);
  lv_obj_add_event_cb(b1, nav, LV_EVENT_CLICKED, (void *)(intptr_t)1);
  lv_obj_t *b2 = ui_mk_button(parent, "INFO", 528, 90, 240, 120, UI_COL_CARD);
  lv_obj_add_event_cb(b2, nav, LV_EVENT_CLICKED, (void *)(intptr_t)2);

  char ch[96];
  snprintf(ch, sizeof(ch), "Chamber %.1fC (fake) | Setpoint %.1fC | Stirrer %s", g.chamberTempC,
           g.tempSetpointC, g.stirrerOn ? "ON" : "OFF");
  ui_mk_label(parent, ch, 16, 230, 760, UI_COL_WHITE, &lv_font_montserrat_14);
  ui_mk_label(parent, "Tip: send HELP over USB serial to tweak fake values (e.g. TEMP 38.3).",
              16, 262, 760, UI_COL_DIM, &lv_font_montserrat_14);
  ui_mk_label(parent, "Session: one bounded run, then shut down for bottle retrieval.", 16,
              294, 760, UI_COL_DIM, &lv_font_montserrat_14);
}
