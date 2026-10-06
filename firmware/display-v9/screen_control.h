#pragma once
// /control: temp setpoint + stirrer toggle send commands to the WT32
// (fake: Serial log). POST enable is a Display-side hold-to-confirm toggle.
#include <lvgl.h>
void screen_control_show(lv_obj_t *parent);
// Polled from loop(): applies late DB sync to the open /control screen.
void screen_control_poll();
// The poll-request line to the WT32 (Display IO11 -> WT32 IO14) is owned by
// link_modbus: link_modbus_init() sets the pin up and link_notify_pulse()
// drives it. The /control test button calls the latter.
