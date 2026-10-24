/* alerts.h: Public controls and types for supplementary alerts.
 * Keep this header focused on the contracts used by other modules.
 */
/* Classify urgent-low, low, high, urgent-high and missing-data alerts,
 * along with sensor and infusion-set reminders. Acknowledgment and snooze
 * prevent unbounded repeats; critical alerts may escalate audible volume. */
/* Public alert types, priority and acknowledgment controls. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    ALERT_NONE = -1,
    ALERT_VERY_LOW = 0, /* Enum order defines alert priority. */
    ALERT_LOW,
    ALERT_NO_DATA,
    ALERT_VERY_HIGH,
    ALERT_HIGH,
    ALERT_SENSOR,
    ALERT_CATHETER,     /* Infusion-set reminder; see consumables.h. */
    ALERT_TEST,         /* Test alerts must never mask a more important active low-glucose alert. */
    ALERT_COUNT,
} alert_type_t;

typedef struct {
    alert_type_t type;  /* Unacknowledged, unsnoozed alert to display; ALERT_NONE otherwise. */
    int value;          /* Context: glucose mg/dL, data age or consumable reminder type. */
    char title[48];
    char message[96];
    uint32_t seq;       /* Identifier of the wake of this alert: new value to each new lift */
    alert_type_t style; /* Display color derives from the real or simulated alert type. */
    alert_type_t ongoing; /* low / very low / high / very high or lack of current data,
                             even acquitted or deferred (persistent indicator), ALERT_NONE if not */
    bool ongoing_test;  /* condition simulated by the test alert (no real condition in progress) */
} alert_view_t;

esp_err_t alerts_start(void);
void alerts_get_view(alert_view_t *out);
void alerts_acknowledge(void);
void alerts_snooze(int minutes);
const char *alerts_title(alert_type_t type); /* Localized alert title; empty when no alert is active. */

/* Test (console): plays the sound and voice of the alert without changing the states */
void alerts_test_sound(alert_type_t type, int value);

/* Display a simulated alert panel without changing live Dexcom data or
 * real alert state. Mark it as a test, never write it to microSD, and keep
 * its indicator until alerts_test_stop() is called. */
void alerts_test_start(alert_type_t as, int value);
void alerts_test_stop(void);
void alerts_print_status(void);
