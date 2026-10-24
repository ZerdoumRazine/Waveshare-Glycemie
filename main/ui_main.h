/* ui_main.h: Main UI lifecycle and data update interface.
 * Shared definitions for the firmware implementation.
 */
/*
 * Interface 1024x600 (LVGL 9.5) : bandeau d'etat, 5 pages et barre de navigation tactile.
 */
/* Public interface / Interface publique
 * UI lifecycle and queued requests from non-UI tasks.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* To call with lock LVGL (bsp_display_lock) */
bool ui_main_create(void);

/* Capture of the screen sent to the serial console (development tool) */
void ui_snapshot_dump(void);

/* Display test: artificially ages last measurement (0 = desactive) */
void ui_test_set_age_offset(int minutes);

/* Console commands (any stain): page 0.5, range of Chart 3/6/12/24 h */
void ui_request_page(int page);
void ui_request_chart_hours(int hours);
/* Confirmation NOUVEAU CAPTEUR (0) / NOUVEAU CATHETER (1) on the page CONSOMMABLES ; other value: closure */
void ui_request_consumable_dialog(int kind);
/* Voice command confirmed (any stain): l'heure actuelle devient l'time of departure, recorded by the spot LVGL.
 * Returns a request number; ui_consumable_new_result() : 0 in progress, 1 saves, -1 hour invalid. */
uint32_t ui_request_consumable_new(int kind);
int ui_consumable_new_result(uint32_t seq);
void ui_request_alert_button(bool acknowledge);
void ui_request_settings_scroll(int y);
/* Language change of the interface (lang_t) : applies by the LVGLwhich patch rebuilds the pages.
 * Callable from a button callback or from the console; no effect if the value is off range. */
void ui_request_lang(int lang);

/* Diagnosis of spot LVGL (all spots) */
int64_t ui_heartbeat_age_ms(void);         /* anciennete du dernier rafraichissement periodique (1 s) */
void ui_request_render_stress(int seconds, int mode); /* Performance endurance test: 1 full screen, 2 small areas */
void ui_print_stats(void);
void ui_graph_print_explore(void); /* etat de l'exploration tactile du graphique */
