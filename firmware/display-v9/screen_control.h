#pragma once
// /control: setpoint, experiment number and duration, run, stirrers, pump.
#include <lvgl.h>
void screen_control_show(lv_obj_t *parent);
void screen_control_refresh();  // in-place update; no widget is rebuilt
void screen_control_hide();
void screen_control_poll();     // late DB sync -> status line
