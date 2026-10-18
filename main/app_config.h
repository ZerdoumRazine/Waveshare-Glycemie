/*
 * glycemie_display - constantes de l'application.
 * Afficheur familial complementaire : ce n'est pas un dispositif medical.
 */
#pragma once

#define GLY_APP_NAME                "glycemie_display"

/* Heure locale France (heure d'ete automatique) */
#define GLY_TIMEZONE                "CET-1CEST,M3.5.0,M10.5.0/3"
#define GLY_SNTP_SERVER_1           "pool.ntp.org"
#define GLY_SNTP_SERVER_2           "time.cloudflare.com"

/* Test Internet neutre (sans Dexcom) : reponse HTTP 204 attendue */
#define GLY_INTERNET_PROBE_URL      "https://www.gstatic.com/generate_204"
#define GLY_INTERNET_PROBE_HOST     "shareous1.dexcom.com"

/* Historique en memoire : 24 h = 288 mesures (une toutes les 5 min) */
#define GLY_HISTORY_MAX             288
#define GLY_HISTORY_FULL_MINUTES    1440
#define GLY_BACKFILL_MINUTES        180      /* rattrapage des mesures publiees en retard */
#define GLY_BACKFILL_PERIOD_S       3600
#define GLY_FULL_RESYNC_PERIOD_S    (6 * 3600)

/* Cadence G7 et fraicheur */
#define GLY_CADENCE_DEFAULT_S       300
#define GLY_AGE_FRESH_MAX_MIN       6    /* < 6 min  : A JOUR */
#define GLY_AGE_STALE_MIN           10   /* >= 10 min : DONNEES ANCIENNES */
#define GLY_DELTA_MIN_S             150  /* delta seulement entre mesures consecutives (2,5 a 7,5 min) */
#define GLY_DELTA_MAX_S             450

/* Plages d'affichage et statistiques (mg/dL) ; les seuils d'alerte sont dans les reglages */
#define GLY_RANGE_VERY_LOW          54   /* couleur d'accent "tres basse" (affichage seulement) */
#define GLY_RANGE_LOW               70
#define GLY_RANGE_HIGH              180
#define GLY_RANGE_VERY_HIGH         250
#define GLY_STATS_MIN_COVERAGE_PCT  70   /* en dessous : statistiques non fiables */

/* Cadence Dexcom */
#define GLY_READING_PERIOD_S        300  /* une mesure G7 toutes les 5 min */
#define GLY_READING_MARGIN_S        20   /* delai de publication via le telephone */
#define GLY_MIN_REQUEST_INTERVAL_S  30   /* jamais moins de 30 s entre deux requetes */
#define GLY_LOCKOUT_PAUSE_S         3600 /* compte bloque temporairement : 60 min */
