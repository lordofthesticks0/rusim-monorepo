
/*******************************************************************************
 * LVGL Widgets
 * This is a widgets demo for LVGL - Light and Versatile Graphics Library
 * import from: https://github.com/lvgl/lv_demos.git
 *
 * Dependent libraries:
 * LVGL: https://github.com/lvgl/lvgl.git

 * Touch libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 *
 * LVGL v9 uses firmware/display-v9/lv_conf.h. Rendering and the display
 * flush callback are configured explicitly for RGB565 below.
 *
 * Optional: Show CPU usage and FPS count
 * #define LV_USE_PERF_MONITOR 1
 ******************************************************************************/
//#include "lv_demo_widgets.h"
#include <Arduino.h>
#include <lvgl.h>

static uint32_t lvgl_tick_get(void)
{
  return static_cast<uint32_t>(millis());
}

static void drag_event_handler(lv_event_t *event)
{
  lv_obj_t *obj = lv_event_get_target_obj(event);
  lv_indev_t *indev = lv_indev_active();
  if (indev == NULL)
  {
    return;
  }

  lv_point_t movement;
  lv_indev_get_vect(indev, &movement);
  lv_obj_set_pos(obj, lv_obj_get_x_aligned(obj) + movement.x,
                 lv_obj_get_y_aligned(obj) + movement.y);
}

/*******************************************************************************
 * Custom Hello World widget (LVGL v9)
 ******************************************************************************/
static void hello_world_widget(void)
{
  lv_obj_t *scr = lv_screen_active();
  /* Black background */
  lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  /* A centered card that can be dragged with the touch screen. */
  lv_obj_t *card = lv_obj_create(scr);
  lv_obj_set_size(card, 220, 80);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x202A44), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(0x55C7E8), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, drag_event_handler, LV_EVENT_PRESSING, NULL);
  lv_obj_center(card);

  /* White centered "Hello World" text */
  lv_obj_t *label = lv_label_create(card);
  lv_label_set_text(label, "if this is visible you did it");
  lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
  lv_obj_center(label);
}
/*******************************************************************************
 ******************************************************************************/
#include <Arduino_GFX_Library.h>
#define TFT_BL 2
#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

/* More dev device declaration: https://github.com/moononournation/Arduino_GFX/wiki/Dev-Device-Declaration */
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
//Arduino_DataBus *bus = create_default_Arduino_DataBus();

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
//Arduino_GFX *gfx = new Arduino_ILI9341(bus, DF_GFX_RST, 0 /* rotation */, false /* IPS */);

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */
);
// option 1:
// ST7262 IPS LCD 800x480
Arduino_RPi_DPI_RGBPanel *lcd = new Arduino_RPi_DPI_RGBPanel(
   bus,
   800 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
   480 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
   1 /* pclk_active_neg */, 16000000 /* prefer_speed */, true /* auto_flush */);
#endif /* !defined(DISPLAY_DEV_KIT) */
/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

/*******************************************************************************
 * Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"

/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_display_t *display;
static uint8_t *disp_draw_buf;

/* Display flushing */
static void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
  const uint32_t w = area->x2 - area->x1 + 1;
  const uint32_t h = area->y2 - area->y1 + 1;

  lcd->draw16bitRGBBitmap(area->x1, area->y1,
                          reinterpret_cast<uint16_t *>(px_map), w, h);
  lv_display_flush_ready(display);
}

static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PRESSED;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup()
{
  Serial.begin(115200);
  // while (!Serial);
  Serial.println("LVGL Hello World");

  // Init touch device


  // Init Display
  lcd->begin();
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
  lcd->fillScreen(RED);
  delay(500);
  lcd->fillScreen(GREEN);
  delay(500);
  lcd->fillScreen(BLUE);
  delay(500);
  lcd->fillScreen(BLACK);
  delay(500);
  lv_init();
  lv_tick_set_cb(lvgl_tick_get);
  delay(10);
  touch_init();
  screenWidth = lcd->width();
  screenHeight = lcd->height();
#ifdef ESP32
  const uint32_t draw_buf_size = screenWidth * screenHeight / 4 * 2;
  disp_draw_buf = static_cast<uint8_t *>(heap_caps_malloc(draw_buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
  const uint32_t draw_buf_size = screenWidth * screenHeight / 4 * 2;
  disp_draw_buf = static_cast<uint8_t *>(malloc(draw_buf_size));
#endif
  if (!disp_draw_buf)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    display = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(display, my_disp_flush);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, disp_draw_buf, NULL, draw_buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Initialize the pointer input device */
    static lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    hello_world_widget();

    Serial.println("Setup done");
  }
}

void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
