/* glucose_math.h: Pure helpers for numerical glucose calculations.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Calculations on glycemia history (pure functions, without state):
 * age and freshness, delta between consecutive measures, real pace, next measurement, statistics.
 * The tables are sorted from the most recent to the oldest measure.
 */
/* Public interface / Interface publique
 * Pure calculations of freshness and deltas; safe for host tests.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_state.h"

typedef enum {
    GM_FRESH_NONE = 0, /* no measurement or time unknown */
    GM_FRESH_OK,       /* A JOUR */
    GM_FRESH_LATE,     /* RETARD : the measure expected late */
    GM_FRESH_STALE,    /* DONNEES ANCIENNES */
} gm_freshness_t;


/* Age of last measurement in seconds, -1 if unknown */
int64_t gm_age_s(const glucose_point_t *pts, int count, int64_t now_ms, bool time_ok);

gm_freshness_t gm_freshness(int64_t age_s);

/* Real Cadence (median of recent consecutive intervals), GLY_CADENCE_DEFAULT_S a defaut */
int32_t gm_cadence_s(const glucose_point_t *pts, int count);

/* Delta between two valid and consecutive measurements (2.5 to 7.5 min. */
bool gm_delta_between(const glucose_point_t *newer, const glucose_point_t *older, int *delta);

/* Delta displayable: consecutive measurements AND last non-old measure */
bool gm_delta_current(const glucose_point_t *pts, int count, int64_t age_s, int *delta);

/* Estimated time of next measurement (last measure + real rate), 0 if unknown */
int64_t gm_next_expected_ms(const glucose_point_t *pts, int count);

/* Last Hours Statistics */
