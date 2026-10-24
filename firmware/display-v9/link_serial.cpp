#include "link_serial.h"

#include <stdlib.h>
#include <string.h>

// rusim-line v1 peer endpoint for the display.
//
// Receives complete lines only: bytes accumulate until '\n', a line longer
// than LINK_LINE_MAX is discarded in full, and a stalled partial line is
// dropped after LINK_GAP_MS of silence so the parser cannot wedge.
// Everything is fixed-size static storage; no heap is used per line.

static char s_rx[LINK_LINE_MAX + 1];
static size_t s_rxLen = 0;
static bool s_rxOverflow = false;
static uint32_t s_lastByteMs = 0;
static uint32_t s_lastBeatMs = 0;

// Staged pair. Kept for the whole session: HELLO after a WT32 reboot clears
// the verdict bookkeeping and expects the pair to be re-pushed.
static char s_user[LINK_USER_MAX + 1];
static char s_pass[LINK_PASS_MAX + 1];
static bool s_havePair = false;
static bool s_verdictForPair = false;

static int s_durationH = 24;
static bool s_expRunning = false;
static int s_expNum = 0;
static bool s_expValid = false;
static bool s_expLogged = false;

static bool s_loginReq = false;
static bool s_loginReqPending = false;
static int s_result = LINK_RESULT_NONE;
static bool s_resultPending = false;

// WT32-pushed network status.
static char s_ip[LINK_IP_MAX + 1] = "";
static uint16_t s_pingMs = 0xFFFF;
static bool s_netUp = false;
static uint32_t s_netStampMs = 0;

// WT32-pushed DB state: -2 unknown, -1 none, >=0 latest id.
static int s_dbLatest = -2;
static bool s_timeOk = false;
static int s_dbBottles = -1;
static uint16_t s_pushDrops = 0;

static bool s_passthrough = false;

// --- small bounded helpers -------------------------------------------------

static void trim_ws(char *s) {
  if (s == nullptr) return;
  size_t len = strlen(s);
  size_t start = 0;
  while (start < len && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' ||
                         s[start] == '\n'))
    start++;
  while (len > start && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' ||
                         s[len - 1] == '\n'))
    len--;
  if (start > 0) memmove(s, s + start, len - start);
  s[len - start] = '\0';
}

// Reads key=value out of a space-separated argument string. Bounded copy.
static bool arg_lookup(const char *args, const char *key, char *out, size_t cap) {
  if (args == nullptr || key == nullptr || out == nullptr || cap == 0) return false;
  size_t keyLen = strlen(key);
  const char *p = args;
  while (p != nullptr && *p != '\0') {
    const char *end = strchr(p, ' ');
    size_t len = (end != nullptr) ? (size_t)(end - p) : strlen(p);
    if (len > keyLen && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=') {
      size_t vlen = len - keyLen - 1;
      if (vlen >= cap) vlen = cap - 1;
      memcpy(out, p + keyLen + 1, vlen);
      out[vlen] = '\0';
      return true;
    }
    p = (end != nullptr) ? end + 1 : nullptr;
  }
  return false;
}

// Percent-encodes user/pass values. Only [0-9A-Za-z-._~] pass unescaped;
// '%' and '=' are escaped like any other reserved byte.
static bool pct_encode(const char *in, char *out, size_t cap) {
  static const char *hex = "0123456789ABCDEF";
  size_t o = 0;
  for (size_t i = 0; in[i] != '\0'; i++) {
    unsigned char c = (unsigned char)in[i];
    bool plain = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                 (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~';
    size_t need = plain ? 1 : 3;
    if (o + need + 1 > cap) return false;
    if (plain) {
      out[o++] = (char)c;
    } else {
      out[o++] = '%';
      out[o++] = hex[(c >> 4) & 0xF];
      out[o++] = hex[c & 0xF];
    }
  }
  out[o] = '\0';
  return true;
}

// --- outbound --------------------------------------------------------------

// Echoes one outbound command on the USB console. The CREDS payload is
// replaced by the word alone so the password never reaches a log.
static void echo_line(const char *line, bool hideArgs) {
  if (hideArgs) {
    const char *sp = strchr(line, ' ');
    if (sp != nullptr) {
      char word[24];
      size_t n = (size_t)(sp - line);
      if (n >= sizeof(word)) n = sizeof(word) - 1;
      memcpy(word, line, n);
      word[n] = '\0';
      Serial.printf("[LINK>] %s\n", word);
      return;
    }
  }
  Serial.printf("[LINK>] %s\n", line);
}

static void write_line(const char *line, bool hideArgs) {
  if (s_passthrough || line == nullptr) return;
  Serial1.print(line);
  Serial1.print('\n');
  echo_line(line, hideArgs);
}

static void emit_hello() { write_line("HELLO", false); }

static void emit_experiment() {
  char line[64];
  snprintf(line, sizeof(line), "EXPERIMENT run=%d num=%d dur=%d",
           s_expRunning ? 1 : 0, s_expNum, s_durationH);
  write_line(line, false);
}

static void emit_creds() {
  char userEnc[3 * LINK_USER_MAX + 1];
  char passEnc[3 * LINK_PASS_MAX + 1];
  if (!pct_encode(s_user, userEnc, sizeof(userEnc))) return;
  if (!pct_encode(s_pass, passEnc, sizeof(passEnc))) return;
  char line[sizeof(userEnc) + sizeof(passEnc) + 16];
  int n = snprintf(line, sizeof(line), "CREDS user=%s pass=%s", userEnc, passEnc);
  if (n <= 0 || (size_t)n >= sizeof(line)) return;
  write_line(line, true);
}

// --- inbound parsing -------------------------------------------------------

static void handle_hello() {
  // A fresh WT32 knows nothing. Drop the old verdict and pending bookkeeping
  // so a pair that was already staged is pushed to it, and let the WT32's
  // snapshot decide whether the login gate comes back.
  s_result = LINK_RESULT_NONE;
  s_resultPending = false;
  s_verdictForPair = false;
  s_loginReq = false;
  s_loginReqPending = false;
  Serial.println("[LINK] WT32 HELLO, link up (pair kept for re-push)");
}

static void handle_login_req() {
  if (!s_loginReq) s_loginReqPending = true;
  s_loginReq = true;
}

static void handle_result(const char *args) {
  if (strcmp(args, "OK") == 0) {
    s_result = LINK_RESULT_SUCCESS;
    s_loginReq = false;
  } else if (strcmp(args, "FAIL") == 0) {
    s_result = LINK_RESULT_FAIL;
  } else {
    return;
  }
  s_resultPending = true;
  s_verdictForPair = true;
  Serial.printf("[LINK] WT32 RESULT %s\n", args);
}

static void handle_net(const char *args) {
  char ip[LINK_IP_MAX + 1] = "";
  char upRaw[8] = "0";
  char pingRaw[12] = "-";
  (void)arg_lookup(args, "ip", ip, sizeof(ip));
  (void)arg_lookup(args, "up", upRaw, sizeof(upRaw));
  bool up = (atoi(upRaw) != 0);
  (void)arg_lookup(args, "ping", pingRaw, sizeof(pingRaw));

  if (up) {
    strncpy(s_ip, ip, sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';
    if (strcmp(pingRaw, "-") == 0) {
      s_pingMs = 0xFFFF;
    } else {
      long v = strtol(pingRaw, nullptr, 10);
      if (v < 0) v = 0;
      if (v >= 0xFFFF) v = 0xFFFE;
      s_pingMs = (uint16_t)v;
    }
  } else {
    s_ip[0] = '\0';
    s_pingMs = 0xFFFF;
  }
  s_netUp = up;
  s_netStampMs = millis();
}

static void handle_db(const char *args) {
  char latestRaw[16] = "-";
  char timeRaw[8] = "0";
  char bottlesRaw[16] = "-";
  char dropsRaw[16] = "0";
  (void)arg_lookup(args, "latest", latestRaw, sizeof(latestRaw));
  (void)arg_lookup(args, "time", timeRaw, sizeof(timeRaw));
  (void)arg_lookup(args, "bottles", bottlesRaw, sizeof(bottlesRaw));
  (void)arg_lookup(args, "drops", dropsRaw, sizeof(dropsRaw));

  if (strcmp(latestRaw, "-") == 0) {
    s_dbLatest = -2;
  } else {
    long v = strtol(latestRaw, nullptr, 10);
    // 0 means "no rows yet" on the wire; the display domain uses -1 for that.
    s_dbLatest = (v <= 0) ? -1 : (int)v;
  }
  s_timeOk = (atoi(timeRaw) != 0);
  if (strcmp(bottlesRaw, "-") == 0) {
    s_dbBottles = -1;
  } else {
    long v = strtol(bottlesRaw, nullptr, 10);
    s_dbBottles = (v < 0) ? -1 : (int)v;
  }
  long d = strtol(dropsRaw, nullptr, 10);
  if (d < 0) d = 0;
  if (d > 0xFFFF) d = 0xFFFF;
  s_pushDrops = (uint16_t)d;
}

// LOG payloads are verbatim text and must never become a printf format.
static void handle_log(const char *payload) {
  Serial.print("[W32] ");
  Serial.println(payload);
}

static void handle_line(char *line) {
  // LOG is the only word whose payload may contain spaces; handle it first.
  if (strncmp(line, "LOG ", 4) == 0) {
    handle_log(line + 4);
    return;
  }
  char *args = strchr(line, ' ');
  if (args != nullptr) {
    *args++ = '\0';
    while (*args == ' ') args++;
  }
  if (strcmp(line, "HELLO") == 0) {
    handle_hello();
  } else if (strcmp(line, "LOGINREQ") == 0) {
    handle_login_req();
  } else if (strcmp(line, "RESULT") == 0 && args != nullptr) {
    handle_result(args);
  } else if (strcmp(line, "NET") == 0 && args != nullptr) {
    handle_net(args);
  } else if (strcmp(line, "DB") == 0 && args != nullptr) {
    handle_db(args);
  }
  // Everything else (display-direction words, boot ROM text, noise) is ignored.
}

static void rx_byte(char c) {
  s_lastByteMs = millis();
  if (c == '\r') return;
  if (c == '\n') {
    if (!s_rxOverflow && s_rxLen > 0) {
      s_rx[s_rxLen] = '\0';
      handle_line(s_rx);
    }
    s_rxLen = 0;
    s_rxOverflow = false;
    return;
  }
  if (s_rxOverflow) return;
  if (s_rxLen >= LINK_LINE_MAX) {
    // Overlong line: drop the whole thing rather than parse a prefix.
    s_rxLen = 0;
    s_rxOverflow = true;
    return;
  }
  s_rx[s_rxLen++] = c;
}

// --- public surface --------------------------------------------------------

void link_serial_init() {
  // Give the flash tunnel room for bursts while the main loop services USB
  // and UART in turn. Set this before begin(), as required by Arduino-ESP32.
  Serial1.setRxBufferSize(4096);
  Serial1.begin(LINK_UART_BAUD, SERIAL_8N1, LINK_UART_RX_PIN, LINK_UART_TX_PIN);
  delay(20);
  while (Serial1.available()) Serial1.read();
  s_lastBeatMs = millis();
  Serial.printf("[LINK] line link on Serial1 RX=%d TX=%d %d 8N1 (rusim-line v1)\n",
                LINK_UART_RX_PIN, LINK_UART_TX_PIN, LINK_UART_BAUD);
  emit_hello();
}

void link_serial_poll() {
  if (s_passthrough) {
    while (Serial1.available() > 0) {
      int c = Serial1.read();
      if (c < 0) break;
      Serial.write((uint8_t)c);
    }
    return;
  }

  while (Serial1.available() > 0) {
    int c = Serial1.read();
    if (c < 0) break;
    rx_byte((char)c);
  }
  // A stalled partial line never wedges the parser.
  if (s_rxLen > 0 && (millis() - s_lastByteMs) > LINK_GAP_MS) {
    s_rxLen = 0;
    s_rxOverflow = false;
  }

  if (millis() - s_lastBeatMs >= LINK_BEAT_MS) {
    s_lastBeatMs = millis();
    emit_experiment();
    if (s_havePair && !s_verdictForPair) emit_creds();
  }
}

bool link_serial_take_login_request() {
  bool v = s_loginReqPending;
  s_loginReqPending = false;
  return v;
}

void link_serial_set_credentials(const char *user, const char *pass) {
  memset(s_user, 0, sizeof(s_user));
  memset(s_pass, 0, sizeof(s_pass));
  if (user != nullptr) {
    strncpy(s_user, user, sizeof(s_user) - 1);
    trim_ws(s_user);
  }
  if (pass != nullptr) {
    strncpy(s_pass, pass, sizeof(s_pass) - 1);
    trim_ws(s_pass);
  }
  if (s_user[0] == '\0' || s_pass[0] == '\0') {
    s_havePair = false;
    s_verdictForPair = false;
    Serial.println("[LINK] credentials ignored: username and password both required");
    return;
  }
  s_havePair = true;
  s_verdictForPair = false;
  s_result = LINK_RESULT_NONE;
  s_resultPending = false;
  Serial.printf("[LINK] creds staged user='%s'\n", s_user);
  // Sent immediately, so a corrected pair takes effect without waiting for the
  // heartbeat. Every staging path lands here.
  emit_creds();
}

void link_serial_set_exp_state(bool running, int expNum, int durationH) {
  if (expNum < 0) expNum = 0;
  if (expNum > 65535) expNum = 65535;
  if (durationH < 1) durationH = 1;
  if (durationH > 99) durationH = 99;
  bool changed = !s_expValid || running != s_expRunning || expNum != s_expNum ||
                 durationH != s_durationH;
  s_expRunning = running;
  s_expNum = expNum;
  s_durationH = durationH;
  s_expValid = true;
  if (changed) {
    if (s_expLogged) {
      Serial.printf("[LINK] exp state run=%d exp=%d dur=%dh\n", s_expRunning ? 1 : 0,
                    s_expNum, s_durationH);
    }
    s_expLogged = true;
    emit_experiment();
  }
}

void link_serial_emit_experiment() { emit_experiment(); }

bool link_serial_creds_ready() { return s_havePair && !s_verdictForPair; }

int link_serial_result() { return s_result; }

bool link_serial_take_result(int &out) {
  if (!s_resultPending) return false;
  s_resultPending = false;
  out = s_result;
  return true;
}

void link_serial_debug_print() {
  char ip[LINK_IP_MAX + 1];
  link_serial_get_ip(ip, sizeof(ip));
  Serial.printf("[LINK] link=%s beat=%lums req=%d pair=%d pending=%d result=%d user='%s' "
                "exp=%d run=%d dur=%d ip='%s' ping=%d up=%d age=%lu db=%d tok=%d "
                "bottles=%d drops=%d\n",
                s_passthrough ? "passthrough" : "line", (unsigned long)(millis() - s_lastBeatMs),
                s_loginReq ? 1 : 0, s_havePair ? 1 : 0, link_serial_creds_ready() ? 1 : 0,
                s_result, s_user, s_expNum, s_expRunning ? 1 : 0, s_durationH, ip,
                link_serial_get_ping_ms(), s_netUp ? 1 : 0,
                (unsigned long)link_serial_net_age_ms(), s_dbLatest, s_timeOk ? 1 : 0,
                s_dbBottles, (int)s_pushDrops);
}

void link_serial_get_ip(char *out, size_t n) {
  if (out == nullptr || n == 0) return;
  strncpy(out, s_ip, n - 1);
  out[n - 1] = '\0';
}

int link_serial_get_ping_ms() {
  if (s_pingMs == 0xFFFF) return -1;
  return (int)s_pingMs;
}

bool link_serial_net_up() { return s_netUp; }

int link_serial_get_db_latest() { return s_dbLatest; }

bool link_serial_time_ok() { return s_timeOk; }

int link_serial_get_db_bottles() { return s_dbBottles; }

int link_serial_get_push_drops() { return (int)s_pushDrops; }

uint32_t link_serial_net_age_ms() {
  if (s_netStampMs == 0) return 0;
  return millis() - s_netStampMs;
}

bool link_serial_send_raw(const char *line) {
  if (line == nullptr || s_passthrough) return false;
  if (strlen(line) > LINK_LINE_MAX) return false;
  write_line(line, false);
  return true;
}

void link_serial_passthrough_set(bool on) {
  if (on == s_passthrough) return;
  s_passthrough = on;
  // Never let a half-received line from either mode leak into the other.
  s_rxLen = 0;
  s_rxOverflow = false;
  if (on) {
    Serial.println("[LINK] passthrough ON: USB <-> IO17/IO18, link parser suspended");
    Serial.println("[LINK] flash: IO0 low, 115200 baud, --before no_reset, power-cycle after");
  } else {
    s_lastBeatMs = millis();
    Serial.println("[LINK] passthrough OFF: link parser resumed");
  }
}

bool link_serial_passthrough_active() { return s_passthrough; }

void link_serial_passthrough_write(const uint8_t *data, size_t n) {
  if (!s_passthrough || data == nullptr || n == 0) return;
  Serial1.write(data, n);
}
