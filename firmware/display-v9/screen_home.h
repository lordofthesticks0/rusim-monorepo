#pragma once
// /home: run card (progress, start/stop, duration) and a status card.
#include <lvgl.h>
void screen_home_show(lv_obj_t *parent);
void screen_home_refresh();  // in-place update; no widget is rebuilt
void screen_home_hide();
void screen_home_poll();     // 1 s tick for elapsed time
