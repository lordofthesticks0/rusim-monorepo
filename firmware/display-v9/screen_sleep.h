#pragma once
// Screen sleep: backlight off after 30 s without touch.
// main.cpp calls sleep_note_input() from the touch read callback.
#include <Arduino.h>
void sleep_note_input();
void sleep_tick();
