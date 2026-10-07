#include "screen_sleep.h"

#include "provision_ap.h"
#include "ui_config.h"

static uint32_t s_lastInputMs = 0;
static bool s_sleeping = false;

void sleep_note_input() { s_lastInputMs = millis(); }

void sleep_tick() {
  if (s_lastInputMs == 0) s_lastInputMs = millis();
  // A QR code is on screen exactly while the provisioning AP is up (both
  // /setup and /info hide the canvas when the AP is down). Keep the backlight
  // on so the code stays scannable, and refresh the idle timer so the screen
  // gets a full UI_SLEEP_MS grace period after the AP stops instead of going
  // dark instantly.
  if (provision_ap_active()) {
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
