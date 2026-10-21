/* ui_main.h: Main UI lifecycle and data update interface.
 * FR: Cycle de vie et mise à jour de l'interface.
 * Shared definitions for the firmware implementation.
 */
/*
 * Interface 1024x600 (LVGL 9.5) : bandeau d'etat, 5 pages et barre de navigation tactile.
 */
/* Public interface / Interface publique
 * EN: UI lifecycle and queued requests from non-UI tasks.
 * FR: Cycle de vie de l'interface et requêtes des tâches secondaires.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* A appeler avec le verrou LVGL (bsp_display_lock) */
bool ui_main_create(void);

/* Capture de l'ecran envoyee sur la console serie (outil de developpement) */
void ui_snapshot_dump(void);

/* Test d'affichage : vieillit artificiellement la derniere mesure (0 = desactive) */
void ui_test_set_age_offset(int minutes);

/* Commandes console (toute tache) : page 0..5, plage du graphique 3/6/12/24 h */
void ui_request_page(int page);
void ui_request_chart_hours(int hours);
/* Confirmation NOUVEAU CAPTEUR (0) / NOUVEAU CATHETER (1) sur la page CONSOMMABLES ; autre valeur : fermeture */
void ui_request_consumable_dialog(int kind);
/* Commande vocale confirmee (toute tache) : l'heure actuelle devient l'heure de depart, enregistree par la tache LVGL.
 * Retourne un numero de demande ; ui_consumable_new_result() : 0 en cours, 1 enregistre, -1 heure invalide. */
uint32_t ui_request_consumable_new(int kind);
int ui_consumable_new_result(uint32_t seq);
void ui_request_alert_button(bool acknowledge);
void ui_request_settings_scroll(int y);
/* Changement de langue de l'interface (lang_t) : applique par la tache LVGL, qui reconstruit les pages.
 * Appelable depuis un callback de bouton ou depuis la console ; sans effet si la valeur est hors plage. */
void ui_request_lang(int lang);

/* Diagnostic de la tache LVGL (toute tache) */
int64_t ui_heartbeat_age_ms(void);         /* anciennete du dernier rafraichissement periodique (1 s) */
void ui_request_render_stress(int seconds, int mode); /* test d'endurance du rendu : 1 plein ecran, 2 petites zones */
void ui_print_stats(void);
void ui_graph_print_explore(void); /* etat de l'exploration tactile du graphique */
