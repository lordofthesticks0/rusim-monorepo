#include <Arduino.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// WT32-ETH01 captive portal recon:
// - Brings up LAN8720 over RMII (ADDR 1, PWR 16, MDC 23, MDIO 18)
// - Asks for the portal credentials on the serial console, so nothing
//   sensitive is baked into the firmware image or stored on disk
// - Probes for the Unpad captive portal, then reports the portal URL,
//   the login form's hidden fields, and whether the port is walled
// Memory notes: responses are read in small chunks with a hard cap,
// because the WT32-ETH01 has no PSRAM.

static volatile bool eth_connected = false;

static const char* kProbeUrl = "http://example.com/";
// A real internet resource. If the port is walled, the campus network answers
// with the interception page instead of the lyrics; if it is authenticated,
// the actual body comes back.
static const char* kDataUrl = "https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ";

static const uint32_t kPromptTimeoutMs = 30000;
static const size_t kMaxBody = 4096;

enum Stage { STAGE_WAIT_IP, STAGE_PROMPT_USER, STAGE_PROMPT_PASS, STAGE_RECON, STAGE_DONE };
static Stage stage = STAGE_WAIT_IP;
static String username;
static String password;

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
      ETH.setHostname("wt32-eth01-recon");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[ETH] Link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("[ETH] IP address: ");
      Serial.println(ETH.localIP());
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

// Reads the response into a String, capped at kMaxBody characters. Only used
// for the short interception page; the login page is scanned instead.
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

// What a login page scan recovers. The Unpad login page is roughly 8 KB of
// inline CSS before the form, so the body cannot be buffered on this board.
// Instead the stream is scanned through a small rolling window.
struct PageScan {
  bool hasForm = false;
  String formAction;
  String redir;
  String magic;
  bool hasUsernameField = false;
  bool cloudflareChallenge = false;
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

// Streams the body, keeping only a rolling window in RAM so that a token
// straddling a chunk boundary is still seen.
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
    // Appends exactly n bytes. The buffer is NOT NUL-terminated, so the
    // plain `+=` operator would over-read into stale stack memory and
    // corrupt tokens that span a chunk boundary.
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
    if (!out.cloudflareChallenge &&
        (win.indexOf("cf-browser-verification") >= 0 ||
         win.indexOf("challenge-platform") >= 0 ||
         win.indexOf("Just a moment") >= 0)) {
      out.cloudflareChallenge = true;
    }

    if (win.length() > kWindow) win = win.substring(win.length() - kWindow);
  }
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

// Fetches a real internet resource and reports whether its body is genuine
// or the portal's interception page. Doubles as the login success detector.
void probeVerify() {
  Serial.println("=== VERIFY: GET real data ===");
  Serial.print("URL: ");
  Serial.println(kDataUrl);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, kDataUrl);
  http.setTimeout(8000);
  int code = http.GET();
  Serial.print("HTTP status: ");
  Serial.println(code);
  if (code > 0) {
    String body = readBody(http);
    String target = extractWindowLocation(body);
    if (target.length() > 0) {
      Serial.println("Result: WALLED (interception page returned)");
      Serial.print("Interception target: ");
      Serial.println(target);
    } else {
      Serial.println("Result: AUTHENTICATED (real content received)");
      Serial.print("Body length: ");
      Serial.println(body.length());
      Serial.println("--- body (truncated at 4 KB) ---");
      Serial.println(body);
    }
  } else {
    Serial.print("Client error: ");
    Serial.println(http.errorToString(code));
  }
  http.end();
}

void runRecon() {
  Serial.print("Credentials held in RAM for username='");
  Serial.print(username);
  Serial.println("', password=<hidden>");

  // --- Step 1: trigger the interception, pull the portal URL out of the JS ---
  String portalUrl;
  {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, kProbeUrl);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setTimeout(5000);
    const char* headerKeys[] = {"Location", "Set-Cookie"};
    http.collectHeaders(headerKeys, 2);

    Serial.print("=== STEP 1: GET ");
    Serial.println(kProbeUrl);
    int code = http.GET();
    Serial.print("HTTP status: ");
    Serial.println(code);
    Serial.print("Location header: ");
    Serial.println(http.header("Location"));

    if (code > 0) {
      String body = readBody(http);
      portalUrl = extractWindowLocation(body);
      Serial.print("window.location: ");
      Serial.println(portalUrl.length() ? portalUrl : "(none)");
    } else {
      Serial.print("Client error: ");
      Serial.println(http.errorToString(code));
    }
    http.end();
  }

  if (portalUrl.length() == 0) {
    Serial.println("No portal URL found. Run the verify probe to see whether");
    Serial.println("the port is already authenticated.");
    probeVerify();
    stage = STAGE_DONE;
    return;
  }

  // --- Step 2: fetch the login page, report the form's hidden fields ---
  Serial.println("=== STEP 2: GET portal login page ===");
  Serial.print("Portal URL: ");
  Serial.println(portalUrl);
  {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, portalUrl);
    http.setTimeout(8000);
    const char* headerKeys[] = {"Set-Cookie", "Content-Type", "Server"};
    http.collectHeaders(headerKeys, 3);

    int code = http.GET();
    Serial.print("HTTP status: ");
    Serial.println(code);
    Serial.print("Content-Type: ");
    Serial.println(http.header("Content-Type"));
    Serial.print("Server: ");
    Serial.println(http.header("Server"));
    Serial.print("Set-Cookie: ");
    Serial.println(http.header("Set-Cookie"));

    if (code == 200) {
      PageScan scan;
      scanPage(http, scan);
      Serial.print("form action: ");
      Serial.println(scan.hasForm ? (scan.formAction.length() ? scan.formAction
                                                             : "(present, no action)")
                                   : "(no form found)");
      Serial.print("hidden 4Tredir: ");
      Serial.println(scan.redir.length() ? scan.redir : "(not found)");
      Serial.print("hidden magic: ");
      Serial.println(scan.magic.length() ? scan.magic : "(not found)");
      Serial.print("username field: ");
      Serial.println(scan.hasUsernameField ? "present" : "MISSING");
      Serial.print("cloudflare challenge: ");
      Serial.println(scan.cloudflareChallenge ? "YES" : "no");
      Serial.print("body size: ");
      Serial.println(scan.totalBytes);
    } else if (code < 0) {
      Serial.print("Client error: ");
      Serial.println(http.errorToString(code));
      Serial.println("(a Cloudflare challenge here would mean the ESP32 cannot");
      Serial.println(" complete headless authentication)");
    }
    http.end();
  }

  probeVerify();

  // Credentials are no longer needed by this sketch; drop them from RAM.
  password = "";
  username = "";
  stage = STAGE_DONE;
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
  Serial.println("WT32-ETH01 captive portal recon");
  Serial.println("Credentials are read from this console and never stored.");

  waitForEnter();

  WiFi.onEvent(onEthEvent);
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO,
            ETH_PHY_TYPE, ETH_CLK_MODE);
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
        runRecon();
      }
      break;
    }

    case STAGE_RECON:
    case STAGE_DONE:
      delay(1000);
      break;
  }
}