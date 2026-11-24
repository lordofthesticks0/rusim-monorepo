#pragma once

// Verification instrumentation for display-v9 (docs/verification-toolkit.md).
//
// Built only with -D RUSIM_METRICS=1 ([env:display-v9-metrics]). Every hook
// below is an empty inline function otherwise, so production and the PC
// simulator compile exactly as before.
//
// Output: one "MET <kind> key=value ..." line per event on the USB console,
// plus "[LINK<] <line>" for every inbound rusim-line line except LOG. Results
// measured inside LVGL callbacks are queued and printed from met_tick(), so
// printing never happens inside a render.

#include <stdint.h>

#ifndef RUSIM_METRICS
#define RUSIM_METRICS 0
#endif

// Optional logic-analyser probe: a free GPIO driven HIGH at a touch edge and
// LOW when the frame that answers it has been flushed. -1 = off.
#ifndef RUSIM_MET_GPIO
#define RUSIM_MET_GPIO -1
#endif

#include <lvgl.h>

#if RUSIM_METRICS
// First call in setup(), before Serial.begin(): enlarges the USB TX buffer
// so MET output does not block the UI loop.
void met_init_early();
// Boot milestone, logged once with millis() since the app started.
void met_boot(const char *stage);
// Hooks the display's invalidate/render events and wraps the indev read
// timer so an invalidation can be attributed to touch processing.
void met_attach(lv_display_t *disp, lv_indev_t *indev);
// Every touch read: raw controller state, and whether the sleep filter
// swallowed it (the tap that wakes the backlight).
void met_touch(bool rawPressed, bool swallowed, int x, int y);
// Pixels written by one flush call.
void met_flush(uint32_t px);
// After every lv_timer_handler() call in loop().
void met_after_timer_handler();
// Link receive path: every complete inbound line (before parsing), and every
// discarded partial (gap) or overlong (overflow) line.
void met_link_line(const char *line);
void met_link_discard(bool overflow);
// SENS: arguments before parsing (reads the WT32's seq=), the display's own
// sequence number once accepted, and the moment the UI model consumed it.
void met_sens_rx(const char *args);
void met_sens_commit(uint32_t localSeq);
void met_sens_applied(uint32_t localSeq);
// Hold-to-confirm: "start", "commit" or "cancel" with the elapsed hold time.
void met_hold(const char *event, uint32_t heldMs);
// Once per loop(): prints queued results and the periodic MEM/LINK/FPS lines.
void met_tick();
// USB console commands REBOOT, BOOTLOG, HEAPCHK. Returns true if handled.
bool met_console(const char *cmd);
#else
inline void met_init_early() {}
inline void met_boot(const char *) {}
inline void met_attach(lv_display_t *, lv_indev_t *) {}
inline void met_touch(bool, bool, int, int) {}
inline void met_flush(uint32_t) {}
inline void met_after_timer_handler() {}
inline void met_link_line(const char *) {}
inline void met_link_discard(bool) {}
inline void met_sens_rx(const char *) {}
inline void met_sens_commit(uint32_t) {}
inline void met_sens_applied(uint32_t) {}
inline void met_hold(const char *, uint32_t) {}
inline void met_tick() {}
inline bool met_console(const char *) { return false; }
#endif
