#pragma once
// Screen sleep: backlight off after 60 s without touch, unless the /setup login
// QR code is on screen. main.cpp calls sleep_note_input() from the
// touch read callback.
#include <Arduino.h>
void sleep_note_input();
void sleep_tick();
