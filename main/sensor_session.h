/* sensor_session.h: Sensor insertion and replacement date API.
 * FR: Dates de pose et renouvellement du capteur.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Suivi LOCAL de la duree du capteur Dexcom G7 (aucune source automatique fiable disponible) :
 * l'heure de pose est enregistree en NVS par NOUVEAU CAPTEUR (bouton ou voix), puis
 * echeance = pose + 10 jours EXACTEMENT. La periode de grace Dexcom de 12 h n'est utilisee ni pour le compteur,
 * ni pour l'echeance, ni pour les rappels : a l'echeance, le capteur est "A CHANGER".
 * Deux rappels a usage unique : 24 h et 2 h avant l'echeance.
 */
/* Public interface / Interface publique
 * EN: Sensor insertion sessions, expiry times and reminder state.
 * FR: Pose du capteur, échéance et rappels.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SENSOR_SESSION_S (10LL * 24 * 3600)
#define SENSOR_GRACE_S   (12LL * 3600) /* grace Dexcom : information technique, ignoree par le suivi (voir ci-dessus) */

typedef enum {
    SENSOR_REMIND_24H = 0,  /* 24 h avant l'echeance */
    SENSOR_REMIND_2H,       /* 2 h avant l'echeance */
    SENSOR_REMIND_COUNT,
} sensor_reminder_t;

typedef struct {
    bool known;
    int64_t start_s;        /* heure de pose (epoch UTC) */
    int64_t due_s;          /* echeance = pose + 10 jours */
    int day;                /* jour 1..10 */
    bool expired;           /* echeance atteinte : A CHANGER */
    int64_t remaining_s;    /* avant l'echeance (negatif si depassee) */
    int progress_pct;       /* avancement sur les 10 jours */
} sensor_status_t;

esp_err_t sensor_session_init(void);
void sensor_session_get(int64_t now_s, sensor_status_t *out);
esp_err_t sensor_session_start(int64_t start_s, int64_t now_s);
esp_err_t sensor_session_adjust(int64_t delta_s, int64_t now_s);
esp_err_t sensor_session_clear(void);

/* Incremente a chaque modification (rafraichissement de l'affichage) */
uint32_t sensor_session_revision(void);

/* Rappel en cours : le plus recent dont l'heure est passee, echeance non atteinte ; -1 sinon.
 * *done : deja presente (memorise en NVS : jamais rejoue, meme apres un redemarrage) */
int sensor_session_current_reminder(int64_t now_s, bool *done);
/* Rappel presente ou acquitte : memorise en NVS, avec les rappels precedents */
void sensor_session_mark_reminder_done(int reminder);
/* Test : rend a nouveau dus les rappels deja passes */
void sensor_session_reset_reminders(void);
const char *sensor_reminder_text(int reminder);

/* Console : masque des rappels deja signales, restauration exacte d'une date et de ses rappels (0 = aucun capteur) */
uint8_t sensor_session_reminder_mask(void);
esp_err_t sensor_session_restore(int64_t start_s, uint8_t reminders_mask);
