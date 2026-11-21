#pragma once
// /data: node picker, node page (one tile per bottle), bottle page (5 sensor
// tiles + trend of the selected sensor). Numbers shown to the operator are
// 1-based ("Node 1", "Bottle 1"); the model and wire stay 0-based.
#include <lvgl.h>
void screen_data_picker_show(lv_obj_t *parent);
void screen_cluster_show(lv_obj_t *parent, int c);
void screen_bottle_show(lv_obj_t *parent, int c, int b);
void screen_data_refresh();  // in-place update of whichever /data page is open
void screen_data_hide();
