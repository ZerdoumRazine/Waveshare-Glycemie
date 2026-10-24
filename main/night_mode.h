/* night_mode.h: Night display settings and schedule API.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Night mode: ECRAN NUIT MINIMALISTE (hour, very large glycemia, fleche, status, last measurement) on a black background,
 * A luminosite night. LVGL, Dexcom, Wi-Fi, microSD, history, alerts, sound and voice continue normally;
 * No P4 watches.
 *
 * - Night period (local time) SNTP, adjustments MODE NUIT) : toggle towards the night screen ; end of period:
 *   back to normal interface to luminosite day.
 * - Touch the night screen: this touch does not trigger anything else; the normal interface remains visible 30 s after
 *   the last interaction, then back to the screen night.
 * - Alert sign TRES BASSE / BASSE / TRES ELEVEE / ELEVEE / AUCUNE DONNEE : priority, luminosite day ;
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
    NIGHT_REQ_OFF,   /* test: end of simulation (real schedule) */
    NIGHT_REQ_WAKE,  /* test : dream as a touch */
    NIGHT_REQ_TEST,  /* test: seen, like the button TEST MODE NUIT */
} night_request_t;

/* Tache LVGL */
void night_mode_init(void);
void night_mode_update(const ui_data_t *d); /* every second, after ui_alert_update() */
void night_mode_brightness_changed(void);   /* luminosite modified in LOGAGES: immediate application */
void night_mode_preview(void);              /* bouton TEST MODE NUIT */
void night_mode_relang(void);               /* language change : texts of the screen night to repeat */
bool night_mode_screen_visible(void);       /* night screen displays (reading only, hosting animations) */

/* Console (any stain) */
void night_mode_request(night_request_t req);
void night_mode_print_info(void);
