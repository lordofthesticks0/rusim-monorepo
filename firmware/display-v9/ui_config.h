#pragma once

// Display UI compile-time constants (fake-data pass).
// Screen: Sunton 800x480, ST7262, RGB565, LVGL v9.

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
#define UI_SLEEP_MS 30000

// Fake WT32 push cadence (5 min). Fires an lv_timer that jitters fake values.
#define UI_PUSH_PERIOD_MS (5UL * 60UL * 1000UL)

// Overheat thresholds (fake chamber temp, settable over serial).
#define UI_OVERHEAT_WARN_C 41.0f
#define UI_OVERHEAT_TRIP_C 45.0f

#define FW_VERSION "v9-fake-0.1.0"

// Palette (dark background, matches earlier demo card).
#define UI_COL_BG 0x000000
#define UI_COL_CARD 0x202A44
#define UI_COL_ACCENT 0x55C7E8
#define UI_COL_OK 0x2EB85C
#define UI_COL_WARN 0xE8A13D
#define UI_COL_ALARM 0xE84C4C
#define UI_COL_DIM 0x8A93A6
#define UI_COL_WHITE 0xFFFFFF
