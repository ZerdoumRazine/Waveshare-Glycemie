/* glucose_math.h: Pure helpers for numerical glucose calculations.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Calculations on glycemia history (pure functions, without state):
 * age et fraicheur, delta entre mesures consecutives, cadence reelle, prochaine mesure, statistiques.
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
    GM_FRESH_NONE = 0, /* aucune mesure ou heure inconnue */
    GM_FRESH_OK,       /* A JOUR */
    GM_FRESH_LATE,     /* RETARD : la mesure attendue tarde */
    GM_FRESH_STALE,    /* DONNEES ANCIENNES */
} gm_freshness_t;


/* Age de la derniere mesure en secondes, -1 si inconnu */
int64_t gm_age_s(const glucose_point_t *pts, int count, int64_t now_ms, bool time_ok);

gm_freshness_t gm_freshness(int64_t age_s);

/* Real Cadence (median of recent consecutive intervals), GLY_CADENCE_DEFAULT_S a defaut */
int32_t gm_cadence_s(const glucose_point_t *pts, int count);

/* Delta entre deux mesures valides et consecutives (2,5 a 7,5 min d'ecart) */
bool gm_delta_between(const glucose_point_t *newer, const glucose_point_t *older, int *delta);

/* Delta affichable : mesures consecutives ET derniere mesure non ancienne */
bool gm_delta_current(const glucose_point_t *pts, int count, int64_t age_s, int *delta);

/* Heure estimee de la prochaine mesure (derniere mesure + cadence reelle), 0 si inconnue */
int64_t gm_next_expected_ms(const glucose_point_t *pts, int count);

/* Last Hours Statistics */
