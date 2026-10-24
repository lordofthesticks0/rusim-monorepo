// Stubs so the sim links without the WT32 UART link, the SoftAP provisioning
// web server, or any ESP32 hardware. State is kept in RAM so the UI state
// machine (login gate -> RESULT -> /control) is still exercisable: CREDS in the
// sim serial protocol stages the credentials and reports them ready; RESULT
// stays NONE, mirroring "WT32 never answered".

#include <string.h>

#include "link_serial.h"
#include "provision_ap.h"

SimSerial Serial;
SimESPClass ESP;

static char s_user[LINK_USER_MAX];
static bool s_credsReady;
static int s_result = LINK_RESULT_NONE;
static bool s_expRunning;
static int s_expNum, s_durationH;
static char s_ip[LINK_IP_MAX];
static bool s_passthrough;

void link_serial_init() {}
void link_serial_poll() {}

bool link_serial_take_login_request() { return false; }

void link_serial_set_credentials(const char *user, const char *pass) {
  (void)pass;
  strncpy(s_user, user, sizeof(s_user) - 1);
  s_user[sizeof(s_user) - 1] = '\0';
  s_credsReady = true;
}
bool link_serial_creds_ready() { return s_credsReady; }
int link_serial_result() { return s_result; }
void link_serial_set_exp_state(bool running, int expNum, int durationH) {
  s_expRunning = running;
  s_expNum = expNum;
  s_durationH = durationH;
}
void link_serial_emit_experiment() {}
bool link_serial_take_result(int &out) {
  if (s_result == LINK_RESULT_NONE) return false;
  out = s_result;
  s_result = LINK_RESULT_NONE;
  return true;
}
void link_serial_get_ip(char *out, size_t n) {
  const char *src = s_ip[0] ? s_ip : "0.0.0.0";
  strncpy(out, src, n - 1);
  out[n - 1] = '\0';
}
int link_serial_get_ping_ms() { return -1; }
bool link_serial_net_up() { return false; }
uint32_t link_serial_net_age_ms() { return 0xFFFFFFFFu; }
int link_serial_get_db_latest() { return -2; }
bool link_serial_time_ok() { return false; }
int link_serial_get_db_bottles() { return -1; }
int link_serial_get_push_drops() { return 0; }
void link_serial_debug_print() {
  Serial.printf("[LINK] sim stub: creds_ready=%d result=%d run=%d exp=%d dur=%d\n",
                s_credsReady ? 1 : 0, s_result, s_expRunning ? 1 : 0, s_expNum,
                s_durationH);
}
bool link_serial_send_raw(const char *line) {
  (void)line;
  return false;
}
void link_serial_passthrough_set(bool on) { s_passthrough = on; }
bool link_serial_passthrough_active() { return s_passthrough; }
void link_serial_passthrough_write(const uint8_t *data, size_t n) {
  (void)data;
  (void)n;
}

static bool s_apActive = false;

void provision_ap_begin() { s_apActive = true; }
void provision_ap_end() { s_apActive = false; }
void provision_ap_poll() {}
bool provision_ap_active() { return s_apActive; }
const char *provision_ap_ssid() { return "RUSIM-SETUP-SIM"; }
const char *provision_ap_passphrase() { return "sim-passphrase"; }
bool provision_ap_hidden() { return true; }
void provision_ap_wifi_qr_payload(char *out, size_t n) {
  snprintf(out, n, "WIFI:T:WPA;S:%s;P:%s;H:true;;", provision_ap_ssid(),
           provision_ap_passphrase());
}
const char *provision_ap_url() { return "http://192.168.4.1/"; }
int provision_ap_client_count() { return 0; }
bool provision_ap_awaiting_verdict() { return false; }
