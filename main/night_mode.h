/*
 * Mode nuit : ECRAN NUIT MINIMALISTE (heure, glycemie tres grande, fleche, statut, derniere mesure) sur fond noir,
 * a la luminosite nuit. LVGL, Dexcom, Wi-Fi, microSD, historique, alertes, son et voix continuent normalement ;
 * aucune veille du P4.
 *
 * - Periode nuit (heure locale SNTP, reglages RÉGLAGES > MODE NUIT) : bascule vers l'ecran nuit ; fin de periode :
 *   retour a l'interface normale a la luminosite jour.
 * - Toucher sur l'ecran nuit : ce toucher ne declenche rien d'autre ; l'interface normale reste visible 30 s apres
 *   la derniere interaction, puis retour a l'ecran nuit.
 * - Panneau d'alerte TRES BASSE / BASSE / TRES ELEVEE / ELEVEE / AUCUNE DONNEE : prioritaire, luminosite jour ;
 *   retour a l'ecran nuit 45 s apres la disparition du panneau.
 * - Apercu (bouton TEST MODE NUIT) : ecran nuit immediat sans modifier les horaires ; un toucher ou 60 s y mettent fin.
 */
/* Public interface / Interface publique
 * EN: Night schedule, brightness transitions and temporary wake-up preview.
 * FR: Horaires du mode nuit et retour temporaire à l'écran principal.
 */
#pragma once

#include <stdbool.h>
#include "ui_common.h"

typedef enum {
    NIGHT_REQ_NONE = 0,
    NIGHT_REQ_ON,    /* test : periode nuit simulee */
    NIGHT_REQ_OFF,   /* test : fin de la simulation (horaire reel) */
    NIGHT_REQ_WAKE,  /* test : reveil comme par un toucher */
    NIGHT_REQ_TEST,  /* test : apercu, comme le bouton TEST MODE NUIT */
} night_request_t;

/* Tache LVGL */
void night_mode_init(void);
void night_mode_update(const ui_data_t *d); /* chaque seconde, apres ui_alert_update() */
void night_mode_brightness_changed(void);   /* luminosite modifiee dans RÉGLAGES : application immediate */
void night_mode_preview(void);              /* bouton TEST MODE NUIT */
void night_mode_relang(void);               /* langue changee : textes de l'ecran nuit a refaire */
bool night_mode_screen_visible(void);       /* ecran nuit affiche (lecture seule, animations de l'accueil) */

/* Console (toute tache) */
void night_mode_request(night_request_t req);
void night_mode_print_info(void);
