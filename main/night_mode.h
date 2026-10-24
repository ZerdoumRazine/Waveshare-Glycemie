/* night_mode.h: Night display settings and schedule API.
 * The declarations below are shared with other firmware modules.
 */
/* Night mode renders a minimal dark screen with time, large glucose value,
 * trend and last-reading status at the configured brightness. Networking,
 * history, audio and alert evaluation remain active.
 * - Follow the local SNTP schedule, returning to daytime brightness afterward.
 * - A touch temporarily shows the regular interface for 30 seconds.
 * - A glucose or no-data alert takes priority and uses normal brightness;
 *   return to night mode 45 seconds after that alert closes.
 * - A manual preview does not change the saved schedule and ends on touch
 *   or after 60 seconds. */
/* Night scheduling, brightness transitions and temporary preview APIs. */
#pragma once

#include <stdbool.h>
#include "ui_common.h"

typedef enum {
    NIGHT_REQ_NONE = 0,
    NIGHT_REQ_ON,    /* Temporarily simulate a scheduled night interval. */
    NIGHT_REQ_OFF,   /* Stop simulation and resume the saved schedule. */
    NIGHT_REQ_WAKE,  /* Simulate a touch to wake the regular UI temporarily. */
    NIGHT_REQ_TEST,  /* Start the same preview as the Night Mode Test button. */
} night_request_t;

/* Called only from the LVGL task. */
void night_mode_init(void);
void night_mode_update(const ui_data_t *d); /* every second, after ui_alert_update() */
void night_mode_brightness_changed(void);   /* Apply brightness changes from Settings immediately. */
void night_mode_preview(void);              /* Start a temporary night-mode preview without persisting a new schedule. */
void night_mode_relang(void);               /* language change : texts of the screen night to repeat */
bool night_mode_screen_visible(void);       /* night screen displays (reading only, hosting animations) */

/* Console (any stain) */
void night_mode_request(night_request_t req);
void night_mode_print_info(void);
