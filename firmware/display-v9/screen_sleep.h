#pragma once
// Screen sleep: backlight off after 60 s without touch, unless the /setup login
// QR code is on screen. main.cpp passes every touch through
// sleep_filter_touch(), which also swallows the tap that wakes the screen.
#include <Arduino.h>
void sleep_note_input();
void sleep_tick();
// Turns the backlight on now and restarts the idle timer (alerts, modals).
void sleep_wake();
// Call from the touch read callback with the raw pressed state. Returns true
// while the touch must NOT reach LVGL: the press that woke a dark screen is
// swallowed until the finger lifts, so it cannot also press a widget.
bool sleep_filter_touch(bool pressed);
