#pragma once

// ui_shell.h only needs opaque widget/font types here. No LVGL functions or
// rendering are implemented: the host tests cover USB command delegation.
struct lv_obj_t;
struct lv_font_t;
