/* ui_alert.h: Public controls for the on-screen alert panel.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Alertes a l'ecran :
 * - panneau plein ecran (titre, valeur, OK = acquitter, RAPPEL DANS 30 MIN) a chaque nouvelle levee d'alerte,
 *   displays 30 s at most and closes automatically with return to reception; the alert remains active and continuous
 *   de sonner selon ses repetitions ;
 * - small persistent indicator in the upper headband: full until the alert is discharged (support
 *   rouvre le panneau), contour seul tant que la glycemie reste basse / tres basse / elevee / tres elevee ou que
 *   data is missing after OK or RAPPEL.
 */
/* Public interface / Interface publique
 * LVGL alert overlays, acknowledgement and snooze controls.
 */
#pragma once

#include <stdbool.h>
#include "alerts.h"
#include "lvgl.h"

void ui_alert_create(lv_obj_t *screen);
void ui_alert_create_indicator(lv_obj_t *parent); /* conteneur en ligne du bandeau superieur */
void ui_alert_relang(void); /* langue changee : intitules du panneau et de l'indicateur a refaire */
void ui_alert_update(void);
void ui_alert_acknowledge(void);
void ui_alert_snooze(void);

/* Lecture seule (tache LVGL) : full screen panel displays and its appearance (type or type simulate) */
bool ui_alert_panel_visible(void);
alert_type_t ui_alert_panel_style(void);
