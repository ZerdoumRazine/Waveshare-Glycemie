/* voice_cmd.h: Experimental voice-command recognition API.
 * Shared declarations for the implementation.
 */
/*
 * Commandes vocales LOCALES with microphones ES7210 de la carte (ESP-SR 2.5.3 Espressive, no recognition in
 * cloud, no connection).
 *
 * - Commandes : "GLYCEMIE", "ENREGISTRER NOUVEAU CAPTEUR" puis "CONFIRMER CAPTEUR" (K19), "NOUVEAU CATHETER",
 *   reconnues par MultiNet7 (mn7_en) from phonemes written for the French pronunciation (Espressive
 *   provides neither French MultiNet nor French WakeNet personalized self-service), and at the same time
 *   equivalents anglais "GLUCOSE" et "NEW CATHETER" Answers always in French. K19 (2026-09-21): sentences
 *   courtes "NOUVEAU CAPTEUR" / "NEW SENSOR" are withdrawn ("New sensor" provoked the spontaneous announcements).
 *   Option : mot d'eveil officiel francais WakeNet9L "Bonjour ESP" before the order.
 * - Models in the "model" partition (srmodels.bin generate and flash by the esp-sr component, place after the
 *   original scores, at 16 MB): read once in PSRAM puis charges par srmodel_load() ; aucune
 *   depends on microSD. First listens 60 s after the powering; Never start automatically after
 *   panic or watchdog (no restart loop possible).
 * - "GLYCEMIE": immediate response with the Chirp voice of microSD (value + trend, or "Old data").
 * - Capteur (K19) : "ENREGISTRER NOUVEAU CAPTEUR" does not alter anything; answer "New sensor. Say confirm
 *   capteur." ; seule la phrase DIFFERENTE "CONFIRMER CAPTEUR", heard in the 10 s after the end of
 *   l'annonce, enregistre l'heure actuelle (NVS, historique microSD) ; sinon annulation automatique, puis
 *   "ENREGISTRER NOUVEAU CAPTEUR" ignore pendant 5 min (aucune annonce repetee). Seuils propres a ces deux phrases,
 *   more stringent than reading commands. Touch button NOUVEAU CAPTEUR ne depend pas de la voix.
 * - "NOUVEAU CATHETER": a single detection does not change JAMAIS a meter; answer "Repeal to confirm",
 *   second detection of the same command within 10 seconds of the end of the sentence -> current time
 *   enregistree, "... enregistre" ; sinon annulation automatique.
 * - Detection suspended during any audio playback (alerts, voices, beeps) and 0.6 seconds after: the device does not react
 *   has his own voice. Models reset to zero at recovery.
 * - Marks: reading the microphones and detecting in the "voicecmd" spot PSRAM) ; l'enregistrement NVS est fait
 *   par la tache LVGL (ui_request_consumable_new).
 */
/* Public interface / Interface publique
 * Optional experimental recognition API; not enabled in the public firmware.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    VOICE_CMD_NONE = 0,
    VOICE_CMD_GLUCOSE,   /* "Glycemie" */
    VOICE_CMD_SENSOR,    /* capteur : "Enregistrer nouveau capteur" puis "Confirmer capteur" */
    VOICE_CMD_CATHETER,  /* "Nouveau catheter" */
} voice_cmd_id_t;

/* At the start, after audio_out_init() : start listening if it is active in the settings */
esp_err_t voice_cmd_init(void);

/* Modified settings (REGISTRATION or console): start / stop listening, apply the cover word and gain */
void voice_cmd_apply_settings(void);

bool voice_cmd_running(void);

/* Texte de la pastille du bandeau superieur ("" si aucune commande en cours) ; tache LVGL */
void voice_cmd_ui_hint(char *buf, size_t size);

/* State COMMANDES VOCALES) : "a l'ecoute", "arretees", "erreur ..." */
const char *voice_cmd_status_text(void);

/* Console */
void voice_cmd_print_info(void);
/* Simultaneous detection of a MultiNet sentence (1 glycemia, 3 catheter, 4 glucose, 5 record new sensor,
 * 6 confirm sensor) with its probability: same path as a true detection (threshold, confirmation, anti-repetition) */
void voice_cmd_simulate(int mn_id, float prob);
/* Test : son PCM 16 bits mono 16 kHz adds to the signal microphones in real time (console vrx_inject) */
bool voice_cmd_inject(const uint8_t *pcm, size_t bytes);
void voice_cmd_set_level_log(bool on);        /* microphone level every second */
void voice_cmd_set_decode_log(bool on);       /* MultiNet decode string (phone fixes) */
void voice_cmd_add_phrase(int mn_id, const char *phonemes); /* variante temporaire (perdue au redemarrage) */
