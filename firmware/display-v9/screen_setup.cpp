#include "screen_setup.h"

#include "fake_data.h"
#include "link_modbus.h"
#include "provision_ap.h"
#include "ui_config.h"
#include "ui_shell.h"
// LV_USE_QRCODE and LV_USE_CANVAS are set in lv_conf.h. No LVGL theme is
// enabled, so the QR colours are set explicitly below.

static lv_obj_t *s_qr = nullptr;
static lv_obj_t *s_stepLabel = nullptr;
static lv_obj_t *s_ssidLabel = nullptr;
static lv_obj_t *s_passLabel = nullptr;
static lv_obj_t *s_status = nullptr;
static lv_obj_t *s_apBtn = nullptr;
static bool s_built = false;

static void setup_close_success();
static void on_debug_bypass(lv_event_t *e);

// The QR canvas is 1-bit indexed and allocated by lv_draw_buf_create(), so a
// 300 px square costs ceil(300/8) * 300 = 11400 bytes of LVGL heap, not of
// internal RAM. LV_MEM_SIZE is 64 KB. Two codes of this size would be 22.8 KB,
// which would fit, but one object re-encoded in place is simpler and leaves
// the headroom: the spinbox, labels, and chart all draw from the same pool.
static const int kQrSize = 300;

static void status_show(const char *t) {
  if (s_status != nullptr) lv_label_set_text(s_status, t);
}

// The payload currently painted on the canvas.
//
// lv_qrcode_update() clears the canvas and walks the whole buffer, so it runs
// only when the text differs from this. Tracking the string rather than a
// two-value stage enum means every path that changes the payload is handled the
// same way, including the ones that do not go through the Start button: the idle
// timeout in provision_ap_poll(), and returning to the route. An earlier version
// tracked a stage and had to poke it to a wrong value to force a repaint, which
// missed the idle-timeout path.
static char s_qrShown[PROVISION_QR_MAX] = {0};

static void qr_show(const char *payload) {
  if (s_qr == nullptr) return;
  if (strcmp(s_qrShown, payload) == 0) return;
  Serial.printf("[SETUP] QR show len=%d payload='%s'\n", (int)strlen(payload),
                payload);
  lv_obj_set_hidden(s_qr, false);
  if (lv_qrcode_update(s_qr, payload, strlen(payload)) != LV_RESULT_OK) {
    Serial.println("[SETUP] QR encode failed");
  }
  strncpy(s_qrShown, payload, sizeof(s_qrShown) - 1);
  s_qrShown[sizeof(s_qrShown) - 1] = '\0';
}

// Latch for the association popup. qr_refresh() runs every loop, so without this
// the modal would be rebuilt on every pass. Cleared whenever the count returns to
// zero, so the next phone to associate pops it again.
//
// Latched on the client count rather than on the painted payload: the payload
// also changes when the AP is stopped and restarted with a phone still attached,
// which is not a phone arriving and should not raise the popup.
static bool s_phoneSeen = false;

// The step text currently in s_stepLabel.
//
// lv_label_set_text() reallocates and re-wraps the text buffer on every call, and
// qr_refresh() runs every loop, so setting it unconditionally churns a heap
// allocation about 200 times a second for a string that changes twice per login.
static char s_stepShown[64] = {0};

// Paints whichever of the two codes applies right now. The phone cannot open the
// URL until it has joined the AP, so the join code goes first and the URL code
// replaces it once a station associates. Going back the other way matters too: a
// phone that walks away and returns has to be told how to rejoin.
static void qr_refresh() {
  int clients = provision_ap_active() ? provision_ap_client_count() : 0;

  if (!provision_ap_active()) {
    // Hide instead of painting a decoy: a scannable " " code reads as a
    // broken QR. Clearing s_qrShown forces a fresh encode on next Start.
    if (s_qr != nullptr) lv_obj_set_hidden(s_qr, true);
    s_qrShown[0] = '\0';
    // Re-arm the popup: the next association after a Start is a new arrival.
    s_phoneSeen = false;
  } else if (clients > 0) {
    static char urlBuf[PROVISION_QR_MAX];
    snprintf(urlBuf, sizeof(urlBuf), "%s", provision_ap_url());
    qr_show(urlBuf);
    if (!s_phoneSeen) {
      s_phoneSeen = true;
      Serial.println("[SETUP] client associated -> prompt for second scan");
      // Deliberately covers the QR. The replacement code is a different payload
      // in the same square at the same size, so nothing about the panel itself
      // says it changed, and the user is looking at their phone rather than the
      // screen. Blocking on OK is the only thing that reliably brings their eyes
      // back, and it doubles as the moment they are told what to do next.
      ui_modal_show("Phone connected",
                    "Your phone joined this device's Wi-Fi.\n"
                    "\n"
                    "Press OK, then scan the code now on screen to open the "
                    "login page and enter the portal credentials.",
                    "OK", nullptr);
    }
  } else {
    s_phoneSeen = false;
    static char wifiBuf[PROVISION_QR_MAX];
    provision_ap_wifi_qr_payload(wifiBuf, sizeof(wifiBuf));
    qr_show(wifiBuf);
  }

  if (s_stepLabel == nullptr) return;
  const char *step;
  if (!provision_ap_active()) {
    step = "Tap Start to show the Wi-Fi code.";
  } else if (clients > 0) {
    step = "Step 2: scan again to open the login page.";
  } else {
    // Kept to one line: the label is 20 px tall and a wrap would push it down
    // over the QR. The manual-join fallback is the SSID and passphrase readout
    // on the right, so it needs no explanation here.
    step = "Step 1: scan to join this device's Wi-Fi.";
  }
  if (strcmp(s_stepShown, step) != 0) {
    lv_label_set_text(s_stepLabel, step);
    strncpy(s_stepShown, step, sizeof(s_stepShown) - 1);
    s_stepShown[sizeof(s_stepShown) - 1] = '\0';
  }
}

// Raises or tears down the SoftAP the phone form is served from. The AP is
// off by default and dies on the idle timeout in provision_ap_poll(), so
// this is a deliberate act rather than a mode the device sits in.
//
// The SSID and passphrase readouts have to follow the AP up and down, so they
// are rebuilt here rather than only in screen_setup_show().
static void join_details_refresh() {
  if (s_ssidLabel == nullptr) return;

  static char line[64];
  if (!provision_ap_active()) {
    snprintf(line, sizeof(line), "(off)");
  } else if (provision_ap_hidden()) {
    snprintf(line, sizeof(line), "%s  (hidden)", provision_ap_ssid());
  } else {
    snprintf(line, sizeof(line), "%s", provision_ap_ssid());
  }
  lv_label_set_text(s_ssidLabel, line);

  static char pass[80];
  snprintf(pass, sizeof(pass), "%s",
           provision_ap_active() ? provision_ap_passphrase() : "-");
  lv_label_set_text(s_passLabel, pass);
}

static void on_ap_toggle(lv_event_t *e) {
  (void)e;
  if (provision_ap_active()) {
    provision_ap_end();
    status_show("Stopped. Tap Start to show the Wi-Fi code again.");
  } else {
    provision_ap_begin();
    status_show(provision_ap_active()
                    ? "Scan the code with your phone's camera app."
                    : "Could not start the access point.");
  }
  qr_refresh();
  join_details_refresh();
  if (s_apBtn != nullptr) {
    lv_label_set_text(lv_obj_get_child(s_apBtn, 0),
                      provision_ap_active() ? "Stop" : "Start phone login");
  }
}

void screen_setup_show(lv_obj_t *parent) {
  ui_mk_label(parent, "Setup: network login", 16, 6, 500, UI_COL_TEXT,
              &lv_font_montserrat_20);
  s_stepLabel = ui_mk_label(parent, "Tap Start to show the Wi-Fi code.", 16, 34,
                            760, UI_COL_DIM, &lv_font_montserrat_14);

  // QR on the left, controls on the right. 300 px leaves a quiet zone and
  // stays scannable from roughly arm's length on the bench.
  s_qr = lv_qrcode_create(parent);
  lv_obj_set_pos(s_qr, 24, 64);
  lv_obj_set_size(s_qr, kQrSize, kQrSize);
  lv_qrcode_set_size(s_qr, kQrSize);
  // Black modules on a white field. Phone cameras key on the light field, and
  // the light theme sets the panel background to UI_COL_BG, so a light code
  // would vanish into it.
  lv_qrcode_set_dark_color(s_qr, lv_color_hex(0x000000));
  lv_qrcode_set_light_color(s_qr, lv_color_hex(0xFFFFFF));
  // Fresh canvas, so nothing is painted yet. Forget the previous payload too:
  // the object is new and blank, and leaving the string set would skip the
  // first paint. Stay hidden until qr_refresh() below paints a real code.
  s_qrShown[0] = '\0';
  // The label is a new object, but it is built with the same string the AP-down
  // branch uses, so the cache has to be cleared or that first paint is skipped.
  s_stepShown[0] = '\0';
  lv_obj_set_hidden(s_qr, true);

  const int rx = 356;
  ui_mk_label(parent, "Network:", rx, 64, 130, UI_COL_TEXT,
              &lv_font_montserrat_14);
  s_ssidLabel = ui_mk_label(parent, "(off)", rx, 86, 400, UI_COL_ACCENT,
                            &lv_font_montserrat_14);

  // The SSID and passphrase are on screen as a fallback for a phone that will
  // not auto-join a cloaked network from a QR code. Both come off the MAC, so
  // neither is a secret worth protecting from someone already holding the
  // device, and the AP only exists while someone is standing here.
  ui_mk_label(parent, "Password:", rx, 116, 76, UI_COL_TEXT,
              &lv_font_montserrat_14);
  s_passLabel = ui_mk_label(parent, "-", rx + 80, 116, 260, UI_COL_ACCENT,
                            &lv_font_montserrat_14);

  s_apBtn = ui_mk_button(parent,
                         provision_ap_active() ? "Stop" : "Start phone login",
                         rx, 236, 300, 48, UI_COL_CARD);
  lv_obj_add_event_cb(s_apBtn, on_ap_toggle, LV_EVENT_CLICKED, nullptr);

#if IS_DEBUG
  lv_obj_t *bypassBtn =
      ui_mk_button(parent, "DEBUG: bypass login", rx, 292, 300, 48, UI_COL_WARN);
  lv_obj_add_event_cb(bypassBtn, on_debug_bypass, LV_EVENT_CLICKED, nullptr);
#endif

  s_status = ui_mk_label(parent, "Credentials go to the WT32 over Modbus.",
                         24, 380, 740, UI_COL_WARN, &lv_font_montserrat_14);

  join_details_refresh();
  qr_refresh();
  s_built = true;
}

static void setup_close_success() {
  g.loggedIn = true;
  g.experimentRunning = false;
  Serial.println("[SETUP] WT32 auth OK, logged in; start the run from /control");
  link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  // Never leave the credential form reachable once the run has started.
  provision_ap_end();

  // The modal is a sibling of s_content, not a child of it, so ui_show_home()
  // below cleans the route out from under it and leaves it on screen. Drop the
  // association popup here or it sits on top of the home screen until dismissed.
  ui_modal_hide();

  s_qr = nullptr;
  s_stepLabel = nullptr;
  s_ssidLabel = nullptr;
  s_passLabel = nullptr;
  s_status = nullptr;
  s_apBtn = nullptr;
  s_qrShown[0] = '\0';
  s_built = false;
  ui_show_home();
  ui_update_bell();
}

// Debug bypass: routes straight to /home without WT32 RESULT=success.
// Only reachable when IS_DEBUG=1 (button is not built otherwise).
static void on_debug_bypass(lv_event_t *e) {
  (void)e;
#if IS_DEBUG
  Serial.println("[SETUP] DEBUG bypass login -> home");
  setup_close_success();
#endif
}

// Poll the Modbus flags each loop. The master write of LOGIN_REQ routes to
// /setup. RESULT drives the home route or a retry.
void screen_setup_poll() {
  if (link_modbus_take_login_request()) {
    Serial.println("[SETUP] WT32 LOGIN_REQ poll -> showing setup");
    if (!ui_is_setup()) {
      ui_show_setup();
    } else {
      status_show("WT32 requests login. Scan the code on this screen.");
    }
  }
  int res = LINK_RESULT_NONE;
  if (link_modbus_take_result(res)) {
    if (res == LINK_RESULT_SUCCESS) {
      setup_close_success();
    } else if (res == LINK_RESULT_FAIL) {
      Serial.println("[SETUP] WT32 auth FAILED, staying in setup for retry");
      if (!ui_is_setup()) ui_show_setup();
      if (provision_ap_active()) {
        // The credentials came from the phone, so the phone is where they have
        // to be corrected. The result page shows the same verdict and offers a
        // link back to the form, which is served with the failure banner on it.
        status_show("Wrong username or password. Correct them on your phone "
                    "and resend.");
      } else {
        status_show("Wrong username or password.");
      }
    }
    return;
  }

  // The step shown depends on whether a phone has joined, so the QR has to be
  // repainted whenever that changes. Only when the route is up: the widgets
  // are nulled on close.
  if (ui_is_setup()) {
    qr_refresh();
    return;
  }

  // Leaving /setup closes the credential form: an AP that accepts passwords
  // should not outlive the screen that offers it. /info offers the same form
  // through its own QR, so while that route is up it owns the AP instead.
  if (provision_ap_active() && !ui_is_info()) {
    Serial.println("[SETUP] left /setup, stopping phone login");
    provision_ap_end();
  }
}
