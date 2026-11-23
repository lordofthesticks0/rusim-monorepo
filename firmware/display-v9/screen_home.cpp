#include "screen_home.h"

#include "fake_data.h"
#include "link_serial.h"
#include "ui_config.h"
#include "ui_hold_button.h"
#include "ui_shell.h"

// Layout (content coordinates, 16 px grid):
//   title row "Overview"
//   left card  (16, 56, 376 x 360): run progress, start/stop, duration
//   right card (408, 56, 376 x 360): status rows
// Widgets are built once in show(); refresh() only changes text and colours.

// Run card.
static lv_obj_t *s_runState = nullptr;
static lv_obj_t *s_arc = nullptr;
static lv_obj_t *s_arcBig = nullptr;
static lv_obj_t *s_arcSub = nullptr;
static lv_obj_t *s_btnRun = nullptr;
static lv_obj_t *s_runHint = nullptr;
static lv_obj_t *s_btnDurMinus = nullptr;
static lv_obj_t *s_btnDurPlus = nullptr;
static lv_obj_t *s_durValue = nullptr;
static lv_obj_t *s_durNote = nullptr;
static lv_obj_t *s_expLine = nullptr;

// Status card: one value label (right-aligned, so only its left edge moves
// when the number changes) and one sub line per row.
enum { ROW_BOTTLES = 0, ROW_ALERTS, ROW_SETPOINT, ROW_CHAMBER, ROW_STIRRER, ROW_PUMP, ROW_COUNT };
static lv_obj_t *s_rowValue[ROW_COUNT] = {};
static lv_obj_t *s_rowSub[ROW_COUNT] = {};

static uint32_t s_lastPollMs = 0;

static int bottles_online() {
  int n = 0;
  for (int c = 0; c < UI_CLUSTERS; c++) {
    if (!g.cluster[c].online) continue;
    for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
      if (g.cluster[c].bottle[b].present) n++;
    }
  }
  return n;
}

static int nodes_online() {
  int n = 0;
  for (int c = 0; c < UI_CLUSTERS; c++) {
    if (g.cluster[c].online) n++;
  }
  return n;
}

static void set_row(int row, const char *value, uint32_t valueCol, const char *sub) {
  if (s_rowValue[row] == nullptr) return;
  lv_label_set_text(s_rowValue[row], value);
  lv_obj_set_style_text_color(s_rowValue[row], lv_color_hex(valueCol), LV_PART_MAIN);
  if (sub != nullptr) lv_label_set_text(s_rowSub[row], sub);
}

static void render_status() {
  char v[32];
  char sub[48];
  snprintf(v, sizeof(v), "%d / %d", bottles_online(), UI_CLUSTERS * UI_BOTTLES_PER_CLUSTER);
  snprintf(sub, sizeof(sub), "%d of %d nodes connected", nodes_online(), UI_CLUSTERS);
  set_row(ROW_BOTTLES, v, UI_COL_TEXT, sub);

  const int alerts = fake_alarm_count();
  if (alerts > 0) {
    snprintf(v, sizeof(v), "%d missing", alerts);
    set_row(ROW_ALERTS, v, UI_COL_ALARM, "Tap the bell for the list");
  } else {
    set_row(ROW_ALERTS, "None", UI_COL_OK, "Connected bottles only");
  }

  snprintf(v, sizeof(v), "%.1f " UI_DEG_C, g.tempSetpointC);
  set_row(ROW_SETPOINT, v, UI_COL_TEXT, nullptr);

  if (isfinite(g.chamberTempC)) {
    snprintf(v, sizeof(v), "%.1f " UI_DEG_C, g.chamberTempC);
    set_row(ROW_CHAMBER, v, UI_COL_TEXT, "Chamber sensor");
  } else {
    set_row(ROW_CHAMBER, "--", UI_COL_DIM, "No chamber sensor connected");
  }

  set_row(ROW_STIRRER, g.stirrerOn ? "On" : "Off", g.stirrerOn ? UI_COL_OK : UI_COL_TEXT, nullptr);
  set_row(ROW_PUMP, g.pumpOn ? "On" : "Off", g.pumpOn ? UI_COL_OK : UI_COL_TEXT, nullptr);
}

static float elapsed_hours() {
  if (!g.experimentRunning || g.runStartUnknown || g.experimentStartMs == 0) return 0.0f;
  return (float)(millis() - g.experimentStartMs) / 3600000.0f;
}

static void render_run() {
  if (s_arc == nullptr) return;
  const int durH = ui_run_duration_h();
  char big[16];
  char sub[48];
  int pct = 0;
  const char *state;
  uint32_t stateCol = UI_COL_TEXT;
  const char *hint;
  uint32_t arcCol = UI_COL_ACCENT;

  if (!g.experimentRunning) {
    state = "Idle";
    snprintf(big, sizeof(big), "%d h", durH);
    snprintf(sub, sizeof(sub), "planned");
    hint = "Hold 2 s to start";
  } else if (g.runStartUnknown) {
    state = "Running";
    stateCol = UI_COL_OK;
    snprintf(big, sizeof(big), "--");
    // Adopted from the WT32 after a display reboot: no start time is known.
    snprintf(sub, sizeof(sub), "elapsed unknown");
    hint = "Hold 2 s to stop";
  } else {
    const float el = elapsed_hours();
    pct = durH > 0 ? (int)(el / (float)durH * 100.0f) : 0;
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    snprintf(big, sizeof(big), "%d%%", pct);
    snprintf(sub, sizeof(sub), "%.1f / %d h", el, durH);
    if (el >= (float)durH) {
      state = "Complete";
      stateCol = UI_COL_OK;
      arcCol = UI_COL_OK;
      hint = "Duration reached. Hold 2 s to stop";
    } else {
      state = "Running";
      stateCol = UI_COL_OK;
      hint = "Hold 2 s to stop";
    }
  }
  lv_label_set_text(s_runState, state);
  lv_obj_set_style_text_color(s_runState, lv_color_hex(stateCol), LV_PART_MAIN);
  lv_arc_set_value(s_arc, pct);
  lv_obj_set_style_arc_color(s_arc, lv_color_hex(arcCol), LV_PART_INDICATOR);
  lv_label_set_text(s_arcBig, big);
  lv_label_set_text(s_arcSub, sub);
  lv_label_set_text(s_runHint, hint);
  ui_hold_button_set_symbol(s_btnRun, g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY);
  ui_hold_button_set_state(s_btnRun, g.experimentRunning);

  // Duration is frozen while a run is active (the run keeps the value it
  // started with), so the steppers are disabled rather than silently ignored.
  char d[16];
  snprintf(d, sizeof(d), "%d h", g.durationH);
  lv_label_set_text(s_durValue, d);
  if (g.experimentRunning) {
    lv_obj_add_state(s_btnDurMinus, LV_STATE_DISABLED);
    lv_obj_add_state(s_btnDurPlus, LV_STATE_DISABLED);
    lv_label_set_text(s_durNote, "Locked while running");
  } else {
    lv_obj_remove_state(s_btnDurMinus, LV_STATE_DISABLED);
    lv_obj_remove_state(s_btnDurPlus, LV_STATE_DISABLED);
    lv_label_set_text(s_durNote, "Used if the DB has none");
  }

  char line[64];
  const uint32_t col = ui_exp_status_text(line, sizeof(line));
  lv_label_set_text(s_expLine, line);
  lv_obj_set_style_text_color(s_expLine, lv_color_hex(col), LV_PART_MAIN);
}

static void on_duration(lv_event_t *e) {
  const int dir = (int)(intptr_t)lv_event_get_user_data(e);
  g.durationH += dir;
  if (g.durationH < UI_DURATION_MIN_H) g.durationH = UI_DURATION_MIN_H;
  if (g.durationH > UI_DURATION_MAX_H) g.durationH = UI_DURATION_MAX_H;
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  render_run();
}

static void on_run_commit(void *user) {
  (void)user;
  ui_experiment_toggle();  // refreshes this route in place
  render_run();
}

static void mk_row(lv_obj_t *card, int row, const char *label) {
  const int y = 30 + row * 50;
  ui_mk_label(card, label, 0, y + 2, 170, UI_COL_TEXT, &lv_font_montserrat_14);
  s_rowValue[row] = ui_mk_label(card, "", 150, y - 2, 200, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(s_rowValue[row], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  s_rowSub[row] = ui_mk_label(card, "", 0, y + 22, 350, UI_COL_DIM, &lv_font_montserrat_14);
}

void screen_home_show(lv_obj_t *parent) {
  ui_mk_page_title(parent, "Overview", nullptr, nullptr);

  // --- Run card (inner area 350 x 334) ---
  lv_obj_t *run = ui_mk_card(parent, UI_MARGIN, UI_PAGE_TOP, 376, 360);
  ui_mk_caption(run, "EXPERIMENT RUN", 0, 0, 350);
  s_runState = ui_mk_label(run, "", 0, 22, 350, UI_COL_TEXT, &lv_font_montserrat_20);

  s_arc = lv_arc_create(run);
  lv_obj_set_pos(s_arc, 0, 58);
  lv_obj_set_size(s_arc, 150, 150);
  lv_arc_set_range(s_arc, 0, 100);
  lv_arc_set_value(s_arc, 0);
  lv_obj_remove_style(s_arc, nullptr, LV_PART_KNOB);
  lv_obj_set_clickable(s_arc, false);  // gauge only, not a control
  lv_obj_set_style_arc_color(s_arc, lv_color_hex(UI_COL_BAR), LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_arc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_arc, 12, LV_PART_INDICATOR);
  s_arcBig = ui_mk_label(run, "", 0, 0, 120, UI_COL_TEXT, &lv_font_montserrat_40);
  lv_obj_set_style_text_align(s_arcBig, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align_to(s_arcBig, s_arc, LV_ALIGN_CENTER, 0, -10);
  s_arcSub = ui_mk_label(run, "", 0, 0, 120, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_arcSub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align_to(s_arcSub, s_arc, LV_ALIGN_CENTER, 0, 24);

  s_btnRun = ui_hold_button_create(run, 214, 66, 96,
                                   g.experimentRunning ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY,
                                   g.experimentRunning, on_run_commit, nullptr);
  s_runHint = ui_mk_label(run, "", 172, 178, 178, UI_COL_DIM, &lv_font_montserrat_14);
  lv_obj_set_style_text_align(s_runHint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  ui_mk_caption(run, "DURATION", 0, 226, 160);
  s_btnDurMinus = ui_mk_button(run, LV_SYMBOL_MINUS, 0, 248, 56, 52, UI_COL_CARD);
  ui_stepper_events(s_btnDurMinus, on_duration, (void *)(intptr_t)-1);
  s_durValue = ui_mk_label(run, "", 60, 262, 80, UI_COL_TEXT, &lv_font_montserrat_20);
  lv_obj_set_style_text_align(s_durValue, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  s_btnDurPlus = ui_mk_button(run, LV_SYMBOL_PLUS, 144, 248, 56, 52, UI_COL_CARD);
  ui_stepper_events(s_btnDurPlus, on_duration, (void *)(intptr_t)1);
  s_durNote = ui_mk_label(run, "", 212, 256, 138, UI_COL_DIM, &lv_font_montserrat_14);

  s_expLine = ui_mk_label(run, "", 0, 314, 350, UI_COL_DIM, &lv_font_montserrat_14);

  // --- Status card ---
  lv_obj_t *st = ui_mk_card(parent, 408, UI_PAGE_TOP, 376, 360);
  ui_mk_caption(st, "STATUS", 0, 0, 350);
  mk_row(st, ROW_BOTTLES, "Bottles online");
  mk_row(st, ROW_ALERTS, "Sensor alerts");
  mk_row(st, ROW_SETPOINT, "Temperature setpoint");
  mk_row(st, ROW_CHAMBER, "Chamber temperature");
  mk_row(st, ROW_STIRRER, "Stirrers");
  mk_row(st, ROW_PUMP, "Water pump");
  lv_label_set_text(s_rowSub[ROW_SETPOINT], "Display only, not sent to the rig yet");
  lv_label_set_text(s_rowSub[ROW_STIRRER], "Display only, not sent to the rig yet");
  lv_label_set_text(s_rowSub[ROW_PUMP], "Display only, not sent to the rig yet");

  screen_home_refresh();
  s_lastPollMs = millis();
}

void screen_home_refresh() {
  if (s_arc == nullptr) return;
  render_status();
  render_run();
}

void screen_home_hide() {
  s_runState = nullptr;
  s_arc = nullptr;
  s_arcBig = nullptr;
  s_arcSub = nullptr;
  s_btnRun = nullptr;
  s_runHint = nullptr;
  s_btnDurMinus = nullptr;
  s_btnDurPlus = nullptr;
  s_durValue = nullptr;
  s_durNote = nullptr;
  s_expLine = nullptr;
  for (int i = 0; i < ROW_COUNT; i++) {
    s_rowValue[i] = nullptr;
    s_rowSub[i] = nullptr;
  }
  s_lastPollMs = 0;
}

void screen_home_poll() {
  if (s_arc == nullptr) return;
  const uint32_t now = millis();
  if (now - s_lastPollMs < 1000) return;
  s_lastPollMs = now;
  screen_home_refresh();
}
