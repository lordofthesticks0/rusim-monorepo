#pragma once

// Persistent shell: status bar (nav tabs, link chip, bell), content router,
// a single blocking-modal primitive, and the styled widget helpers every
// screen builds from.
// NOTE: no LVGL theme is enabled (LV_USE_THEME_DEFAULT 0), so every widget is
// styled explicitly through the ui_mk_* helpers below.
//
// Rendering rule: screens build their widgets once per route change and then
// only update text/colours in place (screen_*_refresh). New sensor frames
// never delete widgets, so a hold or press in progress is never cancelled.

#include <lvgl.h>

void ui_init();
// Call each loop(): boot run adoption from the WT32, the experiment-number
// suggestion, and the status bar (link chip, bell).
void ui_poll();

// Routes (mirror the design-note screen tree).
void ui_show_home();
void ui_show_data_picker();
void ui_show_cluster(int c);
void ui_show_bottle(int c, int b);
void ui_show_control();
void ui_show_info();
// First-class setup route (QR to the WT32 login page). Shown first on boot,
// and on demand when the WT32 asks for a new login later.
void ui_show_setup();
// Back to the route that was open before /setup (re-login), else /home.
void ui_show_previous();
bool ui_is_setup();
// Current route as a short path ("/home", "/control", ...), for logs.
const char *ui_route_name();
bool ui_is_control();
bool ui_is_info();

// Update the current route in place (after real frames / USB edits).
void ui_refresh_current();
// Recompute the bell from alert fields and repaint it.
void ui_update_bell();
// Repaint the nav tabs to reflect the current route.
void ui_update_nav();
// Repaint the link chip and the bell.
void ui_update_status_bar();
// Called after any data mutation: bell + content refresh.
void ui_shell_on_data();
// Flip experiment running state (shared by /home, /control, and USB RUN).
void ui_experiment_toggle();
// 41C warn / 45C trip evaluation against g.chamberTempC.
void ui_eval_overheat();

// Blocking modal. Only one at a time. A new modal replaces the current one
// only if it is at least as severe, so an alarm is never hidden by a notice.
// Showing a modal also turns the backlight on.
enum UiSeverity { UI_SEV_INFO = 0, UI_SEV_WARN = 1, UI_SEV_ALARM = 2 };
typedef void (*UiAckFn)();
void ui_modal_show(const char *title, const char *body, const char *ackLabel, UiAckFn onAck);
void ui_modal_show_sev(const char *title, const char *body, const char *ackLabel, UiAckFn onAck,
                       UiSeverity sev);
void ui_modal_hide();
bool ui_modal_visible();

// --- Styled widget helpers (no theme). All screens must use these. ---------

lv_obj_t *ui_mk_label(lv_obj_t *parent, const char *text, int x, int y, int w,
                      uint32_t color, const lv_font_t *font);
// Button with pressed and disabled looks. Text colour is set on the button
// (inherited by its label): accent on a white button, white on a filled one.
lv_obj_t *ui_mk_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       uint32_t bg);
// White card, 1 px neutral border, 12 px padding, not scrollable.
lv_obj_t *ui_mk_card(lv_obj_t *parent, int x, int y, int w, int h);
// Card caption: 14 px, dim, written in UPPERCASE by the caller.
lv_obj_t *ui_mk_caption(lv_obj_t *parent, const char *text, int x, int y, int w);
// Page title row: optional back button at the top-left, then the 20 px title.
// Returns the title label (content width, for aligning secondary text).
lv_obj_t *ui_mk_page_title(lv_obj_t *parent, const char *title, lv_event_cb_t backCb,
                           void *user);
// Stepper button events: one step per short tap, then auto-repeat while held
// (first repeat after 400 ms, then every 100 ms). cb gets SHORT_CLICKED,
// LONG_PRESSED and LONG_PRESSED_REPEAT.
void ui_stepper_events(lv_obj_t *btn, lv_event_cb_t cb, void *user);
// Step multiplier for an accelerating stepper: 1, or 10 after ~2 s of repeat.
int ui_stepper_mult(lv_event_t *e);

// --- Experiment helpers shared by /home and /control -----------------------

// One-line description of what the next start (or the running run) uses.
// Writes the text and returns its colour.
uint32_t ui_exp_status_text(char *out, size_t n);
// Duration that applies: the frozen run duration while running, the DB value
// for the selected id when one exists, else the local stepper value.
int ui_run_duration_h();
// Operator picked an experiment number: store, mark as user-set, mirror.
void ui_exp_set_user(int expNum);
