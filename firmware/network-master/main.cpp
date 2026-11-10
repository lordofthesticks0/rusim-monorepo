#include <Arduino.h>
#include <ETH.h>
#include <ESPping.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include <cstdarg>
#include <cstring>

#include "db_contract.h"
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
// - Polls the legacy Mega slave on Serial1 (IO5 RX / IO17 TX, 9600 8N1) once
//   per 30 s with "#1:PING*", logs the four-bottle JSON reply as one
//   "LOG [SENS]" line, and enqueues the present bottles while an experiment
//   run is active. See docs/llm-generated/migration-plan.md.
// - Pulls devices/experiments/device_config from Supabase every 30 s and
//   freezes a run configuration snapshot at each run start. Uploads go to the
//   push_readings RPC only while a run is active. See
//   docs/llm-generated/network-master-database-plan.md.
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
static int dbLatest = -2;  // -2 unknown, -1 none yet, >=0 latest experiment_id
static int dbBottleCount = -1;  // -1 unknown, else bottle_count from devices
// Owner contact from the device/config pull, logs and /info only. Bounded
// char arrays, never heap Strings: truncated at 63 chars + NUL.
static char dbOwnerName[64] = {0};
static char dbOwnerEmail[64] = {0};
static uint32_t lastDbPollMs = 0;
static const uint32_t kDbPollMs = 30000;

// --- Database contract state (network-master-database-plan.md) -------------
// All DB responses are bounded; nothing here grows with server data. The
// session lives in RAM only and is compared against the queue's provisioning
// identity before every upload.
enum DbFault : uint8_t {
  DB_FAULT_NONE = 0,
  DB_FAULT_NET,       // timeout, TLS, 429 or 5xx; retry with backoff
  DB_FAULT_AUTH,      // rejected/missing session, bad token
  DB_FAULT_PAYLOAD,   // our body or a queued row was rejected (22023)
  DB_FAULT_SCHEMA,    // function/schema/permission problem; do not loosen TLS or grants
  DB_FAULT_CLOCK,     // NTP unsynced or an implausible timestamp
  DB_FAULT_IDENTITY,  // queue was collected under a different device identity
};

static DbFault dbFault = DB_FAULT_NONE;
static bool dbSessionValid = false;
static char dbSession[65] = {0};  // 64 hex + NUL, never logged
static int32_t dbSessionDevice = -1;
static uint32_t dbSessionExpiryUnix = 0;  // from authenticate()'s expires_at
// -1 = unbound; a successful authentication binds it to the returned
// device_id (or to RUSIM_DEVICE_ID when provisioning pins one).
static int32_t dbQueueDeviceId = RUSIM_DEVICE_ID > 0 ? RUSIM_DEVICE_ID : -1;
static bool dbConfigReady = false;      // >=1 successful full pull since boot
static uint32_t dbConfigReadOkMs = 0;   // millis of last successful pull, 0 = never
static uint32_t dbUploadAckMs = 0;      // millis of last acknowledged upload, 0 = never
static uint32_t dbBackoffMs = 0;        // upload backoff, capped
static uint32_t dbNextPushMs = 0;       // millis gate for the next upload attempt
static const size_t kDbBodyCap = 4096;  // one bounded DB response
static const uint32_t kDbRenewMarginS = 300;   // renew 5 min before expiry
static const uint32_t kDbRetryBaseMs = 2000;
static const uint32_t kDbRetryMaxMs = 60000;
static const uint32_t kDbMaxFutureSkewS = 600;  // mirrors the server backstop

// Cooperative DB pass state: at most one HTTPS operation per loop iteration.
enum DbPass : uint8_t {
  DB_PASS_IDLE = 0,
  DB_PASS_AUTH,
  DB_PASS_DEVICES,
  DB_PASS_CONFIG,
  DB_PASS_EXPERIMENTS,
  DB_PASS_LATEST,
  DB_PASS_PUSH,
  DB_PASS_REPORT,
};
static DbPass dbPass = DB_PASS_IDLE;
static uint8_t dbPassAuthRetried = 0;      // one reauth attempt per pass
static uint8_t dbPushRoundsThisPass = 0;   // kPushFlushRounds budget

// Run snapshot. Frozen when a run starts; DB updates during the run only
// change the pending configuration, never these values.
static bool runActive = false;
static int32_t runExperimentId = -1;
static int32_t runDurationH = -1;
static int32_t runSampleIntervalS = -1;
static bool runConfigFromDb = false;
static uint32_t runStartUnix = 0;
static bool runStartRefused = false;

// Display-selected (pending) configuration: the operator's experiment number
// and duration. Applied at the NEXT run start, never to a run in progress.
static int32_t dbPendingExp = -1;

// Bounded experiment history pulled from device_data.experiments, newest
// first. 16 rows keeps the response inside kDbBodyCap; the plan calls for a
// documented local history limit rather than an unlimited allocation.
static const size_t kDbExpHistory = 16;
struct DbExperiment {
  int32_t id;
  int32_t durationH;        // -1 when the column is null
  int32_t sampleIntervalS;  // -1 when the column is null
  char setpoints[96];       // raw JSON text, logged only (never actuated)
};
static DbExperiment dbExperiments[kDbExpHistory];
static size_t dbExperimentCount = 0;

static const size_t kMaxBody = 4096;
// Push body is sized separately from the portal's kMaxBody: one round of up
// to 24 bottle objects as JSON is ~3 KB, so 6 KB leaves headroom without
// inheriting the portal's 8 KB-page constant.
static const size_t kPushBodyCap = 6144;
// Ring capacity in bottle rows: 1200 rows = 50 full 24-bottle rounds at
// ~40 bytes/row (~48 KB). Well under the 100-150 KB ceiling; a dropped row
// is better than an overrun, and drops are counted, not silent.
static const size_t kPushQueueRows = 1200;
// Diagnostic counters. They are separate from queue overflow because a skipped
// idle round or a clock-rejected frame is not a memory loss.
static uint32_t s_pushIdleRounds = 0;    // rounds collected while not running
static uint32_t s_pushClockRejects = 0;  // rounds with an untrustworthy timestamp
static uint32_t s_pushDupRejects = 0;    // timestamp reuse after a clock change
static uint32_t s_pushBadValues = 0;     // non-finite fields converted to null
static uint32_t s_pushAckRounds = 0;     // rounds acknowledged by the server
static uint32_t s_lastQueuedUnix = 0;
// Upload rounds per DB pass: bounds how much one pass spends on POSTs (each
// has its own 10 s timeout) while still draining faster than collection.
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

// Sensor poll state (migration-plan.md). One fixed RX frame: no Arduino
// String and no JSON library, because the reply shape is fixed and flash is
// at ~75 %. IO5/IO17 are the freed display-link pair; the ESP32 UART1
// defaults (IO9/IO10) are flash pins on this module, so pins are explicit.
static const int kSensRxPin = 5;
static const int kSensTxPin = 17;
static const uint32_t kSensBaud = 9600;
static const uint32_t kSensPollMs = 30000;  // one round per 30 s
static const uint32_t kSensReplyMs = 1500;  // 50 ms capture + ~310 ms line time
static const size_t kSensFrameCap = 513;    // 512-byte frame + NUL
static const size_t kSensBottles = 4;

// nullMask bits, matching PushRow below.
static const uint8_t kSensNullAll = 0x1F;
static const uint8_t kSensNullPh = 0x01;
static const uint8_t kSensNullPressure = 0x02;
static const uint8_t kSensNullTemp = 0x04;
static const uint8_t kSensNullCo2 = 0x08;
static const uint8_t kSensNullCh4 = 0x10;

static char sensRx[kSensFrameCap];
static size_t sensRxLen = 0;
static bool sensRxOverflow = false;
static uint32_t lastSensPollMs = 0;
static uint32_t sensRoundsOk = 0;    // rounds with a parseable frame
static uint32_t sensRoundsFail = 0;  // timeouts, overlong or non-JSON frames
static uint32_t sensLastReplyMs = 0;
static bool sensReplySeen = false;

// CRC16-Modbus, unused on the line link and unused by the Mega TTL poll
// (that protocol has no checksum). Kept for a future Modbus RTU master; the
// freed IO5/IO17 pair now carries the Mega link, so it would need other pins.
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

static const char* dbFaultName(DbFault f) {
  switch (f) {
    case DB_FAULT_NONE: return "none";
    case DB_FAULT_NET: return "net";
    case DB_FAULT_AUTH: return "auth";
    case DB_FAULT_PAYLOAD: return "payload";
    case DB_FAULT_SCHEMA: return "schema";
    case DB_FAULT_CLOCK: return "clock";
    case DB_FAULT_IDENTITY: return "identity";
  }
  return "unknown";
}

static const DbExperiment* dbFindExperiment(int32_t id);
static void dbApplyRunState();

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
  // Age fields are stamp-based, never refreshed by re-sending cached values.
  char reads[16];
  if (dbConfigReadOkMs != 0) {
    snprintf(reads, sizeof(reads), "%u", (unsigned)(millis() - dbConfigReadOkMs));
  } else {
    snprintf(reads, sizeof(reads), "-");
  }
  char ack[16];
  if (dbUploadAckMs != 0) {
    snprintf(ack, sizeof(ack), "%u", (unsigned)(millis() - dbUploadAckMs));
  } else {
    snprintf(ack, sizeof(ack), "-");
  }
  char aexp[16];
  char adur[16];
  const char* asrc = "-";
  if (runActive) {
    snprintf(aexp, sizeof(aexp), "%ld", (long)runExperimentId);
    asrc = runConfigFromDb ? "db" : "local";
    if (runDurationH > 0) {
      snprintf(adur, sizeof(adur), "%ld", (long)runDurationH);
    } else {
      snprintf(adur, sizeof(adur), "-");
    }
  } else {
    snprintf(aexp, sizeof(aexp), "-");
    snprintf(adur, sizeof(adur), "-");
  }
  // Per-sensor limitation: the Mega frame carries no per-sensor timestamp, so
  // these rows are stamped with WT32 collection time, which is a proxy.
  char pdur[16];
  const DbExperiment* pending = dbFindExperiment(dbPendingExp);
  if (pending != nullptr && pending->durationH > 0) {
    snprintf(pdur, sizeof(pdur), "%ld", (long)pending->durationH);
  } else {
    snprintf(pdur, sizeof(pdur), "-");
  }
  // Highest CONFIGURED experiment, which may be higher than the highest one
  // with readings. The display uses it for its "next id" hint so a reserved
  // configuration is not silently reused.
  int32_t cfgMax = -1;
  for (size_t i = 0; i < dbExperimentCount; i++) {
    if (dbExperiments[i].id > cfgMax) cfgMax = dbExperiments[i].id;
  }
  Serial.printf(
      "DB latest=%s time=%d bottles=%s drops=%u ready=%d aexp=%s adur=%s "
      "asrc=%s pexp=%ld pdur=%s cfgmax=%ld queued=%u reads=%s ack=%s state=%s\n",
      latest, ntpSynced ? 1 : 0, bottles, (unsigned)s_pushDrops,
      dbConfigReady ? 1 : 0, aexp, adur, asrc, (long)dbPendingExp, pdur,
      (long)cfgMax, (unsigned)s_pushCount, reads, ack, dbFaultName(dbFault));
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
  dbPendingExp = expNum;  // selects the DB row shown as `pdur` on the DB line
  if (durationH != lastDurH || expNum != lastExpNum ||
      (int)expRunning != lastExpRunning) {
    logf("[LINK] exp state: run=%d exp=%d dur=%dh", expRunning ? 1 : 0, expNum,
         durationH);
    lastDurH = durationH;
    lastExpNum = expNum;
    lastExpRunning = expRunning ? 1 : 0;
  }
  // A false -> true edge freezes the run snapshot; stopping pauses uploads but
  // keeps queued active-run rows. Changes during a run only affect the
  // pending configuration for the next run.
  dbApplyRunState();
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
       "bottles=%d queued=%u drops=%u verdict=%d sensOk=%u sensFail=%u sensAge=%d "
       "ready=%d runActive=%d aexp=%ld adur=%ld asrc=%s ack=%ld reads=%ld "
       "idle=%u clock=%u dup=%u bad=%u acks=%u state=%s",
       (int)stage, eth_connected ? 1 : 0, ip, expNum, expRunning ? 1 : 0, durationH,
       dbLatest, ntpSynced ? 1 : 0, dbBottleCount, (unsigned)s_pushCount,
       (unsigned)s_pushDrops, haveVerdict ? (lastVerdict ? 1 : 2) : 0,
       (unsigned)sensRoundsOk, (unsigned)sensRoundsFail,
       sensReplySeen ? (int)(millis() - sensLastReplyMs) : -1,
       dbConfigReady ? 1 : 0, runActive ? 1 : 0, (long)runExperimentId,
       (long)runDurationH, runActive ? (runConfigFromDb ? "db" : "local") : "-",
       dbUploadAckMs != 0 ? (long)(millis() - dbUploadAckMs) : -1L,
       dbConfigReadOkMs != 0 ? (long)(millis() - dbConfigReadOkMs) : -1L,
       (unsigned)s_pushIdleRounds, (unsigned)s_pushClockRejects,
       (unsigned)s_pushDupRejects, (unsigned)s_pushBadValues,
       (unsigned)s_pushAckRounds, dbFaultName(dbFault));
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

// --- Ethernet connectivity and portal login ---

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

// --- Sample push queue: fixed ring, oldest-drop (memory-safety skill:
// bounded buffer, no heap in the hot path) ---
//
// One row per bottle that reported. An absent bottle (disconnected) is
// simply not enqueued, which is distinct from a present-but-null sensor
// (nullMask bit set). Rows carry sample time, never flush time. The structs
// and the JSON envelope live in db_contract.h so the native contract test
// compiles exactly the code that ships.
using dbc::BottleSample;
using dbc::PushRow;
typedef BottleSample BottleReading;

static PushRow s_pushQueue[kPushQueueRows];
static size_t s_pushHead = 0;   // oldest row
static size_t s_pushTail = 0;   // next write slot
static char s_pushBody[kPushBodyCap];

// Queues one sample round (up to 24 bottles). `readings` holds only bottles
// that were present in the frame, with explicit 0-based DB ids, so a sparse
// round (bottles 1 and 3 only) yields ids 0 and 2 and never renumbers. Rejects
// while NTP is unsynced, while no run is active, and any timestamp that is not
// plausible UTC for this moment. Full queue drops the oldest row, never the
// newest, and counts it.
static bool pushEnqueueRound(time_t sampleUnix, int experimentId,
                             BottleSample* readings, size_t n) {
  if (!ntpSynced) {
    s_pushClockRejects++;
    logf("[PUSH] refused: NTP unsynced (time=0)");
    return false;
  }
  if (!runActive) {
    s_pushIdleRounds++;
    return false;
  }
  if (readings == nullptr || n == 0 || n > 24) return false;
  time_t now = time(nullptr);
  if (sampleUnix < (time_t)kMinSampleUnix ||
      sampleUnix > now + (time_t)kDbMaxFutureSkewS) {
    s_pushClockRejects++;
    logf("[PUSH] refused: sample time %ld outside 2025-01-01..now+10m",
         (long)sampleUnix);
    return false;
  }
  // Second-resolution primary keys mean two rounds stamped the same second
  // would collide server-side, and ON CONFLICT DO NOTHING would hide it. The
  // clock can step backwards after an NTP correction, so detect the reuse
  // locally and keep the queue immutable instead of guessing.
  if (s_lastQueuedUnix != 0 && (uint32_t)sampleUnix <= s_lastQueuedUnix) {
    s_pushDupRejects++;
    logf("[PUSH] refused: timestamp reuse (%ld <= %lu)", (long)sampleUnix,
         (unsigned long)s_lastQueuedUnix);
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    s_pushBadValues += dbc::sanitizeNonFinite(readings[i]);
    if (s_pushCount == kPushQueueRows) {
      s_pushHead = (s_pushHead + 1) % kPushQueueRows;
      s_pushCount--;
      if (s_pushDrops < 0xFFFFFFFFu) s_pushDrops++;
    }
    PushRow& r = s_pushQueue[s_pushTail];
    r.bottleId = readings[i].bottleId;
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
  s_lastQueuedUnix = (uint32_t)sampleUnix;
  return true;
}

// --- Legacy Mega slave poll (migration-plan.md) ----------------------------
//
// Direct TTL on Serial1: Mega D18 TX -> IO5 RX, Mega D19 RX <- IO17 TX,
// 9600 8N1, common GND. Request "#1:PING*" (8 bytes, no checksum); the slave
// answers on the port the request arrived on with one JSON line of four
// bottle objects. Each accepted frame is logged and, while a run is active,
// copied into the push queue with explicit bottle ids. The slave frame has no
// measurement timestamp, so the WT32 collection time is a proxy.

static const uint8_t kSensRequest[] = {'#', '1', ':', 'P', 'I', 'N', 'G', '*'};

// Formats one field for the log line: "null" when the nullMask bit is set.
static void sens_format_value(char* out, size_t cap, float v, bool isNull) {
  if (isNull) {
    snprintf(out, cap, "null");
  } else {
    snprintf(out, cap, "%.4g", (double)v);
  }
}

// Parses "key":<number> inside [begin, end). The frame is NUL-terminated, so
// strtof can run off the end of the region but never the buffer; stop is
// checked against end so one bottle cannot borrow the next bottle's value.
static bool sens_float_field(const char* begin, const char* end, const char* key,
                             float& out) {
  if (begin == nullptr || end == nullptr || key == nullptr || begin >= end) return false;
  char needle[16];
  int n = snprintf(needle, sizeof(needle), "\"%s\":", key);
  if (n <= 0 || (size_t)n >= sizeof(needle)) return false;
  const char* at = strstr(begin, needle);
  if (at == nullptr || at >= end) return false;
  at += (size_t)n;
  while (at < end && (*at == ' ' || *at == '\t')) at++;
  if (at >= end) return false;
  char* stop = nullptr;
  float v = strtof(at, &stop);
  if (stop == at || stop > end) return false;
  out = v;
  return true;
}

// Stores one field unless it is absent or carries the slave's invalid marker,
// in which case the nullMask bit stays set. `valid` counts accepted values.
static void sens_take(BottleReading& r, const char* begin, const char* end,
                      const char* key, uint8_t nullBit, float invalid, float& slot,
                      unsigned& valid) {
  float v = 0;
  if (!sens_float_field(begin, end, key, v)) return;
  if (v == invalid) return;
  slot = v;
  r.nullMask = (uint8_t)(r.nullMask & ~nullBit);
  valid++;
}

// Counts a failed round. Only the first and then every 20th are logged, so a
// disconnected Mega cannot flood the display's log.
static void sens_log_failure(const char* why) {
  sensRoundsFail++;
  if (sensRoundsFail == 1 || (sensRoundsFail % 20) == 0) {
    logf("[SENS] poll failed: %s (fail=%u)", why, (unsigned)sensRoundsFail);
  }
}

// Turns one received frame into the single [SENS] log line. Ids outside 1-4
// are ignored. A frame that holds no bottle object is a failed round.
static void sens_report_round() {
  BottleReading readings[kSensBottles];
  bool seen[kSensBottles] = {false, false, false, false};
  for (size_t i = 0; i < kSensBottles; i++) {
    readings[i] = BottleReading();
    // Mega bottle 1..4 -> database bottle_id 0..3. The id travels with the
    // sample, so a missing bottle leaves a hole instead of renumbering.
    readings[i].bottleId = (uint8_t)i;
    readings[i].nullMask = kSensNullAll;
  }

  unsigned valid = 0;
  const char* p = sensRx;
  while (true) {
    const char* idKey = strstr(p, "\"id\":");
    if (idKey == nullptr) break;
    const char* value = idKey + 5;  // strlen("\"id\":")
    char* stop = nullptr;
    long id = strtol(value, &stop, 10);
    if (stop == value) {
      p = value;  // not a number; keep scanning past this key
      continue;
    }
    const char* next = strstr(stop, "\"id\":");
    const char* end = (next != nullptr) ? next : (sensRx + sensRxLen);
    if (id >= 1 && id <= (long)kSensBottles) {
      size_t idx = (size_t)(id - 1);
      BottleReading& r = readings[idx];
      seen[idx] = true;
      sens_take(r, stop, end, "ph", kSensNullPh, -1.0f, r.ph, valid);
      sens_take(r, stop, end, "methane", kSensNullCh4, -1.0f, r.ch4, valid);
      sens_take(r, stop, end, "temp", kSensNullTemp, -999.0f, r.tempC, valid);
      sens_take(r, stop, end, "co2", kSensNullCo2, -1.0f, r.co2, valid);
      sens_take(r, stop, end, "press", kSensNullPressure, -1.0f, r.pressure, valid);
    }
    p = end;
  }

  unsigned present = 0;
  for (size_t i = 0; i < kSensBottles; i++) {
    if (seen[i]) present++;
  }
  if (present == 0) {
    sens_log_failure("reply has no bottle objects");
    return;
  }

  // Queue the round only for an accepted frame collected during an active run.
  // Idle rounds stay log-only: no experiment-0 samples and no uploads. A
  // requested run that could not start (DB configuration never pulled) is
  // counted but not queued, so nothing is mislabelled later.
  if (runActive) {
    BottleSample batch[kSensBottles];
    size_t nb = 0;
    for (size_t i = 0; i < kSensBottles; i++) {
      if (seen[i]) batch[nb++] = readings[i];
    }
    (void)pushEnqueueRound(time(nullptr), runExperimentId, batch, nb);
  } else if (expRunning) {
    runStartRefused = true;
    s_pushIdleRounds++;
  } else {
    s_pushIdleRounds++;
  }

  // Worst case is ~73 characters per bottle at %.4g, so 384 holds four
  // bottles plus the prefix. snprintf truncates rather than overrunning.
  char line[384];
  int used = snprintf(line, sizeof(line), "%u bottles ok=%u fail=%u", present, valid,
                      (unsigned)(kSensBottles - present));
  if (used < 0) return;
  size_t pos = (size_t)used < sizeof(line) ? (size_t)used : sizeof(line) - 1;
  for (size_t i = 0; i < kSensBottles; i++) {
    char phB[16], chB[16], teB[16], coB[16], prB[16];
    const BottleReading& r = readings[i];
    sens_format_value(phB, sizeof(phB), r.ph, (r.nullMask & kSensNullPh) != 0);
    sens_format_value(chB, sizeof(chB), r.ch4, (r.nullMask & kSensNullCh4) != 0);
    sens_format_value(teB, sizeof(teB), r.tempC, (r.nullMask & kSensNullTemp) != 0);
    sens_format_value(coB, sizeof(coB), r.co2, (r.nullMask & kSensNullCo2) != 0);
    sens_format_value(prB, sizeof(prB), r.pressure, (r.nullMask & kSensNullPressure) != 0);
    int w = snprintf(line + pos, sizeof(line) - pos, " b%u ph=%s ch4=%s t=%s co2=%s p=%s",
                     (unsigned)(i + 1), phB, chB, teB, coB, prB);
    if (w <= 0) break;
    if ((size_t)w >= sizeof(line) - pos) {
      pos = sizeof(line) - 1;
      break;
    }
    pos += (size_t)w;
  }
  sensRoundsOk++;
  logf("[SENS] %s", line);
}

// One request/reply round, gated to one every 30 s. The wait is blocking for
// at most 1.5 s, so the display link is pulled from inside it: a CREDS line
// arriving during the reply would otherwise sit in the FIFO.
static void sens_poll() {
  if (millis() - lastSensPollMs < kSensPollMs) return;
  lastSensPollMs = millis();

  // Anything already in the FIFO is stale (a partial reply or Mega boot
  // noise), so drop it before asking.
  while (Serial1.available() > 0) (void)Serial1.read();
  sensRxLen = 0;
  sensRxOverflow = false;

  Serial1.write(kSensRequest, sizeof(kSensRequest));
  Serial1.flush();

  bool complete = false;
  uint32_t start = millis();
  while (millis() - start < kSensReplyMs) {
    int c = Serial1.read();
    if (c < 0) {
      link_line_pull();  // keep the display link alive during the wait
      delay(1);
      continue;
    }
    if (c == '\n') {
      complete = true;
      break;
    }
    if (c == '\r') continue;
    if (sensRxLen + 1 >= sizeof(sensRx)) {
      sensRxOverflow = true;
      break;
    }
    sensRx[sensRxLen++] = (char)c;
  }
  sensRx[sensRxLen] = '\0';

  if (!complete) {
    sens_log_failure(sensRxOverflow ? "reply over 512 bytes" : "reply timeout");
    return;
  }
  if (sensRxLen == 0 || sensRx[0] != '{') {
    sens_log_failure("reply is not a JSON object");
    return;
  }
  sensLastReplyMs = millis();
  sensReplySeen = true;
  sens_report_round();
}

// --- Supabase device session, configuration pull and upload ----------------
//
// HTTP contract (network-master-database-plan.md §3):
//   POST /rest/v1/rpc/authenticate  {"p_token":"..."}
//   GET  /rest/v1/devices?select=device_id,bottle_count,owner_name,owner_email
//   GET  /rest/v1/device_config?select=bottle_count,owner_name,owner_email,updated_at
//   GET  /rest/v1/experiments?select=experiment_id,duration_h,sample_interval_s,
//        setpoints&order=experiment_id.desc&limit=16
//   GET  /rest/v1/readings?select=experiment_id&order=experiment_id.desc&limit=1
//   POST /rest/v1/rpc/push_readings {"p_readings":[...]}
//
// Every request names the device_data schema explicitly, because a hosted
// project whose default profiles include public would resolve the wrong
// namespace. `x-device-session` is a custom device token, not a Supabase Auth
// JWT, so it never goes in Authorization. No service-role key or database
// password exists on this board.

static const char* kDbSchema = "device_data";
static char s_dbUrl[320];
static char s_dbBody[kDbBodyCap];

static bool sbConfigured() {
  return String(SUPABASE_URL).length() > 0 && String(SUPABASE_ANON_KEY).length() > 0 &&
         String(DEVICE_TOKEN).length() > 0;
}

static bool dbSessionExpiring() {
  if (!dbSessionValid) return true;
  if (!ntpSynced) return true;  // cannot reason about expiry without UTC
  uint32_t now = (uint32_t)time(nullptr);
  return now + kDbRenewMarginS >= dbSessionExpiryUnix;
}

static void dbSessionInvalidate(const char* why) {
  if (dbSessionValid) logf("[DB] session invalidated (%s)", why);
  memset(dbSession, 0, sizeof(dbSession));
  dbSessionValid = false;
  dbSessionDevice = -1;
  dbSessionExpiryUnix = 0;
}

// Bounded response reader. `truncated` means the server body was larger than
// the buffer: callers must treat that as a payload fault, not parse a prefix.
static size_t readBodyCapped(HTTPClient& http, char* out, size_t cap, bool& truncated) {
  truncated = false;
  if (out == nullptr || cap < 2) {
    truncated = true;
    return 0;
  }
  WiFiClient* stream = http.getStreamPtr();
  if (stream == nullptr) {
    out[0] = '\0';
    return 0;
  }
  // Use Content-Length when the server sends one, so a short body never waits
  // out the socket timeout. Chunked replies fall back to connection state.
  const int contentLen = http.getSize();
  const bool haveLen = contentLen >= 0;
  size_t want = haveLen ? (size_t)contentLen : (cap - 1);
  if (want > cap - 1) {
    truncated = true;
    want = cap - 1;
  }
  size_t total = 0;
  uint8_t buf[128];
  while (total < want) {
    if (!haveLen && !http.connected()) break;
    size_t room = want - total;
    if (room > sizeof(buf)) room = sizeof(buf);
    int n = stream->readBytes(reinterpret_cast<char*>(buf), room);
    if (n <= 0) break;
    memcpy(out + total, buf, (size_t)n);
    total += (size_t)n;
  }
  out[total] = '\0';
  if (haveLen && (size_t)contentLen > total) truncated = true;
  return total;
}

enum DbMethod : uint8_t { DB_GET, DB_POST };
struct DbReply {
  int code;
  size_t len;
  bool truncated;
};

// Performs one request and keeps the bounded body in s_dbBody. No tokens or
// headers are logged. Retry-After is returned in seconds (0 when absent).
static DbReply dbRequest(DbMethod method, const char* pathAndQuery, const char* jsonBody,
                         const char* sessionToken, uint16_t& retryAfterS) {
  DbReply rep = {0, 0, false};
  retryAfterS = 0;
  int w = snprintf(s_dbUrl, sizeof(s_dbUrl), "%s%s", SUPABASE_URL, pathAndQuery);
  if (w <= 0 || (size_t)w >= sizeof(s_dbUrl)) {
    rep.code = -100;  // URL will not fit: configuration fault, not a network one
    return rep;
  }
  WiFiClientSecure client;
  sbApplyTls(client);
  HTTPClient http;
  http.begin(client, s_dbUrl);
  http.setTimeout(10000);
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Accept", "application/json");
  if (method == DB_GET) {
    http.addHeader("Accept-Profile", kDbSchema);
  } else {
    http.addHeader("Content-Profile", kDbSchema);
    http.addHeader("Content-Type", "application/json");
  }
  if (sessionToken != nullptr && sessionToken[0] != '\0') {
    http.addHeader("x-device-session", sessionToken);  // never logged
  }
  const char* headerKeys[] = {"Retry-After"};
  http.collectHeaders(headerKeys, 1);
  rep.code = (method == DB_GET) ? http.GET()
                                : http.POST((uint8_t*)jsonBody, strlen(jsonBody));
  if (rep.code > 0) {
    rep.len = readBodyCapped(http, s_dbBody, sizeof(s_dbBody), rep.truncated);
    if (http.hasHeader("Retry-After")) {
      long v = strtol(http.header("Retry-After").c_str(), nullptr, 10);
      if (v > 0 && v <= 3600) retryAfterS = (uint16_t)v;
    }
  }
  http.end();
  return rep;
}

// Maps an HTTP result and PostgREST error body to one fault category. The
// classification decides whether to retry, reauthenticate or stop hot-looping.
static DbFault dbClassify(const DbReply& rep, char* sqlstate, size_t sqlstateCap) {
  sqlstate[0] = '\0';
  if (rep.code < 0) return DB_FAULT_NET;
  if (rep.len > 0) dbc::parseErrorCode(s_dbBody, rep.len, sqlstate, sqlstateCap);
  if (rep.code == 401 || rep.code == 403) return DB_FAULT_AUTH;
  if (rep.code == 429 || rep.code >= 500) return DB_FAULT_NET;
  if (strcmp(sqlstate, "28000") == 0) return DB_FAULT_AUTH;
  if (strcmp(sqlstate, "42501") == 0 || strncmp(sqlstate, "PGRST", 5) == 0) {
    return DB_FAULT_SCHEMA;
  }
  if (rep.code >= 400) return DB_FAULT_PAYLOAD;
  return DB_FAULT_NONE;
}

static bool dbArrayIsEmpty(const char* body, size_t len) {
  const char* end = body + len;
  const char* p = dbc::skipWs(body, end);
  if (p >= end || *p != '[') return false;
  ++p;
  p = dbc::skipWs(p, end);
  return p < end && *p == ']';
}

// Copies a JSON string field into a fixed buffer. Absent key keeps the
// caller's default; an explicit null yields "".
static bool dbStringField(const char* ob, const char* oe, const char* key, char* out,
                          size_t cap) {
  const char* vb = nullptr;
  const char* ve = nullptr;
  if (!dbc::findField(ob, oe, key, vb, ve)) return false;
  const char* p = dbc::skipWs(vb, ve);
  if ((size_t)(ve - p) >= 4 && strncmp(p, "null", 4) == 0) {
    if (cap > 0) out[0] = '\0';
    return true;
  }
  return dbc::parseString(vb, ve, out, cap);
}

static bool dbIntField(const char* ob, const char* oe, const char* key, int32_t& out) {
  const char* vb = nullptr;
  const char* ve = nullptr;
  if (!dbc::findField(ob, oe, key, vb, ve)) return false;
  const char* p = dbc::skipWs(vb, ve);
  if ((size_t)(ve - p) >= 4 && strncmp(p, "null", 4) == 0) return false;
  return dbc::parseInt(vb, ve, out);
}

static bool sbAuthenticate() {
  if (!sbConfigured()) {
    logf("[DB] Supabase not configured (SUPABASE_URL/ANON_KEY/DEVICE_TOKEN empty)");
    dbFault = DB_FAULT_AUTH;
    return false;
  }
  if (!ntpSynced) {
    dbFault = DB_FAULT_CLOCK;
    logf("[DB] authenticate deferred: clock unsynced");
    return false;
  }

  // {"p_token":"<escaped>"}: the token is provisioning data, never logged.
  static char body[300];
  size_t o = (size_t)snprintf(body, sizeof(body), "{\"p_token\":\"");
  for (const char* p = DEVICE_TOKEN; *p != '\0'; ++p) {
    if (o + 8 >= sizeof(body)) {
      logf("[DB] device token too long for the request buffer");
      dbFault = DB_FAULT_AUTH;
      return false;
    }
    if (*p == '"' || *p == '\\') body[o++] = '\\';
    body[o++] = *p;
  }
  if (o + 4 >= sizeof(body)) {
    dbFault = DB_FAULT_AUTH;
    return false;
  }
  body[o++] = '"';
  body[o++] = '}';
  body[o] = '\0';

  uint16_t retryAfter = 0;
  DbReply rep = dbRequest(DB_POST, "/rest/v1/rpc/authenticate", body, nullptr, retryAfter);
  char sqlstate[8];
  DbFault fault = dbClassify(rep, sqlstate, sizeof(sqlstate));
  if (rep.code != 200) {
    dbFault = fault;
    logf("[DB] authenticate HTTP %d%s%s (retry in %us)", rep.code,
         sqlstate[0] != '\0' ? " " : "", sqlstate, (unsigned)retryAfter);
    return false;
  }
  if (rep.truncated) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] authenticate reply exceeds %u bytes", (unsigned)sizeof(s_dbBody));
    return false;
  }

  // The response is a one-row array: {"session_token":...,"device_id":...,
  // "bottle_count":...,"owner_name":...,"owner_email":...,"expires_at":...}
  const char* ob = nullptr;
  const char* oe = nullptr;
  size_t cursor = 0;
  if (!dbc::nextObject(s_dbBody, rep.len, cursor, ob, oe)) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] authenticate: reply is not a one-row array (wrong token?)");
    return false;
  }
  char token[80];
  if (!dbStringField(ob, oe, "session_token", token, sizeof(token))) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] authenticate: no session_token in reply");
    return false;
  }
  size_t tokenLen = strlen(token);
  bool tokenHex = tokenLen >= 32 && tokenLen <= 64;
  for (size_t i = 0; tokenHex && i < tokenLen; ++i) {
    char c = token[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      tokenHex = false;
    }
  }
  if (!tokenHex) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] authenticate: session_token is not a hex token");
    return false;
  }

  int32_t dev = -1;
  int32_t bottles = -1;
  if (!dbIntField(ob, oe, "device_id", dev) || dev < 0) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] authenticate: missing or invalid device_id");
    return false;
  }
  if (!dbIntField(ob, oe, "bottle_count", bottles) || bottles < 1 || bottles > 99) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] authenticate: missing or invalid bottle_count");
    return false;
  }
  char expiresRaw[48];
  uint32_t expiresUnix = 0;
  if (!dbStringField(ob, oe, "expires_at", expiresRaw, sizeof(expiresRaw)) ||
      !dbc::parseIsoUtc(expiresRaw, expiresUnix)) {
    dbFault = DB_FAULT_CLOCK;
    logf("[DB] authenticate: unparseable expires_at; refusing the session");
    return false;
  }
  uint32_t now = (uint32_t)time(nullptr);
  if (expiresUnix <= now + 60) {
    dbFault = DB_FAULT_CLOCK;
    logf("[DB] authenticate: session expiry is not in the future (clock?)");
    return false;
  }

  strncpy(dbSession, token, sizeof(dbSession) - 1);
  dbSession[sizeof(dbSession) - 1] = '\0';
  dbSessionValid = true;
  dbSessionDevice = dev;
  dbSessionExpiryUnix = expiresUnix;
  dbBottleCount = (int)bottles;
  (void)dbStringField(ob, oe, "owner_name", dbOwnerName, sizeof(dbOwnerName));
  (void)dbStringField(ob, oe, "owner_email", dbOwnerEmail, sizeof(dbOwnerEmail));

  // The queue belongs to the provisioning identity. A different device must
  // never receive rows collected before this authentication.
  if (dbQueueDeviceId < 0) {
    dbQueueDeviceId = dev;
  } else if (dbQueueDeviceId != dev) {
    dbFault = DB_FAULT_IDENTITY;
    logf("[DB] identity mismatch: queue=%ld authenticated=%ld, uploads blocked",
         (long)dbQueueDeviceId, (long)dev);
    return true;  // reads stay available; uploads are refused below
  }
  if (dbFault == DB_FAULT_IDENTITY) dbFault = DB_FAULT_NONE;
  logf("[DB] authenticated device=%ld bottles=%d expires_in=%us owner='%s'",
       (long)dev, (int)bottles, (unsigned)(expiresUnix - now), dbOwnerName);
  return true;
}

enum DbFetch : uint8_t { DB_FETCH_OK, DB_FETCH_AUTH, DB_FETCH_FAIL, DB_FETCH_PAYLOAD };

// GET helper shared by the configuration reads. Returns the HTTP status in
// `rep` and the classified fault, mapping an empty or truncated body to a
// payload fault so no caller parses a prefix.
static DbFetch dbGetJson(const char* pathAndQuery, DbReply& rep) {
  uint16_t retryAfter = 0;
  rep = dbRequest(DB_GET, pathAndQuery, nullptr, dbSession, retryAfter);
  char sqlstate[8];
  DbFault fault = dbClassify(rep, sqlstate, sizeof(sqlstate));
  if (fault == DB_FAULT_AUTH) {
    logf("[DB] read rejected (HTTP %d%s%s)", rep.code,
         sqlstate[0] != '\0' ? " " : "", sqlstate);
    return DB_FETCH_AUTH;
  }
  if (fault != DB_FAULT_NONE) {
    logf("[DB] read failed (HTTP %d, %s)", rep.code, dbFaultName(fault));
    dbFault = fault;
    return DB_FETCH_FAIL;
  }
  if (rep.truncated) {
    logf("[DB] read exceeds %u bytes", (unsigned)sizeof(s_dbBody));
    dbFault = DB_FAULT_PAYLOAD;
    return DB_FETCH_PAYLOAD;
  }
  return DB_FETCH_OK;
}

// devices is authoritative for bottle_count; a missing row means the session
// is no longer valid (RLS returns 200 + [] for a dead session).
static DbFetch sbFetchDevices() {
  DbReply rep;
  DbFetch r = dbGetJson("/rest/v1/devices?select=device_id,bottle_count,owner_name,owner_email",
                        rep);
  if (r != DB_FETCH_OK) return r;
  if (dbArrayIsEmpty(s_dbBody, rep.len)) {
    dbFault = DB_FAULT_AUTH;
    logf("[DB] devices read returned no row: session not live");
    return DB_FETCH_AUTH;
  }
  const char* ob = nullptr;
  const char* oe = nullptr;
  size_t cursor = 0;
  if (!dbc::nextObject(s_dbBody, rep.len, cursor, ob, oe)) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] devices: reply is not a JSON object array");
    return DB_FETCH_PAYLOAD;
  }
  int32_t dev = -1;
  int32_t bottles = -1;
  if (!dbIntField(ob, oe, "device_id", dev) || dev != dbSessionDevice) {
    dbFault = DB_FAULT_IDENTITY;
    logf("[DB] devices: row does not match session device");
    return DB_FETCH_PAYLOAD;
  }
  if (!dbIntField(ob, oe, "bottle_count", bottles) || bottles < 1 || bottles > 99) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] devices: missing or invalid bottle_count");
    return DB_FETCH_PAYLOAD;
  }
  dbBottleCount = (int)bottles;
  (void)dbStringField(ob, oe, "owner_name", dbOwnerName, sizeof(dbOwnerName));
  (void)dbStringField(ob, oe, "owner_email", dbOwnerEmail, sizeof(dbOwnerEmail));
  return DB_FETCH_OK;
}

// device_config mirrors devices without a synchronization trigger. It is
// pulled for display/log only; devices stays authoritative for bottle_count.
static DbFetch sbFetchDeviceConfig() {
  DbReply rep;
  DbFetch r = dbGetJson(
      "/rest/v1/device_config?select=bottle_count,owner_name,owner_email,updated_at", rep);
  if (r != DB_FETCH_OK) return r;
  if (dbArrayIsEmpty(s_dbBody, rep.len)) {
    return DB_FETCH_OK;  // no copy row: not an error, devices is authoritative
  }
  const char* ob = nullptr;
  const char* oe = nullptr;
  size_t cursor = 0;
  if (!dbc::nextObject(s_dbBody, rep.len, cursor, ob, oe)) {
    dbFault = DB_FAULT_PAYLOAD;
    logf("[DB] device_config: reply is not a JSON object array");
    return DB_FETCH_PAYLOAD;
  }
  int32_t cfgBottles = -1;
  if (dbIntField(ob, oe, "bottle_count", cfgBottles) && dbBottleCount > 0 &&
      cfgBottles != dbBottleCount) {
    logf("[DB] device_config bottle_count=%ld differs from devices=%d; using devices",
         (long)cfgBottles, dbBottleCount);
  }
  return DB_FETCH_OK;
}

static DbFetch sbFetchExperiments() {
  DbReply rep;
  DbFetch r = dbGetJson(
      "/rest/v1/experiments?select=experiment_id,duration_h,sample_interval_s,setpoints"
      "&order=experiment_id.desc&limit=16",
      rep);
  if (r != DB_FETCH_OK) return r;

  dbExperimentCount = 0;
  const char* ob = nullptr;
  const char* oe = nullptr;
  size_t cursor = 0;
  while (dbExperimentCount < kDbExpHistory &&
         dbc::nextObject(s_dbBody, rep.len, cursor, ob, oe)) {
    int32_t id = -1;
    if (!dbIntField(ob, oe, "experiment_id", id) || id < 0) {
      dbFault = DB_FAULT_PAYLOAD;
      logf("[DB] experiments: object without a valid experiment_id");
      return DB_FETCH_PAYLOAD;
    }
    DbExperiment& e = dbExperiments[dbExperimentCount];
    e.id = id;
    e.durationH = -1;
    e.sampleIntervalS = -1;
    e.setpoints[0] = '\0';
    (void)dbIntField(ob, oe, "duration_h", e.durationH);
    (void)dbIntField(ob, oe, "sample_interval_s", e.sampleIntervalS);
    const char* vb = nullptr;
    const char* ve = nullptr;
    if (dbc::findField(ob, oe, "setpoints", vb, ve)) {
      (void)dbc::parseRaw(vb, ve, e.setpoints, sizeof(e.setpoints));
    }
    dbExperimentCount++;
  }
  return DB_FETCH_OK;
}

// Highest experiment with readings: the existing display hint. It is NOT the
// running experiment and nothing here starts a run from it.
static DbFetch sbFetchLatestReadingExp(int& latest) {
  DbReply rep;
  DbFetch r = dbGetJson(
      "/rest/v1/readings?select=experiment_id&order=experiment_id.desc&limit=1", rep);
  if (r != DB_FETCH_OK) return r;
  latest = -1;
  const char* ob = nullptr;
  const char* oe = nullptr;
  size_t cursor = 0;
  if (dbc::nextObject(s_dbBody, rep.len, cursor, ob, oe)) {
    int32_t id = -1;
    if (dbIntField(ob, oe, "experiment_id", id) && id >= 0) latest = (int)id;
  }
  return DB_FETCH_OK;
}

static const DbExperiment* dbFindExperiment(int32_t id) {
  if (id < 0) return nullptr;
  for (size_t i = 0; i < dbExperimentCount; i++) {
    if (dbExperiments[i].id == id) return &dbExperiments[i];
  }
  return nullptr;
}

// Freezes the run snapshot on a false -> true edge and pauses uploads on a
// stop. A stop never discards queued active-run rows; they resume with their
// original timestamps and experiment ids when a run is active again.
static void dbApplyRunState() {
  if (expRunning && !runActive) {
    if (!dbConfigReady) {
      // Decision 2026-11-10: no run may start until one successful pull has
      // happened since boot. Cached or local configuration is not substituted.
      if (!runStartRefused) {
        logf("[DB] run start refused: no successful DB configuration pull since boot");
      }
      runStartRefused = true;
      return;
    }
    const DbExperiment* cfg = dbFindExperiment(expNum);
    runExperimentId = expNum;
    runConfigFromDb = (cfg != nullptr && cfg->durationH > 0);
    runDurationH = runConfigFromDb ? cfg->durationH : durationH;
    runSampleIntervalS = (cfg != nullptr) ? cfg->sampleIntervalS : -1;
    runStartUnix = ntpSynced ? (uint32_t)time(nullptr) : 0;
    runActive = true;
    runStartRefused = false;
    logf("[DB] run exp=%ld started at %lu: src=%s dur=%ldh interval=%lds "
         "setpoints=%s",
         (long)runExperimentId, (unsigned long)runStartUnix,
         runConfigFromDb ? "db" : "local", (long)runDurationH,
         (long)runSampleIntervalS,
         (cfg != nullptr && cfg->setpoints[0] != '\0') ? cfg->setpoints : "{}");
    // Setpoints are recorded, not actuated: this build has no actuator path
    // for them, and applying them is out of scope for telemetry.
  } else if (!expRunning && runActive) {
    runActive = false;
    logf("[PUSH] run stopped: %u rows retained, uploads paused",
         (unsigned)s_pushCount);
  }
}

// Formats the oldest queued round (all rows sharing the head's sampleUnix,
// at most 24) as {"p_readings":[...]} in s_pushBody. Returns the row count,
// or 0 when the queue is empty or the envelope would not fit.
static size_t pushFormatHeadRound() {
  if (s_pushCount == 0) return 0;
  int64_t headTs = s_pushQueue[s_pushHead].sampleUnix;
  size_t rows = 0;
  while (rows < s_pushCount && rows < 24) {
    if (s_pushQueue[(s_pushHead + rows) % kPushQueueRows].sampleUnix != headTs) break;
    rows++;
  }
  // The round may wrap the ring, so copy it out before formatting.
  PushRow batch[24];
  for (size_t k = 0; k < rows; k++) {
    batch[k] = s_pushQueue[(s_pushHead + k) % kPushQueueRows];
  }
  if (dbc::formatPushEnvelope(batch, rows, s_pushBody, sizeof(s_pushBody)) == 0) {
    return 0;
  }
  return rows;
}

// Capped exponential backoff with jitter. A Retry-After wins when it is
// longer. Payload and schema faults back off hard instead of hot-looping a
// batch the server will keep rejecting; the batch itself is kept.
static void dbPushBackoff(DbFault fault, uint16_t retryAfterS) {
  if (dbBackoffMs == 0) {
    dbBackoffMs = kDbRetryBaseMs;
  } else if (dbBackoffMs < kDbRetryMaxMs) {
    dbBackoffMs *= 2;
    if (dbBackoffMs > kDbRetryMaxMs) dbBackoffMs = kDbRetryMaxMs;
  }
  if (fault == DB_FAULT_PAYLOAD || fault == DB_FAULT_SCHEMA) {
    dbBackoffMs = kDbRetryMaxMs;
  }
  uint32_t wait = dbBackoffMs + (uint32_t)(esp_random() % 500u);
  if ((uint32_t)retryAfterS * 1000u > wait) wait = (uint32_t)retryAfterS * 1000u;
  dbNextPushMs = millis() + wait;
  dbFault = fault;
}

// POSTs the oldest queued round to push_readings(). The RPC takes the device
// from the session and ignores unknown keys, so identity cannot be forged
// here. The batch is dequeued only after a readable acknowledgement whose
// count is within the submitted row count; a replay may legitimately return
// 0, and the server's first-write-wins rule means a changed value with the
// same key is silently kept (the scalar result cannot reveal that).
static bool sbPushOneRound() {
  if (s_pushCount == 0) return true;
  if (!runActive) return false;
  if (!sbConfigured()) return false;
  if (dbSessionExpiring() && !sbAuthenticate()) return false;
  if (dbQueueDeviceId >= 0 && dbSessionDevice >= 0 && dbQueueDeviceId != dbSessionDevice) {
    dbFault = DB_FAULT_IDENTITY;
    dbNextPushMs = millis() + kDbRetryMaxMs;
    return false;
  }
  size_t rows = pushFormatHeadRound();
  if (rows == 0) {
    logf("[PUSH] format failed: %u bytes for %u queued rows", (unsigned)sizeof(s_pushBody),
         (unsigned)s_pushCount);
    dbPushBackoff(DB_FAULT_PAYLOAD, 0);
    return false;
  }
  for (int attempt = 0; attempt < 2; attempt++) {
    uint16_t retryAfter = 0;
    DbReply rep =
        dbRequest(DB_POST, "/rest/v1/rpc/push_readings", s_pushBody, dbSession, retryAfter);
    char sqlstate[8];
    DbFault fault = dbClassify(rep, sqlstate, sizeof(sqlstate));
    if (rep.code >= 200 && rep.code < 300) {
      long acked = -1;
      if (!rep.truncated && dbc::parseScalarCount(s_dbBody, rep.len, acked) &&
          acked >= 0 && acked <= (long)rows) {
        s_pushHead = (s_pushHead + rows) % kPushQueueRows;
        s_pushCount -= rows;
        s_pushAckRounds++;
        dbUploadAckMs = millis();
        dbBackoffMs = 0;
        dbNextPushMs = millis();
        dbFault = DB_FAULT_NONE;
        logf("[PUSH] acked %u rows (server inserted %ld, %u queued)", (unsigned)rows,
             acked, (unsigned)s_pushCount);
        return true;
      }
      // Never discard queued data on a 2xx we cannot read.
      logf("[PUSH] unreadable acknowledgement (HTTP %d, %u bytes); batch kept", rep.code,
           (unsigned)rep.len);
      dbPushBackoff(DB_FAULT_PAYLOAD, 0);
      return false;
    }
    if (fault == DB_FAULT_AUTH && attempt == 0) {
      logf("[PUSH] session rejected (HTTP %d%s%s), reauthenticating", rep.code,
           sqlstate[0] != '\0' ? " " : "", sqlstate);
      dbSessionInvalidate("push rejected");
      if (sbAuthenticate()) continue;  // replay the unchanged batch
      dbPushBackoff(DB_FAULT_AUTH, retryAfter);
      return false;
    }
    logf("[PUSH] push_readings HTTP %d%s%s (%u queued); batch kept", rep.code,
         sqlstate[0] != '\0' ? " " : "", sqlstate, (unsigned)s_pushCount);
    dbPushBackoff(fault == DB_FAULT_NONE ? DB_FAULT_NET : fault, retryAfter);
    return false;
  }
  return false;
}

// Ends a pass. The 30 s cadence is measured from the END of a pass, so a slow
// or failing pass cannot immediately chain into another one.
static void dbPassFinish() {
  lastDbPollMs = millis();
  link_emit_db();
  dbPass = DB_PASS_IDLE;
}

// One cooperative DB pass. Each call performs AT MOST ONE blocking HTTPS
// operation and then returns to loop(), which services the display link, the
// sensor poll and the heartbeat before the next step. A 10 s socket timeout
// therefore delays the UART once, not for the whole pass (auth + four reads +
// two uploads). The queue has a single owner: all steps run in loop().
static void dbPassTick() {
  switch (dbPass) {
    case DB_PASS_IDLE:
      if (millis() - lastDbPollMs < kDbPollMs) return;
      lastDbPollMs = millis();
      ensureNtpSync();
      if (!ntpSynced) {
        dbFault = DB_FAULT_CLOCK;
        dbPassFinish();
        return;
      }
      if (!sbConfigured()) {
        dbFault = DB_FAULT_AUTH;
        dbPassFinish();
        return;
      }
      dbPassAuthRetried = 0;
      dbPass = dbSessionExpiring() ? DB_PASS_AUTH : DB_PASS_DEVICES;
      return;

    case DB_PASS_AUTH:
      if (!sbAuthenticate()) {
        logf("[DB] pass aborted: authentication failed (%s)", dbFaultName(dbFault));
        dbPassFinish();
        return;
      }
      dbPass = DB_PASS_DEVICES;
      return;

    case DB_PASS_DEVICES: {
      // An empty devices array means the session is not live (RLS 200 + []).
      DbFetch r = sbFetchDevices();
      if (r == DB_FETCH_AUTH && dbPassAuthRetried == 0) {
        dbPassAuthRetried = 1;
        dbSessionInvalidate("devices read empty");
        dbPass = DB_PASS_AUTH;
        return;
      }
      if (r != DB_FETCH_OK) {
        logf("[DB] pass aborted: devices (%s)", dbFaultName(dbFault));
        dbPassFinish();
        return;
      }
      dbPass = DB_PASS_CONFIG;
      return;
    }

    case DB_PASS_CONFIG: {
      DbFetch r = sbFetchDeviceConfig();
      if (r == DB_FETCH_AUTH && dbPassAuthRetried == 0) {
        dbPassAuthRetried = 1;
        dbSessionInvalidate("device_config rejected");
        dbPass = DB_PASS_AUTH;
        return;
      }
      if (r != DB_FETCH_OK) {
        logf("[DB] pass aborted: device_config (%s)", dbFaultName(dbFault));
        dbPassFinish();
        return;
      }
      dbPass = DB_PASS_EXPERIMENTS;
      return;
    }

    case DB_PASS_EXPERIMENTS: {
      DbFetch r = sbFetchExperiments();
      if (r == DB_FETCH_AUTH && dbPassAuthRetried == 0) {
        dbPassAuthRetried = 1;
        dbSessionInvalidate("experiments rejected");
        dbPass = DB_PASS_AUTH;
        return;
      }
      if (r != DB_FETCH_OK) {
        logf("[DB] pass aborted: experiments (%s)", dbFaultName(dbFault));
        dbPassFinish();
        return;
      }
      dbPass = DB_PASS_LATEST;
      return;
    }

    case DB_PASS_LATEST: {
      int latest = -1;
      DbFetch r = sbFetchLatestReadingExp(latest);
      if (r == DB_FETCH_AUTH && dbPassAuthRetried == 0) {
        dbPassAuthRetried = 1;
        dbSessionInvalidate("readings rejected");
        dbPass = DB_PASS_AUTH;
        return;
      }
      if (r != DB_FETCH_OK) {
        logf("[DB] pass aborted: readings (%s)", dbFaultName(dbFault));
        dbPassFinish();
        return;
      }
      if (latest != dbLatest) {
        logf("[DB] latest reading experiment %d -> %d", dbLatest, latest);
        dbLatest = latest;
      }
      dbConfigReady = true;
      dbConfigReadOkMs = millis();
      // A stale identity mismatch keeps uploads blocked and must stay visible
      // even though the reads themselves just succeeded.
      dbFault = (dbQueueDeviceId >= 0 && dbSessionDevice >= 0 &&
                 dbQueueDeviceId != dbSessionDevice)
                    ? DB_FAULT_IDENTITY
                    : DB_FAULT_NONE;
      logf("[DB] config pull ok: bottles=%d experiments=%u latest=%d", dbBottleCount,
           (unsigned)dbExperimentCount, dbLatest);
      // A start refused for want of configuration can proceed now; the
      // operator's run request is still standing.
      if (runStartRefused && expRunning && !runActive) {
        logf("[DB] configuration now available, honouring the pending run start");
        dbApplyRunState();
      }
      dbPushRoundsThisPass = 0;
      dbPass = DB_PASS_PUSH;
      return;
    }

    case DB_PASS_PUSH: {
      // Idle runs never upload and never start a request. Queued active-run
      // rows wait for the next run, keeping their stamps and experiment ids.
      if (!runActive || s_pushCount == 0) {
        dbPass = DB_PASS_REPORT;
        return;
      }
      if (!ntpSynced || dbPushRoundsThisPass >= (uint8_t)kPushFlushRounds) {
        dbPass = DB_PASS_REPORT;
        return;
      }
      if ((int32_t)(millis() - dbNextPushMs) < 0) {
        dbPass = DB_PASS_REPORT;  // backing off
        return;
      }
      dbPushRoundsThisPass++;
      if (!sbPushOneRound()) {
        dbPass = DB_PASS_REPORT;
        return;
      }
      return;  // another round next loop iteration, still bounded by the budget
    }

    case DB_PASS_REPORT:
      // One declaration per pass, including a failed one: the display shows
      // the last known truth, while `reads`/`ack` keep the last success time.
      dbPassFinish();
      return;
  }
}

// Makes the next pass due immediately (used after login and on demand).
static void dbPassForce() {
  lastDbPollMs = millis() - kDbPollMs;
  if (dbPass == DB_PASS_IDLE) dbPassTick();
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

  // UART1 is the legacy Mega slave link: Mega D18 TX -> IO5 RX, Mega D19 RX
  // <- IO17 TX. Pins are remapped because the ESP32 UART1 defaults (IO9/IO10)
  // are flash pins on this module.
  Serial1.begin(kSensBaud, SERIAL_8N1, kSensRxPin, kSensTxPin);
  logf("[SENS] UART1 on IO%d RX / IO%d TX, %u 8N1", kSensRxPin, kSensTxPin,
       (unsigned)kSensBaud);
  lastSensPollMs = millis();

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
  // Sensor rounds run in every stage, including while the portal gate is
  // open; the poll has its own 30 s gate and does not depend on the network.
  sens_poll();
  // At most one HTTPS operation per iteration, after the link and the sensor
  // poll have been serviced. A slow DB read delays the next step, not the UART.
  dbPassTick();

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
        dbPassForce();
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
