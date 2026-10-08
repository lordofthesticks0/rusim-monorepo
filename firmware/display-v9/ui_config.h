#pragma once

// Display UI compile-time constants (fake-data pass).
// Screen: Sunton 800x480, ST7262, RGB565, LVGL v9.6.

// Topology: 6 cluster nodes x 4 bottles = 24 bottles.
// Only the first 2 nodes are online; the rest are "not connected" (fake).
#define UI_CLUSTERS 6
#define UI_BOTTLES_PER_CLUSTER 4
#define UI_ONLINE_CLUSTERS 2
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
#define UI_HOLD_TO_CONFIRM_MS 500

// Screen sleep: backlight off after inactivity. NOTE: design notes say GPIO45,
// but GPIO45 is R0 of the RGB panel bus in main.cpp, so this pass keeps the
// proven TFT_BL (pin 2). Revisit with EE before moving the backlight pin.
#define UI_BL_PIN 2
#define UI_SLEEP_MS 60000

// Fake WT32 push cadence (5 min). Fires an lv_timer that jitters fake values.
#define UI_PUSH_PERIOD_MS (5UL * 60UL * 1000UL)

// Overheat thresholds (fake chamber temp, settable over serial).
#define UI_OVERHEAT_WARN_C 41.0f
#define UI_OVERHEAT_TRIP_C 45.0f

#define FW_VERSION "v9.6-fake-0.2.0"

// Debug: general debug-build gate. Hardcoded default OFF; enable per-build
// with -DIS_DEBUG=1 (e.g. extra build_flags). When on, /setup shows a
// "DEBUG: bypass login" button that routes straight to /home.
#ifndef IS_DEBUG
#define IS_DEBUG 0
#endif

// Palette (light background). UI_COL_TEXT is the primary foreground, so it is
// near-black here; UI_COL_CARD doubles as the button fill and is white.
#define UI_COL_BG 0xEEF2F7
#define UI_COL_CARD 0xFFFFFF
#define UI_COL_ACCENT 0x2C6E9B
#define UI_COL_OK 0x1E8E4E
#define UI_COL_WARN 0xB87314
#define UI_COL_ALARM 0xC0392B
#define UI_COL_DIM 0x5B6472
#define UI_COL_TEXT 0x1B2330
// Fill for the raised status bar, which must read as separate from UI_COL_BG.
#define UI_COL_BAR 0xDCE3EC
// Fill for a button that is present but not usable (offline node).
#define UI_COL_CARD_OFF 0xD5DBE3
