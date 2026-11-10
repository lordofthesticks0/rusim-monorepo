// Stubs so the sim links without the WT32 UART link or any ESP32 hardware.
// The WT32 never answers in the sim, so RESULT stays NONE and the login gate
// never opens. The UI state machine is still exercisable from /setup.

#include <string.h>

#include "link_serial.h"

SimSerial Serial;
SimESPClass ESP;

static int s_result = LINK_RESULT_NONE;
static bool s_expRunning;
static int s_expNum, s_durationH;
static char s_ip[LINK_IP_MAX];
static bool s_passthrough;

void link_serial_init() {}
void link_serial_poll() {}

bool link_serial_take_login_request() { return false; }

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
bool link_serial_db_ready() { return false; }
int link_serial_get_db_active_exp() { return -1; }
int link_serial_get_db_active_dur() { return -1; }
int link_serial_get_db_active_src() { return 0; }
int link_serial_get_db_pending_exp() { return -1; }
int link_serial_get_db_pending_dur() { return -1; }
int link_serial_get_db_cfg_max() { return -1; }
int link_serial_get_db_queued() { return 0; }
const char *link_serial_db_state() { return "none"; }
uint32_t link_serial_get_db_read_age_ms() { return 0; }
uint32_t link_serial_get_db_ack_age_ms() { return 0; }

// Sim: no WT32 link, so sensor values remain unavailable, as on hardware
// before the first real SENS frame.
int32_t link_serial_get_sens(LinkSensFrame *out) {
  if (out != nullptr) memset(out, 0, sizeof(*out));
  return -1;
}
void link_serial_debug_print() {
  Serial.printf("[LINK] sim stub: result=%d run=%d exp=%d dur=%d\n", s_result,
                s_expRunning ? 1 : 0, s_expNum, s_durationH);
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
