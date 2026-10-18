/*
 * Pages de l'interface : ACCUEIL, GRAPHIQUE, AUJOURD'HUI, CONSOMMABLES, REGLAGES.
 * Toutes les fonctions s'executent dans la tache LVGL.
 */
#pragma once

#include <stdbool.h>
#include "consumables.h"
#include "ui_common.h"

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_GRAPH,
    UI_PAGE_TODAY,
    UI_PAGE_CONSUMABLES,
    UI_PAGE_SETTINGS,
    UI_PAGE_COUNT,
} ui_page_t;

void ui_home_create(lv_obj_t *page);
void ui_home_update(const ui_data_t *d, bool force);

void ui_graph_create(lv_obj_t *page);
void ui_graph_update(const ui_data_t *d, bool force);

void ui_today_create(lv_obj_t *page);
void ui_today_update(const ui_data_t *d, bool force);

void ui_consumables_create(lv_obj_t *page);
void ui_consumables_update(const ui_data_t *d, bool force);
void ui_consumables_open_dialog(consumable_t c); /* confirmation "Nouveau capteur maintenant ?" */
void ui_consumables_close_dialog(void);

void ui_settings_create(lv_obj_t *page);
void ui_settings_update(const ui_data_t *d, bool force);

/* Assistant de premiere mise en route (langue, Wi-Fi, compte Dexcom) : couche au-dessus des pages */
void ui_setup_create(void);
void ui_setup_open(bool premier_demarrage);
void ui_setup_close(void);
bool ui_setup_visible(void);
void ui_setup_tick(void); /* avancement de la recherche de reseaux et de l'essai de connexion */

/* Navigation et plage du graphique (tache LVGL) */
void ui_show_page(ui_page_t page);
int ui_chart_hours(void);
void ui_set_chart_hours(int hours);
