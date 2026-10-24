/* touch_diag.h: Touch diagnostics and visual hit-area markers.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Diagnostic du tactile (developpement) : GT911 -> coordonnees brutes -> miroir/inversion (esp_lcd_touch)
 * -> LVGL -> object touches -> click. Connection by theAPI Official esp_lv_adapter_set_touch_callbacks :
 * the reading remains that of the adapter (esp_lcd_touch_read_data + esp_lcd_touch_get_data).
 * Journal only on change of state (press, release, click), enabled by the console.
 */
/* Public interface / Interface publique
 * Touchscreen diagnostics, event mapping and synthetic input testing.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/* To call once after bsp_display_start_with_config ; rotation_deg = rotation de l'adaptateur */
void touch_diag_init(lv_indev_t *indev, int rotation_deg);

/* Readable name of an object in the journal (tache LVGL, chaine persistante) */
void touch_diag_name(lv_obj_t *obj, const char *name);

/* Console (any stain) */
void touch_diag_set_log(bool on);
bool touch_diag_log_enabled(void); /* traces de diagnostic actives (touch_log 1) */
void touch_diag_request_info(void);
void touch_diag_request_mapping(bool swap_xy, bool mirror_x, bool mirror_y);

/* Test: Synthetic support injects into tactile reading (the same path) LVGL one finger).
 * From (x1, y1) a (x2, y2) for ms milliseconds, then relay. Returns false if the line is full. */
bool touch_diag_inject(int x1, int y1, int x2, int y2, int ms);

/* Duree du rafraichissement periodique de l'interface (tache LVGL) */
void touch_diag_ui_timer_ms(uint32_t ms);
