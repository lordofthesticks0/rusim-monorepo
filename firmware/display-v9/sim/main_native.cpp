// PC simulator entry point for the display-v9 UI. Same fake-data model and
// serial command protocol as firmware/display-v9/main.cpp, but rendered
// through LVGL's SDL driver instead of the ST7262 + GT911.

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <lvgl.h>

#include "fake_data.h"
#include "link_serial.h"
#include "serial_cmd.h"
#include "screen_control.h"
#include "screen_home.h"
#include "screen_info.h"
#include "screen_setup.h"
#include "screen_sleep.h"
#include "ui_config.h"
#include "ui_shell.h"

// --- SIM_SHOTS screenshot mode ---------------------------------------------
// SIM_SHOTS=<dir> renders each route once, writes <dir>/<name>.bmp and exits.
// Works headless with SDL_VIDEODRIVER=dummy. Uses lv_snapshot (enabled in
// sim/lv_conf.h only).

static void settle() {
  for (int i = 0; i < 5; i++) {
    lv_timer_handler();
    SDL_Delay(5);
  }
  lv_refr_now(nullptr);
}

static void shot(const char *dir, const char *name) {
  settle();
  lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
  if (buf == nullptr) {
    printf("[SHOT] %s: snapshot failed\n", name);
    return;
  }
  const int w = buf->header.w;
  const int h = buf->header.h;
  const int stride = buf->header.stride;
  const int rowBytes = w * 3;
  const int pad = (4 - rowBytes % 4) % 4;
  const unsigned dataSize = (unsigned)(rowBytes + pad) * (unsigned)h;
  char path[512];
  snprintf(path, sizeof(path), "%s/%s.bmp", dir, name);
  FILE *f = fopen(path, "wb");
  if (f != nullptr) {
    unsigned char hdr[54] = {'B', 'M'};
    const unsigned fileSize = 54 + dataSize;
    for (int i = 0; i < 4; i++) hdr[2 + i] = (unsigned char)(fileSize >> (8 * i));
    hdr[10] = 54;
    hdr[14] = 40;
    for (int i = 0; i < 4; i++) hdr[18 + i] = (unsigned char)((unsigned)w >> (8 * i));
    for (int i = 0; i < 4; i++) hdr[22 + i] = (unsigned char)((unsigned)h >> (8 * i));
    hdr[26] = 1;
    hdr[28] = 24;
    for (int i = 0; i < 4; i++) hdr[34 + i] = (unsigned char)(dataSize >> (8 * i));
    fwrite(hdr, 1, sizeof(hdr), f);
    const unsigned char zeros[3] = {0, 0, 0};
    // LVGL RGB888 is stored B, G, R: the same byte order as a 24-bit BMP.
    for (int y = h - 1; y >= 0; y--) {
      fwrite(buf->data + (size_t)y * (size_t)stride, 1, (size_t)rowBytes, f);
      fwrite(zeros, 1, (size_t)pad, f);
    }
    fclose(f);
    printf("[SHOT] %s\n", path);
  }
  lv_draw_buf_destroy(buf);
}

static void run_shots(const char *dir) {
  const bool loggedIn = g.loggedIn;
  g.loggedIn = false;
  ui_update_nav();
  ui_show_setup();
  shot(dir, "01-setup");
  g.loggedIn = loggedIn;
  ui_show_home();
  shot(dir, "02-home");
  ui_show_data_picker();
  shot(dir, "03-data-picker");
  ui_show_cluster(0);
  shot(dir, "04-node");
  ui_show_bottle(0, 0);
  shot(dir, "05-bottle");
  ui_show_bottle(0, 3);
  shot(dir, "06-bottle-missing");
  ui_show_control();
  shot(dir, "07-control");
  ui_show_info();
  shot(dir, "08-info");
  ui_show_control();
  ui_experiment_toggle();  // the stub DB is never ready -> start-blocked modal
  shot(dir, "09-modal");
  ui_modal_hide();
  g.experimentRunning = true;
  g.experimentStartMs = millis() - 9UL * 3600000UL;
  ui_show_home();
  shot(dir, "10-home-running");
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  link_serial_init();
  setvbuf(stdout, nullptr, _IONBF, 0);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  lv_init();  // the SDL driver installs its own tick callback and event pump

  lv_display_t *disp = lv_sdl_window_create(800, 480);
  lv_sdl_window_set_title(disp, "rusim display-v9 sim");
  lv_sdl_mouse_create();
  lv_sdl_keyboard_create();

  fake_init();
  link_serial_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  // TEMP PREVIEW INJECTION (sim only, removed after review): gives the screens
  // realistic numbers while the sim has no WT32 sensor feed.
  g.loggedIn = true;
  g.cluster[0].online = true;
  for (int b = 0; b < UI_BOTTLES_PER_CLUSTER; b++) {
    BottleFake &bt = g.cluster[0].bottle[b];
    bt.present = (b != 3);
    const float base[S_COUNT] = {1180.0f, 93.0f, 1.04f, 6.82f, 37.2f};
    for (int s = 0; s < S_COUNT; s++) {
      bt.sensor[s] = base[s] + b;
      bt.sensorNull[s] = !bt.present;
    }
    const float amp[S_COUNT] = {40.0f, 6.0f, 0.15f, 0.10f, 0.60f};
    for (int s = 0; s < S_COUNT; s++)
      for (int i = 0; i < UI_TREND_PTS; i++)
        bt.trend[s][i] = base[s] + b + amp[s] * sinf((float)i * 0.55f + (float)s) +
                         amp[s] * 0.02f * (float)i;
  }
  printf("[SIM] TEMP preview data injected (sim build only)\n");
  ui_init();
  ui_poll();  // releases the boot gate (the stub reports one first DB line)

  const char *shotDir = getenv("SIM_SHOTS");
  if (shotDir != nullptr && shotDir[0] != '\0') {
    run_shots(shotDir);
    SDL_Quit();
    return 0;
  }

  printf("display-v9 sim. Type serial commands here (PUSH, TEMP 42, STATUS, ...; HELP for list).\n");

  for (;;) {
    serial_cmd_poll();
    link_serial_poll();
    screen_setup_poll();
    screen_home_poll();
    screen_control_poll();
    screen_info_poll();
    ui_poll();
    sleep_tick();
    lv_timer_handler();
    SDL_Delay(5);
  }

  SDL_Quit();
  return 0;
}
