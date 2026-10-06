// Stubs so the sim links without the WT32 UART/Modbus, the SoftAP
// provisioning web server, or any ESP32 hardware. State is kept in RAM so
// the UI state machine (login gate -> RESULT -> /control) is still exercisable:
// CREDS in the sim serial protocol stages the credentials and reports them
// ready; RESULT stays NONE, mirroring "WT32 never answered".

#include <string.h>

#include "link_modbus.h"
#include "provision_ap.h"

SimSerial Serial;
SimESPClass ESP;

static char s_user[LINK_USER_MAX];
static bool s_credsReady;
static int s_result = LINK_RESULT_NONE;
static bool s_expRunning;
static int s_expNum, s_durationH;
static char s_ip[LINK_IP_MAX];

void link_modbus_init() {}
void link_modbus_poll() {}
bool link_modbus_take_login_request() { return false; }

void link_modbus_set_credentials(const char *user, const char *pass) {
  (void)pass;
  strncpy(s_user, user, sizeof(s_user) - 1);
  s_user[sizeof(s_user) - 1] = '\0';
  s_credsReady = true;
}
bool link_modbus_creds_ready() { return s_credsReady; }
int link_modbus_result() { return s_result; }
void link_modbus_set_exp_state(bool running, int expNum, int durationH) {
  s_expRunning = running;
  s_expNum = expNum;
  s_durationH = durationH;
}
bool link_modbus_take_result(int &out) {
  if (s_result == LINK_RESULT_NONE) return false;
  out = s_result;
  s_result = LINK_RESULT_NONE;
  return true;
}
void link_modbus_get_ip(char *out, size_t n) {
  const char *src = s_ip[0] ? s_ip : "0.0.0.0";
  strncpy(out, src, n - 1);
  out[n - 1] = '\0';
}
int link_modbus_get_ping_ms() { return -1; }
bool link_modbus_net_up() { return false; }
uint32_t link_modbus_net_age_ms() { return 0xFFFFFFFFu; }
int link_modbus_get_db_latest() { return -2; }
bool link_modbus_time_ok() { return false; }
void link_modbus_debug_print() {
  Serial.printf("[LINK] sim stub: creds_ready=%d result=%d run=%d exp=%d dur=%d\n",
                s_credsReady ? 1 : 0, s_result, s_expRunning ? 1 : 0, s_expNum,
                s_durationH);
}
uint16_t link_modbus_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else crc >>= 1;
    }
  }
  return crc;
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
