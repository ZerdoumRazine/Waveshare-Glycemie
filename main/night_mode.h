/* night_mode.h: Night display settings and schedule API.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Mode nuit : ECRAN NUIT MINIMALISTE (heure, glycemie tres grande, fleche, statut, derniere mesure) sur fond noir,
 * a la luminosite nuit. LVGL, Dexcom, Wi-Fi, microSD, historique, alertes, son et voix continuent normalement ;
 * aucune veille du P4.
 *
 * - Periode nuit (heure locale SNTP, adjustments MODE NUIT) : bascule vers l'ecran nuit ; fin de periode :
 *   retour a l'interface normale a la luminosite jour.
 * - Touch the night screen: this touch does not trigger anything else; the normal interface remains visible 30 s after
 *   la derniere interaction, puis retour a l'ecran nuit.
 * - Panneau d'alerte TRES BASSE / BASSE / TRES ELEVEE / ELEVEE / AUCUNE DONNEE : prioritaire, luminosite jour ;
 *   Back to the screen night 45 seconds after the sign disappeared.
 * - Apercu (bouton TEST MODE NUIT) : short night screen without changing the timetable ; A touch or 60 s end it.
 */
/* Public interface / Interface publique
 * Night schedule, brightness transitions and temporary wake-up preview.
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
void night_mode_update(const ui_data_t *d); /* every second, after ui_alert_update() */
void night_mode_brightness_changed(void);   /* luminosite modified in LOGAGES: immediate application */
void night_mode_preview(void);              /* bouton TEST MODE NUIT */
void night_mode_relang(void);               /* langue changee : textes de l'ecran nuit a refaire */
bool night_mode_screen_visible(void);       /* night screen displays (reading only, hosting animations) */

/* Console (toute tache) */
void night_mode_request(night_request_t req);
void night_mode_print_info(void);
