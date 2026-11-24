#include "metrics.h"

#if RUSIM_METRICS

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <string.h>

#include "ui_shell.h"

// Verification instrumentation. Definitions and the analysis that consumes
// these lines: docs/verification-toolkit.md.
//
// Touch latency (MET TOUCH). A touch edge is the first indev read that sees
// the controller state change (press = "down", lift = "up"). The measurement
// ends when LVGL finishes rendering and flushing the frame that contains the
// widgets the touch changed (LV_EVENT_RENDER_READY). Only invalidations made
// while the indev read timer runs count as "caused by the touch", so a status
// bar or sensor redraw a few ms later cannot complete the measurement. An
// edge that changes nothing on screen within kNoFrameUs is logged with nf=1.
//
// The start point excludes the GT911 scan and the up-to-33 ms indev polling
// period; the end point is the framebuffer write, not light from the panel.
// Validate a subset with RUSIM_MET_GPIO and a logic analyser, or video.

static const uint32_t kNoFrameUs = 500000;   // no touch-caused invalidation
static const uint32_t kStuckUs = 2000000;    // invalidated but never rendered
static const uint32_t kSensFrameUs = 2000000;
static const uint32_t kPeriodMs = 60000;     // MEM and LINK lines
static const uint32_t kFpsPeriodMs = 5000;   // FPS line (only when frames > 0)

// --- boot milestones ---------------------------------------------------------

struct BootMark {
  const char *stage;
  uint32_t ms;
};
static BootMark s_boot[24];
static size_t s_bootN = 0;

void met_boot(const char *stage) {
  for (size_t i = 0; i < s_bootN; i++) {
    if (strcmp(s_boot[i].stage, stage) == 0) return;
  }
  uint32_t ms = millis();
  if (s_bootN < sizeof(s_boot) / sizeof(s_boot[0])) s_boot[s_bootN++] = {stage, ms};
  Serial.printf("MET BOOT stage=%s ms=%u\n", stage, (unsigned)ms);
}

static const char *reset_reason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "ext";
    case ESP_RST_SW: return "sw";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "unknown";
  }
}

void met_init_early() {
  // Must precede Serial.begin(). A full TX FIFO would otherwise block the
  // UI loop on every MET or [LINK<] line and inflate the latencies measured.
  Serial.setTxBufferSize(8192);
}

// --- touch latency -----------------------------------------------------------

struct TouchPending {
  bool armed;
  bool down;
  bool wake;
  bool inv;         // a touch-caused invalidation has been seen
  bool renderSeen;  // a render started after that invalidation
  uint32_t t0Us;
  uint32_t invUs;
  uint32_t renderStartUs;
  int x, y;
  const char *from;
};
static TouchPending s_touch = {};
static bool s_rawPrev = false;
static bool s_inIndev = false;
static lv_timer_cb_t s_indevCbOrig = nullptr;

// Finished results wait here until met_tick() prints them.
struct TouchResult {
  bool down, wake, nf, stuck;
  uint32_t latUs, invUs, renderUs;
  int x, y;
  const char *from;
  const char *to;
};
static TouchResult s_results[16];
static uint8_t s_resHead = 0, s_resCount = 0;
static uint32_t s_resDropped = 0;

static void push_result(const TouchResult &r) {
  if (s_resCount == sizeof(s_results) / sizeof(s_results[0])) {
    s_resDropped++;
    return;
  }
  s_results[(s_resHead + s_resCount) % (sizeof(s_results) / sizeof(s_results[0]))] = r;
  s_resCount++;
}

static void probe(bool level) {
#if RUSIM_MET_GPIO >= 0
  digitalWrite(RUSIM_MET_GPIO, level ? HIGH : LOW);
#else
  (void)level;
#endif
}

static void finish_touch(bool nf, bool stuck, uint32_t nowUs) {
  TouchResult r = {};
  r.down = s_touch.down;
  r.wake = s_touch.wake;
  r.nf = nf;
  r.stuck = stuck;
  r.latUs = nowUs - s_touch.t0Us;
  r.invUs = s_touch.inv ? s_touch.invUs : 0;
  r.renderUs = s_touch.renderSeen ? nowUs - s_touch.renderStartUs : 0;
  r.x = s_touch.x;
  r.y = s_touch.y;
  r.from = s_touch.from;
  r.to = ui_route_name();
  push_result(r);
  s_touch.armed = false;
  probe(false);
}

void met_touch(bool rawPressed, bool swallowed, int x, int y) {
  if (rawPressed == s_rawPrev) return;
  s_rawPrev = rawPressed;
  const uint32_t now = micros();
  // A new edge before the previous one resolved: the previous one never
  // produced a frame of its own.
  if (s_touch.armed) finish_touch(true, false, now);
  s_touch = {};
  s_touch.armed = true;
  s_touch.down = rawPressed;
  s_touch.wake = swallowed;
  s_touch.t0Us = now;
  s_touch.x = x;
  s_touch.y = y;
  s_touch.from = ui_route_name();
  probe(true);
}

static void indev_timer_wrapper(lv_timer_t *t) {
  s_inIndev = true;
  if (s_indevCbOrig != nullptr) s_indevCbOrig(t);
  s_inIndev = false;
}

// --- render statistics -------------------------------------------------------

static bool s_rendering = false;
static uint32_t s_renderStartUs = 0;
static uint32_t s_fpsWindowMs = 0;
static uint32_t s_frames = 0;
static uint64_t s_renderSumUs = 0;
static uint32_t s_renderMaxUs = 0;
static uint64_t s_px = 0;
static bool s_firstFrame = false;

// SENS frame waiting for the render that shows it.
struct SensPending {
  bool rxArmed;       // received, not yet consumed by the UI model
  bool applyArmed;    // consumed, waiting for a frame
  uint32_t wseq;      // WT32 seq= (0 when the WT32 is not a metrics build)
  uint32_t localSeq;  // display sequence number
  uint32_t rxUs;
  uint32_t applyUs;
};
static SensPending s_sens = {};
struct SensResult {
  uint32_t wseq, localSeq, applyUs, renderUs;
  bool noframe;
};
static SensResult s_sensRes[8];
static uint8_t s_sensResCount = 0;

static void push_sens(bool noframe, uint32_t nowUs) {
  if (s_sensResCount < sizeof(s_sensRes) / sizeof(s_sensRes[0])) {
    s_sensRes[s_sensResCount++] = {s_sens.wseq, s_sens.localSeq,
                                   s_sens.applyUs - s_sens.rxUs,
                                   noframe ? 0 : nowUs - s_sens.rxUs, noframe};
  }
  s_sens.applyArmed = false;
}

static void on_display_event(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  const uint32_t now = micros();
  if (code == LV_EVENT_INVALIDATE_AREA) {
    if (s_touch.armed && s_inIndev && !s_touch.inv) {
      s_touch.inv = true;
      s_touch.invUs = now - s_touch.t0Us;
    }
  } else if (code == LV_EVENT_RENDER_START) {
    s_rendering = true;
    s_renderStartUs = now;
    if (s_touch.armed && s_touch.inv && !s_touch.renderSeen) {
      s_touch.renderSeen = true;
      s_touch.renderStartUs = now;
    }
  } else if (code == LV_EVENT_RENDER_READY) {
    if (s_rendering) {
      const uint32_t dur = now - s_renderStartUs;
      s_frames++;
      s_renderSumUs += dur;
      if (dur > s_renderMaxUs) s_renderMaxUs = dur;
    }
    s_rendering = false;
    if (!s_firstFrame) {
      s_firstFrame = true;
      met_boot("first_frame");
    }
    if (s_touch.armed && s_touch.renderSeen) finish_touch(false, false, now);
    if (s_sens.applyArmed) push_sens(false, now);
  }
}

void met_attach(lv_display_t *disp, lv_indev_t *indev) {
#if RUSIM_MET_GPIO >= 0
  pinMode(RUSIM_MET_GPIO, OUTPUT);
  digitalWrite(RUSIM_MET_GPIO, LOW);
#endif
  if (disp != nullptr) {
    lv_display_add_event_cb(disp, on_display_event, LV_EVENT_INVALIDATE_AREA, nullptr);
    lv_display_add_event_cb(disp, on_display_event, LV_EVENT_RENDER_START, nullptr);
    lv_display_add_event_cb(disp, on_display_event, LV_EVENT_RENDER_READY, nullptr);
  }
  if (indev != nullptr) {
    lv_timer_t *t = lv_indev_get_read_timer(indev);
    if (t != nullptr) {
      s_indevCbOrig = lv_indev_read_timer_cb;
      lv_timer_set_cb(t, indev_timer_wrapper);
    }
  }
  Serial.printf("MET RESET reason=%s\n", reset_reason());
  Serial.println("[MET] metrics build: MET lines, [LINK<] echo, REBOOT/BOOTLOG/HEAPCHK");
}

void met_flush(uint32_t px) { s_px += px; }

void met_after_timer_handler() {
  // The first handler pass that rendered a frame also ran the indev timer,
  // so the UI is drawn and touch input is live from here on.
  if (s_firstFrame) met_boot("touch_ready");
}

// --- link --------------------------------------------------------------------

static uint32_t s_lnTotal = 0, s_lnHello = 0, s_lnLoginReq = 0, s_lnResult = 0;
static uint32_t s_lnNet = 0, s_lnDb = 0, s_lnSens = 0, s_lnLog = 0, s_lnOther = 0;
static uint32_t s_lnGap = 0, s_lnOvf = 0;
static uint32_t s_lastNetMs = 0, s_netGapMaxMs = 0;
static uint32_t s_lastDbMs = 0, s_dbGapMaxMs = 0;

static void gap_track(uint32_t &last, uint32_t &maxGap) {
  const uint32_t now = millis();
  if (last != 0 && now - last > maxGap) maxGap = now - last;
  last = now;
}

static bool word_is(const char *line, const char *word) {
  const size_t n = strlen(word);
  return strncmp(line, word, n) == 0 && (line[n] == ' ' || line[n] == '\0');
}

void met_link_line(const char *line) {
  s_lnTotal++;
  met_boot("w32_first_line");
  if (word_is(line, "LOG")) {
    s_lnLog++;
    return;  // already printed as [W32] by the link
  }
  Serial.printf("[LINK<] %s\n", line);
  if (word_is(line, "HELLO")) {
    s_lnHello++;
    met_boot("w32_hello");
  } else if (word_is(line, "LOGINREQ")) {
    s_lnLoginReq++;
    met_boot("loginreq");
  } else if (word_is(line, "RESULT")) {
    s_lnResult++;
    if (strstr(line, " OK") != nullptr) met_boot("login_ok");
  } else if (word_is(line, "NET")) {
    s_lnNet++;
    gap_track(s_lastNetMs, s_netGapMaxMs);
    if (strstr(line, " up=1") != nullptr) met_boot("net_up");
  } else if (word_is(line, "DB")) {
    s_lnDb++;
    gap_track(s_lastDbMs, s_dbGapMaxMs);
    met_boot("db_first");
    if (strstr(line, " ready=1") != nullptr) met_boot("db_ready");
  } else if (word_is(line, "SENS")) {
    s_lnSens++;
  } else {
    s_lnOther++;
  }
}

void met_link_discard(bool overflow) {
  if (overflow) {
    s_lnOvf++;
  } else {
    s_lnGap++;
  }
}

void met_sens_rx(const char *args) {
  s_sens.rxArmed = true;
  s_sens.rxUs = micros();
  s_sens.wseq = 0;
  if (args != nullptr && strncmp(args, "seq=", 4) == 0) {
    s_sens.wseq = (uint32_t)strtoul(args + 4, nullptr, 10);
  }
}

void met_sens_commit(uint32_t localSeq) {
  s_sens.localSeq = localSeq;
  met_boot("sens_first");
}

void met_sens_applied(uint32_t localSeq) {
  if (!s_sens.rxArmed || localSeq != s_sens.localSeq) return;
  s_sens.rxArmed = false;
  s_sens.applyArmed = true;
  s_sens.applyUs = micros();
}

// --- hold to confirm ---------------------------------------------------------

void met_hold(const char *event, uint32_t heldMs) {
  Serial.printf("MET HOLD event=%s held_ms=%u route=%s\n", event, (unsigned)heldMs,
                ui_route_name());
}

// --- periodic output ---------------------------------------------------------

static uint32_t s_periodMs = 0;

void met_tick() {
  const uint32_t nowUs = micros();
  if (s_touch.armed) {
    const uint32_t age = nowUs - s_touch.t0Us;
    if (!s_touch.inv && age > kNoFrameUs) finish_touch(true, false, nowUs);
    else if (s_touch.inv && age > kStuckUs) finish_touch(false, true, nowUs);
  }
  if (s_sens.applyArmed && nowUs - s_sens.applyUs > kSensFrameUs) push_sens(true, nowUs);

  while (s_resCount > 0) {
    const TouchResult &r = s_results[s_resHead];
    Serial.printf(
        "MET TOUCH edge=%s lat_us=%u inv_us=%u render_us=%u nf=%d stuck=%d wake=%d "
        "from=%s to=%s x=%d y=%d\n",
        r.down ? "down" : "up", (unsigned)r.latUs, (unsigned)r.invUs, (unsigned)r.renderUs,
        r.nf ? 1 : 0, r.stuck ? 1 : 0, r.wake ? 1 : 0, r.from, r.to, r.x, r.y);
    s_resHead = (s_resHead + 1) % (sizeof(s_results) / sizeof(s_results[0]));
    s_resCount--;
  }
  for (uint8_t i = 0; i < s_sensResCount; i++) {
    const SensResult &r = s_sensRes[i];
    Serial.printf("MET SENSRX wseq=%u seq=%u apply_us=%u render_us=%u noframe=%d route=%s\n",
                  (unsigned)r.wseq, (unsigned)r.localSeq, (unsigned)r.applyUs,
                  (unsigned)r.renderUs, r.noframe ? 1 : 0, ui_route_name());
  }
  s_sensResCount = 0;

  const uint32_t nowMs = millis();
  if (s_fpsWindowMs == 0) s_fpsWindowMs = nowMs;
  if (nowMs - s_fpsWindowMs >= kFpsPeriodMs) {
    const uint32_t window = nowMs - s_fpsWindowMs;
    if (s_frames > 0) {
      Serial.printf(
          "MET FPS window_ms=%u frames=%u avg_render_us=%u max_render_us=%u px=%llu "
          "idle_pct=%u\n",
          (unsigned)window, (unsigned)s_frames, (unsigned)(s_renderSumUs / s_frames),
          (unsigned)s_renderMaxUs, (unsigned long long)s_px, (unsigned)lv_timer_get_idle());
    }
    s_fpsWindowMs = nowMs;
    s_frames = 0;
    s_renderSumUs = 0;
    s_renderMaxUs = 0;
    s_px = 0;
  }

  if (s_periodMs == 0) s_periodMs = nowMs;
  if (nowMs - s_periodMs < kPeriodMs) return;
  const uint32_t window = nowMs - s_periodMs;
  s_periodMs = nowMs;
  // O(1) counters only: ESP.getHeapSize()-style heap walks hang the board when
  // the heap is corrupted (docs/plans-and-audits/display-info-reboot.md).
  // getMaxAllocHeap()/getMaxAllocPsram() are heap walks too (heap_caps_get_info
  // -> tlsf_walk_pool), so they are deliberately absent; a 60 s sampler must
  // not be able to trip the interrupt watchdog mid-run.
  Serial.printf(
      "MET MEM uptime_s=%u heap_free=%u heap_min=%u psram_free=%u psram_min=%u\n",
      (unsigned)(nowMs / 1000u), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
      (unsigned)ESP.getFreePsram(), (unsigned)ESP.getMinFreePsram());
  Serial.printf(
      "MET LINK window_ms=%u rx=%u hello=%u loginreq=%u result=%u net=%u db=%u sens=%u "
      "log=%u other=%u gap=%u ovf=%u net_gap_max_ms=%u db_gap_max_ms=%u touch_drop=%u\n",
      (unsigned)window, (unsigned)s_lnTotal, (unsigned)s_lnHello, (unsigned)s_lnLoginReq,
      (unsigned)s_lnResult, (unsigned)s_lnNet, (unsigned)s_lnDb, (unsigned)s_lnSens,
      (unsigned)s_lnLog, (unsigned)s_lnOther, (unsigned)s_lnGap, (unsigned)s_lnOvf,
      (unsigned)s_netGapMaxMs, (unsigned)s_dbGapMaxMs, (unsigned)s_resDropped);
  s_lnTotal = s_lnHello = s_lnLoginReq = s_lnResult = s_lnNet = s_lnDb = s_lnSens = 0;
  s_lnLog = s_lnOther = s_lnGap = s_lnOvf = 0;
  s_netGapMaxMs = s_dbGapMaxMs = 0;
  s_resDropped = 0;
}

// --- console -----------------------------------------------------------------

bool met_console(const char *cmd) {
  if (strcmp(cmd, "REBOOT") == 0) {
    Serial.println("MET REBOOT requested");
    Serial.flush();
    delay(50);
    ESP.restart();
    return true;
  }
  if (strcmp(cmd, "BOOTLOG") == 0) {
    Serial.printf("MET RESET reason=%s replay=1\n", reset_reason());
    for (size_t i = 0; i < s_bootN; i++) {
      Serial.printf("MET BOOT stage=%s ms=%u replay=1\n", s_boot[i].stage,
                    (unsigned)s_boot[i].ms);
    }
    return true;
  }
  if (strcmp(cmd, "HEAPCHK") == 0) {
    // Walks every heap block. On a corrupted heap this can hang or reset the
    // board, which is itself the finding (display-info-reboot.md).
    const uint32_t t0 = millis();
    const bool ok = heap_caps_check_integrity_all(true);
    Serial.printf("MET HEAPCHK ok=%d ms=%u\n", ok ? 1 : 0, (unsigned)(millis() - t0));
    return true;
  }
  return false;
}

#endif  // RUSIM_METRICS
