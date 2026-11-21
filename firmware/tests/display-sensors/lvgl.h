#pragma once

// ui_shell.h only needs opaque widget/font/event types here. No LVGL functions
// or rendering are implemented: the host tests cover USB command delegation.
struct lv_obj_t;
struct lv_font_t;
struct lv_event_t;
typedef void (*lv_event_cb_t)(lv_event_t *e);
