#pragma once
// /control: temp setpoint + stirrer toggle send commands to the WT32
// (fake: Serial log). POST enable is a Display-side hold-to-confirm toggle.
#include <lvgl.h>
void screen_control_show(lv_obj_t *parent);
// Polled from loop(): applies late DB sync to the open /control screen.
void screen_control_poll();
// Test interrupt line to WT32: Display IO11 -> WT32 IO14. Display idles LOW;
// IO14 is non-strapping so no WT32 boot-order constraint. The /control test
// button pulses it.
void int_test_init();
