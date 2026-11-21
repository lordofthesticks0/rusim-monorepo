#pragma once

// Shared round hold-to-confirm button (2000 ms, progress ring). Used by
// /control (run, stirrers, water pump) and /home (run). On = UI_COL_OK fill,
// off/idle = UI_COL_IDLE fill; red is reserved for faults. Callers show the
// state in words next to the button, because colour alone is not enough.
#include <lvgl.h>

typedef void (*UiHoldFn)(void *user);

lv_obj_t *ui_hold_button_create(lv_obj_t *parent, int x, int y, int size,
                                const char *symbol, bool initial,
                                UiHoldFn onCommit, void *user);
void ui_hold_button_set_state(lv_obj_t *btn, bool on);
void ui_hold_button_set_symbol(lv_obj_t *btn, const char *symbol);
bool ui_hold_button_state(lv_obj_t *btn);
