/*
 * Diagnostic du tactile (developpement) : GT911 -> coordonnees brutes -> miroir/inversion (esp_lcd_touch)
 * -> LVGL -> objet touche -> clic. Branche par l'API officielle esp_lv_adapter_set_touch_callbacks :
 * la lecture reste celle de l'adaptateur (esp_lcd_touch_read_data + esp_lcd_touch_get_data).
 * Journal uniquement sur changement d'etat (appui, relachement, clic), active par la console.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/* A appeler une fois apres bsp_display_start_with_config ; rotation_deg = rotation de l'adaptateur */
void touch_diag_init(lv_indev_t *indev, int rotation_deg);

/* Nom lisible d'un objet dans le journal (tache LVGL, chaine persistante) */
void touch_diag_name(lv_obj_t *obj, const char *name);

/* Console (toute tache) */
void touch_diag_set_log(bool on);
bool touch_diag_log_enabled(void); /* traces de diagnostic actives (touch_log 1) */
void touch_diag_request_info(void);
void touch_diag_request_mapping(bool swap_xy, bool mirror_x, bool mirror_y);

/* Test : appui synthetique injecte dans la lecture tactile (meme chemin LVGL qu'un doigt).
 * De (x1, y1) a (x2, y2) pendant ms millisecondes, puis relachement. Retourne false si la file est pleine. */
bool touch_diag_inject(int x1, int y1, int x2, int y2, int ms);

/* Duree du rafraichissement periodique de l'interface (tache LVGL) */
void touch_diag_ui_timer_ms(uint32_t ms);
