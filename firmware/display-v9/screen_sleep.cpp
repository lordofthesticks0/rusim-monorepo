#include "screen_sleep.h"

#include "ui_config.h"
#include "ui_shell.h"

static uint32_t s_lastInputMs = 0;
static bool s_sleeping = false;

void sleep_note_input() { s_lastInputMs = millis(); }

void sleep_tick() {
  if (s_lastInputMs == 0) s_lastInputMs = millis();
  // /setup shows the login QR code, which must stay scannable. Keep the
  // backlight on while it is up, and refresh the idle timer so the screen gets
  // a full UI_SLEEP_MS grace period after leaving /setup instead of going dark
  // at once.
  if (ui_is_setup()) {
    s_lastInputMs = millis();
    if (s_sleeping) {
      s_sleeping = false;
      digitalWrite(UI_BL_PIN, HIGH);
      Serial.println("[INFO] backlight ON (QR visible)");
    }
    return;
  }
  bool shouldSleep = (millis() - s_lastInputMs) > UI_SLEEP_MS;
  if (shouldSleep && !s_sleeping) {
    s_sleeping = true;
    digitalWrite(UI_BL_PIN, LOW);
    Serial.println("[INFO] backlight OFF (60s idle)");
  } else if (!shouldSleep && s_sleeping) {
    s_sleeping = false;
    digitalWrite(UI_BL_PIN, HIGH);
    Serial.println("[INFO] backlight ON (touch)");
  }
}
