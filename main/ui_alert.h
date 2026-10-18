/*
 * Alertes a l'ecran :
 * - panneau plein ecran (titre, valeur, OK = acquitter, RAPPEL DANS 30 MIN) a chaque nouvelle levee d'alerte,
 *   affiche 30 s au plus puis ferme automatiquement avec retour a l'accueil ; l'alerte reste active et continue
 *   de sonner selon ses repetitions ;
 * - petit indicateur persistant dans le bandeau superieur : plein tant que l'alerte n'est pas acquittee (un appui
 *   rouvre le panneau), contour seul tant que la glycemie reste basse / tres basse / elevee / tres elevee ou que
 *   les donnees manquent apres OK ou RAPPEL.
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

/* Lecture seule (tache LVGL) : panneau plein ecran affiche et son apparence (type ou type simule) */
bool ui_alert_panel_visible(void);
alert_type_t ui_alert_panel_style(void);
