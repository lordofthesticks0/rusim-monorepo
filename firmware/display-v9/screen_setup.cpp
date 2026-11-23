#include "screen_setup.h"

#include <string.h>

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_shell.h"
// LV_USE_QRCODE and LV_USE_CANVAS are set in lv_conf.h. No LVGL theme is
// enabled, so the QR colours are set explicitly below.

// Layout: QR in a white 316 x 316 box on the left (the box is the quiet zone),
// numbered steps, typed URL and status on the right.
static lv_obj_t *s_qr = nullptr;
static lv_obj_t *s_waiting = nullptr;
static lv_obj_t *s_urlLabel = nullptr;
static lv_obj_t *s_status = nullptr;

// Status line state survives route changes, so re-opening /setup shows the
// latest message (e.g. a failed login).
static const char *s_statusText = nullptr;
static uint32_t s_statusCol = UI_COL_DIM;

static void setup_close_success();
static void on_debug_bypass(lv_event_t *e);

// The QR canvas is 1-bit indexed and allocated by lv_draw_buf_create(), so a
// 280 px square costs ceil(280/8) * 280 = 9800 bytes of LVGL heap.
static const int kQrSize = 280;
static const int kBox = 316;

static const char *kDefaultStatus = "This display never sees your username or password.";
static const char *kReloginStatus =
    "The network login expired. The experiment keeps running; "
    "uploads wait until you log in again.";

static void status_show(const char *t, uint32_t col) {
  s_statusText = t;
  s_statusCol = col;
  if (s_status == nullptr) return;
  lv_label_set_text(s_status, t);
  lv_obj_set_style_text_color(s_status, lv_color_hex(col), LV_PART_MAIN);
}

// The WT32 address last painted. The WT32 pushes NET on a timer, so the code is
// re-encoded only when the address changes. lv_qrcode_update() walks the whole
// canvas, which is too costly to run on every loop.
static char s_ipShown[LINK_IP_MAX + 1] = {0};
static bool s_addrPainted = false;

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
    lv_obj_set_hidden(s_waiting, false);
    lv_label_set_text(s_urlLabel, "--");
    return;
  }

  char url[32];
  snprintf(url, sizeof(url), "http://%s/", ip);
  Serial.printf("[SETUP] login QR payload='%s'\n", url);
  lv_obj_set_hidden(s_waiting, true);
  lv_obj_set_hidden(s_qr, false);
  if (lv_qrcode_update(s_qr, url, strlen(url)) != LV_RESULT_OK) {
    Serial.println("[SETUP] QR encode failed");
  }
  lv_label_set_text(s_urlLabel, url);
}

void screen_setup_show(lv_obj_t *parent) {
  ui_mk_page_title(parent, "Network login", nullptr, nullptr);

  // White box = quiet zone; also keeps the layout stable while waiting.
  lv_obj_t *box = ui_mk_card(parent, UI_MARGIN, UI_PAGE_TOP, kBox, kBox);
  lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
  s_qr = lv_qrcode_create(box);
  lv_qrcode_set_size(s_qr, kQrSize);
  lv_obj_center(s_qr);
  // Black modules on a white field: phone cameras key on the light field.
  lv_qrcode_set_dark_color(s_qr, lv_color_hex(0x000000));
  lv_qrcode_set_light_color(s_qr, lv_color_hex(0xFFFFFF));
  s_waiting = ui_mk_label(box,
                          "Waiting for the WT32 to report its address.\n\n"
                          "Check that the WT32 is powered and its Ethernet cable "
                          "is plugged in.",
                          0, 0, kBox - 48, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_waiting, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_center(s_waiting);

  // Right column: flex column so wrapped lines push the next item down.
  const int rx = UI_MARGIN + kBox + 24;
  const int rw = 800 - UI_MARGIN - rx;
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_set_pos(col, rx, UI_PAGE_TOP);
  lv_obj_set_size(col, rw, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(col, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(col, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(col, 12, LV_PART_MAIN);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scrollable(col, false);

  static const char *const kSteps[] = {
      "1.  Scan the code with your phone.",
      "2.  Log in to the portal on the page that opens.",
      "3.  Wait here. This screen moves on by itself.",
  };
  for (const char *step : kSteps) {
    lv_obj_t *l = ui_mk_label(col, step, 0, 0, rw, UI_COL_TEXT, &lv_font_montserrat_20);
    lv_obj_set_width(l, lv_pct(100));
  }
  lv_obj_t *or_ = ui_mk_label(col, "Or type this address:", 0, 0, rw, UI_COL_DIM,
                              &lv_font_montserrat_14);
  lv_obj_set_style_margin_top(or_, 8, LV_PART_MAIN);
  s_urlLabel = ui_mk_label(col, "", 0, 0, rw, UI_COL_ACCENT, &lv_font_montserrat_28);
  s_status = ui_mk_label(col, "", 0, 0, rw, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_margin_top(s_status, 8, LV_PART_MAIN);

#if IS_DEBUG
  // Development tool (kept for now, audit 3.10).
  lv_obj_t *bypassBtn =
      ui_mk_button(parent, "DEBUG: bypass login", rx, UI_PAGE_BOTTOM - 44, 240, 44, UI_COL_CARD);
  lv_obj_add_event_cb(bypassBtn, on_debug_bypass, LV_EVENT_CLICKED, nullptr);
#endif

  if (s_statusText == nullptr) s_statusText = kDefaultStatus;
  status_show(s_statusText, s_statusCol);
  s_addrPainted = false;
  paint_address();
}

void screen_setup_hide() {
  s_qr = nullptr;
  s_waiting = nullptr;
  s_urlLabel = nullptr;
  s_status = nullptr;
}

// First successful login since boot. The run state is NOT touched: it is
// false at boot unless an active run was adopted from the WT32, and forcing
// it false here used to stop a running experiment (audit finding 2.1).
static void setup_close_success() {
  g.loggedIn = true;
  Serial.println("[SETUP] WT32 auth OK, logged in");
  status_show(kDefaultStatus, UI_COL_DIM);
  // Modals are siblings of the content area, so a route change keeps them.
  ui_modal_hide();
  ui_show_home();
  ui_update_status_bar();
}

static void on_debug_bypass(lv_event_t *e) {
  (void)e;
#if IS_DEBUG
  Serial.println("[SETUP] DEBUG bypass login -> home");
  setup_close_success();
#endif
}

static void open_setup() { ui_show_setup(); }

// Poll the link flags each loop. Before the first login, LOGINREQ/RESULT drive
// this route directly. After it (portal session expired), the operator is
// told with a modal and the status-bar chip instead of being pulled away
// from the open screen, and a success returns to where they were.
void screen_setup_poll() {
  if (link_serial_take_login_request()) {
    if (g.loggedIn) {
      Serial.println("[SETUP] WT32 LOGINREQ after login: session expired");
      status_show(kReloginStatus, UI_COL_WARN);
      if (!ui_is_setup()) {
        ui_modal_show_sev("Network login needed",
                          "The WT32 lost its portal login.\n"
                          "The experiment keeps running. Uploads wait until\n"
                          "you log in again.",
                          "Open login", open_setup, UI_SEV_WARN);
      }
    } else {
      Serial.println("[SETUP] WT32 LOGINREQ -> showing setup");
      status_show("The WT32 is waiting for the login.", UI_COL_DIM);
      if (!ui_is_setup()) ui_show_setup();
    }
  }
  int res = LINK_RESULT_NONE;
  if (link_serial_take_result(res)) {
    if (res == LINK_RESULT_SUCCESS) {
      if (g.loggedIn) {
        Serial.println("[SETUP] WT32 re-login OK");
        status_show(kDefaultStatus, UI_COL_DIM);
        ui_modal_hide();
        if (ui_is_setup()) ui_show_previous();
        ui_update_status_bar();
      } else {
        setup_close_success();
      }
    } else if (res == LINK_RESULT_FAIL) {
      Serial.println("[SETUP] WT32 auth FAILED, waiting for a retry");
      status_show("Login failed: wrong username or password. Try again on your phone.",
                  UI_COL_WARN);
      if (!g.loggedIn && !ui_is_setup()) ui_show_setup();
    }
    return;
  }

  // The address can arrive after the route is up; widgets are nulled on hide.
  if (ui_is_setup()) paint_address();
}
