#pragma once
// /data: cluster picker, bottle grid, single bottle (5 sensors + trend).
#include <lvgl.h>
void screen_data_picker_show(lv_obj_t *parent);
void screen_cluster_show(lv_obj_t *parent, int c);
void screen_bottle_show(lv_obj_t *parent, int c, int b);
