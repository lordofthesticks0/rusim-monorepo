#pragma once
// /info: diagnostic-only, rebuilt fresh on every open.
#include <lvgl.h>
void screen_info_show(lv_obj_t *parent);
void screen_info_poll();  // call each loop(): keeps the network QR in step
