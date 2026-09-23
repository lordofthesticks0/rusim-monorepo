#pragma once

// Persistent shell: status bar (network, POST, bell), content router, and a
// single blocking-modal primitive shared by the boot gate and overheat alerts.
// NOTE: no LVGL theme is enabled (LV_USE_THEME_DEFAULT 0), so every widget is
// styled explicitly through the ui_mk_* helpers below.

#include <lvgl.h>

void ui_init();

// Routes (mirror the design-note screen tree).
void ui_show_home();
void ui_show_data_picker();
void ui_show_cluster(int c);
void ui_show_bottle(int c, int b);
void ui_show_control();
void ui_show_info();

// Re-render the current route in place (called after fake pushes / serial).
void ui_refresh_current();
// Recompute bell state from NULL fields and repaint the status bar.
void ui_update_bell();
// Called after any data mutation: bell + content refresh.
void ui_shell_on_data();
// 41C warn / 45C trip evaluation against g.chamberTempC.
void ui_eval_overheat();

// Blocking modal (used for 41C/45C alerts and the bell NULL-field list).
// Only one modal at a time; a new show() replaces the old one.
typedef void (*UiAckFn)();
void ui_modal_show(const char *title, const char *body, const char *ackLabel, UiAckFn onAck);
void ui_modal_hide();
bool ui_modal_visible();

// Explicitly styled widget helpers (no theme). All screens must use these.
lv_obj_t *ui_mk_label(lv_obj_t *parent, const char *text, int x, int y, int w,
                      uint32_t color, const lv_font_t *font);
lv_obj_t *ui_mk_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       uint32_t bg);
lv_obj_t *ui_mk_card(lv_obj_t *parent, int x, int y, int w, int h);
