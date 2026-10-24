/* sensor_session.h: Sensor insertion and replacement date API.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Suivi LOCAL de la duree du capteur Dexcom G7 (aucune source automatique fiable disponible) :
 * l'heure de pose est enregistree en NVS par NOUVEAU CAPTEUR (bouton ou voix), puis
 * echeance = pose + 10 jours EXACTEMENT. The 12-hour Dexcom grace period is not used for the meter,
 * neither for the delay nor for the reminders: at the end, the sensor is "A CHANGER".
 * Two single-use reminders: 24 hours and 2 hours before schedule.
 */
/* Public interface / Interface publique
 * Sensor insertion sessions, expiry times and reminder state.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SENSOR_SESSION_S (10LL * 24 * 3600)
#define SENSOR_GRACE_S   (12LL * 3600) /* grace Dexcom : information technique, ignoree par le suivi (voir ci-dessus) */

typedef enum {
    SENSOR_REMIND_24H = 0,  /* 24 hours before schedule */
    SENSOR_REMIND_2H,       /* 2 hours before schedule */
    SENSOR_REMIND_COUNT,
} sensor_reminder_t;

typedef struct {
    bool known;
    int64_t start_s;        /* heure de pose (epoch UTC) */
    int64_t due_s;          /* echeance = pose + 10 jours */
    int day;                /* jour 1..10 */
    bool expired;           /* echeance atteinte : A CHANGER */
    int64_t remaining_s;    /* before maturity (negative if exceeded) */
    int progress_pct;       /* promotion over 10 days */
} sensor_status_t;

esp_err_t sensor_session_init(void);
void sensor_session_get(int64_t now_s, sensor_status_t *out);
esp_err_t sensor_session_start(int64_t start_s, int64_t now_s);
esp_err_t sensor_session_adjust(int64_t delta_s, int64_t now_s);
esp_err_t sensor_session_clear(void);

/* Incremente a chaque modification (rafraichissement de l'affichage) */
uint32_t sensor_session_revision(void);

/* Rappel en cours : le plus recent dont l'heure est passee, echeance non atteinte ; -1 sinon.
 * *done : deja presente (memorise en NVS : never replay, even after a restart) */
int sensor_session_current_reminder(int64_t now_s, bool *done);
/* Rappel presente ou acquitte : memorise en NVS, with previous reminders */
void sensor_session_mark_reminder_done(int reminder);
/* Test: again renders the reminders already passed */
void sensor_session_reset_reminders(void);
const char *sensor_reminder_text(int reminder);

/* Console: mask of reminders already reported, exact restoration of a date and its reminders (0 = no sensor) */
uint8_t sensor_session_reminder_mask(void);
esp_err_t sensor_session_restore(int64_t start_s, uint8_t reminders_mask);
