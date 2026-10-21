/*
 * Reglages utilisateur persistants (NVS, espace "settings") : volume, voix, alertes, seuils, luminosite.
 * Les modifications sont enregistrees en differe (2 s) par une tache dediee : jamais depuis l'interface.
 */
/* Public interface / Interface publique
 * EN: Validated user preferences and delayed persistent storage.
 * FR: Préférences validées et sauvegarde différée.
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
    uint8_t volume_level;    /* volume_level_t, recalcule d'apres volume_pct */
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

/* Mode nuit : blob NVS separe (cle "nuit"), le blob des reglages ci-dessus reste inchange.
 * La luminosite jour est settings_t.brightness. */
typedef struct {
    bool enabled;            /* mode nuit selon l'horaire */
    bool black;              /* ancien choix "ecran noir" : garde pour le format NVS, sans effet (ecran nuit minimaliste) */
    uint16_t start_min;      /* debut, en minutes depuis minuit (22:00 = 1320) */
    uint16_t end_min;        /* fin (07:00 = 420) */
    uint8_t brightness;      /* luminosite nuit 1..100 % */
} night_settings_t;

/* Commandes vocales locales (ESP-SR, micros de la carte) : blob NVS separe (cle "vcmd") */
typedef struct {
    bool enabled;            /* ecoute des micros active */
    bool wake_word;          /* "Bonjour ESP" exige avant la commande */
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

/* Langue de l'interface : cle NVS separee (un octet), les autres blocs restent inchanges.
 * settings_lang_set() applique la langue immediatement et demande l'enregistrement. */
lang_t settings_lang_get(void);
void settings_lang_set(lang_t lang);
uint32_t settings_revision(void);
void settings_print(void); /* console : valeurs actuelles, sans secret */

int settings_level_volume(volume_level_t level);
