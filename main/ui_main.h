/* ui_main.h: Main UI lifecycle and data update interface.
 * Shared definitions for the firmware implementation.
 */
/* Main 1024×600 LVGL interface: shared header, five touch pages and navigation. */
/* UI lifecycle and queued requests from worker tasks. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* To call with lock LVGL (bsp_display_lock) */
bool ui_main_create(void);

/* Capture of the screen sent to the serial console (development tool) */
void ui_snapshot_dump(void);

/* Debug control: add synthetic age to the latest reading without changing its value. */
void ui_test_set_age_offset(int minutes);

/* Console commands (any stain): page 0.5, range of Chart 3/6/12/24 h */
void ui_request_page(int page);
void ui_request_chart_hours(int hours);
/* Request confirmation for sensor (0) or infusion-set (1) replacement;
 * all other values close the consumables dialog. */
void ui_request_consumable_dialog(int kind);
/* Queue a confirmed replacement on LVGL's task, which records the
 * synchronized current time. Return a request ID for status polling. */
uint32_t ui_request_consumable_new(int kind);
int ui_consumable_new_result(uint32_t seq);
void ui_request_alert_button(bool acknowledge);
void ui_request_settings_scroll(int y);
/* Schedule a safe language change; LVGL rebuilds translated pages outside
 * the initiating callback. Out-of-range language values are ignored. */
void ui_request_lang(int lang);

/* Diagnosis of spot LVGL (all spots) */
int64_t ui_heartbeat_age_ms(void);         /* Time elapsed since the last periodic LVGL refresh. */
void ui_request_render_stress(int seconds, int mode); /* Performance endurance test: 1 full screen, 2 small areas */
void ui_print_stats(void);
void ui_graph_print_explore(void); /* Current touchscreen exploration state for the history graph. */
