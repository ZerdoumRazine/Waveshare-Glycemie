/* settings.h: Persistent user preferences and threshold contracts.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Persistent user settings (NVS, espace "settings") : volume, voix, alertes, seuils, luminosite.
 * The changes are saved in different (2 s) by a dediee spot: never from the interface.
 */
/* Public interface / Interface publique
 * Validated user preferences and delayed persistent storage.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "i18n.h"

typedef enum {
    VOLUME_MUTE = 0,
    VOLUME_LOW,
    VOLUME_MEDIUM,
    VOLUME_HIGH,
    VOLUME_MAX,
    VOLUME_CUSTOM,
} volume_level_t;

typedef struct {
    uint8_t volume_level;    /* volume_level_t, recalculated after volume_pct */
    uint8_t volume_pct;      /* volume general effectif 0..100 (enregistre en NVS) */
    bool voice;              /* annonces vocales */
    bool alerts;             /* alertes sonores */
    bool ramp;               /* volume croissant si alerte critique non acquittee */
    bool very_low_always;    /* alerte tres basse audible meme en MUTE */
    uint16_t thr_very_low;   /* mg/dL */
    uint16_t thr_low;
    uint16_t thr_high;
    uint16_t thr_very_high;
    uint8_t brightness;      /* 10..100 % */
} settings_t;

/* Mode nuit : blob NVS The above adjustment blob remains unchanged.
 * La luminosite jour est settings_t.brightness. */
typedef struct {
    bool enabled;            /* mode nuit selon l'horaire */
    bool black;              /* old choice "black screen" : guard for format NVS, without effect (minimalist night screen) */
    uint16_t start_min;      /* debut, en minutes depuis minuit (22:00 = 1320) */
    uint16_t end_min;        /* fin (07:00 = 420) */
    uint8_t brightness;      /* luminosite nuit 1..100 % */
} night_settings_t;

/* Commandes vocales locales (ESP-SR, micros de la carte) : blob NVS separe (cle "vcmd") */
typedef struct {
    bool enabled;            /* Listen to active microphones */
    bool wake_word;          /* "Bonjour ESP" requires before ordering */
    uint8_t mic_gain_db;     /* gain analogique ES7210 (dB) */
    uint8_t reserved;
} voice_cmd_settings_t;

esp_err_t settings_init(void);
void settings_get(settings_t *out);
void settings_set(const settings_t *in);
void settings_night_get(night_settings_t *out);
void settings_night_set(const night_settings_t *in);
void settings_vcmd_get(voice_cmd_settings_t *out);
void settings_vcmd_set(const voice_cmd_settings_t *in);

/* Langue de l'interface : cle NVS separated (one byte), the other blocks remain unchanged.
 * settings_lang_set() applique la langue immediatement et demande l'enregistrement. */
lang_t settings_lang_get(void);
void settings_lang_set(lang_t lang);
uint32_t settings_revision(void);
void settings_print(void); /* console: current values, without secrecy */

int settings_level_volume(volume_level_t level);
