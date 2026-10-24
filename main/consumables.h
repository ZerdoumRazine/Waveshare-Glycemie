/* consumables.h: Consumables lifecycle and reminders interface.
 * Keep this header focused on the contracts used by other modules.
 */
/*
 * Consumables: Dexcom G7 sensor (followed in sensor_session.c) and infusion site / catheter.
 *
 * NOUVEAU CAPTEUR / NOUVEAU CATHETER (button with confirmation, or voice command confirmed): the current time
 * (SNTP) devient l'heure de depart, enregistree en NVSwithout any date or time entry. Line added
 * a /glycemie/logs/consumables.csv for each actual confirmation ("2026-09-15 16:42,G7,new"); if microSD is
 * not available, the line is written on its return (marker NVS, never duplicate or line for restoration).
 *
 * Catheter: 72 h interval per default (48 h possible), one single recall has single use 2 h before default, then
 * "A CHANGER" on schedule. It is a reminder configured by the user, not a therapeutic recommendation.
 */
/* Public interface / Interface publique
 * Cateter dates, discounts and their persistence contract.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    CONSUMABLE_SENSOR = 0,  /* Dexcom G7 sensor */
    CONSUMABLE_CATHETER,    /* infusion site / catheter */
    CONSUMABLE_COUNT,
} consumable_t;

typedef enum {
    CATHETER_REMIND_2H = 0, /* 2 hours before completion (only recall) */
    CATHETER_REMIND_COUNT,
} catheter_reminder_t;

typedef struct {
    bool known;
    int64_t start_s;        /* heure du changement (epoch UTC) */
    int64_t due_s;          /* echeance = changement + intervalle */
    int interval_h;         /* 48 ou 72 */
    int day;                /* Day 1.days_total */
    int days_total;
    bool overdue;           /* echeance depassee */
    int64_t remaining_s;    /* before maturity (negative if exceeded) */
    int progress_pct;       /* promotion over the interval */
} catheter_status_t;

esp_err_t consumables_init(void); /* After sensor_session_init() */

/* Heure actuelle -> heure de depart (NVS + ligne CSV en attente). ESP_ERR_INVALID_STATE si l'heure SNTP is not
 * valid. source: "button", "voice" or "console" (journal only). Not since a pile spot in PSRAM. */
esp_err_t consumable_start_now(consumable_t c, const char *source);
const char *consumable_name(consumable_t c); /* "NOUVEAU CAPTEUR"... */

void catheter_get(int64_t now_s, catheter_status_t *out);
int catheter_interval_h(void);
void catheter_set_interval_h(int hours); /* 48 ou 72 */

/* Incremented with each modification (sensor or catheter): refreshing the display */
uint32_t consumables_revision(void);

/* Reminder of the current catheter (time pass, time not reached), -1 otherwise; *donone: already present (NVS) */
int catheter_current_reminder(int64_t now_s, bool *done);
void catheter_mark_reminder_done(int reminder); /* presente ou acquitte : memorise en NVS */
void catheter_reset_reminders(void);
const char *catheter_reminder_text(int reminder);

/* MicroSD cache, mount card: write lines CSV en attente */
void consumables_sd_flush(void);

/* Console: state, exact restoration of a real date after a test (no line CSV), effacement */
void consumables_print(void);
esp_err_t consumable_restore(consumable_t c, int64_t start_s, int reminders_mask);
