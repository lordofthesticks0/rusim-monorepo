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

// WT32-pushed configuration state. Ages are stored as reported plus the line
// timestamp, so /info can keep counting between the WT32's 30 s DB passes.
static bool s_dbReady = false;
static int s_dbActiveExp = -1;
static int s_dbActiveDur = -1;
static int s_dbActiveSrc = 0;  // 0 none, 1 db, 2 local
static int s_dbPendingExp = -1;
static int s_dbPendingDur = -1;
static int s_dbCfgMax = -1;
static uint16_t s_dbQueued = 0;
static bool s_dbReadSeen = false;
static bool s_dbAckSeen = false;
static uint32_t s_dbReadAge0 = 0;
static uint32_t s_dbAckAge0 = 0;
static uint32_t s_dbStampMs = 0;
static char s_dbState[12] = "none";

// Real sensor frame from the WT32's legacy-slave poll (SENS lines). seq bumps
// on every accepted line so the UI can tell a new frame from a re-parse of
// the same values; the copy-out getter still returns the full frame, so the
// UI never indexes behind our back.
static LinkSensFrame s_sens = {};
static bool s_sensSeen = false;
static uint32_t s_sensSeq = 0;
static uint32_t s_sensMs = 0;

static bool s_passthrough = false;

// --- small bounded helpers -------------------------------------------------


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


// --- outbound --------------------------------------------------------------

static void write_line(const char *line) {
  if (s_passthrough || line == nullptr) return;
  Serial1.print(line);
  Serial1.print('\n');
  Serial.printf("[LINK>] %s\n", line);
}

static void emit_hello() { write_line("HELLO"); }

static void emit_experiment() {
  char line[64];
  snprintf(line, sizeof(line), "EXPERIMENT run=%d num=%d dur=%d",
           s_expRunning ? 1 : 0, s_expNum, s_durationH);
  write_line(line);
}

// --- inbound parsing -------------------------------------------------------

static void handle_hello() {
  // A fresh WT32 knows nothing. Drop the old verdict and pending bookkeeping
  // so the login gate is driven by the WT32's next LOGINREQ.
  s_result = LINK_RESULT_NONE;
  s_resultPending = false;
  s_loginReq = false;
  s_loginReqPending = false;
  // A rebooted WT32 has no configuration and no session: drop the previous
  // ready/active state so the display cannot start a run until the new WT32
  // has completed a pull.
  s_dbReady = false;
  s_dbActiveExp = -1;
  s_dbActiveDur = -1;
  s_dbActiveSrc = 0;
  s_dbPendingExp = -1;
  s_dbPendingDur = -1;
  s_dbCfgMax = -1;
  s_dbQueued = 0;
  s_dbReadSeen = false;
  s_dbAckSeen = false;
  snprintf(s_dbState, sizeof(s_dbState), "none");
  s_sensSeen = false;
  s_sens = {};
  Serial.println("[LINK] WT32 HELLO, link up");
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

// Parses one "<key>=<value>" age field. "-" (or absence) means never.
static bool parse_age(const char *args, const char *key, bool &seen, uint32_t &value) {
  char raw[16] = "-";
  if (!arg_lookup(args, key, raw, sizeof(raw))) return false;
  if (strcmp(raw, "-") == 0) return false;
  char *end = nullptr;
  unsigned long v = strtoul(raw, &end, 10);
  if (end == raw) return false;
  seen = true;
  value = (uint32_t)v;
  return true;
}

static int parse_opt_int(const char *args, const char *key) {
  char raw[16] = "-";
  if (!arg_lookup(args, key, raw, sizeof(raw))) return -1;
  if (strcmp(raw, "-") == 0) return -1;
  char *end = nullptr;
  long v = strtol(raw, &end, 10);
  if (end == raw || v < 0) return -1;
  return (int)v;
}

static void handle_db(const char *args) {
  char latestRaw[16] = "-";
  char timeRaw[8] = "0";
  char bottlesRaw[16] = "-";
  char dropsRaw[16] = "0";
  char readyRaw[8] = "0";
  char srcRaw[8] = "-";
  char queuedRaw[16] = "0";
  char stateRaw[16] = "none";
  (void)arg_lookup(args, "latest", latestRaw, sizeof(latestRaw));
  (void)arg_lookup(args, "time", timeRaw, sizeof(timeRaw));
  (void)arg_lookup(args, "bottles", bottlesRaw, sizeof(bottlesRaw));
  (void)arg_lookup(args, "drops", dropsRaw, sizeof(dropsRaw));
  (void)arg_lookup(args, "ready", readyRaw, sizeof(readyRaw));
  (void)arg_lookup(args, "asrc", srcRaw, sizeof(srcRaw));
  (void)arg_lookup(args, "queued", queuedRaw, sizeof(queuedRaw));
  (void)arg_lookup(args, "state", stateRaw, sizeof(stateRaw));

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

  // Configuration state. ready=0 blocks run starts on this display; an older
  // WT32 sends no fields, which also stays "not ready" rather than starting a
  // run whose configuration provenance is unknown.
  s_dbReady = (atoi(readyRaw) != 0);
  s_dbActiveExp = parse_opt_int(args, "aexp");
  s_dbActiveDur = parse_opt_int(args, "adur");
  s_dbActiveSrc =
      (strcmp(srcRaw, "db") == 0) ? 1 : (strcmp(srcRaw, "local") == 0) ? 2 : 0;
  s_dbPendingExp = parse_opt_int(args, "pexp");
  s_dbPendingDur = parse_opt_int(args, "pdur");
  s_dbCfgMax = parse_opt_int(args, "cfgmax");
  long q = strtol(queuedRaw, nullptr, 10);
  if (q < 0) q = 0;
  if (q > 0xFFFF) q = 0xFFFF;
  s_dbQueued = (uint16_t)q;
  strncpy(s_dbState, stateRaw, sizeof(s_dbState) - 1);
  s_dbState[sizeof(s_dbState) - 1] = '\0';
  s_dbStampMs = millis();
  (void)parse_age(args, "reads", s_dbReadSeen, s_dbReadAge0);
  (void)parse_age(args, "ack", s_dbAckSeen, s_dbAckAge0);
}

// Parses one SENS line's arguments: "b<i>" switches the active bottle and the
// following "<key>=<float>" tokens fill it. Keys repeat per bottle, so
// arg_lookup (first match wins) cannot be used here. An unparseable token is
// skipped, not fatal: the frame ends complete for every field that did parse.
// Accepts "nan" and "inf" literally; the UI stores them and isfinite() keeps
// the NULL marker.
//
// A line whose tail is missing (or a master-side bug) reads the missing
// bottles as absent, which the UI renders as NULL — the same view as a poll
// failure. The next 30 s round overwrites everything, so no half-fed frame
// outlives its poll.
static void handle_sens(char *args) {
  LinkSensFrame f = {};
  for (int b = 0; b < LINK_SENS_BOTTLES; b++) {
    for (int s = 0; s < LINK_SENS_VALS; s++) f.value[b][s] = NAN;
  }
  bool anyBottle = false;
  int cur = -1;  // active 0-based bottle slot; fields before any bN are dropped
  const char delim[] = " \t";
  for (char *tok = strtok(args, delim); tok != nullptr;
       tok = strtok(nullptr, delim)) {
    if (tok[0] == 'b' && tok[1] >= '1' &&
        tok[1] <= (char)('0' + LINK_SENS_BOTTLES) && tok[2] == '\0') {
      cur = tok[1] - '1';
      f.present[cur] = true;
      anyBottle = true;
      continue;
    }
    char *eq = strchr(tok, '=');
    if (eq == nullptr || cur < 0) continue;
    *eq = '\0';
    const char *val = eq + 1;
    char *stop = nullptr;
    float v = strtof(val, &stop);
    if (stop == val || *stop != '\0') continue;
    int col = -1;
    if (strcmp(tok, "co2") == 0) col = LINK_SENS_CO2;
    else if (strcmp(tok, "ch4") == 0) col = LINK_SENS_CH4;
    else if (strcmp(tok, "p") == 0) col = LINK_SENS_PRESS;
    else if (strcmp(tok, "ph") == 0) col = LINK_SENS_PH;
    else if (strcmp(tok, "t") == 0) col = LINK_SENS_TEMP;
    if (col < 0) continue;
    f.value[cur][col] = v;
  }
  if (!anyBottle) return;  // no bottle token: nothing to commit

  s_sensSeq++;
  f.seq = s_sensSeq;
  s_sensMs = millis();
  s_sens = f;
  s_sensSeen = true;
  Serial.printf("[LINK] SENS frame seq=%lu\n", (unsigned long)s_sensSeq);
}

// Copy-out snapshot. The copy happens before the age math so a caller combining
// frame and age can never see a torn pair; the link and LVGL run in the same
// task anyway, but the copy keeps the calling order irrelevant.
int32_t link_serial_get_sens(LinkSensFrame *out) {
  if (out != nullptr) *out = s_sens;
  if (!s_sensSeen) return -1;
  return (int32_t)(millis() - s_sensMs);
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
  } else if (strcmp(line, "SENS") == 0 && args != nullptr) {
    handle_sens(args);
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
  }
}

bool link_serial_take_login_request() {
  bool v = s_loginReqPending;
  s_loginReqPending = false;
  return v;
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



bool link_serial_take_result(int &out) {
  if (!s_resultPending) return false;
  s_resultPending = false;
  out = s_result;
  return true;
}

void link_serial_debug_print() {
  char ip[LINK_IP_MAX + 1];
  link_serial_get_ip(ip, sizeof(ip));
  Serial.printf("[LINK] link=%s beat=%lums req=%d result=%d "
                "exp=%d run=%d dur=%d ip='%s' ping=%d up=%d age=%lu db=%d tok=%d "
                "bottles=%d drops=%d\n",
                s_passthrough ? "passthrough" : "line", (unsigned long)(millis() - s_lastBeatMs),
                s_loginReq ? 1 : 0, s_result, s_expNum, s_expRunning ? 1 : 0, s_durationH, ip,
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

bool link_serial_db_ready() { return s_dbReady; }

int link_serial_get_db_active_exp() { return s_dbActiveExp; }

int link_serial_get_db_active_dur() { return s_dbActiveDur; }

int link_serial_get_db_active_src() { return s_dbActiveSrc; }

int link_serial_get_db_pending_exp() { return s_dbPendingExp; }

int link_serial_get_db_pending_dur() { return s_dbPendingDur; }

int link_serial_get_db_cfg_max() { return s_dbCfgMax; }

int link_serial_get_db_queued() { return (int)s_dbQueued; }

const char *link_serial_db_state() { return s_dbState; }

uint32_t link_serial_get_db_read_age_ms() {
  if (!s_dbReadSeen) return 0;
  return s_dbReadAge0 + (millis() - s_dbStampMs);
}

uint32_t link_serial_get_db_ack_age_ms() {
  if (!s_dbAckSeen) return 0;
  return s_dbAckAge0 + (millis() - s_dbStampMs);
}

uint32_t link_serial_net_age_ms() {
  if (s_netStampMs == 0) return 0;
  return millis() - s_netStampMs;
}

bool link_serial_send_raw(const char *line) {
  if (line == nullptr || s_passthrough) return false;
  if (strlen(line) > LINK_LINE_MAX) return false;
  write_line(line);
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
