#include "link_modbus.h"

// Minimal Modbus RTU slave. Non-blocking: bytes accumulate in a small
// buffer, a frame is parsed after 8 ms of line silence or a 128 ms
// frame timeout. Only FC 0x03 / 0x06 / 0x10 are answered. All other
// codes return exception 0x01, bad addresses return 0x02.
//
// Also drives the poll-request output (LINK_NOTIFY_PIN -> WT32 IO14): a staged
// credential pair pulses it so the master reads the pair immediately instead of
// waiting out its 5 s poll cycle. See link_modbus.h.

static uint8_t s_rx[260];
static size_t s_rxLen = 0;
static uint32_t s_lastByteMs = 0;

// Poll-request pulse deadline in millis(). 0 = line idle (LOW).
static uint32_t s_notifyUntilMs = 0;

static char s_user[LINK_USER_MAX + 1];
static char s_pass[LINK_PASS_MAX + 1];
static int s_durationH = 24;
static bool s_expRunning = false;
static int s_expNum = 0;
static bool s_loginReq = false;
static bool s_loginReqPending = false;
static bool s_credsReady = false;
static int s_result = LINK_RESULT_NONE;
static bool s_resultPending = false;

// WT32-pushed network status (master writes, display reads for /info).
static char s_ip[LINK_IP_MAX + 1] = "";
static uint16_t s_pingMs = LINK_PING_NONE;
static bool s_netUp = false;
static uint32_t s_netStampMs = 0;

// WT32-pushed DB state (backlog #9): -2 unknown, -1 none, >=0 latest id.
static int s_dbLatest = -2;
static bool s_timeOk = false;

static uint16_t regCount() { return 0x007A; }

static void regs_get(const char *s, uint16_t base, uint16_t idx, uint16_t &out) {
  (void)base;
  size_t o = (size_t)idx * 2;
  uint8_t hi = 0, lo = 0;
  if (o < strlen(s)) hi = (uint8_t)s[o];
  if (o + 1 < strlen(s)) lo = (uint8_t)s[o + 1];
  out = (uint16_t)((hi << 8) | lo);
}

static uint16_t reg_read(uint16_t addr) {
  if (addr == LINK_REG_LOGIN_REQ) return s_loginReq ? 1 : 0;
  if (addr == LINK_REG_CREDS_READY) return s_credsReady ? 1 : 0;
  if (addr == LINK_REG_RESULT) return (uint16_t)s_result;
  if (addr == LINK_REG_EXP_RUNNING) return s_expRunning ? 1 : 0;
  if (addr == LINK_REG_EXP_NUM) return (uint16_t)s_expNum;
  if (addr == LINK_REG_DB_LATEST) return s_dbLatest < 0 ? 0xFFFF : (uint16_t)s_dbLatest;
  if (addr == LINK_REG_TIME_OK) return s_timeOk ? 1 : 0;
  if (addr >= LINK_REG_USER_BASE && addr < LINK_REG_USER_BASE + LINK_REG_USER_REGS) {
    uint16_t v = 0;
    regs_get(s_user, LINK_REG_USER_BASE, addr - LINK_REG_USER_BASE, v);
    return v;
  }
  if (addr >= LINK_REG_PASS_BASE && addr < LINK_REG_PASS_BASE + LINK_REG_PASS_REGS) {
    uint16_t v = 0;
    regs_get(s_pass, LINK_REG_PASS_BASE, addr - LINK_REG_PASS_BASE, v);
    return v;
  }
  if (addr == LINK_REG_DURATION) return (uint16_t)s_durationH;
  if (addr >= LINK_REG_IP_BASE && addr < LINK_REG_IP_BASE + LINK_REG_IP_REGS) {
    uint16_t v = 0;
    regs_get(s_ip, LINK_REG_IP_BASE, addr - LINK_REG_IP_BASE, v);
    return v;
  }
  if (addr == LINK_REG_PING_MS) return s_pingMs;
  if (addr == LINK_REG_NET_UP) return s_netUp ? 1 : 0;
  return 0;
}

static bool reg_addr_valid(uint16_t addr) {
  if (addr <= LINK_REG_RESULT) return true;
  if (addr == LINK_REG_EXP_RUNNING || addr == LINK_REG_EXP_NUM) return true;
  if (addr == LINK_REG_DB_LATEST || addr == LINK_REG_TIME_OK) return true;
  if (addr >= LINK_REG_USER_BASE && addr < LINK_REG_USER_BASE + LINK_REG_USER_REGS) return true;
  if (addr >= LINK_REG_PASS_BASE && addr < LINK_REG_PASS_BASE + LINK_REG_PASS_REGS) return true;
  if (addr == LINK_REG_DURATION) return true;
  if (addr >= LINK_REG_IP_BASE && addr < LINK_REG_IP_BASE + LINK_REG_IP_REGS) return true;
  if (addr == LINK_REG_PING_MS) return true;
  if (addr == LINK_REG_NET_UP) return true;
  return false;
}

// Writes the two ASCII bytes of one register into a string buffer.
static void regs_put(char *s, size_t cap, uint16_t idx, uint16_t value) {
  size_t o = (size_t)idx * 2;
  if (o < cap) s[o] = (char)(value >> 8);
  if (o + 1 < cap) s[o + 1] = (char)(value & 0xFF);
  s[cap - 1] = '\0';
}

// Writable by the master: LOGIN_REQ, CREDS_READY (consume-ack), RESULT,
// plus the WT32-pushed status block (IP_ADDR, PING_MS, NET_UP).
// Credential and duration blocks are read-only:
// the display owns them after the user taps Confirm.
static bool reg_write(uint16_t addr, uint16_t value) {
  if (addr == LINK_REG_LOGIN_REQ) {
    bool v = (value != 0);
    if (v && !s_loginReq) s_loginReqPending = true;
    s_loginReq = v;
    if (!v) s_result = LINK_RESULT_NONE;
    return true;
  }
  if (addr == LINK_REG_CREDS_READY) {
    // Master writes 0 to ack consumption after reading the strings.
    if (value == 0) s_credsReady = false;
    return true;
  }
  if (addr == LINK_REG_RESULT) {
    s_result = (value == LINK_RESULT_SUCCESS) ? LINK_RESULT_SUCCESS
               : (value == LINK_RESULT_FAIL)  ? LINK_RESULT_FAIL
                                              : LINK_RESULT_NONE;
    s_resultPending = true;
    return true;
  }
  if (addr >= LINK_REG_IP_BASE && addr < LINK_REG_IP_BASE + LINK_REG_IP_REGS) {
    regs_put(s_ip, sizeof(s_ip), addr - LINK_REG_IP_BASE, value);
    return true;
  }
  if (addr == LINK_REG_PING_MS) {
    s_pingMs = value;
    return true;
  }
  if (addr == LINK_REG_DB_LATEST) {
    if (value == 0xFFFF) {
      s_dbLatest = -2;
    } else {
      s_dbLatest = (int)value;
    }
    return true;
  }
  if (addr == LINK_REG_TIME_OK) {
    s_timeOk = (value != 0);
    return true;
  }
  if (addr == LINK_REG_NET_UP) {
    s_netUp = (value != 0);
    s_netStampMs = millis();
    if (!s_netUp) s_pingMs = LINK_PING_NONE;
    return true;
  }
  return false;
}

uint16_t link_modbus_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

void link_modbus_init() {
  Serial1.begin(LINK_UART_BAUD, SERIAL_8N1, LINK_UART_RX_PIN, LINK_UART_TX_PIN);
  // Drain boot line noise so it can never parse as a frame.
  delay(20);
  while (Serial1.available()) Serial1.read();
  pinMode(LINK_NOTIFY_PIN, OUTPUT);
  digitalWrite(LINK_NOTIFY_PIN, LOW);
  Serial.printf("[LINK] poll-request out on IO%d (idle LOW)\n", LINK_NOTIFY_PIN);
  Serial.printf("[LINK] Modbus slave ID %d on Serial1 RX=%d TX=%d %d 8N1\n",
                LINK_MODBUS_SLAVE_ID, LINK_UART_RX_PIN, LINK_UART_TX_PIN,
                LINK_UART_BAUD);
}

static void send_bytes(const uint8_t *p, size_t n) {
  delay(2);  // turnaround: master half-duplex needs a gap
  Serial1.write(p, n);
  Serial1.flush();
}

static void send_exception(uint8_t fn, uint8_t code) {
  uint8_t r[5];
  r[0] = LINK_MODBUS_SLAVE_ID;
  r[1] = fn | 0x80;
  r[2] = code;
  uint16_t crc = link_modbus_crc16(r, 3);
  r[3] = crc & 0xFF;
  r[4] = crc >> 8;
  send_bytes(r, 5);
}

static void handle_frame(const uint8_t *f, size_t n) {
  if (n < 4) return;
  uint16_t crcRx = (uint16_t)f[n - 2] | ((uint16_t)f[n - 1] << 8);
  if (link_modbus_crc16(f, n - 2) != crcRx) return;
  if (f[0] != LINK_MODBUS_SLAVE_ID) return;
  uint8_t fn = f[1];

  if (fn == 0x03) {
    if (n != 8) return;
    uint16_t addr = ((uint16_t)f[2] << 8) | f[3];
    uint16_t cnt = ((uint16_t)f[4] << 8) | f[5];
    if (cnt == 0 || cnt > 64 || addr + cnt > regCount()) {
      send_exception(fn, 0x02);
      return;
    }
    for (uint16_t i = 0; i < cnt; i++) {
      if (!reg_addr_valid(addr + i)) {
        send_exception(fn, 0x02);
        return;
      }
    }
    uint8_t r[5 + 128];
    r[0] = LINK_MODBUS_SLAVE_ID;
    r[1] = 0x03;
    r[2] = (uint8_t)(cnt * 2);
    for (uint16_t i = 0; i < cnt; i++) {
      uint16_t v = reg_read(addr + i);
      r[3 + i * 2] = v >> 8;
      r[4 + i * 2] = v & 0xFF;
    }
    uint16_t crc = link_modbus_crc16(r, 3 + cnt * 2);
    r[3 + cnt * 2] = crc & 0xFF;
    r[4 + cnt * 2] = crc >> 8;
    send_bytes(r, 5 + cnt * 2);
    return;
  }

  if (fn == 0x06) {
    if (n != 8) return;
    uint16_t addr = ((uint16_t)f[2] << 8) | f[3];
    uint16_t val = ((uint16_t)f[4] << 8) | f[5];
    if (!reg_addr_valid(addr) || !reg_write(addr, val)) {
      send_exception(fn, 0x02);
      return;
    }
    uint8_t r[8];
    memcpy(r, f, 6);
    uint16_t crc = link_modbus_crc16(r, 6);
    r[6] = crc & 0xFF;
    r[7] = crc >> 8;
    send_bytes(r, 8);
    return;
  }

  if (fn == 0x10) {
    if (n < 9) return;
    uint16_t addr = ((uint16_t)f[2] << 8) | f[3];
    uint16_t cnt = ((uint16_t)f[4] << 8) | f[5];
    uint8_t blen = f[6];
    if (cnt == 0 || blen != cnt * 2 || n != (size_t)(7 + blen + 2)) {
      send_exception(fn, 0x02);
      return;
    }
    for (uint16_t i = 0; i < cnt; i++) {
      if (!reg_addr_valid(addr + i)) {
        send_exception(fn, 0x02);
        return;
      }
    }
    // Validate all writes before committing any.
    // Writable by master: LOGIN_REQ, CREDS_READY (ack), RESULT, plus the
    // WT32-pushed network status (IP_ADDR, PING_MS, NET_UP) and DB state
    // (DB_LATEST, TIME_OK). Credential and duration blocks stay read-only:
    // the display owns them.
    for (uint16_t i = 0; i < cnt; i++) {
      uint16_t a = addr + i;
      if (a == LINK_REG_LOGIN_REQ || a == LINK_REG_CREDS_READY ||
          a == LINK_REG_RESULT || a == LINK_REG_PING_MS ||
          a == LINK_REG_NET_UP || a == LINK_REG_DB_LATEST ||
          a == LINK_REG_TIME_OK ||
          (a >= LINK_REG_IP_BASE && a < LINK_REG_IP_BASE + LINK_REG_IP_REGS))
        continue;
      send_exception(fn, 0x02);
      return;
    }
    for (uint16_t i = 0; i < cnt; i++) {
      uint16_t v = ((uint16_t)f[7 + i * 2] << 8) | f[8 + i * 2];
      reg_write(addr + i, v);
    }
    uint8_t r[8];
    r[0] = LINK_MODBUS_SLAVE_ID;
    r[1] = 0x10;
    r[2] = f[2];
    r[3] = f[3];
    r[4] = f[4];
    r[5] = f[5];
    uint16_t crc = link_modbus_crc16(r, 6);
    r[6] = crc & 0xFF;
    r[7] = crc >> 8;
    send_bytes(r, 8);
    return;
  }

  send_exception(fn, 0x01);
}

// Returns the line to idle once the pulse has been high for its full width.
// Called from link_modbus_poll() so staging never blocks the UI.
static void notify_tick() {
  if (s_notifyUntilMs == 0) return;
  if ((int32_t)(millis() - s_notifyUntilMs) < 0) return;
  digitalWrite(LINK_NOTIFY_PIN, LOW);
  s_notifyUntilMs = 0;
}

void link_notify_pulse() {
  digitalWrite(LINK_NOTIFY_PIN, HIGH);
  s_notifyUntilMs = millis() + LINK_NOTIFY_PULSE_MS;
}

void link_modbus_poll() {
  notify_tick();
  while (Serial1.available() > 0) {
    int c = Serial1.read();
    if (c < 0) break;
    if (s_rxLen < sizeof(s_rx)) {
      s_rx[s_rxLen++] = (uint8_t)c;
      s_lastByteMs = millis();
    } else {
      // Overlong: drop the frame, keep consuming to resync.
      s_rxLen = 0;
    }
  }
  if (s_rxLen == 0) return;
  // A stalled partial frame never wedges the parser.
  if (millis() - s_lastByteMs > 150) {
    s_rxLen = 0;
    return;
  }
  // Fixed-length FC 0x03 / 0x06 requests are exactly 8 bytes:
  // parse eagerly once 8 bytes have arrived.
  if (s_rxLen >= 8 && (s_rx[1] == 0x03 || s_rx[1] == 0x06)) {
    uint8_t f[8];
    memcpy(f, s_rx, 8);
    memmove(s_rx, s_rx + 8, s_rxLen - 8);
    s_rxLen -= 8;
    handle_frame(f, 8);
    return;
  }
  // Variable-length FC 0x10 (and anything else) parses after
  // 8 ms of line silence ends the frame. At 9600 baud one byte is
  // ~1 ms, so 8 ms exceeds the 3.5-char Modbus gap (~4 ms).
  if (millis() - s_lastByteMs >= 8) {
    size_t n = s_rxLen;
    s_rxLen = 0;
    handle_frame(s_rx, n);
  }
}

bool link_modbus_take_login_request() {
  bool v = s_loginReqPending;
  s_loginReqPending = false;
  return v;
}

void link_modbus_set_credentials(const char *user, const char *pass) {
  strncpy(s_user, user ? user : "", sizeof(s_user) - 1);
  s_user[sizeof(s_user) - 1] = '\0';
  strncpy(s_pass, pass ? pass : "", sizeof(s_pass) - 1);
  s_pass[sizeof(s_pass) - 1] = '\0';
  s_credsReady = true;
  s_result = LINK_RESULT_NONE;
  // Ask the master to read the pair now rather than on its next 5 s cycle. The
  // registers above are already written, so the poll cannot observe a
  // half-staged pair. The phone form and the CREDS serial command both land
  // here, which is why the pulse is fired here and not at either call site.
  link_notify_pulse();
  Serial.printf("[LINK] creds staged user='%s' ready=1\n", s_user);
}

void link_modbus_set_exp_state(bool running, int expNum, int durationH) {
  s_expRunning = running;
  if (expNum < 0) expNum = 0;
  if (expNum > 65535) expNum = 65535;
  s_expNum = expNum;
  if (durationH < 1) durationH = 1;
  if (durationH > 99) durationH = 99;
  s_durationH = durationH;
}

bool link_modbus_creds_ready() { return s_credsReady; }

int link_modbus_result() { return s_result; }

bool link_modbus_take_result(int &out) {
  if (!s_resultPending) return false;
  s_resultPending = false;
  out = s_result;
  return true;
}

void link_modbus_debug_print() {
  char ip[LINK_IP_MAX + 1];
  link_modbus_get_ip(ip, sizeof(ip));
  Serial.printf("[LINK] req=%d ready=%d result=%d user='%s' exp=%d run=%d dur=%d ip='%s' ping=%d up=%d db=%d tok=%d\n",
                s_loginReq, s_credsReady, s_result, s_user, s_expNum,
                s_expRunning ? 1 : 0, s_durationH, ip,
                link_modbus_get_ping_ms(), s_netUp, s_dbLatest, s_timeOk ? 1 : 0);
}

void link_modbus_get_ip(char *out, size_t n) {
  if (out == nullptr || n == 0) return;
  strncpy(out, s_ip, n - 1);
  out[n - 1] = '\0';
}

int link_modbus_get_ping_ms() {
  if (s_pingMs == LINK_PING_NONE) return -1;
  return (int)s_pingMs;
}

bool link_modbus_net_up() { return s_netUp; }

int link_modbus_get_db_latest() { return s_dbLatest; }

bool link_modbus_time_ok() { return s_timeOk; }

uint32_t link_modbus_net_age_ms() {
  if (s_netStampMs == 0) return 0;
  return millis() - s_netStampMs;
}
