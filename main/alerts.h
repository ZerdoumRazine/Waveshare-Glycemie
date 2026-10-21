/* alerts.h: Public controls and types for supplementary alerts.
 * FR: Types et commandes publiques des alertes.
 * Keep this header focused on the contracts used by other modules.
 */
/*
 * Gestionnaire d'alertes : glycemie tres basse / basse / elevee / tres elevee, absence de donnees,
 * rappels du capteur. Anti-repetition : acquittement, rappel differe (snooze), repetitions limitees,
 * volume croissant (RAMP) pour l'alerte critique. Les alertes glycemiques exigent une mesure recente.
 */
/* Public interface / Interface publique
 * EN: Public alert types, priorities and acknowledgement controls.
 * FR: Types d'alertes et interfaces d'acquittement.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    ALERT_NONE = -1,
    ALERT_VERY_LOW = 0, /* ordre = priorite */
    ALERT_LOW,
    ALERT_NO_DATA,
    ALERT_VERY_HIGH,
    ALERT_HIGH,
    ALERT_SENSOR,
    ALERT_CATHETER,     /* rappel du site de perfusion (consumables.h) */
    ALERT_TEST,         /* alerte de test (console) : affichee avant ELEVEE, jamais avant BASSE / TRES BASSE */
    ALERT_COUNT,
} alert_type_t;

typedef struct {
    alert_type_t type;  /* alerte a presenter (non acquittee, non reportee), ALERT_NONE sinon */
    int value;          /* mg/dL, minutes sans donnee, ou numero de rappel capteur / catheter */
    char title[48];
    char message[96];
    uint32_t seq;       /* identifiant de la levee de cette alerte : nouvelle valeur a chaque nouvelle levee */
    alert_type_t style; /* apparence (couleur) : type, ou type simule par l'alerte de test */
    alert_type_t ongoing; /* glycemie basse / tres basse / elevee / tres elevee ou absence de donnees en cours,
                             meme acquittee ou reportee (indicateur persistant), ALERT_NONE sinon */
    bool ongoing_test;  /* condition simulee par l'alerte de test (aucune vraie condition en cours) */
} alert_view_t;

esp_err_t alerts_start(void);
void alerts_get_view(alert_view_t *out);
void alerts_acknowledge(void);
void alerts_snooze(int minutes);
const char *alerts_title(alert_type_t type); /* "GLYCÉMIE BASSE"... ("" pour ALERT_NONE) */

/* Test (console) : joue le son et la voix de l'alerte sans modifier les etats */
void alerts_test_sound(alert_type_t type, int value);

/* Alerte de test affichee (OK / RAPPEL DANS 30 MIN) sans toucher aux donnees Dexcom ni aux vraies alertes.
 * as = ALERT_TEST : alerte de test generique ; as = un vrai type : simulation complete de ce type avec la valeur
 * (panneau, indicateur, son, voix), marquee "(test)" dans le message et l'indicateur, jamais journalisee sur la
 * microSD ; la condition simulee reste affichee par l'indicateur jusqu'a alerts_test_stop(). */
void alerts_test_start(alert_type_t as, int value);
void alerts_test_stop(void);
void alerts_print_status(void);
