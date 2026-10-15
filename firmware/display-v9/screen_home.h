#pragma once
// /home: run console (redesigned). Status bar owns navigation.
#include <lvgl.h>
void screen_home_show(lv_obj_t *parent);
void screen_home_hide();
void screen_home_poll();