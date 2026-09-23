#pragma once
// /control: temp setpoint + stirrer toggle send commands to the WT32
// (fake: Serial log). POST enable is a Display-side hold-to-confirm toggle.
#include <lvgl.h>
void screen_control_show(lv_obj_t *parent);
