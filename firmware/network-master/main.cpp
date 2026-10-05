#include <Arduino.h>
#include <ETH.h>
#include <ESPping.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// network-master: production WT32-ETH01 firmware.
//
// - Brings up the LAN8720 over RMII (variant defaults: ADDR 1, PWR 16,
//   MDC 23, MDIO 18).
// - Acts as Modbus RTU master (slave ID 1, 9600 8N1) over Serial1 to the
//   display: AT-header TXD (IO17) = Serial1 TX, AT-header RXD (IO5) =
//   Serial1 RX. Display IO17 (TX) -> WT32 IO5 (RX),
//   Display IO18 (RX) <- WT32 IO17 (TX), plus common GND.
//   Serial (USB-UART debug header, TX0/RX0) is debug only and never
//   carries Modbus frames.
// - Gets the captive portal username/password/duration from the display
//   instead of the serial console. While credentials are missing the
//   main loop pauses its network work and polls the display every 5 s.
//   Each poll writes LOGIN_REQ=1, which raises the combined
//   login+duration gate on the display.
// - Runs the proven ping-test portal login (trigger -> login page scan
//   -> POST -> data-fetch verify), then loops ping of 1.1.1.1 plus an
//   HTTPS fetch. A WALLED fetch in RUN falls back to NEED_CREDS for
//   re-auth instead of halting.
// - Credentials live in RAM only and are zeroed after use.
//
// Register map (must match firmware/display-v9/link_modbus.h):
//   0x0000 LOGIN_REQ   master WR  1 = show the login gate
//   0x0001 CREDS_READY slave set  master WR 0 to ack after reading
//   0x0002 RESULT      master WR  0=none 1=success 2=fail
//   0x0010-0x002F USERNAME 32 regs = 64 bytes ASCII NUL-padded
//   0x0030-0x004F PASSWORD 32 regs = 64 bytes ASCII NUL-padded
//   0x0060 DURATION_H 1 reg, 1-99
//   0x0070-0x0077 IP_ADDR 8 regs = 16 bytes ASCII NUL-padded, master WR
//   0x0078 PING_MS 1 reg, master WR. Avg ping to 1.1.1.1 in ms,
//                        0xFFFF = no data / ping failed
//   0x0079 NET_UP 1 reg, master WR. 1 = WT32 has DHCP IP, 0 = link down
// Pushed every RUN cycle so /info on the display shows the real IP and
// the latest ping latency.

#define LINK_RX_PIN 5   // AT-header RXD <- Display IO17 (TX)
#define LINK_TX_PIN 17  // AT-header TXD -> Display IO18 (RX)
#define LINK_BAUD 9600
#define LINK_SLAVE_ID 1

#define LINK_REG_LOGIN_REQ 0x0000
#define LINK_REG_CREDS_READY 0x0001
#define LINK_REG_RESULT 0x0002
#define LINK_REG_USER_BASE 0x0010
#define LINK_REG_USER_REGS 32
#define LINK_REG_PASS_BASE 0x0030
#define LINK_REG_PASS_REGS 32
#define LINK_REG_DURATION 0x0060
#define LINK_REG_IP_BASE 0x0070
#define LINK_REG_IP_REGS 8
#define LINK_REG_PING_MS 0x0078
#define LINK_REG_NET_UP 0x0079
#define LINK_PING_NONE 0xFFFF

static const IPAddress kPingTarget(1, 1, 1, 1);
static const uint8_t kPingCount = 3;
static const uint32_t kIntervalMs = 5000;
static const char* kDataUrl = "https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ";
// Captive-portal trigger: online networks answer 204 with an empty body;
// walled networks answer 200 (interception page) or a 3xx redirect to login.
static const char* kCaptiveProbeUrl = "http://connectivitycheck.gstatic.com/generate_204";

static const size_t kMaxBody = 4096;
static const uint32_t kCredPollMs = 5000;
// Re-probe for an already-valid portal session while waiting for creds.
// Without this the gate deadlocks when no login is needed: the display
// waits for RESULT=success, but doLogin() (which detects "already through")
// is only reached after staged credentials arrive, which never come.
static const uint32_t kAuthProbeMs = 30000;
static uint32_t lastAuthProbeMs = (uint32_t)-30000;

enum Stage { STAGE_WAIT_IP, STAGE_NEED_CREDS, STAGE_LOGIN, STAGE_RUN };
static Stage stage = STAGE_WAIT_IP;
static String username;
static String password;
static int durationH = 24;
static uint32_t lastCredPollMs = 0;

static volatile bool eth_connected = false;

// --- Modbus RTU master (minimal, FC 0x03 read / FC 0x06 write) ---

static uint16_t mbCrc(const uint8_t* data, size_t len) {
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

static void mbDrain() {
  while (Serial1.available()) Serial1.read();
}

// Reads one Modbus response frame with a 1 s timeout. Returns the
// payload length, or 0 on timeout / CRC error / exception.
static size_t mbReadFrame(uint8_t* buf, size_t cap) {
  uint32_t deadline = millis() + 1000;
  size_t n = 0;
  uint32_t lastByte = millis();
  while (millis() < deadline) {
    while (Serial1.available()) {
      int c = Serial1.read();
      if (c < 0) break;
      if (n < cap) buf[n++] = (uint8_t)c;
      lastByte = millis();
    }
    // End of frame: 8 ms of silence after at least the 5-byte minimum.
    if (n >= 5 && millis() - lastByte >= 8) break;
    delay(2);
  }
  if (n < 5) return 0;
  uint16_t crcRx = (uint16_t)buf[n - 2] | ((uint16_t)buf[n - 1] << 8);
  if (mbCrc(buf, n - 2) != crcRx) {
    Serial.println("[MB] CRC error, frame dropped");
    return 0;
  }
  if (buf[0] != LINK_SLAVE_ID) return 0;
  if (buf[1] & 0x80) {
    Serial.printf("[MB] exception fn=0x%02X code=%d\n", buf[1] & 0x7F, buf[2]);
    return 0;
  }
  return n;
}

static bool mbReadHolding(uint16_t addr, uint16_t count, uint16_t* out) {
  uint8_t req[8];
  req[0] = LINK_SLAVE_ID;
  req[1] = 0x03;
  req[2] = addr >> 8;
  req[3] = addr & 0xFF;
  req[4] = count >> 8;
  req[5] = count & 0xFF;
  uint16_t crc = mbCrc(req, 6);
  req[6] = crc & 0xFF;
  req[7] = crc >> 8;
  mbDrain();
  delay(2);
  Serial1.write(req, 8);
  Serial1.flush();
  uint8_t rsp[5 + 128];
  size_t n = mbReadFrame(rsp, sizeof(rsp));
  if (n < 5 || rsp[1] != 0x03 || rsp[2] != count * 2) return false;
  for (uint16_t i = 0; i < count; i++) {
    out[i] = ((uint16_t)rsp[3 + i * 2] << 8) | rsp[4 + i * 2];
  }
  return true;
}

static bool mbWriteSingle(uint16_t addr, uint16_t value) {
  uint8_t req[8];
  req[0] = LINK_SLAVE_ID;
  req[1] = 0x06;
  req[2] = addr >> 8;
  req[3] = addr & 0xFF;
  req[4] = value >> 8;
  req[5] = value & 0xFF;
  uint16_t crc = mbCrc(req, 6);
  req[6] = crc & 0xFF;
  req[7] = crc >> 8;
  mbDrain();
  delay(2);
  Serial1.write(req, 8);
  Serial1.flush();
  uint8_t rsp[8];
  size_t n = mbReadFrame(rsp, sizeof(rsp));
  if (n != 8 || rsp[1] != 0x06) return false;
  return true;
}

// FC 0x10 write multiple holding registers. Returns true on valid echo.
static bool mbWriteMultiple(uint16_t addr, uint16_t count, const uint16_t* values) {
  if (count == 0 || count > 16) return false;
  uint8_t req[9 + 32];
  req[0] = LINK_SLAVE_ID;
  req[1] = 0x10;
  req[2] = addr >> 8;
  req[3] = addr & 0xFF;
  req[4] = count >> 8;
  req[5] = count & 0xFF;
  req[6] = (uint8_t)(count * 2);
  for (uint16_t i = 0; i < count; i++) {
    req[7 + i * 2] = values[i] >> 8;
    req[8 + i * 2] = values[i] & 0xFF;
  }
  size_t reqlen = 7 + count * 2;
  uint16_t crc = mbCrc(req, reqlen);
  req[reqlen] = crc & 0xFF;
  req[reqlen + 1] = crc >> 8;
  mbDrain();
  delay(2);
  Serial1.write(req, reqlen + 2);
  Serial1.flush();
  uint8_t rsp[8];
  size_t n = mbReadFrame(rsp, sizeof(rsp));
  if (n != 8 || rsp[1] != 0x10) return false;
  uint16_t echoAddr = ((uint16_t)rsp[2] << 8) | rsp[3];
  uint16_t echoCnt = ((uint16_t)rsp[4] << 8) | rsp[5];
  return echoAddr == addr && echoCnt == count;
}

static String regsToString(const uint16_t* regs, uint16_t count) {
  String s;
  for (uint16_t i = 0; i < count; i++) {
    char hi = (char)(regs[i] >> 8);
    char lo = (char)(regs[i] & 0xFF);
    if (hi == '\0') break;
    s += hi;
    if (lo == '\0') break;
    s += lo;
  }
  return s;
}

// One credential poll: request the gate, then read staged credentials.
// Returns true when a fresh username/password pair was captured.
static bool pollDisplayCreds() {
  // Best effort: the gate is already up on display boot, so a lost
  // LOGIN_REQ write only delays the popup to the next poll.
  if (!mbWriteSingle(LINK_REG_LOGIN_REQ, 1)) {
    Serial.println("[MB] LOGIN_REQ write failed (retry in 5 s)");
  }
  uint16_t ready = 0;
  if (!mbReadHolding(LINK_REG_CREDS_READY, 1, &ready)) {
    Serial.println("[MB] CREDS_READY read failed (retry in 5 s)");
    return false;
  }
  if (ready == 0) {
    Serial.println("[MB] display has no staged credentials yet");
    return false;
  }
  uint16_t uregs[LINK_REG_USER_REGS];
  uint16_t pregs[LINK_REG_PASS_REGS];
  uint16_t dreg = 0;
  if (!mbReadHolding(LINK_REG_USER_BASE, LINK_REG_USER_REGS, uregs)) {
    Serial.println("[MB] USERNAME read failed");
    return false;
  }
  if (!mbReadHolding(LINK_REG_PASS_BASE, LINK_REG_PASS_REGS, pregs)) {
    Serial.println("[MB] PASSWORD read failed");
    return false;
  }
  if (!mbReadHolding(LINK_REG_DURATION, 1, &dreg)) {
    Serial.println("[MB] DURATION read failed");
    return false;
  }
  String u = regsToString(uregs, LINK_REG_USER_REGS);
  String p = regsToString(pregs, LINK_REG_PASS_REGS);
  u.trim();
  if (u.length() == 0 || p.length() == 0) {
    Serial.println("[MB] staged credentials are empty, waiting for Confirm");
    return false;
  }
  username = u;
  password = p;
  durationH = dreg;
  if (durationH < 1) durationH = 1;
  if (durationH > 99) durationH = 99;
  Serial.printf("[MB] captured creds user='%s' dur=%dh\n", username.c_str(),
                durationH);
  return true;
}

static void consumeDisplayCreds(int result) {
  // Order matters: the display clears RESULT when LOGIN_REQ is written to 0,
  // so RESULT must be written last or a SUCCESS verdict is wiped to NONE
  // before the display polls it.
  mbWriteSingle(LINK_REG_CREDS_READY, 0);
  if (result == 1) mbWriteSingle(LINK_REG_LOGIN_REQ, 0);
  mbWriteSingle(LINK_REG_RESULT, (uint16_t)result);
}

// Pushes the current IP + ping latency to the display for /info.
// pingMs: averaged Ping.averageTime(), or -1 for no data / failed ping.
// netUp: false clears the display's IP verdict to "link down".
static void pushNetStatus(int pingMs, bool netUp) {
  String ip = netUp ? ETH.localIP().toString() : String("");
  uint16_t regs[LINK_REG_IP_REGS] = {0};
  for (uint16_t i = 0; i < LINK_REG_IP_REGS; i++) {
    size_t o = (size_t)i * 2;
    uint8_t hi = o < ip.length() ? (uint8_t)ip[o] : 0;
    uint8_t lo = o + 1 < ip.length() ? (uint8_t)ip[o + 1] : 0;
    regs[i] = ((uint16_t)hi << 8) | lo;
  }
  if (!mbWriteMultiple(LINK_REG_IP_BASE, LINK_REG_IP_REGS, regs)) {
    Serial.println("[MB] IP_ADDR write failed");
  }
  uint16_t pingReg =
      (pingMs >= 0 && pingMs < LINK_PING_NONE) ? (uint16_t)pingMs : LINK_PING_NONE;
  if (!mbWriteSingle(LINK_REG_PING_MS, pingReg)) {
    Serial.println("[MB] PING_MS write failed");
  }
  if (!mbWriteSingle(LINK_REG_NET_UP, netUp ? 1 : 0)) {
    Serial.println("[MB] NET_UP write failed");
  }
}

// --- Ethernet + portal login (ported from wt32-eth01_ping-test) ---

void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[ETH] Started");
      ETH.setHostname("rusim-master");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[ETH] Link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("[ETH] Got IP");
      Serial.print("[ETH] IP address: ");
      Serial.println(ETH.localIP());
      Serial.print("[ETH] Gateway: ");
      Serial.println(ETH.gatewayIP());
      Serial.print("[ETH] Netmask: ");
      Serial.println(ETH.subnetMask());
      Serial.print("[ETH] MAC: ");
      Serial.println(ETH.macAddress());
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[ETH] Link down");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("[ETH] Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

// Reads the response into a String, capped at kMaxBody characters.
String readBody(HTTPClient& http) {
  String body;
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return body;
  uint8_t buf[256];
  while (http.connected() && body.length() < kMaxBody) {
    int n = stream->readBytes(reinterpret_cast<char*>(buf), sizeof(buf));
    if (n <= 0) break;
    // Appends exactly n bytes. The buffer is NOT NUL-terminated, so the
    // plain `+=` operator would over-read into stale stack memory.
    body.concat(reinterpret_cast<const char*>(buf), n);
  }
  return body;
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
// instead of being buffered.
struct PageScan {
  bool hasForm = false;
  String formAction;
  String redir;
  String magic;
  bool hasUsernameField = false;
  size_t totalBytes = 0;
};

// Searches `win` for needle, then returns the value of the first
// "value=\"" attribute at or after it, or "" when absent.
String valueAfter(const String& win, const String& needle, size_t limit = 512) {
  int at = win.indexOf(needle);
  if (at < 0) return "";
  int v = win.indexOf("value=\"", at + needle.length());
  if (v < 0 || v - at > limit) return "";
  int end = win.indexOf('"', v + 7);
  if (end < 0) return "";
  return win.substring(v + 7, end);
}

void scanPage(HTTPClient& http, PageScan& out) {
  static const size_t kWindow = 320;  // > longest needle, small enough for RAM
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return;

  String win;
  uint8_t buf[256];
  while (http.connected()) {
    int n = stream->readBytes(reinterpret_cast<char*>(buf), sizeof(buf));
    if (n <= 0) break;
    out.totalBytes += n;
    win.concat(reinterpret_cast<const char*>(buf), n);

    if (!out.hasForm) {
      int f = win.indexOf("<form");
      if (f >= 0) {
        out.hasForm = true;
        int a = win.indexOf("action=\"", f);
        if (a >= 0) {
          int e = win.indexOf('"', a + 8);
          if (e > 0) out.formAction = win.substring(a + 8, e);
        }
      }
    }
    if (out.magic.length() == 0) {
      out.magic = valueAfter(win, "name=\"magic\"");
    }
    if (out.redir.length() == 0) {
      out.redir = valueAfter(win, "name=\"4Tredir\"");
    }
    if (!out.hasUsernameField && win.indexOf("name=\"username\"") >= 0) {
      out.hasUsernameField = true;
    }

    if (win.length() > kWindow) win = win.substring(win.length() - kWindow);
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

  Serial.print("[LOGIN] GET trigger ");
  Serial.println(kCaptiveProbeUrl);
  int code = http.GET();
  Serial.print("[LOGIN] trigger status: ");
  Serial.println(code);
  String portalUrl;
  if (code == 204) {
    Serial.println("[LOGIN] portal URL: (none, 204 online)");
  } else if (code == 301 || code == 302 || code == 303 || code == 307 ||
             code == 308) {
    portalUrl = http.header("Location");
    Serial.print("[LOGIN] portal URL: ");
    Serial.println(portalUrl.length() ? portalUrl : "(redirect without Location)");
  } else if (code == 200) {
    portalUrl = extractWindowLocation(readBody(http));
    Serial.print("[LOGIN] portal URL: ");
    Serial.println(portalUrl.length() ? portalUrl : "(200 without redirect URL)");
  } else if (code > 0) {
    Serial.println("[LOGIN] unexpected trigger status (treating as walled)");
  } else {
    Serial.print("[LOGIN] trigger error: ");
    Serial.println(http.errorToString(code));
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

  Serial.print("[LOGIN] GET login page ");
  Serial.println(portalUrl);
  int code = http.GET();
  Serial.print("[LOGIN] login page status: ");
  Serial.println(code);
  if (code != 200) {
    if (code < 0) {
      Serial.print("[LOGIN] login page error: ");
      Serial.println(http.errorToString(code));
    }
    http.end();
    return false;
  }
  scanPage(http, scan);
  http.end();

  Serial.print("[LOGIN] form action: ");
  Serial.println(scan.hasForm ? scan.formAction : "(no form found)");
  Serial.print("[LOGIN] 4Tredir: ");
  Serial.println(scan.redir.length() ? scan.redir : "(not found)");
  Serial.print("[LOGIN] magic: ");
  Serial.println(scan.magic.length() ? scan.magic : "(not found)");
  return scan.hasForm && scan.magic.length() > 0;
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

  Serial.print("[LOGIN] POST ");
  Serial.println(postUrl);
  Serial.print("[LOGIN] body length: ");
  Serial.println(body.length());
  int code = http.POST(body);
  Serial.print("[LOGIN] POST status: ");
  Serial.println(code);

  bool accepted = false;
  if (code > 0) {
    Serial.print("[LOGIN] Location: ");
    Serial.println(http.header("Location"));
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      accepted = true;
    }
    String resp = readBody(http);
    Serial.print("[LOGIN] response bytes: ");
    Serial.println(resp.length());
    Serial.println("[LOGIN] response head:");
    Serial.println(resp.substring(0, 800));
    // A success page echoes a meta-refresh or a link to the 4Tredir target;
    // an error page names the failure. Either way the verifier decides.
    if (resp.indexOf("success") >= 0 || resp.indexOf("Success") >= 0 ||
        resp.indexOf(scan.redir) >= 0) {
      accepted = true;
    }
  } else {
    Serial.print("[LOGIN] POST error: ");
    Serial.println(http.errorToString(code));
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

  Serial.print("[HTTP] GET ");
  Serial.println(kDataUrl);
  int code = http.GET();
  Serial.print("[HTTP] status: ");
  Serial.println(code);

  bool genuine = false;
  if (code > 0) {
    String body = readBody(http);
    if (extractWindowLocation(body).length() > 0) {
      Serial.println("[HTTP] body is the portal interception page (WALLED)");
    } else {
      genuine = true;
      Serial.println("[HTTP] body is genuine content (AUTHENTICATED):");
      Serial.println(body);
    }
  } else {
    Serial.print("[HTTP] error: ");
    Serial.println(http.errorToString(code));
  }
  http.end();
  return genuine;
}

static void clearCreds() {
  // Overwrite before releasing so the password never lingers in heap.
  for (size_t i = 0; i < password.length(); i++) password[i] = 'x';
  password = "";
  username = "";
}

// Returns true on authenticated, false otherwise. Reports the verdict to
// the display and consumes the staged credentials.
bool doLogin() {
  Serial.printf("[LOGIN] user='%s' dur=%dh from display (password in RAM only)\n",
                username.c_str(), durationH);

  String portalUrl = fetchPortalUrl();
  if (portalUrl.length() == 0) {
    Serial.println("[LOGIN] No interception. Checking whether we are already through...");
    if (fetchData()) {
      Serial.println("[LOGIN] Already authenticated, skipping POST.");
      consumeDisplayCreds(1);
      clearCreds();
      return true;
    }
    Serial.println("[LOGIN] No portal and no internet. Will re-poll display.");
    consumeDisplayCreds(2);
    clearCreds();
    return false;
  }

  PageScan scan;
  if (!fetchLoginPage(portalUrl, scan)) {
    Serial.println("[LOGIN] Could not read the login page. Will re-poll display.");
    consumeDisplayCreds(2);
    clearCreds();
    return false;
  }

  postCredentials(portalUrl, scan);

  Serial.println("[LOGIN] Verifying with a real data fetch...");
  bool ok = fetchData();
  if (ok) {
    Serial.println("[LOGIN] SUCCESS: port is authenticated.");
    consumeDisplayCreds(1);
  } else {
    Serial.println("[LOGIN] FAILED: still walled (wrong credentials or portal changed).");
    consumeDisplayCreds(2);
  }
  clearCreds();
  return ok;
}

void setup() {
  Serial.begin(115200);
  // Give the USB-serial monitor time to attach, then push the ROM
  // bootloader's boot message (printed at 74880 baud, so it shows as
  // garbage at 115200) off screen with a few blank lines.
  delay(1000);
  for (int i = 0; i < 5; i++) Serial.println();
  Serial.flush();

  Serial.println();
  Serial.println("network-master (production WT32 portal + Modbus master)");
  Serial.println("Ping target: 1.1.1.1");
  Serial.print("Data URL: ");
  Serial.println(kDataUrl);
  Serial.println("Credentials come from the display over Modbus RTU, never stored.");

  Serial1.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  delay(20);
  mbDrain();
  Serial.printf("[MB] master on Serial1 RX=%d TX=%d %d 8N1 slave=%d\n", LINK_RX_PIN,
                LINK_TX_PIN, LINK_BAUD, LINK_SLAVE_ID);

  WiFi.onEvent(onEthEvent);

  // Explicit WT32-ETH01 RMII pins (same as variant defaults in pins_arduino.h):
  // PHY addr 1, power pin 16, MDC 23, MDIO 18, LAN8720, 50 MHz clock in on GPIO0.
  if (!ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO,
                 ETH_PHY_TYPE, ETH_CLK_MODE)) {
    Serial.println("[ETH] ERROR: ETH.begin() failed");
  } else {
    Serial.println("[ETH] Waiting for link + DHCP...");
  }
}

void loop() {
  switch (stage) {
    case STAGE_WAIT_IP:
      if (eth_connected) {
        Serial.println("[MB] link up, polling display for credentials");
        lastCredPollMs = 0;  // poll immediately
        stage = STAGE_NEED_CREDS;
      } else {
        Serial.println("[ETH] Waiting for IP address (DHCP)...");
        delay(2000);
      }
      break;

    case STAGE_NEED_CREDS: {
      // The whole network loop pauses here: nothing but the display poll
      // runs until credentials arrive.
      if (!eth_connected) {
        Serial.println("[ETH] link lost while waiting for creds");
        pushNetStatus(-1, false);
        stage = STAGE_WAIT_IP;
        break;
      }
      if (millis() - lastCredPollMs >= kCredPollMs) {
        lastCredPollMs = millis();
        if (pollDisplayCreds()) {
          stage = STAGE_LOGIN;
        } else if (millis() - lastAuthProbeMs >= kAuthProbeMs) {
          // No staged creds: check whether the port is already through
          // before asking the user for a login they don't need.
          lastAuthProbeMs = millis();
          Serial.println("[LOGIN] no creds staged; probing for existing session...");
          String portalUrl = fetchPortalUrl();
          if (portalUrl.length() == 0 && fetchData()) {
            Serial.println("[LOGIN] session already valid; releasing display gate.");
            consumeDisplayCreds(1);
            stage = STAGE_RUN;
          }
        }
        // Keep /info live while waiting: real IP now, ping comes in RUN.
        pushNetStatus(-1, true);
      } else {
        delay(100);
      }
      break;
    }

    case STAGE_LOGIN:
      if (doLogin()) {
        stage = STAGE_RUN;
      } else {
        Serial.println("[LOGIN] will re-poll display in 5 s");
        lastCredPollMs = millis();
        stage = STAGE_NEED_CREDS;
      }
      break;

    case STAGE_RUN: {
      if (!eth_connected) {
        Serial.println("[ETH] link lost in RUN, back to WAIT_IP");
        pushNetStatus(-1, false);
        stage = STAGE_WAIT_IP;
        break;
      }
      Serial.print("[ETH] Assigned IP: ");
      Serial.println(ETH.localIP());

      Serial.print("[PING] Pinging 1.1.1.1 ... ");
      bool ok = Ping.ping(kPingTarget, kPingCount);
      int pingMs = -1;
      if (ok) {
        pingMs = (int)Ping.averageTime();
        Serial.print("OK, avg time = ");
        Serial.print(pingMs);
        Serial.println(" ms");
      } else {
        Serial.println("FAIL (no reply)");
      }
      // Push the fresh verdict every cycle so /info stays live.
      pushNetStatus(pingMs, true);

      // The data fetch is the portal verdict: WALLED means the session
      // expired, so pause the loop and re-auth via the display.
      if (!fetchData()) {
        Serial.println("[LOGIN] session walled mid-run, re-polling display");
        lastCredPollMs = 0;
        stage = STAGE_NEED_CREDS;
        break;
      }
      Serial.printf("[RUN] experiment duration %dh (from display)\n", durationH);
      delay(kIntervalMs);
      break;
    }
  }
}
