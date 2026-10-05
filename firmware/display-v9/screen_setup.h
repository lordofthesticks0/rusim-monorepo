#pragma once
// /setup: first-class route for portal login + start-experiment. Shown first
// on boot and whenever the WT32 polls LOGIN_REQ over Modbus.
//
// There is no on-screen keyboard. The user taps "Start phone login", which
// raises an open SoftAP, then scans the QR code on this screen with a phone:
// step 1 joins the phone to the AP, step 2 opens the credential form. Either
// way the credentials end up staged for the WT32 master to read. The run
// starts only after the WT32 reports RESULT=success, which routes to /home.
#include <lvgl.h>
void screen_setup_show(lv_obj_t *parent);
void screen_setup_poll();  // call each loop(): master requests + results
