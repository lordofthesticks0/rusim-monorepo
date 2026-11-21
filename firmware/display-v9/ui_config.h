#pragma once

// Display UI compile-time constants.
// Screen: Sunton 800x480, ST7262, RGB565, LVGL v9.6.

// Capacity: 6 cluster nodes x 4 bottles = 24 bottles.
// Nodes and bottles become online only when reported by the WT32.
#define UI_CLUSTERS 6
#define UI_BOTTLES_PER_CLUSTER 4

#define UI_SENSORS_PER_BOTTLE 5
#define UI_TREND_PTS 24

// Start-experiment gate: duration entry, whole hours.
#define UI_DURATION_MIN_H 1
#define UI_DURATION_MAX_H 99
#define UI_DURATION_DEFAULT_H 24

// Experiment number range (matches the dashboard's integer experiment_id:
// 0 is the testing experiment, 1+ are regular). Editable on /control; later
// the master derives it from the database (latest + 1).
#define UI_EXP_NUM_MIN 0
#define UI_EXP_NUM_MAX 9999
#define UI_EXP_NUM_DEFAULT 0

// Temp setpoint range/step shown on /control (command goes to WT32; fake here).
#define UI_TEMP_MIN_C 35.0f
#define UI_TEMP_MAX_C 40.0f
#define UI_TEMP_DEFAULT_C 37.0f
#define UI_TEMP_STEP_C 0.1f

// Locked interaction: binary toggles require hold-to-confirm.
#define UI_HOLD_TO_CONFIRM_MS 2000

// Screen sleep: backlight off after inactivity. NOTE: design notes say GPIO45,
// but GPIO45 is R0 of the RGB panel bus in main.cpp, so this pass keeps the
// proven TFT_BL (pin 2). Revisit with EE before moving the backlight pin.
#define UI_BL_PIN 2
#define UI_SLEEP_MS 60000


// Real SENS frames land at the WT32's ~5 s poll cadence; this apply timer
// copies new frames into the model and refreshes labels, so a frame reaches
// the screen within about 1 s. It also doubles as the staleness watcher: a
// frame older than UI_SENS_STALE_MS has the whole node shown as NULL/offline
// until the slave answers again.
#define UI_SENS_APPLY_MS 1000
// The trend ring gains one point per UI_TREND_STEP_MS, not per frame, so its
// UI_TREND_PTS points keep covering 12 minutes however fast frames arrive.
#define UI_TREND_STEP_MS 30000UL
#define UI_SENS_STALE_MS (3UL * 30000UL)
// Cluster that receives the legacy Mega rig's four real bottles.
#define UI_REAL_CLUSTER 0

// Overheat thresholds; inactive until a real chamber measurement is available.
#define UI_OVERHEAT_WARN_C 41.0f
#define UI_OVERHEAT_TRIP_C 45.0f

// Degree sign + C. The built-in Montserrat fonts include U+00B0 (but not
// U+00B7, U+2026 or U+2212, so those characters must not be used in UI text).
#define UI_DEG_C "\xC2\xB0" "C"

#define FW_VERSION "v9.6-0.2.0"

// Debug: general debug-build gate. Default ON for now; disable per-build
// with -DIS_DEBUG=0 (e.g. extra build_flags). When on, /setup shows a
// "DEBUG: bypass login" button that routes straight to /home and the USB
// serial console accepts W32CMD.
#ifndef IS_DEBUG
#define IS_DEBUG 1
#endif

// Palette (light background). UI_COL_TEXT is the primary foreground, so it is
// near-black here; UI_COL_CARD doubles as the button fill and is white.
// Colour meaning: UI_COL_ACCENT = tappable or selected, UI_COL_OK = running or
// healthy, UI_COL_WARN = needs attention, UI_COL_ALARM = fault only. Idle and
// off states use UI_COL_IDLE, never red. Text colours pass WCAG 4.5:1 on both
// UI_COL_CARD and UI_COL_BG (docs/plans-and-audits/lvgl-ui-ux-audit.md, A4).
#define UI_COL_BG 0xEEF2F7
#define UI_COL_CARD 0xFFFFFF
#define UI_COL_ACCENT 0x2C6E9B
#define UI_COL_OK 0x17773F
#define UI_COL_WARN 0x8F5A0C
#define UI_COL_ALARM 0xC0392B
#define UI_COL_DIM 0x5B6472
#define UI_COL_TEXT 0x1B2330
// Fill of an idle/off hold button (white icon on it is 6:1).
#define UI_COL_IDLE 0x5B6472
// Status bar fill; a 1 px UI_COL_LINE divider separates it from the page.
#define UI_COL_BAR 0xDCE3EC
// Neutral 1 px border for cards and chart grid lines.
#define UI_COL_LINE 0xC9D1DC
// Fill for a button that is present but not usable (offline node, disabled).
#define UI_COL_CARD_OFF 0xD5DBE3
// Text on a disabled control (deliberately low contrast).
#define UI_COL_DISABLED 0x97A1AE
// Opaque modal backdrop. Opaque, so nothing under it is alpha-blended.
#define UI_COL_SCRIM 0x2A313C

// Layout grid. Status bar on top, content below; every page uses a 16 px
// outer margin, a title row of UI_TITLE_H and cards from UI_PAGE_TOP down to
// UI_PAGE_BOTTOM (content coordinates).
#define UI_BAR_H 48
#define UI_CONTENT_H (480 - UI_BAR_H)
#define UI_MARGIN 16
#define UI_PAGE_TOP 56
#define UI_PAGE_BOTTOM (UI_CONTENT_H - UI_MARGIN)

// Status-bar link chip: the WT32 counts as gone after this much silence
// (it pushes NET/DB/SENS lines at least every few seconds).
#define UI_LINK_SILENT_MS 15000UL
