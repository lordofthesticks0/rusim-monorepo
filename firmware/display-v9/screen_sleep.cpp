#include "screen_sleep.h"

#include "ui_config.h"

static uint32_t s_lastInputMs = 0;
static bool s_sleeping = false;

void sleep_note_input() { s_lastInputMs = millis(); }

void sleep_tick() {
  if (s_lastInputMs == 0) s_lastInputMs = millis();
  bool shouldSleep = (millis() - s_lastInputMs) > UI_SLEEP_MS;
  if (shouldSleep && !s_sleeping) {
    s_sleeping = true;
    digitalWrite(UI_BL_PIN, LOW);
    Serial.println("[FAKE] backlight OFF (30s idle)");
  } else if (!shouldSleep && s_sleeping) {
    s_sleeping = false;
    digitalWrite(UI_BL_PIN, HIGH);
    Serial.println("[FAKE] backlight ON (touch)");
  }
}
