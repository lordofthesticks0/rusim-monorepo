// PC simulator entry point for the display-v9 UI. Same fake-data model and
// serial command protocol as firmware/display-v9/main.cpp, but rendered
// through LVGL's SDL driver instead of the ST7262 + GT911.

#include <SDL2/SDL.h>

#include <lvgl.h>

#include "fake_data.h"
#include "link_modbus.h"
#include "serial_cmd.h"
#include "screen_control.h"
#include "screen_setup.h"
#include "screen_sleep.h"
#include "ui_config.h"
#include "ui_shell.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  link_modbus_init();
  int_test_init();
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
  link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  ui_init();

  printf("display-v9 sim. Type serial commands here (PUSH, TEMP 42, STATUS, ...; HELP for list).\n");

  for (;;) {
    serial_cmd_poll();
    link_modbus_poll();
    screen_setup_poll();
    screen_control_poll();
    sleep_tick();
    lv_timer_handler();
    SDL_Delay(5);
  }

  SDL_Quit();
  return 0;
}
