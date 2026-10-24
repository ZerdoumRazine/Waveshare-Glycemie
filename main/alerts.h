/* alerts.h: Public controls and types for supplementary alerts.
 * Keep this header focused on the contracts used by other modules.
 */
/*
 * Alert Manager: very low / low / high / very high, lack of data,
 * Sensor reminders. Anti-repetition: acquittal, different recall (snooze), limit repetitions,
 * volume croissant (RAMP) for critical alert. Glycemic alerts require recent action.
 */
/* Public interface / Interface publique
 * Public alert types, priorities and acknowledgement controls.
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
    ALERT_TEST,         /* test alert (console): displays before ELEVEEnever before BASSE / TRES BASSE */
    ALERT_COUNT,
} alert_type_t;

typedef struct {
    alert_type_t type;  /* alerte a presenter (non acquittee, non reportee), ALERT_NONE sinon */
    int value;          /* mg/dL, minutes without data, gold sensor/catheter number */
    char title[48];
    char message[96];
    uint32_t seq;       /* Identifier of the wake of this alert: new value to each new lift */
    alert_type_t style; /* apparence (couleur) : type, ou type simule par l'alerte de test */
    alert_type_t ongoing; /* low / very low / high / very high or lack of current data,
                             even acquitted or deferred (persistent indicator), ALERT_NONE if not */
    bool ongoing_test;  /* condition simulated by the test alert (no real condition in progress) */
} alert_view_t;

esp_err_t alerts_start(void);
void alerts_get_view(alert_view_t *out);
void alerts_acknowledge(void);
void alerts_snooze(int minutes);
const char *alerts_title(alert_type_t type); /* "GLYCEMIE BASSE"... ("" for ALERT_NONE) */

/* Test (console): plays the sound and voice of the alert without changing the states */
void alerts_test_sound(alert_type_t type, int value);

/* Display test alert (OK / RAPPEL DANS 30 MIN) without touching Dexcom data or real alerts.
 * as = ALERT_TEST : general test alert; as = a true type: complete simulation of this type with the value
 * (sign, indicator, sound, voice), marks "(test)" in the message and indicator, never logged on the
 * microSD; the simulated condition remains displayed by l'indicateur jusqu'a alerts_test_stop(). */
void alerts_test_start(alert_type_t as, int value);
void alerts_test_stop(void);
void alerts_print_status(void);
