/*
 * Langue de l'interface (6 langues europeennes).
 *
 * Seuls les TEXTES AFFICHES sont traduits. Les annonces vocales restent en francais :
 * la banque vocale de la microSD (voix Google Chirp 3 HD) n'existe qu'en francais, et
 * la reconnaissance vocale locale garde ses phrases d'origine. Les journaux, la console
 * et les fichiers CSV restent eux aussi en francais, pour que les enregistrements
 * anciens et nouveaux restent comparables.
 *
 * La langue est enregistree dans la NVS (espace "settings", cle "lang" : un seul octet,
 * separe du bloc "v1" des autres reglages qui n'est donc jamais invalide).
 */
/* Public interface / Interface publique
 * EN: Language IDs and text lookup across the six supported UI languages.
 * FR: Identifiants de traduction des six langues.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    LANG_FR = 0,
    LANG_EN,
    LANG_ES,
    LANG_DE,
    LANG_IT,
    LANG_PT,
    LANG_COUNT,
} lang_t;

typedef enum {
    /* Navigation et bandeau */
    S_NAV_HOME = 0, S_NAV_GRAPH, S_NAV_TODAY, S_NAV_CONSUM, S_NAV_SETTINGS,
    S_DEMO_MODE, S_TIME_NOT_SYNCED,

    /* Fraicheur de la donnee */
    S_FRESH_OK, S_FRESH_LATE, S_FRESH_STALE, S_FRESH_WAITING,

    /* Tendances (ordre de trend_t, l'indice 0 est vide) */
    S_TREND_UP_FAST, S_TREND_UP, S_TREND_UP_SLOW, S_TREND_FLAT,
    S_TREND_DOWN_SLOW, S_TREND_DOWN, S_TREND_DOWN_FAST,
    S_TREND_UNKNOWN, S_TREND_OUT_OF_RANGE, S_TREND_DASH,

    /* Accueil : unite, attente et causes d'une donnee manquante ou ancienne */
    S_UNIT_MGDL, S_NO_READING,
    S_WIFI_NOT_CONFIGURED, S_WIFI_DOWN, S_DEXCOM_REFUSED, S_DEXCOM_LOCKED,
    S_DEXCOM_CHECK, S_DEXCOM_UNAVAILABLE, S_INTERNET_DOWN, S_NO_NEW_READING,
    S_WIFI_SETUP_NEEDED, S_WIFI_CONNECTING, S_DEXCOM_NO_CREDENTIALS,
    S_DEXCOM_WAITING_AUTH, S_DEXCOM_RETRYING, S_NO_READING_24H, S_DEXCOM_CONNECTING,

    /* Graphique */
    S_GRAPH_HINT, S_NO_MEASURE,

    /* Page AUJOURD'HUI */
    S_TODAY_TITLE, S_SINCE_MIDNIGHT, S_DAY_BAND,
    S_LEG_TARGET, S_LEG_LOW, S_LEG_HIGH, S_LEG_NODATA,
    S_TIR, S_RANGE_70_180, S_AVERAGE, S_MINIMUM, S_MAXIMUM, S_COVERAGE,
    S_TIME_LOW, S_UNDER_70, S_TIME_HIGH, S_OVER_180,
    S_EPISODES_LOW, S_EPISODES_HIGH, S_AT_LEAST_15MIN, S_MEASURES,

    /* Consommables */
    S_SENSOR_G7, S_SENSOR_SUB, S_NEW_SENSOR, S_INFUSION_SITE, S_NEW_CATHETER,
    S_STOCK_SENSORS, S_STOCK_CATHETERS, S_STOCK_LOW, S_NOT_SET, S_SET_IT,
    S_NOT_RECORDED, S_BUTTON_BELOW, S_NO_SENSOR_RECORDED, S_NO_CATHETER_RECORDED,
    S_TO_CHANGE, S_PLACED, S_CHANGED, S_DAY_N_OF, S_TIME_LEFT, S_DUE,
    S_OVERDUE_SINCE, S_NEW_SENSOR_NOW, S_NEW_CATHETER_NOW,
    S_RESTART_DAY1_SENSOR, S_RESTART_DAY1_CATHETER,
    S_CANCEL, S_YES_NOW, S_VALIDATE, S_TIME_NOT_SYNCED_NO_RECORD, S_START_TIME_LABEL, S_CATHETER_SUB,

    /* Reglages */
    S_VOLUME_TITLE, S_VOLUME_PCT, S_VOLUME_MUTED,
    S_MUTE, S_LOW, S_MEDIUM, S_HIGH, S_MAX,
    S_TEST_SOUND, S_TEST_VOICE, S_VOICE_ANNOUNCE,
    S_ALERTS_TITLE, S_ALERTS_SOUND, S_RAMP, S_VERYLOW_IN_MUTE,
    S_THRESHOLDS, S_THR_VERY_LOW, S_THR_LOW, S_THR_HIGH, S_THR_VERY_HIGH,
    S_SCREEN_TITLE, S_DAY_BRIGHTNESS,
    S_NIGHT_TITLE, S_NIGHT_ENABLE, S_NIGHT_START, S_NIGHT_END, S_NIGHT_BRIGHTNESS,
    S_NIGHT_TEST, S_NIGHT_HELP,
    S_CONSUM_TITLE, S_CATHETER_REMINDER, S_CATHETER_HELP,
    S_VOICECMD_TITLE, S_VOICECMD_ENABLE, S_WAKEWORD, S_VOICECMD_HELP,
    S_INFO_TITLE, S_LANG_TITLE, S_LANG_HELP,
    S_INFO_VERSION, S_INFO_WIFI, S_INFO_DEXCOM, S_INFO_SD, S_INFO_AUDIO,
    S_CONNECTED, S_DISCONNECTED, S_CONNECTED_EUROPE, S_NOT_CONNECTED, S_READY,
    S_STATE, S_UNAVAILABLE, S_WAKEWORD_NOTE, S_SD_OK_FMT, S_SD_ABSENT, S_SD_UNSUPPORTED_FMT, S_SD_UNSUPPORTED, S_SD_ERROR, S_SD_INIT, S_VOICE_FILES_FMT, S_VOICE_ABSENT,

    /* Alertes */
    S_ALERT_VERY_LOW, S_ALERT_LOW, S_ALERT_NO_DATA, S_ALERT_VERY_HIGH, S_ALERT_HIGH,
    S_ALERT_SENSOR, S_ALERT_CATHETER, S_ALERT_TEST, S_ALERT_TEST_SUFFIX, S_ALERT_TEST_NOTE,
    S_ALERT_OK, S_ALERT_SNOOZE, S_ALERT_NO_DATA_MSG,
    S_REMIND_SENSOR_24H, S_REMIND_SENSOR_2H, S_REMIND_CATHETER_2H,

    /* Ecran nuit */
    S_NIGHT_PREVIEW, S_LAST_READING,

    /* Durees, age et dates courtes (les specificateurs %d doivent rester dans le meme ordre) */
    S_DUR_DH, S_DUR_HM, S_DUR_H, S_DUR_MIN,
    S_AGE_UNKNOWN, S_AGE_LESS_1MIN, S_AGE_MIN, S_AGE_HM, S_DATE_DAY_HM,

    /* Assistant de configuration (premiere mise en route) et section CONNEXION des reglages */
    S_SETUP_WELCOME, S_SETUP_INTRO, S_SETUP_LANG, S_SETUP_START, S_SETUP_DEMO_TRY,
    S_SETUP_WIFI_TITLE, S_SETUP_WIFI_HELP, S_SETUP_SCANNING, S_SETUP_NO_NETWORK, S_SETUP_RESCAN,
    S_SETUP_HIDDEN, S_SETUP_NETWORK_NAME, S_SETUP_WIFI_PASS, S_SETUP_SHOW,
    S_SETUP_DEXCOM_TITLE, S_SETUP_DEXCOM_HELP, S_SETUP_REGION, S_SETUP_USER, S_SETUP_PASSWORD,
    S_SETUP_CHECK_TITLE, S_SETUP_CHECK_WIFI, S_SETUP_CHECK_NET, S_SETUP_CHECK_DEX,
    S_SETUP_OK, S_SETUP_RESTART, S_SETUP_FAIL_WIFI, S_SETUP_FAIL_NET, S_SETUP_FAIL_DEX_AUTH,
    S_SETUP_FAIL_DEX_LOCK, S_SETUP_FAIL_DEX_NET, S_SETUP_FAIL_OTHER, S_SETUP_RETRY, S_SETUP_BACK,
    S_SETUP_NEXT, S_SETUP_FINISH, S_SETUP_DEMO_NOTE, S_SETUP_EMPTY,
    S_CONN_TITLE, S_CONN_WIFI, S_CONN_DEXCOM, S_CONN_MODIFY, S_CONN_ERASE, S_CONN_ERASE_ASK,
    S_CONN_ERASE_HELP, S_CONN_DEMO, S_CONN_HELP, S_CONN_NOT_SET,
    /* Essai de nouveaux acces : les anciens restent enregistres jusqu'a la confirmation */
    S_SETUP_CONFIRM, S_SETUP_KEEP_OLD, S_SETUP_KEPT_OLD, S_CONN_SAVE, S_CONN_TRIAL,

    S_COUNT,
} str_id_t;

/* Langue courante (LANG_FR par defaut, avant toute lecture de la NVS) */
lang_t i18n_lang(void);
void i18n_set_lang(lang_t lang);

/* Texte traduit ; ne renvoie jamais NULL (repli sur le francais si une traduction manque) */
const char *tr(str_id_t id);

/* Toujours le texte francais, quelle que soit la langue choisie : journal de la microSD,
 * console et fichiers CSV, pour que les enregistrements restent comparables dans le temps. */
const char *tr_fr(str_id_t id);

/* Nom de la langue dans la langue elle-meme ("Francais", "English"...) et code ISO court */
const char *lang_name(lang_t lang);
const char *lang_code(lang_t lang);

/* "Jeudi 15 octobre" / "Thursday 15 October" / "Donnerstag, 15. Oktober"... */
void i18n_format_date_long(const struct tm *tm_local, char *buf, size_t size);

/* Nom du jour de la semaine (0 = dimanche), en minuscules dans les langues qui l'exigent */
const char *i18n_day_name(int wday);
