#pragma once
// /info: link, database, run, upload queue, sensor feed and UI-thread load.
#include <lvgl.h>
void screen_info_show(lv_obj_t *parent);
void screen_info_refresh();
void screen_info_hide();
void screen_info_poll();
