// PC simulator entry point for the display-v9 UI. Same fake-data model and
// serial command protocol as firmware/display-v9/main.cpp, but rendered
// through LVGL's SDL driver instead of the ST7262 + GT911.

#include <SDL2/SDL.h>
#include <math.h>

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

  printf("display-v9 sim. Type serial commands here (PUSH, TEMP 42, STATUS, ...; HELP for list).\n");

  for (;;) {
    serial_cmd_poll();
    link_serial_poll();
    screen_setup_poll();
    screen_home_poll();
    screen_control_poll();
    screen_info_poll();
    ui_update_clock();
    sleep_tick();
    lv_timer_handler();
    SDL_Delay(5);
  }

  SDL_Quit();
  return 0;
}
