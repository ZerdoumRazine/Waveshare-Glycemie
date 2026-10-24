/* app_config.h: Build-time application constants and defaults.
 * Keep this header focused on the contracts used by other modules.
 */
/* Shared application constants for an experimental supplementary display. */
/* Shared timing, network, history and freshness constants. */
#pragma once

#define GLY_APP_NAME                "glycemie_display"

/* French local timezone with automatic daylight-saving transitions. */
#define GLY_TIMEZONE                "CET-1CEST,M3.5.0,M10.5.0/3"
#define GLY_SNTP_SERVER_1           "pool.ntp.org"
#define GLY_SNTP_SERVER_2           "time.cloudflare.com"

/* Neutral connectivity probe expects HTTP 204, not a Dexcom login. */
#define GLY_INTERNET_PROBE_URL      "https://www.gstatic.com/generate_204"
#define GLY_INTERNET_PROBE_HOST     "shareous1.dexcom.com"

/* History: 24 h = 288 measurements (one every 5 min) */
#define GLY_HISTORY_MAX             288
#define GLY_HISTORY_FULL_MINUTES    1440
#define GLY_BACKFILL_MINUTES        180      /* catching up on late published measures */
#define GLY_BACKFILL_PERIOD_S       3600
#define GLY_FULL_RESYNC_PERIOD_S    (6 * 3600)

/* Dexcom G7 sampling cadence and reading freshness. */
#define GLY_CADENCE_DEFAULT_S       300
#define GLY_AGE_FRESH_MAX_MIN       6    /* Fresh reading: less than six minutes old. */
#define GLY_AGE_STALE_MIN           10   /* Stale reading: at least ten minutes old. */
#define GLY_DELTA_MIN_S             150  /* delta only between consecutive measures (2.5 to 7.5 min) */
#define GLY_DELTA_MAX_S             450

/* Glucose display/statistics ranges in mg/dL; warning thresholds live in settings. */
#define GLY_RANGE_VERY_LOW          54   /* accent color "tres basse" (view only) */
#define GLY_RANGE_LOW               70
#define GLY_RANGE_HIGH              180
#define GLY_RANGE_VERY_HIGH         250
#define GLY_STATS_MIN_COVERAGE_PCT  70   /* Below this coverage threshold, aggregate statistics are unreliable. */

/* Cadence Dexcom */
#define GLY_READING_PERIOD_S        300  /* one measure G7 every 5 minutes */
#define GLY_READING_MARGIN_S        20   /* period of publication via the telephone */
#define GLY_MIN_REQUEST_INTERVAL_S  30   /* never less than 30 s between two requests */
#define GLY_LOCKOUT_PAUSE_S         3600 /* Temporary account-blocking cooldown: 60 minutes. */
