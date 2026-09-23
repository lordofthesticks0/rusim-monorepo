# LVGL v8 to v9 migration report

## Scope and current setup

The display implementation is in `firmware/display/` (there is no top-level `display/` directory). `platformio.ini` pins the display environment to LVGL 8.3.6. The demo uses an 800×480 Arduino_GFX RGB panel, a GT911 touch controller, RGB565 output, one partial draw buffer, and the LVGL v8 display and input driver APIs.

The widget itself is small, so the migration is mostly a porting-layer and configuration update. The Arduino_GFX panel setup and GT911 sampling/mapping are not LVGL-specific and should remain intact.

## Required changes

### 1. Select and pin an LVGL v9 package

Change `lib_deps` in `platformio.ini` from `lvgl/lvgl@8.3.6` to a specific v9 release supported by the PlatformIO registry. Pin the chosen version while migrating so the API and configuration being checked do not move underneath the project. The other display dependencies do not need to change solely for LVGL v9.

### 2. Replace the display registration and flush API

In `firmware/display/main.cpp`, replace the v8 `lv_disp_draw_buf_t` / `lv_disp_drv_t` setup with a v9 `lv_display_t`:

```cpp
static lv_display_t *display;
static uint8_t *disp_draw_buf;

static void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
  const uint32_t w = area->x2 - area->x1 + 1;
  const uint32_t h = area->y2 - area->y1 + 1;

  lcd->draw16bitRGBBitmap(area->x1, area->y1,
                          reinterpret_cast<uint16_t *>(px_map), w, h);
  lv_display_flush_ready(display);
}
```

Create the display with `lv_display_create(screenWidth, screenHeight)`, set the callback with `lv_display_set_flush_cb`, and provide the buffer with `lv_display_set_buffers(..., byte_count, LV_DISPLAY_RENDER_MODE_PARTIAL)`. V9 takes the buffer size in **bytes**, whereas the v8 `lv_disp_draw_buf_init` call takes a pixel count. The existing buffer is one quarter of the screen (96,000 pixels at 800×480); if retaining that capacity for RGB565, allocate 192,000 bytes and pass that byte count. Prefer a byte buffer and explicit arithmetic over `sizeof(lv_color_t)`: v9's `lv_color_t` is RGB888 even when the configured rendering format is RGB565.

Keep the RGB565 format consistent across the configuration, allocated buffer, and flush callback. Retain `LV_COLOR_DEPTH 16` from the v9 template and make the display format explicit at runtime with `lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565)`. If byte-swapped output is actually required by the panel path, use `LV_COLOR_FORMAT_RGB565_SWAPPED` where supported by the selected v9 release; otherwise swap the RGB565 bytes in the flush path with LVGL's swap helper. The current `LV_COLOR_16_SWAP` conditional should not be carried forward as the v9 format-selection mechanism. Confirm color order on hardware because a mismatch can compile cleanly and still produce wrong colors.

The v8 names `lv_disp_flush_ready`, `lv_disp_draw_buf_t`, `lv_disp_drv_t`, `lv_disp_draw_buf_init`, `lv_disp_drv_init`, and `lv_disp_drv_register` are removed in v9; use the corresponding display object APIs above.

### 3. Replace input-driver registration

The `my_touchpad_read` callback in `main.cpp` should accept `lv_indev_t *` instead of `lv_indev_drv_t *`. Replace the v8 driver descriptor and registration with a v9 input-device object:

```cpp
static lv_indev_t *indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(indev, my_touchpad_read);
```

The callback's `lv_indev_data_t` coordinates and touch code in `touch.h` can otherwise stay as they are. Update the state values from `LV_INDEV_STATE_PR` / `LV_INDEV_STATE_REL` to `LV_INDEV_STATE_PRESSED` / `LV_INDEV_STATE_RELEASED`. Touch library calls and GT911 coordinate mapping do not use LVGL APIs.

### 4. Update the active-screen call and check widget/config names

Change `lv_scr_act()` to `lv_screen_active()` in `hello_world_widget()`. The label creation, style setters, colors, centering, and `lv_timer_handler()` loop have direct v9 equivalents and should need no structural rewrite. Review compile diagnostics for additional deprecated/renamed identifiers, especially if more widgets are added later.

### 5. Refresh the LVGL configuration

`firmware/display/lv_conf.h` identifies itself as a v8.3.0 development configuration, includes `<stdint.h>`, and contains the old 8.x option set. Start from the `lv_conf_template.h` shipped with the exact v9 release selected above, then carry over only settings the project needs. At minimum:

- Keep `LV_COLOR_DEPTH 16` as the v9 configuration's RGB565 default. (The newer `LV_COLOR_FORMAT_DEFAULT` configuration belongs to later migration guidance and is not required for a v9 port.)
- Remove the v8 `LV_COLOR_16_SWAP` setting and use the RGB565 or RGB565-swapped format mechanism described above.
- Do not carry over the `<stdint.h>` include; LVGL's v9 migration notes warn that includes in this configuration can interfere with assembly sections.
- Recheck memory, allocator, font, widget, demo, and monitor option names against the new template. Preserve only needed features; the present config enables many widgets, examples, and monitor options although this demo only creates a label.
- Ensure `LV_CONF_INCLUDE_SIMPLE` and the `-I firmware/display` include path continue to make this local configuration visible. The existing PlatformIO flags already provide both.

## Memory and runtime considerations

The current 800×480 partial buffer occupies about 192 KB with two-byte RGB565 pixels, and it is allocated with `MALLOC_CAP_INTERNAL`. That is a substantial internal-RAM allocation on an ESP32-S3 even though PSRAM is enabled in the PlatformIO environment. Preserve the current partial render mode for this buffer; `DIRECT` and `FULL` modes require full-screen-sized buffers. During the port, verify that the selected heap can provide the allocation and that it is compatible with the display transfer path before moving it to PSRAM or changing buffering. A smaller partial buffer is also possible, but its performance impact should be evaluated on the panel.

`lv_display_flush_ready()` must still be called only after Arduino_GFX has finished using the supplied pixels. The existing code calls it immediately after the bitmap method; this is appropriate only if that method completes the transfer synchronously. If the driver uses asynchronous transfer for this RGB panel, defer the ready notification until transfer completion.

## File-by-file summary

| File | Migration work |
| --- | --- |
| `platformio.ini` | Pin `lvgl/lvgl` to a selected v9 release. |
| `firmware/display/main.cpp` | Convert display and input registration, flush callback types/ready call, draw-buffer sizing and format, and active-screen lookup. |
| `firmware/display/lv_conf.h` | Rebase on the selected v9 template; retain required settings and migrate color-format configuration. |
| `firmware/display/touch.h` | No expected LVGL API changes; retain GT911 setup and coordinate mapping, then confirm it through input behavior after the driver is registered. |

## Suggested migration sequence

1. Choose and pin the v9 release; refresh `lv_conf.h` from that release's template.
2. Port display creation, RGB565 partial-buffer setup, and flush callback.
3. Port pointer input-device creation and update its callback signature.
4. Update `lv_scr_act()` and resolve compile errors against the selected v9 headers.
5. Build the `display` PlatformIO environment, then check the panel's colors, flush completion, touch press/release, and memory allocation on the ESP32-S3 hardware.

## References

- [LVGL v9.0 changelog and v8-to-v9 migration guide](https://lvgl.io/docs/open/9.0/CHANGELOG)
- [LVGL display porting interface](https://lvgl.io/docs/open/9.0/porting/display)
- [LVGL input-device porting interface](https://lvgl.io/docs/open/9.0/porting/indev)
- [LVGL configuration guide](https://lvgl.io/docs/open/9.0/integration/configuration)
- [LVGL display color-format API](https://lvgl.io/docs/open/api/public/display/lv_display_h)
