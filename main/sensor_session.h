/* sensor_session.h: Sensor insertion and replacement date API.
 * The declarations below are shared with other firmware modules.
 */
/* Track locally confirmed Dexcom G7 sensor insertion dates in NVS.
 * The due time is exactly 10 days after insertion. Do not add Dexcom's
 * separate 12-hour grace period. Schedule single reminders 24 hours and
 * 2 hours before the due time. Neither expiry nor reminders are inferred
 * automatically from the Dexcom Share protocol. */
/* Sensor insertion times, due dates and reminder state shared across modules. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define SENSOR_SESSION_S (10LL * 24 * 3600)
#define SENSOR_GRACE_S   (12LL * 3600) /* External Dexcom data is informative and does not alter local sensor dates. */

typedef enum {
    SENSOR_REMIND_24H = 0,  /* 24 hours before schedule */
    SENSOR_REMIND_2H,       /* 2 hours before schedule */
    SENSOR_REMIND_COUNT,
} sensor_reminder_t;

typedef struct {
    bool known;
    int64_t start_s;        /* Sensor insertion time as UTC epoch seconds. */
    int64_t due_s;          /* Due time = insertion time plus exactly ten days. */
    int day;                /* Day 1..10 */
    bool expired;           /* Due time reached: show the replacement-required state. */
    int64_t remaining_s;    /* before maturity (negative if exceeded) */
    int progress_pct;       /* promotion over 10 days */
} sensor_status_t;

esp_err_t sensor_session_init(void);
void sensor_session_get(int64_t now_s, sensor_status_t *out);
esp_err_t sensor_session_start(int64_t start_s, int64_t now_s);
esp_err_t sensor_session_adjust(int64_t delta_s, int64_t now_s);
esp_err_t sensor_session_clear(void);

/* Incremented with each modification (refreshing the display) */
uint32_t sensor_session_revision(void);

/* Reminder in progress: the most recent time passed, time not reached; -1 if not.
 * *done : deja presente (memorise en NVS : never replay, even after a restart) */
int sensor_session_current_reminder(int64_t now_s, bool *done);
/* Remember whether the reminder was shown or acknowledged across reboots. */
void sensor_session_mark_reminder_done(int reminder);
/* Test: again renders the reminders already passed */
void sensor_session_reset_reminders(void);
const char *sensor_reminder_text(int reminder);

/* Console: mask of reminders already reported, exact restoration of a date and its reminders (0 = no sensor) */
uint8_t sensor_session_reminder_mask(void);
esp_err_t sensor_session_restore(int64_t start_s, uint8_t reminders_mask);
