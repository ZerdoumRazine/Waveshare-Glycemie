/* ui_alert.h: Public controls for the on-screen alert panel.
 * The declarations below are shared with other firmware modules.
 */
/* Alerts appear in a temporary full-screen panel with actions to
 * acknowledge or snooze. The panel closes after at most 30 seconds, but
 * this does not silently clear the underlying alert or audible repeat rules.
 * A compact persistent indicator remains in the shared header. */
/* LVGL alert panel, acknowledgment, snooze and status indicator APIs. */
#pragma once

#include <stdbool.h>
#include "alerts.h"
#include "lvgl.h"

void ui_alert_create(lv_obj_t *screen);
void ui_alert_create_indicator(lv_obj_t *parent); /* Persistent alert indicator in the shared header row. */
void ui_alert_relang(void); /* Recreate panel labels and indicator text after a language change. */
void ui_alert_update(void);
void ui_alert_acknowledge(void);
void ui_alert_snooze(void);

/* Read-only LVGL query for the currently presented alert and type. */
bool ui_alert_panel_visible(void);
alert_type_t ui_alert_panel_style(void);
