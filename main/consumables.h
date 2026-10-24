/* consumables.h: Consumables lifecycle and reminders interface.
 * Keep this header focused on the contracts used by other modules.
 */
/* Consumables lifecycle: sensor sessions live in sensor_session.c and
 * infusion-set dates live here. A confirmed replacement records the current
 * synchronized time in NVS and is logged to microSD once. When the card is
 * unavailable, the pending record is written on remount without duplication.
 * The infusion-set interval is user-selectable (48 or 72 hours); its single
 * reminder is two hours before the due time. These are user preferences,
 * not clinical recommendations. */
/* Shared APIs for replacement dates, reminders and persistence. */
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
    int64_t start_s;        /* Confirmed replacement time as UTC epoch seconds. */
    int64_t due_s;          /* Due time = replacement time plus selected interval. */
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
