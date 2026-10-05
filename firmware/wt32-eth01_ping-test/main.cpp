#include <Arduino.h>
#include <ETH.h>
#include <ESPping.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// WT32-ETH01 Ethernet test with captive portal login:
// - Brings up the LAN8720 over RMII (variant defaults: ADDR 1, PWR 16, MDC 23, MDIO 18)
// - Asks for the portal credentials on the serial console (never stored)
// - Performs the Unpad captive portal login over HTTPS:
//     trigger -> parse window.location -> fetch fgtauth page for magic/4Tredir
//     -> POST username/password -> verify with a real data fetch
// - Then loops: ICMP ping of 1.1.1.1 plus an HTTPS fetch of real data
// Memory notes: page bodies are scanned through a small rolling window or
// capped reads, because the WT32-ETH01 has no PSRAM.

static const IPAddress kPingTarget(1, 1, 1, 1);
static const uint8_t kPingCount = 3;
static const uint32_t kIntervalMs = 5000;
static const char* kDataUrl = "https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ";
static const char* kProbeUrl = "http://example.com/";

static const uint32_t kPromptTimeoutMs = 30000;
static const size_t kMaxBody = 4096;

enum Stage { STAGE_WAIT_IP, STAGE_PROMPT_USER, STAGE_PROMPT_PASS,
             STAGE_LOGIN, STAGE_RUN, STAGE_DONE };
static Stage stage = STAGE_WAIT_IP;
static String username;
static String password;

static volatile bool eth_connected = false;

void waitForEnter() {
  // Discard any bytes that arrived as line noise during boot, so they can
  // never be mistaken for typed input later.
  while (Serial.available()) Serial.read();
  Serial.println("Press Enter to start.");
  for (;;) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') break;  // '\r' and anything else is ignored
    }
    delay(10);
  }
  while (Serial.available()) Serial.read();
}

void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[ETH] Started");
      ETH.setHostname("wt32-eth01");
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

// Reads one line from serial. When echo is false nothing is printed back,
// so the password does not end up in the scrollback or a captured log.
// Returns an empty String on timeout.
String readLine(bool echo, uint32_t timeoutMs) {
  String line;
  uint32_t deadline = millis() + timeoutMs;
  while (millis() < deadline) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (echo) Serial.println();
        return line;
      }
      if (c == 8 || c == 127) {  // backspace / delete
        if (line.length() > 0) {
          line.remove(line.length() - 1);
          if (echo) Serial.print("\b \b");
        }
        continue;
      }
      line += c;
      if (echo) Serial.print(c);
    }
    delay(10);
  }
  return String();
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

// GETs the trigger URL and returns the portal URL from the interception page,
// or "" when there is no interception (already authenticated or no portal).
String fetchPortalUrl() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, kProbeUrl);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setTimeout(5000);

  Serial.print("[LOGIN] GET trigger ");
  Serial.println(kProbeUrl);
  int code = http.GET();
  Serial.print("[LOGIN] trigger status: ");
  Serial.println(code);
  String portalUrl;
  if (code > 0) {
    portalUrl = extractWindowLocation(readBody(http));
    Serial.print("[LOGIN] portal URL: ");
    Serial.println(portalUrl.length() ? portalUrl : "(none)");
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

void doLogin() {
  Serial.print("Credentials held in RAM for username='");
  Serial.print(username);
  Serial.println("', password=<hidden>");

  String portalUrl = fetchPortalUrl();
  if (portalUrl.length() == 0) {
    Serial.println("[LOGIN] No interception. Checking whether we are already through...");
    if (fetchData()) {
      Serial.println("[LOGIN] Already authenticated, skipping POST.");
      stage = STAGE_RUN;
    } else {
      Serial.println("[LOGIN] No portal and no internet. Reset to retry.");
      stage = STAGE_DONE;
    }
    return;
  }

  PageScan scan;
  if (!fetchLoginPage(portalUrl, scan)) {
    Serial.println("[LOGIN] Could not read the login page. Reset to retry.");
    stage = STAGE_DONE;
    return;
  }

  postCredentials(portalUrl, scan);

  Serial.println("[LOGIN] Verifying with a real data fetch...");
  if (fetchData()) {
    Serial.println("[LOGIN] SUCCESS: port is authenticated.");
    stage = STAGE_RUN;
  } else {
    Serial.println("[LOGIN] FAILED: still walled (wrong credentials or portal changed).");
    Serial.println("[LOGIN] Reset to retry; the password was not stored anywhere.");
    stage = STAGE_DONE;
  }

  // Credentials are no longer needed; drop them from RAM.
  password = "";
  username = "";
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
  Serial.println("WT32-ETH01 Ethernet test with captive portal login");
  Serial.println("Ping target: 1.1.1.1");
  Serial.print("Data URL: ");
  Serial.println(kDataUrl);
  Serial.println("Credentials are read from this console and never stored.");

  waitForEnter();

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
        stage = STAGE_PROMPT_USER;
      } else {
        Serial.println("[ETH] Waiting for IP address (DHCP)...");
        delay(2000);
      }
      break;

    case STAGE_PROMPT_USER: {
      Serial.print("Portal username: ");
      username = readLine(true, kPromptTimeoutMs);
      if (username.length() == 0) {
        Serial.println("Timed out waiting for a username. Reset to try again.");
        stage = STAGE_DONE;
      } else {
        stage = STAGE_PROMPT_PASS;
      }
      break;
    }

    case STAGE_PROMPT_PASS: {
      Serial.print("Portal password: ");
      password = readLine(false, kPromptTimeoutMs);
      Serial.println();  // newline, since the password was not echoed
      if (password.length() == 0) {
        Serial.println("Timed out waiting for a password. Reset to try again.");
        stage = STAGE_DONE;
      } else {
        stage = STAGE_LOGIN;
      }
      break;
    }

    case STAGE_LOGIN:
      doLogin();
      break;

    case STAGE_RUN: {
      Serial.print("[ETH] Assigned IP: ");
      Serial.println(ETH.localIP());

      Serial.print("[PING] Pinging 1.1.1.1 ... ");
      bool ok = Ping.ping(kPingTarget, kPingCount);
      if (ok) {
        Serial.print("OK, avg time = ");
        Serial.print(Ping.averageTime());
        Serial.println(" ms");
      } else {
        Serial.println("FAIL (no reply)");
      }

      fetchData();
      delay(kIntervalMs);
      break;
    }

    case STAGE_DONE:
      delay(1000);
      break;
  }
}
