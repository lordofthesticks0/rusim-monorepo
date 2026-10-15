#pragma once

// Shared round hold-to-confirm button. Used by /control (stirrer, run) and
// /home (run). Replaces the old lv_switch + lv_bar hold pairs.
#include <lvgl.h>

typedef void (*UiHoldFn)(void *user);

lv_obj_t *ui_hold_button_create(lv_obj_t *parent, int x, int y, int size,
                                const char *symbol, bool initial,
                                UiHoldFn onCommit, void *user);
void ui_hold_button_set_state(lv_obj_t *btn, bool on);
void ui_hold_button_set_symbol(lv_obj_t *btn, const char *symbol);
bool ui_hold_button_state(lv_obj_t *btn);