#include "provision_ap.h"

#include <WiFi.h>
#include <WebServer.h>
#include <esp_efuse.h>
#include <esp_system.h>

#include <string.h>

#include "link_modbus.h"
#include "ui_config.h"

// --- tunables -------------------------------------------------------------

// WPA2 needs 8..63 characters. The default derived below is 15.
static const size_t kPassMax = 63;

// Teardown if nobody touches the AP for this long. The result page has to stay
// reachable long enough for the phone to render the WT32's verdict, so this is
// minutes, not seconds. Any HTTP request resets it.
static const uint32_t kIdleTimeoutMs = 10UL * 60UL * 1000UL;

static const IPAddress kApIp(192, 168, 4, 1);
static const uint16_t kApPort = 80;
static const int kApChannel = 1;

// Cloak the SSID, so the access point does not appear in anyone's network list.
// The join QR carries the SSID, so the phone never has to discover it.
//
// One constant drives both the radio and the H: field of the QR payload, so the
// two cannot drift apart. That matters more than usual here: a payload that
// disagrees with the AP is rejected quietly, as a phone that scans the code and
// then does nothing.
//
// Build with -DPROVISION_AP_HIDDEN=0 to broadcast the SSID. Do that if the
// phone you are testing with will not auto-join a cloaked network from a QR
// code; see section 5 of docs/llm-generated/display-provisioning-ap.md.
#ifndef PROVISION_AP_HIDDEN
#define PROVISION_AP_HIDDEN 1
#endif

static const bool kHidden = PROVISION_AP_HIDDEN != 0;

// Override the default WPA2 passphrase. The default comes from the chip MAC,
// which is stable across reboots but predictable to anyone who knows the MAC.
// Set an explicit secret before deploying anything unattended.
#ifndef PROVISION_AP_PASSWORD
#define PROVISION_AP_PASSWORD nullptr
#endif

// --- state ----------------------------------------------------------------

static WebServer *s_server = nullptr;
static bool s_active = false;
static uint32_t s_lastRequestMs = 0;
static char s_ssid[32] = {0};
static char s_pass[kPassMax + 1] = {0};
static char s_url[32] = {0};
static char s_token[17] = {0};

// Bytes the browser may send us. Bounded so a large POST cannot push the
// webserver past the display's remaining internal heap.
static const size_t kMaxUserLen = LINK_USER_MAX - 1;
static const size_t kMaxPassLen = LINK_PASS_MAX - 1;

static void note_activity() { s_lastRequestMs = millis(); }

// --- identity -------------------------------------------------------------

// Both the SSID and the default passphrase come from the base MAC, so they are
// stable across reboots and unique per board. A phone that joined once can
// reconnect without rereading anything.
//
// Both are built from hex digits and a dash. That is a requirement, not a
// style choice: they are interpolated into a WIFI: QR payload, which has to
// escape the characters "\ ; , :". Nothing here can produce any of them. A
// change that puts punctuation into either string needs an escaper added.
static void build_identity() {
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);

  snprintf(s_ssid, sizeof(s_ssid), "RUSIM-SETUP-%02X%02X%02X", mac[3], mac[4],
           mac[5]);

  const char *override = PROVISION_AP_PASSWORD;
  if (override != nullptr && strlen(override) >= 8 && strlen(override) <= kPassMax) {
    strncpy(s_pass, override, kPassMax);
    s_pass[kPassMax] = '\0';
  } else {
    if (override != nullptr) {
      Serial.println(
          "[AP] PROVISION_AP_PASSWORD must be 8-63 characters; ignoring it and "
          "using the MAC-derived default.");
    }
    // "setup-" + 8 hex digits = 15 characters.
    snprintf(s_pass, sizeof(s_pass), "setup-%02X%02X%02X%02X", mac[0], mac[1],
             mac[2], mac[3]);
  }

  snprintf(s_url, sizeof(s_url), "http://%s/", kApIp.toString().c_str());

  uint32_t r = esp_random();
  snprintf(s_token, sizeof(s_token), "%08lx%08lx", (unsigned long)(r & 0xFFFFFFFF),
           (unsigned long)(esp_random() & 0xFFFFFFFF));
}

// WIFI: URI, per the Wi-Fi Alliance WPA3 specification section 7. This is the
// format ZXing defined and Android 10+ and iOS 11+ read with the stock camera.
//
// All four fields are always present, even though a cloaked WPA2 network is
// fully described by three of them. A payload with a field missing is valid for
// most scanners, but the failure mode when one guesses wrong is a phone that
// scans the code and then does nothing, which reads as "the QR is broken". The
// two fields describing the radio are generated from the same constants that
// configure the radio.
//
// T:WPA, not WPA2. The format has exactly one token for every passphrase
// network and the phone negotiates the actual protocol during the join; "WPA2"
// is not a legal token and behaves unpredictably across handsets. T:nopass is
// the open-network form and would require P to be absent.
//
// The payload must not end in a newline, which makes both iOS and Android
// reject the code.
void provision_ap_wifi_qr_payload(char *out, size_t n) {
  if (out == nullptr || n == 0) return;
  snprintf(out, n, "WIFI:T:WPA;S:%s;P:%s;H:%s;;", s_ssid, s_pass,
           kHidden ? "true" : "false");
}

// --- pages ----------------------------------------------------------------
//
// Nothing user-supplied is ever interpolated into these strings. The staged
// username is reported through /status and inserted with textContent on the
// client, so there is no HTML injection path to reason about.

static const char kPageHead[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>RUSIM setup</title><style>"
    "body{font-family:system-ui,sans-serif;background:#0b0e14;color:#e8ecf4;"
    "margin:0;padding:24px}"
    "main{max-width:420px;margin:0 auto}"
    "h1{font-size:20px;margin:0 0 4px}"
    "p.sub{color:#8a93a6;margin:0 0 24px;font-size:14px}"
    "label{display:block;font-size:14px;color:#8a93a6;margin:0 0 6px}"
    "input{width:100%;box-sizing:border-box;padding:12px;margin:0 0 16px;"
    "border-radius:8px;border:1px solid #2a3350;background:#131a29;"
    "color:#e8ecf4;font-size:16px}"
    "button{width:100%;padding:14px;border:0;border-radius:8px;"
    "background:#55c7e8;color:#0b0e14;font-size:16px;font-weight:600}"
    "#out{margin-top:24px;padding:16px;border-radius:8px;background:#131a29;"
    "border:1px solid #2a3350;font-size:14px;white-space:pre-wrap;display:none}"
    "</style></head><body><main>";

static const char kPageTail[] = "</main></body></html>";

// The handlers below are handed to WebServer::on(), which takes
// std::function<void()>. They reach the server through s_server instead of a
// parameter.
static WebServer *srv() { return s_server; }

static void send_form() {
  String page;
  page.reserve(2048);
  page += kPageHead;
  page += "<h1>Portal login</h1>";
  page += "<p class=sub>Sent to the WT32 over Modbus, then to the campus "
          "portal. Held in memory only.</p>";
  page += "<form method=POST action=/creds>";
  page += "<input type=hidden name=t value=\"";
  page += s_token;
  page += "\">";
  page += "<label>Username</label><input name=user autocomplete=username "
          "autocapitalize=none spellcheck=false required>";
  page += "<label>Password</label><input name=pass type=password "
          "autocomplete=current-password required>";
  page += "<label>Duration (hours, 1-99)</label>";
  page += "<input name=dur type=number min=1 max=99 value=\"";
  page += String(UI_DURATION_DEFAULT_H);
  page += "\">";
  page += "<button type=submit>Send to WT32</button>";
  page += "</form>";
  page += "<div id=out></div>";
  page += kPageTail;
  srv()->send(200, "text/html", page);
}

static void send_result_page() {
  String page;
  page.reserve(1600);
  page += kPageHead;
  page += "<h1>Sending to WT32</h1>";
  page += "<p class=sub>Leave this page open. The WT32 is logging in to the "
          "campus portal now.</p>";
  page += "<div id=out>Waiting for the WT32 to report back...</div>";
  page += "<script>"
          "const o=document.getElementById('out');"
          "o.style.display='block';"
          "async function poll(){try{"
          "const r=await fetch('/status',{cache:'no-store'});"
          "const t=await r.text();o.textContent=t;"
          "if(t.indexOf('Waiting')<0)return;}"
          "catch(e){o.textContent='Lost contact with the display. Is the "
          "access point still in range?';return;}"
          "setTimeout(poll,1500);}poll();</script>";
  page += kPageTail;
  srv()->send(200, "text/html", page);
}

static void send_error_page(const char *what) {
  String page;
  page.reserve(800);
  page += kPageHead;
  page += "<h1>";
  page += what;
  page += "</h1><p class=sub>Reload the page from the device screen and try "
          "again.</p>";
  page += kPageTail;
  srv()->send(400, "text/html", page);
}

// --- handlers -------------------------------------------------------------

static void handle_status() {
  // Non-destructive peek. screen_setup_poll() owns link_modbus_take_result(),
  // so this must not consume the verdict.
  int res = link_modbus_result();

  String out;
  if (res == LINK_RESULT_SUCCESS) {
    out = "Login succeeded. The port is authenticated.";
  } else if (res == LINK_RESULT_FAIL) {
    out = "The WT32 rejected these credentials. Check the username and "
          "password, then send again.";
  } else if (link_modbus_creds_ready()) {
    out = "Credentials received by the display. Waiting for the WT32 login to "
          "finish...";
  } else {
    out = "Waiting for credentials...";
  }
  srv()->send(200, "text/plain", out);
}

static void handle_creds() {
  note_activity();

  // Without this, any page open in a browser on the AP could blind-POST to
  // the device. The token is only obtainable by loading "/" first.
  // arg() returns a String, hence the explicit c_str().
  String token = srv()->arg("t");
  if (token != s_token) {
    Serial.println("[AP] POST /creds rejected: bad token");
    srv()->send(403, "text/plain", "Bad or missing token. Reload the form.");
    return;
  }

  String user = srv()->arg("user");
  String pass = srv()->arg("pass");

  // Strip surrounding whitespace: phone keyboards love to add a trailing
  // space, and the portal would reject it as a wrong password.
  user.trim();
  pass.trim();

  if (user.length() == 0 || pass.length() == 0) {
    send_error_page("Username and password are both required");
    return;
  }
  if (user.length() > kMaxUserLen || pass.length() > kMaxPassLen) {
    send_error_page("Username or password is too long");
    return;
  }

  int dur = UI_DURATION_DEFAULT_H;
  if (srv()->hasArg("dur")) {
    dur = srv()->arg("dur").toInt();
  }
  if (dur < UI_DURATION_MIN_H) dur = UI_DURATION_MIN_H;
  if (dur > UI_DURATION_MAX_H) dur = UI_DURATION_MAX_H;

  // The password is never logged. link_modbus_set_credentials() holds the
  // strings in display RAM until the WT32 reads them; the WT32 overwrites its
  // own copy after login. Nothing reaches flash.
  Serial.printf("[AP] staged user='%s' dur=%dh over the air\n", user.c_str(), dur);

  link_modbus_set_credentials(user.c_str(), pass.c_str(), dur);

  // The plaintext String buffers hold the password in heap. Drop them now.
  pass = String();
  user = String();

  send_result_page();
}

// --- lifecycle ------------------------------------------------------------

void provision_ap_begin() {
  if (s_active) return;

  if (s_server == nullptr) {
    s_server = new WebServer(kApPort);
    if (s_server == nullptr) {
      Serial.println("[AP] ERROR: out of memory allocating the web server");
      return;
    }
  }

  build_identity();

  WiFi.mode(WIFI_AP);
  // WPA2, and cloaked if PROVISION_AP_HIDDEN. Channel 1 so a phone that has
  // already cached this network from a previous session reconnects without a
  // channel scan.
  bool ok = WiFi.softAP(s_ssid, s_pass, kApChannel, kHidden ? 1 : 0);
  if (!ok) {
    Serial.println("[AP] ERROR: softAP() failed");
    WiFi.mode(WIFI_OFF);
    return;
  }

  // The core picks 192.168.4.1 by default; setting it explicitly keeps the
  // URLs in the docs honest if that ever changes.
  WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));

  s_server->on("/", HTTP_GET, []() {
    note_activity();
    send_form();
  });
  s_server->on("/creds", HTTP_POST, handle_creds);
  s_server->on("/status", HTTP_GET, handle_status);
  s_server->onNotFound([]() {
    note_activity();
    s_server->send(404, "text/plain", "Not found.");
  });
  s_server->begin();

  s_active = true;
  note_activity();

  Serial.printf("[AP] up: ssid='%s' wpa2 hidden=%d at %s\n", s_ssid,
                kHidden ? 1 : 0, s_url);
  Serial.printf("[AP] heap free: %u bytes\n", ESP.getFreeHeap());
}

void provision_ap_end() {
  if (!s_active) return;

  if (s_server != nullptr) {
    s_server->stop();
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  s_active = false;
  s_ssid[0] = '\0';
  s_pass[0] = '\0';
  s_url[0] = '\0';
  s_token[0] = '\0';

  Serial.println("[AP] down");
}

void provision_ap_poll() {
  if (!s_active) return;

  // handleClient() returns immediately when nothing is connected, so this
  // costs a few microseconds per loop() outside of an actual request.
  s_server->handleClient();

  // Cover the "walked away from the device" case: a phone that got a verdict
  // walks away too, and the AP would otherwise stay up all night.
  if (millis() - s_lastRequestMs >= kIdleTimeoutMs) {
    Serial.println("[AP] idle timeout, shutting down");
    provision_ap_end();
  }
}

bool provision_ap_active() { return s_active; }

const char *provision_ap_ssid() { return s_ssid; }

const char *provision_ap_passphrase() { return s_pass; }

bool provision_ap_hidden() { return kHidden; }

const char *provision_ap_url() { return s_url; }

int provision_ap_client_count() {
  if (!s_active) return 0;
  return (int)WiFi.softAPgetStationNum();
}

bool provision_ap_awaiting_verdict() {
  return s_active && link_modbus_creds_ready() &&
         link_modbus_result() == LINK_RESULT_NONE;
}
