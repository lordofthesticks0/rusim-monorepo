#include "screen_info.h"

#include "fake_data.h"
#include "link_modbus.h"
#include "provision_ap.h"
#include "ui_config.h"
#include "ui_shell.h"

static void on_back(lv_event_t *e) {
  (void)e;
  ui_show_home();
}

// --- network QR ---
//
// Same two-step flow as /setup (join code, then the credential-form URL), minus
// the "phone connected" modal: this route is for reconnecting a phone after the
// run has started, not for walking a first-time user through login. The SoftAP
// is raised from here on demand, so screen_setup_poll() must not tear it down
// while this route is up; see the guard there.
static lv_obj_t *s_qr = nullptr;
static lv_obj_t *s_step = nullptr;
static lv_obj_t *s_apBtn = nullptr;

// Payload currently painted, so the canvas is re-encoded only when it changes.
static char s_qrShown[PROVISION_QR_MAX] = {0};
// Step text currently in s_step, for the same reason.
static char s_stepShown[64] = {0};
// Button label currently set. String literals, so pointer identity is enough.
static const char *s_apBtnShown = nullptr;

static void step_show(const char *t) {
  if (s_step == nullptr || strcmp(s_stepShown, t) == 0) return;
  lv_label_set_text(s_step, t);
  strncpy(s_stepShown, t, sizeof(s_stepShown) - 1);
  s_stepShown[sizeof(s_stepShown) - 1] = '\0';
}

static void qr_show(const char *payload) {
  if (s_qr == nullptr || strcmp(s_qrShown, payload) == 0) return;
  lv_obj_set_hidden(s_qr, false);
  if (lv_qrcode_update(s_qr, payload, strlen(payload)) != LV_RESULT_OK) {
    Serial.println("[INFO] QR encode failed");
  }
  strncpy(s_qrShown, payload, sizeof(s_qrShown) - 1);
  s_qrShown[sizeof(s_qrShown) - 1] = '\0';
}

// Paints whichever code applies: the Wi-Fi join payload until a phone
// associates, then the URL of the credential form. Runs on show and every loop
// while the route is up, so an idle-timeout stop (provision_ap_poll) is
// reflected as well as the button.
static void info_qr_refresh() {
  if (s_qr == nullptr) return;

  if (s_apBtn != nullptr) {
    const char *want = provision_ap_active() ? "Stop" : "Start phone login";
    if (s_apBtnShown != want) {
      lv_label_set_text(lv_obj_get_child(s_apBtn, 0), want);
      s_apBtnShown = want;
    }
  }

  if (!provision_ap_active()) {
    // Hide rather than paint a decoy; clearing s_qrShown forces a fresh encode
    // when the AP comes back up.
    lv_obj_set_hidden(s_qr, true);
    s_qrShown[0] = '\0';
    step_show("Tap Start to show the Wi-Fi code.");
    return;
  }

  if (provision_ap_client_count() > 0) {
    step_show("Step 2: scan again to open the login page.");
    qr_show(provision_ap_url());
  } else {
    step_show("Step 1: scan to join this device's Wi-Fi.");
    static char wifiBuf[PROVISION_QR_MAX];
    provision_ap_wifi_qr_payload(wifiBuf, sizeof(wifiBuf));
    qr_show(wifiBuf);
  }
}

static void on_ap_toggle(lv_event_t *e) {
  (void)e;
  if (provision_ap_active()) {
    provision_ap_end();
  } else {
    provision_ap_begin();
  }
  info_qr_refresh();
}

void screen_info_poll() {
  if (ui_is_info()) info_qr_refresh();
}

void screen_info_show(lv_obj_t *parent) {
  // Not cached: rebuilt fresh on every open (locked decision). Network rows
  // are read live from the Modbus slave state the WT32 pushes each cycle.
  ui_mk_label(parent, "/info (fresh on open)", 16, 8, 520, UI_COL_TEXT,
              &lv_font_montserrat_20);

  char row[128];
  snprintf(row, sizeof(row), "Storage limits: %s", g.storageNote);
  ui_mk_label(parent, row, 16, 60, 520, UI_COL_TEXT, &lv_font_montserrat_14);

  char ip[LINK_IP_MAX + 1];
  link_modbus_get_ip(ip, sizeof(ip));
  if (ip[0] != '\0') {
    snprintf(row, sizeof(row), "IP address: %s (real, WT32)", ip);
  } else {
    snprintf(row, sizeof(row), "IP address: waiting for WT32...");
  }
  ui_mk_label(parent, row, 16, 100, 520, UI_COL_TEXT, &lv_font_montserrat_14);

  int pingMs = link_modbus_get_ping_ms();
  if (pingMs >= 0) {
    snprintf(row, sizeof(row), "Ping 1.1.1.1: %d ms (real, WT32)", pingMs);
  } else if (link_modbus_net_up()) {
    snprintf(row, sizeof(row), "Ping 1.1.1.1: no reply yet (real, WT32)");
  } else {
    snprintf(row, sizeof(row), "Ping 1.1.1.1: no data yet (link down?)");
  }
  ui_mk_label(parent, row, 16, 140, 520, UI_COL_TEXT, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Latest HTTP reply: %s", g.lastHttp);
  ui_mk_label(parent, row, 16, 180, 520, UI_COL_TEXT, &lv_font_montserrat_14);

  int dbLatest = link_modbus_get_db_latest();
  if (dbLatest == -2) {
    snprintf(row, sizeof(row), "DB latest exp: waiting for master sync...");
  } else {
    snprintf(row, sizeof(row), "DB latest exp: %d (next %d)%s", dbLatest, dbLatest + 1,
             link_modbus_time_ok() ? " NTP OK" : " NTP no sync");
  }
  ui_mk_label(parent, row, 16, 200, 520, UI_COL_TEXT, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Firmware version: %s", FW_VERSION);
  ui_mk_label(parent, row, 16, 220, 520, UI_COL_TEXT, &lv_font_montserrat_14);

  // Real MCU readings, labeled as real to distinguish from fake rows.
  snprintf(row, sizeof(row), "Heap free: %u B (real) | PSRAM: %u B (real)", ESP.getFreeHeap(),
           ESP.getPsramSize());
  ui_mk_label(parent, row, 16, 260, 520, UI_COL_DIM, &lv_font_montserrat_14);

  snprintf(row, sizeof(row), "Fetched at %lu ms (fake stamp)", millis());
  ui_mk_label(parent, row, 16, 300, 520, UI_COL_DIM, &lv_font_montserrat_14);

  // Network QR: 180 px in the right column. Smaller than the /setup code (300
  // px) because this route shows the whole diagnostic list beside it, and it is
  // still scannable at arm's length on the bench.
  s_qr = lv_qrcode_create(parent);
  lv_obj_set_pos(s_qr, 596, 52);
  lv_obj_set_size(s_qr, 180, 180);
  lv_qrcode_set_size(s_qr, 180);
  lv_qrcode_set_dark_color(s_qr, lv_color_hex(0x000000));
  lv_qrcode_set_light_color(s_qr, lv_color_hex(0xFFFFFF));
  lv_obj_set_hidden(s_qr, true);
  // Fresh object: forget the cached payload and step text or the first paint
  // would be skipped.
  s_qrShown[0] = '\0';
  s_stepShown[0] = '\0';
  s_apBtnShown = nullptr;

  s_step = ui_mk_label(parent, "Tap Start to show the Wi-Fi code.", 596, 240, 190,
                       UI_COL_DIM, &lv_font_montserrat_14);

  s_apBtn = ui_mk_button(parent, "Start phone login", 596, 292, 190, 48, UI_COL_CARD);
  lv_obj_add_event_cb(s_apBtn, on_ap_toggle, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *back = ui_mk_button(parent, "< Home", 16, 360, 140, 48, UI_COL_CARD);
  lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);

  info_qr_refresh();
}
