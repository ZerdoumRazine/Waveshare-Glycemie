/* touch_diag.h: Touch diagnostics and visual hit-area markers.
 * The declarations below are shared with other firmware modules.
 */
/* Developer touch diagnostics: GT911 raw coordinates pass through optional
 * mirroring/rotation, then LVGL hit testing and click dispatch. Use the
 * official Espressif adapter callbacks without replacing its read path.
 * Log state changes only when serial diagnostics are enabled. */
/* Debug-only touch mapping, hit-target information and test injection APIs. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/* Initialize after BSP display startup with adapter rotation. */
void touch_diag_init(lv_indev_t *indev, int rotation_deg);

/* Stable diagnostic object name, read only by the LVGL task. */
void touch_diag_name(lv_obj_t *obj, const char *name);

/* Console (any stain) */
void touch_diag_set_log(bool on);
bool touch_diag_log_enabled(void); /* True while touch diagnostics are enabled. */
void touch_diag_request_info(void);
void touch_diag_request_mapping(bool swap_xy, bool mirror_x, bool mirror_y);

/* Test: Synthetic support injects into tactile reading (the same path) LVGL one finger).
 * From (x1, y1) a (x2, y2) for ms milliseconds, then relay. Returns false if the line is full. */
bool touch_diag_inject(int x1, int y1, int x2, int y2, int ms);

/* Measure the periodic LVGL UI refresh duration in milliseconds. */
void touch_diag_ui_timer_ms(uint32_t ms);
