#include <Arduino.h>
#include <ETH.h>
#include <ESPping.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include <cstdarg>
#include <cstring>

#include "supabase_ca.h"

// network-master: production WT32-ETH01 firmware.
//
// - Brings up the LAN8720 over RMII (variant defaults: ADDR 1, PWR 16,
//   MDC 23, MDIO 18).
// - Is a peer on the rusim-line v1 link to the display over UART0:
//   Debug-header TXD (IO1) -> display IO18 (RX),
//   Debug-header RXD (IO3) <- display IO17 (TX), 115200 8N1, common GND.
//   Serial is therefore both the link and the log sink: every log line is
//   emitted as "LOG <text>" and the display re-prints it on its own USB.
//   Serial1 (IO5/IO17) is not opened; that pair is reserved for RS485.
// - Gets the captive portal username/password from the display instead of
//   the serial console. Both ends declare their state every 5 s and push
//   changes immediately; there is no master and no polling.
// - Experiment state (duration, experiment number, running flag) arrives as
//   EXPERIMENT lines, so post-login edits on /control take effect without
//   re-login.
// - Runs the proven ping-test portal login (trigger -> login page scan
//   -> POST -> data-fetch verify), then loops ping of 1.1.1.1 plus an
//   HTTPS fetch. A WALLED fetch in RUN falls back to NEED_CREDS for
//   re-auth instead of halting.
// - Credentials live in RAM only and are zeroed after use.
//
// Wire protocol (must match firmware/display-v9/link_serial.h):
//   display -> WT32: HELLO, EXPERIMENT run num dur, CREDS user pass, DUMP
//   WT32 -> display: HELLO, LOGINREQ, RESULT OK|FAIL, NET ip up ping, DB ...
// See docs/llm-generated/link-serial-protocol.md.

#define LINK_LINE_MAX 1024
#define LINK_GAP_MS 50
#define LINK_BEAT_MS 5000
#define LINK_IP_MAX 16
#define LINK_USER_MAX 64
#define LINK_PASS_MAX 64

// Optional external LED + 1k to GND on IO2 (pulled low at boot, safe for
// strapping). It blinks briefly on any line received from the display as a
// bench confirmation that the link arrived.
#define TEST_LED_PIN 2
#define TEST_LED_MS 300

static const IPAddress kPingTarget(1, 1, 1, 1);
static const uint8_t kPingCount = 3;
static const uint32_t kIntervalMs = 5000;
static const char* kDataUrl = "https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ";
// Captive-portal trigger: online networks answer 204 with an empty body;
// walled networks answer 200 (interception page) or a 3xx redirect to login.
static const char* kCaptiveProbeUrl = "http://connectivitycheck.gstatic.com/generate_204";

// --- Supabase real-data fetch (backlog #1) + NTP (backlog #12) ---
// Supplied as build flags, never committed:
//   -DSUPABASE_URL="https://xyz.supabase.co" -DSUPABASE_ANON_KEY="..." -DDEVICE_TOKEN="..." 
#ifndef SUPABASE_URL
#define SUPABASE_URL ""
#endif
#ifndef SUPABASE_ANON_KEY
#define SUPABASE_ANON_KEY ""
#endif
#ifndef DEVICE_TOKEN
#define DEVICE_TOKEN ""
#endif
#ifndef RUSIM_DEVICE_ID
#define RUSIM_DEVICE_ID 0
#endif
static const char* kNtp1 = "pool.ntp.org";
static const char* kNtp2 = "time.google.com";
static bool ntpSynced = false;
static String dbSession;
static int dbLatest = -2;  // -2 unknown, -1 none yet, >=0 latest experiment_id
static int dbBottleCount = -1;  // -1 unknown, else bottle_count from authenticate()
// Owner contact from authenticate(), kept for logs/info only. Bounded
// char arrays, never heap Strings: truncated at 63 chars + NUL.
static char dbOwnerName[64] = {0};
static char dbOwnerEmail[64] = {0};
static uint32_t lastDbPollMs = 0;
static const uint32_t kDbPollMs = 30000;

static const size_t kMaxBody = 4096;
// Push body is sized separately from the portal's kMaxBody: one round of up
// to 24 bottle objects as JSON is ~3 KB, so 6 KB leaves headroom without
// inheriting the portal's 8 KB-page constant.
static const size_t kPushBodyCap = 6144;
// Ring capacity in bottle rows: 1200 rows = 50 full 24-bottle rounds at
// ~40 bytes/row (~48 KB). Well under the 100-150 KB ceiling; a dropped row
// is better than an overrun, and drops are counted, not silent.
static const size_t kPushQueueRows = 1200;
// Rounds flushed per syncDbLatest cycle: bounds worst-case blocking (each
// POST has its own 10 s timeout) while draining gradually after an outage.
static const int kPushFlushRounds = 2;
// Sample-time floor, mirroring the server backstop (2025-01-01). Rows older
// than this are rejected at enqueue so a 1970 clock can never enter the queue.
static const uint32_t kMinSampleUnix = 1735689600UL;
static const uint32_t kCredPollMs = 5000;
// Re-probe for an already-valid portal session while waiting for creds.
// Without this the gate deadlocks when no login is needed: the display
// waits for RESULT OK, but doLogin() (which detects "already through")
// is only reached after staged credentials arrive, which never come.
static const uint32_t kAuthProbeMs = 30000;
static uint32_t lastAuthProbeMs = (uint32_t)-30000;

enum Stage { STAGE_WAIT_IP, STAGE_NEED_CREDS, STAGE_LOGIN, STAGE_RUN };
static Stage stage = STAGE_WAIT_IP;
static uint32_t stageAtMs = 0;
static uint32_t lastBeatMs = 0;

// Fixed-size credential buffers. No Arduino String here: avoids heap
// fragmentation from repeated concat/realloc, and the password can be zeroed
// deterministically.
static const size_t kCredCap = LINK_USER_MAX + 1;
static char username[kCredCap];
static char password[kCredCap];
// Last attempted pair, kept for the "same pair -> re-emit the verdict" rule.
// RAM only, never a register, never flash.
static char lastUser[kCredCap];
static char lastPass[kCredCap];
static bool haveVerdict = false;
static bool lastVerdict = false;

static int durationH = 24;
static int expNum = 0;
static bool expRunning = false;
// Last values received from the display, so changes are logged once.
static int lastDurH = -1;
static int lastExpNum = -1;
static int lastExpRunning = -1;

static volatile bool eth_connected = false;
static volatile int ethEventCode = 0;  // set by the event task, logged in loop()
static volatile bool netDirty = false;
static int lastPingMs = -1;  // -1 = no measurement yet / failed

static uint32_t ledOffMs = 0;
// Push-queue counters live here because the DB line and the DUMP snapshot
// report them; the queue itself is defined further down.
static size_t s_pushCount = 0;    // rows held
static uint32_t s_pushDrops = 0;  // saturating overflow counter

// CRC16-Modbus, unused on the line link by design. Kept for the phase-2
// Modbus RTU master on the freed IO5/IO17 pair (MAX485, 9600 8N1).
static uint16_t __attribute__((unused)) linkCrc16(const uint8_t* data, size_t len) {
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

// --- line link -------------------------------------------------------------

static char linkRx[LINK_LINE_MAX + 1];
static size_t linkRxLen = 0;
static bool linkRxOverflow = false;
static uint32_t linkLastByteMs = 0;

// Every log line becomes one "LOG " line. Embedded CR/LF are escaped so one
// entry is always one line. Fixed static buffers: no heap, no String.
static void logf(const char* fmt, ...) {
  static char msg[LINK_LINE_MAX];
  static char escaped[2 * LINK_LINE_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  size_t o = 0;
  for (size_t i = 0; msg[i] != '\0' && o + 3 < sizeof(escaped); i++) {
    if (msg[i] == '\n') {
      escaped[o++] = '\\';
      escaped[o++] = 'n';
    } else if (msg[i] == '\r') {
      escaped[o++] = '\\';
      escaped[o++] = 'r';
    } else {
      escaped[o++] = msg[i];
    }
  }
  escaped[o] = '\0';
  Serial.print("LOG ");
  Serial.println(escaped);
}

static void link_emit_hello() { Serial.println("HELLO"); }

static void link_emit_loginreq() { Serial.println("LOGINREQ"); }

static void link_emit_result(bool ok) {
  Serial.println(ok ? "RESULT OK" : "RESULT FAIL");
}

static void link_emit_net() {
  bool up = eth_connected;
  if (!up) {
    // up=0 carries an empty ip= and no ping= field.
    Serial.println("NET ip= up=0");
    return;
  }
  IPAddress a = ETH.localIP();
  char ip[24];
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
  if (lastPingMs >= 0) {
    Serial.printf("NET ip=%s up=1 ping=%d\n", ip, lastPingMs);
  } else {
    Serial.printf("NET ip=%s up=1 ping=-\n", ip);
  }
}

static void link_emit_db() {
  char latest[16];
  if (dbLatest >= 0) {
    snprintf(latest, sizeof(latest), "%d", dbLatest);
  } else if (dbLatest == -1) {
    snprintf(latest, sizeof(latest), "0");  // no rows yet -> display suggests 1
  } else {
    snprintf(latest, sizeof(latest), "-");
  }
  char bottles[16];
  if (dbBottleCount > 0) {
    snprintf(bottles, sizeof(bottles), "%d", dbBottleCount);
  } else {
    snprintf(bottles, sizeof(bottles), "-");
  }
  Serial.printf("DB latest=%s time=%d bottles=%s drops=%u\n", latest,
                ntpSynced ? 1 : 0, bottles, (unsigned)s_pushDrops);
}

// Full current truth, sent once per heartbeat. RESULT is deliberately not
// repeated here: it marks a login attempt, and the HELLO snapshot covers the
// "already running" case.
static void link_emit_truth() {
  link_emit_net();
  link_emit_db();
  if (stage == STAGE_NEED_CREDS) link_emit_loginreq();
}

// Answer to a display HELLO: the same truth, plus RESULT OK when logged in so
// a display that rebooted mid-run can leave the login gate.
static void link_send_snapshot() {
  link_emit_net();
  link_emit_db();
  if (stage == STAGE_NEED_CREDS) link_emit_loginreq();
  if (stage == STAGE_RUN) link_emit_result(true);
}

// Reads key=value out of a space-separated argument string. Bounded copy.
static bool arg_lookup(const char* args, const char* key, char* out, size_t cap) {
  if (args == nullptr || key == nullptr || out == nullptr || cap == 0) return false;
  size_t keyLen = strlen(key);
  const char* p = args;
  while (p != nullptr && *p != '\0') {
    const char* end = strchr(p, ' ');
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

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Decodes the percent-escaped user/pass values. Truncates safely at cap - 1.
static void pct_decode(const char* in, char* out, size_t cap) {
  size_t o = 0;
  for (size_t i = 0; in[i] != '\0' && o + 1 < cap; i++) {
    if (in[i] == '%' && in[i + 1] != '\0' && in[i + 2] != '\0') {
      int hi = hexVal(in[i + 1]);
      int lo = hexVal(in[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out[o++] = (char)((hi << 4) | lo);
        i += 2;
        continue;
      }
    }
    out[o++] = in[i];
  }
  out[o] = '\0';
}

static void trimInPlace(char* s) {
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

static void clearCreds() {
  // Overwrite before clearing so the password never lingers in RAM.
  memset(password, 'x', sizeof(password) - 1);
  memset(password, 0, sizeof(password));
  memset(username, 0, sizeof(username));
}

static void handle_experiment(const char* args) {
  char runRaw[8], numRaw[12], durRaw[8];
  if (!arg_lookup(args, "run", runRaw, sizeof(runRaw)) ||
      !arg_lookup(args, "num", numRaw, sizeof(numRaw)) ||
      !arg_lookup(args, "dur", durRaw, sizeof(durRaw))) {
    logf("[LINK] malformed EXPERIMENT ignored");
    return;
  }
  int run = atoi(runRaw) != 0 ? 1 : 0;
  long n = strtol(numRaw, nullptr, 10);
  long d = strtol(durRaw, nullptr, 10);
  if (n < 0) n = 0;
  if (n > 65535) n = 65535;
  if (d < 1) d = 1;
  if (d > 99) d = 99;
  expRunning = (run != 0);
  expNum = (int)n;
  durationH = (int)d;
  if (durationH != lastDurH || expNum != lastExpNum ||
      (int)expRunning != lastExpRunning) {
    logf("[LINK] exp state: run=%d exp=%d dur=%dh", expRunning ? 1 : 0, expNum,
         durationH);
    lastDurH = durationH;
    lastExpNum = expNum;
    lastExpRunning = expRunning ? 1 : 0;
  }
}

// New pair -> stage LOGIN. Identical in-flight pair -> ignore (the display
// re-pushes every heartbeat until it hears a verdict). Same pair as the last
// attempt -> re-emit that verdict instead of logging in again.
static void handle_creds(const char* args) {
  char userEnc[3 * LINK_USER_MAX + 1];
  char passEnc[3 * LINK_PASS_MAX + 1];
  if (!arg_lookup(args, "user", userEnc, sizeof(userEnc)) ||
      !arg_lookup(args, "pass", passEnc, sizeof(passEnc))) {
    logf("[LINK] malformed CREDS ignored");
    return;
  }
  char u[kCredCap];
  char p[kCredCap];
  pct_decode(userEnc, u, sizeof(u));
  pct_decode(passEnc, p, sizeof(p));
  trimInPlace(u);
  trimInPlace(p);
  if (u[0] == '\0' || p[0] == '\0') {
    memset(p, 0, sizeof(p));
    logf("[LINK] CREDS empty, ignored");
    return;
  }
  if (stage == STAGE_WAIT_IP) {
    memset(p, 0, sizeof(p));
    logf("[LINK] CREDS ignored: no IP yet");
    return;
  }
  if (stage == STAGE_LOGIN && strcmp(u, username) == 0 && strcmp(p, password) == 0) {
    memset(p, 0, sizeof(p));
    logf("[LINK] CREDS duplicate while logging in, ignored");
    return;
  }
  if (haveVerdict && strcmp(u, lastUser) == 0 && strcmp(p, lastPass) == 0) {
    memset(p, 0, sizeof(p));
    logf("[LINK] CREDS same pair, answering %s", lastVerdict ? "OK" : "FAIL");
    link_emit_result(lastVerdict);
    return;
  }

  strncpy(username, u, sizeof(username) - 1);
  username[sizeof(username) - 1] = '\0';
  strncpy(password, p, sizeof(password) - 1);
  password[sizeof(password) - 1] = '\0';
  strncpy(lastUser, u, sizeof(lastUser) - 1);
  lastUser[sizeof(lastUser) - 1] = '\0';
  strncpy(lastPass, p, sizeof(lastPass) - 1);
  lastPass[sizeof(lastPass) - 1] = '\0';
  haveVerdict = false;
  // Wipe the decoded stack copies; the working pair is wiped after login.
  memset(u, 0, sizeof(u));
  memset(p, 0, sizeof(p));
  logf("[LINK] captured creds user='%s' (password in RAM only)", username);
  stage = STAGE_LOGIN;
}

static void link_log_dump() {
  IPAddress a = ETH.localIP();
  char ip[24];
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
  logf("[DUMP] stage=%d eth=%d ip=%s exp=%d run=%d dur=%d dbLatest=%d ntp=%d "
       "bottles=%d queued=%u drops=%u verdict=%d",
       (int)stage, eth_connected ? 1 : 0, ip, expNum, expRunning ? 1 : 0, durationH,
       dbLatest, ntpSynced ? 1 : 0, dbBottleCount, (unsigned)s_pushCount,
       (unsigned)s_pushDrops, haveVerdict ? (lastVerdict ? 1 : 2) : 0);
}

static void handle_line(char* line) {
  char* args = strchr(line, ' ');
  if (args != nullptr) {
    *args++ = '\0';
    while (*args == ' ') args++;
  }
  if (strcmp(line, "HELLO") == 0) {
    logf("[LINK] HELLO from display, sending snapshot");
    link_send_snapshot();
  } else if (strcmp(line, "EXPERIMENT") == 0 && args != nullptr) {
    handle_experiment(args);
  } else if (strcmp(line, "CREDS") == 0 && args != nullptr) {
    handle_creds(args);
  } else if (strcmp(line, "DUMP") == 0) {
    // Read-only, so accepted ungated: the debug gate lives on the display
    // sender (W32CMD).
    link_log_dump();
  }
  // Unknown and WT32-direction words are ignored, which also swallows the
  // boot ROM text and a looped-back command.
}

static void link_line_pull() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    linkLastByteMs = millis();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!linkRxOverflow && linkRxLen > 0) {
        linkRx[linkRxLen] = '\0';
        digitalWrite(TEST_LED_PIN, HIGH);
        ledOffMs = millis() + TEST_LED_MS;
        handle_line(linkRx);
      }
      linkRxLen = 0;
      linkRxOverflow = false;
      continue;
    }
    if (linkRxOverflow) continue;
    if (linkRxLen >= LINK_LINE_MAX) {
      linkRxLen = 0;
      linkRxOverflow = true;
      continue;
    }
    linkRx[linkRxLen++] = (char)c;
  }
  // A stalled partial line never wedges the parser.
  if (linkRxLen > 0 && (millis() - linkLastByteMs) > LINK_GAP_MS) {
    linkRxLen = 0;
    linkRxOverflow = false;
  }
}

static void link_heartbeat_tick() {
  if (millis() - lastBeatMs < LINK_BEAT_MS) return;
  lastBeatMs = millis();
  link_emit_truth();
}

// --- Ethernet + portal login (ported from wt32-eth01_ping-test) ---

void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("rusim-master");
      ethEventCode = 1;
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      ethEventCode = 2;
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      eth_connected = true;
      lastPingMs = -1;
      ethEventCode = 3;
      netDirty = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      eth_connected = false;
      lastPingMs = -1;
      ethEventCode = 4;
      netDirty = true;
      break;
    default:
      break;
  }
}

// Logged from loop(), not from the event task, so a LOG line is never split by
// another task writing to the same UART.
static void log_eth_event() {
  int code = ethEventCode;
  ethEventCode = 0;
  switch (code) {
    case 1:
      logf("[ETH] Started");
      break;
    case 2:
      logf("[ETH] Link up");
      break;
    case 3: {
      IPAddress ip = ETH.localIP();
      IPAddress gw = ETH.gatewayIP();
      IPAddress mask = ETH.subnetMask();
      logf("[ETH] Got IP %u.%u.%u.%u gw %u.%u.%u.%u mask %u.%u.%u.%u mac %s",
           ip[0], ip[1], ip[2], ip[3], gw[0], gw[1], gw[2], gw[3], mask[0],
           mask[1], mask[2], mask[3], ETH.macAddress().c_str());
      break;
    }
    case 4:
      logf("[ETH] Link down");
      break;
    default:
      break;
  }
}

// Reads the response into a caller buffer, capped at kMaxBody characters.
// Fixed-buffer hot path: no incremental String concat, so no heap
// fragmentation from repeated realloc. Returns bytes stored (excl. NUL).
static size_t readBodyFixed(HTTPClient& http, char* out, size_t cap) {
  if (out == nullptr || cap == 0) return 0;
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    out[0] = '\0';
    return 0;
  }
  size_t limit = kMaxBody;
  if (limit > cap - 1) limit = cap - 1;
  size_t total = 0;
  uint8_t buf[256];
  while (http.connected() && total < limit) {
    int n = stream->readBytes(reinterpret_cast<char*>(buf), sizeof(buf));
    if (n <= 0) break;
    size_t room = limit - total;
    size_t take = (size_t)n < room ? (size_t)n : room;
    memcpy(out + total, buf, take);
    total += take;
  }
  out[total] = '\0';
  return total;
}

// One-shot wrapper for callers that need a String once. Single allocation
// from the fixed buffer, not per-chunk concat.
String readBody(HTTPClient& http) {
  static char tmp[kMaxBody + 1];
  size_t n = readBodyFixed(http, tmp, sizeof(tmp));
  return String(tmp);
}

// The interception page is a JS redirect, not an HTTP one:
//   <script language="JavaScript">window.location="URL";</script>
String extractWindowLocation(const String& html) {
  int start = html.indexOf("window.location=\"");
  if (start < 0) start = html.indexOf("window.location='");
  if (start < 0) return "";
  int q = html.indexOf('"', start + 17);
  if (q < 0) q = html.indexOf('\'', start + 17);
  if (q < 0) return "";
  return html.substring(start + 17, q);
}

// What a login page scan recovers. The login page is roughly 8 KB of inline
// CSS before the form, so the body is scanned through a small rolling window
// instead of being buffered. Fixed char arrays: no String heap churn.
struct PageScan {
  bool hasForm = false;
  char formAction[256] = {0};
  char redir[256] = {0};
  char magic[128] = {0};
  bool hasUsernameField = false;
  size_t totalBytes = 0;
};

// Searches NUL-terminated `win` for needle, then copies the first
// value="..." at or after it into out. Returns true on success.
static bool valueAfterFixed(const char* win, const char* needle, char* out, size_t cap,
                            size_t limit = 512) {
  if (!win || !needle || !out || cap == 0) return false;
  const char* at = strstr(win, needle);
  if (!at) return false;
  const char* v = strstr(at + strlen(needle), "value=\"");
  if (!v || (size_t)(v - at) > limit) return false;
  v += 7;
  const char* end = strchr(v, '"');
  if (!end) return false;
  size_t len = (size_t)(end - v);
  if (len >= cap) len = cap - 1;
  memcpy(out, v, len);
  out[len] = '\0';
  return len > 0;
}

void scanPage(HTTPClient& http, PageScan& out) {
  static const size_t kWindow = 320;  // > longest needle, small enough for RAM
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return;

  // Rolling window as a fixed NUL-terminated buffer: no String concat in
  // the hot loop, so no heap realloc churn while scanning ~8 KB pages.
  char win[kWindow + 1] = {0};
  size_t winLen = 0;
  uint8_t buf[256];
  while (http.connected()) {
    int n = stream->readBytes(reinterpret_cast<char*>(buf), sizeof(buf));
    if (n <= 0) break;
    out.totalBytes += (size_t)n;
    // Append chunk, keeping only the last kWindow bytes.
    size_t chunk = (size_t)n;
    if (chunk + winLen > kWindow) {
      size_t drop = chunk + winLen - kWindow;
      if (drop >= winLen) {
        // Chunk alone fills the window; keep its tail.
        size_t off = chunk - kWindow;
        memcpy(win, buf + off, kWindow);
        winLen = kWindow;
      } else {
        memmove(win, win + drop, winLen - drop);
        winLen -= drop;
        memcpy(win + winLen, buf, chunk);
        winLen += chunk;
      }
    } else {
      memcpy(win + winLen, buf, chunk);
      winLen += chunk;
    }
    win[winLen] = '\0';

    if (!out.hasForm) {
      const char* f = strstr(win, "<form");
      if (f) {
        out.hasForm = true;
        const char* a = strstr(f, "action=\"");
        if (a) {
          a += 8;
          const char* e = strchr(a, '"');
          if (e && e > a) {
            size_t len = (size_t)(e - a);
            if (len >= sizeof(out.formAction)) len = sizeof(out.formAction) - 1;
            memcpy(out.formAction, a, len);
            out.formAction[len] = '\0';
          }
        }
      }
    }
    if (out.magic[0] == '\0') {
      valueAfterFixed(win, "name=\"magic\"", out.magic, sizeof(out.magic));
    }
    if (out.redir[0] == '\0') {
      valueAfterFixed(win, "name=\"4Tredir\"", out.redir, sizeof(out.redir));
    }
    if (!out.hasUsernameField && strstr(win, "name=\"username\"") != nullptr) {
      out.hasUsernameField = true;
    }
  }
}

// Percent-encodes s for use in an application/x-www-form-urlencoded body.
String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

// Returns the "scheme://host[:port]" prefix of a URL.
String urlBase(const String& url) {
  int scheme = url.indexOf("://");
  if (scheme < 0) return "";
  int slash = url.indexOf('/', scheme + 3);
  if (slash < 0) return url;
  return url.substring(0, slash);
}

// Resolves the form action against the page URL. Handles absolute URLs,
// root-relative paths ("/"), and bare relative paths.
String resolveAction(const String& pageUrl, const String& action) {
  if (action.startsWith("http://") || action.startsWith("https://")) {
    return action;
  }
  String base = urlBase(pageUrl);
  if (action.startsWith("/")) return base + action;
  int q = pageUrl.indexOf('?');
  String noQuery = q >= 0 ? pageUrl.substring(0, q) : pageUrl;
  int lastSlash = noQuery.lastIndexOf('/');
  return noQuery.substring(0, lastSlash + 1) + action;
}

// GETs the 204 trigger URL and returns the portal login URL, or "" when the
// network is open (204, already authenticated or no portal).
// A 3xx with Location is the common redirect-style portal; a 200 means the
// request was intercepted, so the body is scanned for the JS redirect.
String fetchPortalUrl() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, kCaptiveProbeUrl);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setTimeout(5000);
  const char* headerKeys[] = {"Location"};
  http.collectHeaders(headerKeys, 1);

  logf("[LOGIN] GET trigger %s", kCaptiveProbeUrl);
  int code = http.GET();
  logf("[LOGIN] trigger status: %d", code);
  String portalUrl;
  if (code == 204) {
    logf("[LOGIN] portal URL: (none, 204 online)");
  } else if (code == 301 || code == 302 || code == 303 || code == 307 ||
             code == 308) {
    portalUrl = http.header("Location");
    logf("[LOGIN] portal URL: %s",
         portalUrl.length() ? portalUrl.c_str() : "(redirect without Location)");
  } else if (code == 200) {
    portalUrl = extractWindowLocation(readBody(http));
    logf("[LOGIN] portal URL: %s",
         portalUrl.length() ? portalUrl.c_str() : "(200 without redirect URL)");
  } else if (code > 0) {
    logf("[LOGIN] unexpected trigger status (treating as walled)");
  } else {
    logf("[LOGIN] trigger error: %s", http.errorToString(code).c_str());
  }
  http.end();
  return portalUrl;
}

// Fetches the login page and fills `scan` with the form action and hidden
// fields. Returns false on transport failure.
bool fetchLoginPage(const String& portalUrl, PageScan& scan) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, portalUrl);
  http.setTimeout(8000);

  logf("[LOGIN] GET login page %s", portalUrl.c_str());
  int code = http.GET();
  logf("[LOGIN] login page status: %d", code);
  if (code != 200) {
    if (code < 0) {
      logf("[LOGIN] login page error: %s", http.errorToString(code).c_str());
    }
    http.end();
    return false;
  }
  scanPage(http, scan);
  http.end();

  logf("[LOGIN] form action: %s", scan.hasForm ? scan.formAction : "(no form found)");
  logf("[LOGIN] 4Tredir: %s", scan.redir[0] ? scan.redir : "(not found)");
  logf("[LOGIN] magic: %s", scan.magic[0] ? scan.magic : "(not found)");
  return scan.hasForm && scan.magic[0] != '\0';
}

// POSTs the credentials to the portal. Returns true when the response looks
// like an acceptance (redirect or success page), false otherwise. The final
// word comes from the data-fetch verification, not from here.
bool postCredentials(const String& pageUrl, const PageScan& scan) {
  String postUrl = resolveAction(pageUrl, scan.formAction);
  String body = "4Tredir=" + urlEncode(scan.redir) +
                "&magic=" + urlEncode(scan.magic) +
                "&username=" + urlEncode(username) +
                "&password=" + urlEncode(password);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, postUrl);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Referer", pageUrl);
  const char* headerKeys[] = {"Location", "Set-Cookie"};
  http.collectHeaders(headerKeys, 2);

  logf("[LOGIN] POST %s", postUrl.c_str());
  logf("[LOGIN] body length: %u", (unsigned)body.length());
  int code = http.POST(body);
  logf("[LOGIN] POST status: %d", code);

  bool accepted = false;
  if (code > 0) {
    logf("[LOGIN] Location: %s", http.header("Location").c_str());
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      accepted = true;
    }
    String resp = readBody(http);
    logf("[LOGIN] response bytes: %u", (unsigned)resp.length());
    logf("[LOGIN] response head: %s", resp.substring(0, 800).c_str());
    // A success page echoes a meta-refresh or a link to the 4Tredir target;
    // an error page names the failure. Either way the verifier decides.
    if (resp.indexOf("success") >= 0 || resp.indexOf("Success") >= 0 ||
        resp.indexOf(scan.redir) >= 0) {
      accepted = true;
    }
  } else {
    logf("[LOGIN] POST error: %s", http.errorToString(code).c_str());
  }
  http.end();
  return accepted;
}

// Fetches the real data URL and reports whether the body is genuine content
// (true) or the portal's interception page (false).
bool fetchData() {
  WiFiClientSecure client;
  client.setInsecure();  // skip CA verification for this test
  HTTPClient http;
  http.begin(client, kDataUrl);
  http.setTimeout(8000);

  logf("[HTTP] GET %s", kDataUrl);
  int code = http.GET();
  logf("[HTTP] status: %d", code);

  bool genuine = false;
  if (code > 0) {
    String body = readBody(http);
    if (extractWindowLocation(body).length() > 0) {
      logf("[HTTP] body is the portal interception page (WALLED)");
    } else {
      genuine = true;
      logf("[HTTP] body is genuine content (AUTHENTICATED): %s", body.c_str());
    }
  } else {
    logf("[HTTP] error: %s", http.errorToString(code).c_str());
  }
  http.end();
  return genuine;
}

// --- NTP (backlog #12): UTC via pool.ntp.org. Call after DHCP. ---
static void ensureNtpSync() {
  if (ntpSynced) return;
  configTime(0, 0, kNtp1, kNtp2);
  logf("[NTP] syncing UTC via pool.ntp.org...");
  for (int i = 0; i < 10; i++) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
      ntpSynced = true;
      struct tm tmv;
      gmtime_r(&now, &tmv);
      logf("[NTP] synced: %04d-%02d-%02d %02d:%02d:%02d UTC", tmv.tm_year + 1900,
           tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
      return;
    }
    delay(1000);
  }
  logf("[NTP] sync failed, will retry next cycle");
}

// --- Supabase TLS + auth metadata + push queue (esp-db-sync plan) ---

// Supabase gets a pinned CA bundle; the captive-portal path below stays
// setInsecure() because the interception sits above TLS termination and
// cannot be verified. Never add a setInsecure() fallback here: a broken pin
// must fail loudly (soft, visible degradation) rather than silently reopen
// the hole.
static void sbApplyTls(WiFiClientSecure& client) { client.setCACert(kSupabaseCaPem); }

// Parses one integer field out of a flat JSON object reply. Bounded: no
// allocation, works on the caller's NUL-terminated buffer. Returns false
// when the key is absent or not a number; the caller keeps its default.
static bool sbParseIntField(const char* resp, const char* key, int& out) {
  if (resp == nullptr || key == nullptr) return false;
  char needle[48];
  snprintf(needle, sizeof(needle), "\"%s\":", key);
  const char* at = strstr(resp, needle);
  if (at == nullptr) return false;
  at += strlen(needle);
  while (*at == ' ' || *at == '\t') at++;
  if ((*at < '0' || *at > '9') && *at != '-') return false;
  out = atoi(at);
  return true;
}

// Parses one string-or-null field. Absent key -> false (caller keeps its
// default); explicit null -> true with empty output; otherwise a bounded,
// NUL-terminated copy truncated at cap - 1.
static bool sbParseStrField(const char* resp, const char* key, char* out, size_t cap) {
  if (resp == nullptr || key == nullptr || out == nullptr || cap == 0) return false;
  char needle[48];
  snprintf(needle, sizeof(needle), "\"%s\":", key);
  const char* at = strstr(resp, needle);
  if (at == nullptr) return false;
  at += strlen(needle);
  while (*at == ' ' || *at == '\t') at++;
  if (strncmp(at, "null", 4) == 0) {
    out[0] = '\0';
    return true;
  }
  if (*at != '"') return false;
  at++;
  size_t pos = 0;
  while (*at != '\0' && *at != '"' && pos + 1 < cap) {
    if (*at == '\\' && *(at + 1) != '\0') at++;  // keep the escaped char raw
    out[pos++] = *at++;
  }
  out[pos] = '\0';
  return true;
}

// Keeps bottle_count + owner contact from authenticate() instead of
// discarding them. bottle_count feeds the DB line; owner fields are log only.
static void sbParseAuthMeta(const String& resp) {
  const char* c = resp.c_str();
  int bottles = -1;
  if (sbParseIntField(c, "bottle_count", bottles) && bottles > 0 && bottles <= 99) {
    dbBottleCount = bottles;
  }
  sbParseStrField(c, "owner_name", dbOwnerName, sizeof(dbOwnerName));
  sbParseStrField(c, "owner_email", dbOwnerEmail, sizeof(dbOwnerEmail));
}

// --- Sample push queue: fixed ring, oldest-drop (memory-safety skill:
// bounded buffer, no heap in the hot path) ---
//
// One row per bottle that reported. An absent bottle (disconnected) is
// simply not enqueued, which is distinct from a present-but-null sensor
// (nullMask bit set). Rows carry sample time, never flush time.
struct PushRow {
  uint8_t bottleId;
  int32_t experimentId;  // <0 = omit, server applies the active experiment
  int64_t sampleUnix;    // seconds since epoch at sampling
  float ph;
  float pressure;
  float tempC;
  float co2;
  float ch4;
  uint8_t nullMask;  // bit0 ph, 1 pressure, 2 temp, 3 co2, 4 ch4
};

// One bottle as handed over by the future sensor poller (dormant until the
// deferred sensor chain lands; index in the array is the bottle id).
struct BottleReading {
  float ph;
  float pressure;
  float tempC;
  float co2;
  float ch4;
  uint8_t nullMask;
};

static PushRow s_pushQueue[kPushQueueRows];
static size_t s_pushHead = 0;   // oldest row
static size_t s_pushTail = 0;   // next write slot
static char s_pushBody[kPushBodyCap];

// Queues one sample round (up to 24 bottles). Rejects while NTP is unsynced
// and any sample older than kMinSampleUnix, so no 1970 row can ever enter.
// Full queue drops the oldest row, never the newest, and counts it.
static bool pushEnqueueRound(time_t sampleUnix, int experimentId,
                             const BottleReading* readings, size_t n) {
  if (!ntpSynced) {
    logf("[PUSH] refused: NTP unsynced (time=0)");
    return false;
  }
  if (readings == nullptr || n == 0 || n > 24) return false;
  if (sampleUnix < (time_t)kMinSampleUnix) {
    logf("[PUSH] refused: sample time predates 2025-01-01");
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    if (i > 255) break;  // bottleId is uint8_t; rounds never exceed 24
    if (s_pushCount == kPushQueueRows) {
      s_pushHead = (s_pushHead + 1) % kPushQueueRows;
      s_pushCount--;
      if (s_pushDrops < 0xFFFFFFFFu) s_pushDrops++;
    }
    PushRow& r = s_pushQueue[s_pushTail];
    r.bottleId = (uint8_t)i;
    r.experimentId = (int32_t)experimentId;
    r.sampleUnix = (int64_t)sampleUnix;
    r.ph = readings[i].ph;
    r.pressure = readings[i].pressure;
    r.tempC = readings[i].tempC;
    r.co2 = readings[i].co2;
    r.ch4 = readings[i].ch4;
    r.nullMask = readings[i].nullMask;
    s_pushTail = (s_pushTail + 1) % kPushQueueRows;
    s_pushCount++;
  }
  return true;
}

// --- Supabase real-data fetch (backlog #1) via device token (devices.token_hash) ---
static bool sbAuthenticate() {
  if (String(SUPABASE_URL).length() == 0 || String(SUPABASE_ANON_KEY).length() == 0 ||
      String(DEVICE_TOKEN).length() == 0) {
    logf("[DB] Supabase not configured (SUPABASE_URL/ANON_KEY/DEVICE_TOKEN empty)");
    return false;
  }
  String url = String(SUPABASE_URL) + "/rest/v1/rpc/authenticate";
  String body = String("{\"p_token\":\"") + DEVICE_TOKEN + "\"}";
  WiFiClientSecure client;
  sbApplyTls(client);
  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(10000);
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  int code = http.POST(body);
  bool ok = false;
  if (code == 200) {
    String resp = http.getString();
    int at = resp.indexOf("session_token");
    if (at >= 0) {
      // Value may be quoted; find the 64-hex run directly.
      int hexStart = -1;
      for (int i = at; i + 64 <= (int)resp.length(); i++) {
        bool hex = true;
        for (int j = 0; j < 64; j++) {
          char c = resp[i + j];
          if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { hex = false; break; }
        }
        if (hex) { hexStart = i; break; }
      }
      if (hexStart >= 0) {
        dbSession = resp.substring(hexStart, hexStart + 64);
        sbParseAuthMeta(resp);
        logf("[DB] authenticated, session cached (RAM only), bottles=%d owner='%s'",
             dbBottleCount, dbOwnerName);
        ok = true;
      }
    }
    if (!ok) logf("[DB] authenticate: no session_token in reply (wrong token?)");
  } else {
    logf("[DB] authenticate HTTP %d", code);
  }
  http.end();
  return ok;
}

static int sbParseLatestExp(const String& resp) {
  int at = resp.indexOf("experiment_id");
  if (at < 0) return -1;  // empty array = no experiments yet
  int colon = resp.indexOf(':', at);
  if (colon < 0) return -1;
  return resp.substring(colon + 1).toInt();
}

// Returns latest experiment_id, -1 when none, -2 on failure.
static int sbFetchLatestExp() {
  if (dbSession.length() == 0 && !sbAuthenticate()) return -2;
  String url = String(SUPABASE_URL) +
               "/rest/v1/readings?select=experiment_id&order=experiment_id.desc&limit=1";
  for (int attempt = 0; attempt < 2; attempt++) {
    WiFiClientSecure client;
    sbApplyTls(client);
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(10000);
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("x-device-session", dbSession);
    http.addHeader("Accept", "application/json");
    int code = http.GET();
    if (code == 200) {
      String resp = http.getString();
      http.end();
      int latest = sbParseLatestExp(resp);
      logf("[DB] latest experiment_id=%d (raw %d chars)", latest, resp.length());
      return latest;
    }
    logf("[DB] readings HTTP %d (attempt %d)", code, attempt);
    http.end();
    if (code == 401 || code == 403) {
      dbSession = "";  // session expired, re-login once
      if (!sbAuthenticate()) return -2;
    } else {
      return -2;
    }
  }
  return -2;
}

// Formats one queued round (all rows sharing the head's sampleUnix, up to
// 24) as a JSON array in s_pushBody. Returns the row count, or 0 when the
// queue is empty or the body would overflow (which sizes kPushBodyCap).
static size_t pushFormatHeadRound() {
  if (s_pushCount == 0) return 0;
  int64_t headTs = s_pushQueue[s_pushHead].sampleUnix;
  size_t rows = 0;
  while (rows < s_pushCount && rows < 24) {
    if (s_pushQueue[(s_pushHead + rows) % kPushQueueRows].sampleUnix != headTs) break;
    rows++;
  }
  struct tm tmv;
  time_t ts = (time_t)headTs;
  gmtime_r(&ts, &tmv);
  char tsBuf[32];
  snprintf(tsBuf, sizeof(tsBuf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
           tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
  size_t pos = 0;
  s_pushBody[0] = '[';
  pos = 1;
  for (size_t k = 0; k < rows; k++) {
    const PushRow& r = s_pushQueue[(s_pushHead + k) % kPushQueueRows];
    char obj[256];
    char phB[16], prB[16], teB[16], coB[16], chB[16];
    auto fmtVal = [](float v, bool isNull, char* b, size_t n) {
      if (isNull) snprintf(b, n, "null");
      else snprintf(b, n, "%.6g", (double)v);
    };
    fmtVal(r.ph, r.nullMask & 0x01, phB, sizeof(phB));
    fmtVal(r.pressure, r.nullMask & 0x02, prB, sizeof(prB));
    fmtVal(r.tempC, r.nullMask & 0x04, teB, sizeof(teB));
    fmtVal(r.co2, r.nullMask & 0x08, coB, sizeof(coB));
    fmtVal(r.ch4, r.nullMask & 0x10, chB, sizeof(chB));
    int w;
    if (r.experimentId < 0) {
      w = snprintf(obj, sizeof(obj),
                   "%s{\"bottle_id\":%u,\"timestamp\":\"%s\","
                   "\"ph\":%s,\"pressure\":%s,\"temp\":%s,\"co2\":%s,\"ch4\":%s}",
                   k ? "," : "", r.bottleId, tsBuf, phB, prB, teB, coB, chB);
    } else {
      w = snprintf(obj, sizeof(obj),
                   "%s{\"bottle_id\":%u,\"timestamp\":\"%s\",\"experiment_id\":%ld,"
                   "\"ph\":%s,\"pressure\":%s,\"temp\":%s,\"co2\":%s,\"ch4\":%s}",
                   k ? "," : "", r.bottleId, tsBuf, (long)r.experimentId,
                   phB, prB, teB, coB, chB);
    }
    if (w <= 0 || (size_t)w >= sizeof(obj)) return 0;
    if (pos + (size_t)w + 2 > sizeof(s_pushBody)) return 0;  // +2 for ']' + NUL
    memcpy(s_pushBody + pos, obj, (size_t)w);
    pos += (size_t)w;
  }
  s_pushBody[pos++] = ']';
  s_pushBody[pos] = '\0';
  return rows;
}

// POSTs the oldest queued round to push_readings(). The RPC resolves
// device_id from the session and ignores unknown keys, so identity cannot be
// forged from here; replays are idempotent server-side. Dequeues only on a
// 2xx so a failed flush is retried, never lost (until oldest-drop).
// Returns true when the queue advanced (round sent or queue empty).
static bool sbPushOneRound() {
  if (s_pushCount == 0) return true;
  if (dbSession.length() == 0 && !sbAuthenticate()) return false;
  size_t rows = pushFormatHeadRound();
  if (rows == 0) {
    logf("[PUSH] format failed (queue inconsistent or body cap)");
    return false;
  }
  String url = String(SUPABASE_URL) + "/rest/v1/rpc/push_readings";
  for (int attempt = 0; attempt < 2; attempt++) {
    WiFiClientSecure client;
    sbApplyTls(client);
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(10000);
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    http.addHeader("x-device-session", dbSession);
    int code = http.POST((uint8_t*)s_pushBody, strlen(s_pushBody));
    if (code >= 200 && code < 300) {
      String resp = http.getString();
      http.end();
      s_pushHead = (s_pushHead + rows) % kPushQueueRows;
      s_pushCount -= rows;
      logf("[PUSH] flushed %u rows (server says %s, %u left)", (unsigned)rows,
           resp.c_str(), (unsigned)s_pushCount);
      return true;
    }
    logf("[PUSH] push_readings HTTP %d (attempt %d, %u queued)", code, attempt,
         (unsigned)s_pushCount);
    http.end();
    if (code == 401 || code == 403) {
      dbSession = "";
      if (!sbAuthenticate()) return false;
    } else {
      return false;
    }
  }
  return false;
}

// Flushes buffered rounds, oldest first. Clock-gated: returns early while
// NTP is unsynced so nothing is ever stamped at 1970; rows stay queued.
// Dormant until the sensor chain lands: with an empty queue this is a no-op.
static void sbPushReadings() {
  if (!ntpSynced) return;
  if (s_pushCount == 0) return;
  if (String(SUPABASE_URL).length() == 0 || String(SUPABASE_ANON_KEY).length() == 0 ||
      String(DEVICE_TOKEN).length() == 0) {
    return;
  }
  for (int i = 0; i < kPushFlushRounds && s_pushCount > 0; i++) {
    if (!sbPushOneRound()) break;
  }
}

static void syncDbLatest(bool force) {
  if (millis() - lastDbPollMs < kDbPollMs && !force) return;
  lastDbPollMs = millis();
  ensureNtpSync();
  int latest = sbFetchLatestExp();
  if (latest != -2) {
    if (latest != dbLatest) {
      logf("[DB] latest %d -> %d, next=%d", dbLatest, latest, latest + 1);
    }
    dbLatest = latest;
  } else {
    logf("[DB] fetch failed, keeping last known value");
  }
  // Every DB pass declares the result, including a failed one: the display
  // shows the last known truth with a fresh age stamp.
  link_emit_db();
  // Drain buffered sample rounds (no-op until the sensor chain enqueues).
  // Clock gating lives inside sbPushReadings: time= state above is unaffected,
  // so the display keeps showing unsynced state.
  sbPushReadings();
}

// Returns true on authenticated, false otherwise. The caller emits RESULT and
// wipes the working pair.
bool doLogin() {
  logf("[LOGIN] user='%s' from display (password in RAM only)", username);

  String portalUrl = fetchPortalUrl();
  if (portalUrl.length() == 0) {
    logf("[LOGIN] No interception. Checking whether we are already through...");
    if (fetchData()) {
      logf("[LOGIN] Already authenticated, skipping POST.");
      return true;
    }
    logf("[LOGIN] No portal and no internet. Will re-poll display.");
    return false;
  }

  PageScan scan;
  if (!fetchLoginPage(portalUrl, scan)) {
    logf("[LOGIN] Could not read the login page. Will re-poll display.");
    return false;
  }

  postCredentials(portalUrl, scan);

  logf("[LOGIN] Verifying with a real data fetch...");
  bool ok = fetchData();
  if (ok) {
    logf("[LOGIN] SUCCESS: port is authenticated.");
  } else {
    logf("[LOGIN] FAILED: still walled (wrong credentials or portal changed).");
  }
  return ok;
}

void setup() {
  Serial.begin(115200);
  // Give the USB-serial monitor time to attach, then push the ROM
  // bootloader's boot message (printed at 74880 baud, so it shows as
  // garbage at 115200) off screen with a few blank lines. Blank lines are
  // ignored by the display parser and stay useful for a bare adapter.
  delay(1000);
  for (int i = 0; i < 5; i++) Serial.println();
  Serial.flush();

  logf("network-master (production WT32 portal + rusim-line peer)");
  logf("Ping target: 1.1.1.1");
  logf("Data URL: %s", kDataUrl);
  logf("Credentials come from the display over UART0, never stored.");

  pinMode(TEST_LED_PIN, OUTPUT);
  digitalWrite(TEST_LED_PIN, LOW);

  // UART0 is the link: Debug-header TXD (IO1) and RXD (IO3), 115200 8N1.
  // Serial.begin() above already owns it; Serial1 is deliberately not opened.
  linkRxLen = 0;
  linkRxOverflow = false;
  linkLastByteMs = millis();
  lastBeatMs = millis();
  link_emit_hello();
  logf("[LINK] rusim-line v1 on UART0 IO1/IO3 115200 8N1");

  WiFi.onEvent(onEthEvent);

  // Explicit WT32-ETH01 RMII pins (same as variant defaults in pins_arduino.h):
  // PHY addr 1, power pin 16, MDC 23, MDIO 18, LAN8720, 50 MHz clock in on GPIO0.
  if (!ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO,
                 ETH_PHY_TYPE, ETH_CLK_MODE)) {
    logf("[ETH] ERROR: ETH.begin() failed");
  } else {
    logf("[ETH] Waiting for link + DHCP...");
  }
}

void loop() {
  // Consume the link before anything that can block: a heartbeat that waits
  // in the UART FIFO would otherwise age /info and delay LOG draining.
  link_line_pull();
  if (netDirty) {
    netDirty = false;
    link_emit_net();
  }
  if (ethEventCode != 0) log_eth_event();
  link_heartbeat_tick();

  if (ledOffMs != 0 && (int32_t)(millis() - ledOffMs) >= 0) {
    digitalWrite(TEST_LED_PIN, LOW);
    ledOffMs = 0;
  }

  switch (stage) {
    case STAGE_WAIT_IP:
      if (eth_connected) {
        logf("[LINK] link up, waiting for credentials from the display");
        stage = STAGE_NEED_CREDS;
        stageAtMs = 0;  // run the first cred tick immediately
      } else if (millis() - stageAtMs >= 2000) {
        stageAtMs = millis();
        logf("[ETH] Waiting for IP address (DHCP)...");
      }
      break;

    case STAGE_NEED_CREDS: {
      // Nothing but link work and the auth probe runs until credentials
      // arrive. The display sends LOGINREQ back on every heartbeat.
      if (!eth_connected) {
        logf("[ETH] link lost while waiting for creds");
        link_emit_net();
        stage = STAGE_WAIT_IP;
        stageAtMs = millis();
        break;
      }
      if (millis() - stageAtMs >= kCredPollMs) {
        stageAtMs = millis();
        link_emit_net();
        link_emit_db();
        link_emit_loginreq();
        syncDbLatest(false);
        if (millis() - lastAuthProbeMs >= kAuthProbeMs) {
          // No staged creds: check whether the port is already through
          // before asking the user for a login they don't need.
          lastAuthProbeMs = millis();
          logf("[LOGIN] no creds staged; probing for existing session...");
          String portalUrl = fetchPortalUrl();
          if (portalUrl.length() == 0 && fetchData()) {
            logf("[LOGIN] session already valid; releasing display gate.");
            haveVerdict = true;
            lastVerdict = true;
            link_emit_result(true);
            stage = STAGE_RUN;
            stageAtMs = 0;
          }
        }
      }
      break;
    }

    case STAGE_LOGIN: {
      bool ok = doLogin();
      haveVerdict = true;
      lastVerdict = ok;
      link_emit_result(ok);
      clearCreds();
      if (ok) {
        syncDbLatest(true);
        stage = STAGE_RUN;
        stageAtMs = 0;
      } else {
        logf("[LOGIN] will wait for a corrected pair from the display");
        stage = STAGE_NEED_CREDS;
        stageAtMs = millis();
      }
      break;
    }

    case STAGE_RUN: {
      if (!eth_connected) {
        logf("[ETH] link lost in RUN, back to WAIT_IP");
        link_emit_net();
        stage = STAGE_WAIT_IP;
        stageAtMs = millis();
        break;
      }
      if (millis() - stageAtMs < kIntervalMs) break;
      stageAtMs = millis();

      logf("[ETH] Assigned IP: %s", ETH.localIP().toString().c_str());

      bool ok = Ping.ping(kPingTarget, kPingCount);
      if (ok) {
        lastPingMs = (int)Ping.averageTime();
        logf("[PING] 1.1.1.1 OK, avg time = %d ms", lastPingMs);
      } else {
        lastPingMs = -1;
        logf("[PING] 1.1.1.1 FAIL (no reply)");
      }
      // Declare the fresh verdict every cycle so /info stays live.
      link_emit_net();
      syncDbLatest(false);

      // The data fetch is the portal verdict: WALLED means the session
      // expired, so pause the loop and re-auth via the display.
      if (!fetchData()) {
        logf("[LOGIN] session walled mid-run, re-polling display");
        stage = STAGE_NEED_CREDS;
        stageAtMs = 0;
        break;
      }
      logf("[RUN] exp=%d duration=%dh running=%d dbLatest=%d bottles=%d ntp=%d "
           "queued=%u drops=%u",
           expNum, durationH, expRunning ? 1 : 0, dbLatest, dbBottleCount,
           ntpSynced ? 1 : 0, (unsigned)s_pushCount, (unsigned)s_pushDrops);
      break;
    }
  }
}
