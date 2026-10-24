/* ui_pages.h: Declarations for the five application pages.
 * Shared definitions for the firmware implementation.
 */
/* Five LVGL pages: Home, Chart, Today, Consumables and Settings.
 * Page construction and updates run in the LVGL task. */
/* Shared constructors and updates for all five touchscreen pages. */
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
void ui_consumables_open_dialog(consumable_t c); /* Present the confirmation dialog for a new sensor insertion. */
void ui_consumables_close_dialog(void);

void ui_settings_create(lv_obj_t *page);
void ui_settings_update(const ui_data_t *d, bool force);

/* First Start Assistant (language, Wi-Fi, Dexcom account): layer above pages */
void ui_setup_create(void);
void ui_setup_open(bool premier_demarrage);
void ui_setup_close(void);
bool ui_setup_visible(void);
void ui_setup_tick(void); /* progress in network search and connection test */

/* Change the active page or history range on the LVGL task. */
void ui_show_page(ui_page_t page);
int ui_chart_hours(void);
void ui_set_chart_hours(int hours);
