#include "screen_setup.h"

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_shell.h"
// LV_USE_QRCODE and LV_USE_CANVAS are set in lv_conf.h. No LVGL theme is
// enabled, so the QR colours are set explicitly below.

static lv_obj_t *s_qr = nullptr;
static lv_obj_t *s_stepLabel = nullptr;
static lv_obj_t *s_urlLabel = nullptr;
static lv_obj_t *s_status = nullptr;

static void setup_close_success();
static void on_debug_bypass(lv_event_t *e);

// The QR canvas is 1-bit indexed and allocated by lv_draw_buf_create(), so a
// 300 px square costs ceil(300/8) * 300 = 11400 bytes of LVGL heap, not of
// internal RAM. LV_MEM_SIZE is 64 KB.
static const int kQrSize = 300;

static void status_show(const char *t) {
  if (s_status != nullptr) lv_label_set_text(s_status, t);
}

// The WT32 address last painted. The WT32 pushes NET on a timer, so the code is
// re-encoded only when the address changes. lv_qrcode_update() walks the whole
// canvas, which is too costly to run on every loop.
static char s_ipShown[LINK_IP_MAX + 1] = {0};
static bool s_addrPainted = false;

// Paints the code for http://<ip>/, the login page served by the WT32. Until the
// WT32 reports its address there is nothing to encode, so the code is hidden
// and a waiting message is shown instead.
static void paint_address() {
  if (s_qr == nullptr) return;

  char ip[LINK_IP_MAX + 1];
  link_serial_get_ip(ip, sizeof(ip));
  if (s_addrPainted && strcmp(ip, s_ipShown) == 0) return;
  s_addrPainted = true;
  strncpy(s_ipShown, ip, sizeof(s_ipShown) - 1);
  s_ipShown[sizeof(s_ipShown) - 1] = '\0';

  if (ip[0] == '\0') {
    lv_obj_set_hidden(s_qr, true);
    lv_label_set_text(s_stepLabel, "Waiting for the WT32 to report its IP address.");
    lv_label_set_text(s_urlLabel, "(waiting)");
    return;
  }

  char url[32];
  snprintf(url, sizeof(url), "http://%s/", ip);
  Serial.printf("[SETUP] login QR payload='%s'\n", url);
  lv_obj_set_hidden(s_qr, false);
  if (lv_qrcode_update(s_qr, url, strlen(url)) != LV_RESULT_OK) {
    Serial.println("[SETUP] QR encode failed");
  }
  lv_label_set_text(s_stepLabel, "Scan with your phone to open the login page.");
  lv_label_set_text(s_urlLabel, url);
}

void screen_setup_show(lv_obj_t *parent) {
  ui_mk_label(parent, "Setup: network login", 16, 6, 500, UI_COL_TEXT,
              &lv_font_montserrat_20);
  s_stepLabel = ui_mk_label(parent, "", 16, 34, 760, UI_COL_DIM,
                            &lv_font_montserrat_14);

  // QR on the left, address readout on the right. 300 px leaves a quiet zone and
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

  const int rx = 356;
  ui_mk_label(parent, "Login page:", rx, 64, 130, UI_COL_TEXT,
              &lv_font_montserrat_14);
  // Typed fallback for a phone whose camera will not read the code.
  s_urlLabel = ui_mk_label(parent, "", rx, 86, 400, UI_COL_ACCENT,
                           &lv_font_montserrat_14);

#if IS_DEBUG
  lv_obj_t *bypassBtn =
      ui_mk_button(parent, "DEBUG: bypass login", rx, 236, 300, 48, UI_COL_WARN);
  lv_obj_add_event_cb(bypassBtn, on_debug_bypass, LV_EVENT_CLICKED, nullptr);
#endif

  s_status = ui_mk_label(parent,
                         "Enter the portal username and password on the login page. "
                         "This display never sees them.",
                         24, 380, 740, UI_COL_WARN, &lv_font_montserrat_14);

  s_addrPainted = false;
  paint_address();
}

static void setup_close_success() {
  g.loggedIn = true;
  g.experimentRunning = false;
  g.experimentStartMs = 0;
  Serial.println("[SETUP] WT32 auth OK, logged in; start the run from /control");
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);

  // Modals are siblings of s_content, not children of it, so ui_show_home()
  // below does not clear them. Hide any still open or they sit on top of home.
  ui_modal_hide();

  s_qr = nullptr;
  s_stepLabel = nullptr;
  s_urlLabel = nullptr;
  s_status = nullptr;
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

// Poll the link flags each loop. LOGINREQ routes to /setup. RESULT drives the
// home route or a retry.
void screen_setup_poll() {
  if (link_serial_take_login_request()) {
    Serial.println("[SETUP] WT32 LOGINREQ -> showing setup");
    if (!ui_is_setup()) {
      ui_show_setup();
    } else {
      status_show("WT32 requests login. Open the login page to continue.");
    }
  }
  int res = LINK_RESULT_NONE;
  if (link_serial_take_result(res)) {
    if (res == LINK_RESULT_SUCCESS) {
      setup_close_success();
    } else if (res == LINK_RESULT_FAIL) {
      Serial.println("[SETUP] WT32 auth FAILED, staying in setup for retry");
      if (!ui_is_setup()) ui_show_setup();
      status_show("Wrong username or password. Re-enter them on the login page.");
    }
    return;
  }

  // The address can arrive after the route is up, so the code is checked while
  // the route is visible. The widgets are nulled on close.
  if (ui_is_setup()) paint_address();
}
