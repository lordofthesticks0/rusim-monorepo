#pragma once
// /setup: first-class route for portal login + start-experiment. Shown first
// on boot and whenever the WT32 sends LOGINREQ over the serial link.
//
// The login page is served by the WT32 (network-master) over Ethernet. This
// screen shows a QR code for http://<WT32 IP>/, which the user scans with a
// phone and fills in there. The display never sees the credentials. The run
// starts only after the WT32 reports RESULT=success, which routes to /home.
#include <lvgl.h>
void screen_setup_show(lv_obj_t *parent);
void screen_setup_poll();  // call each loop(): master requests + results
