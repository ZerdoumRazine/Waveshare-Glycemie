/* audio_out.h: Audio playback settings and alert tone API.
 * FR: API audio, volume et bips d'alerte.
 * Keep this header focused on the contracts used by other modules.
 */
/*
 * Sortie audio : codec ES8311 + I2S + amplificateur de la carte, exclusivement via le BSP Waveshare
 * (bsp_audio_codec_speaker_init) et esp_codec_dev. Aucune broche definie ici.
 *
 * Une tache dediee joue des sequences (tonalites synthetisees, extraits PCM) sans bloquer l'appelant.
 * Le codec n'est ouvert que pendant la lecture (amplificateur coupe au repos).
 */
/* Public interface / Interface publique
 * EN: Non-blocking audio requests and codec diagnostic controls.
 * FR: Requêtes audio et contrôles du codec.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define AUDIO_SAMPLE_RATE 16000

typedef enum {
    AUDIO_PATTERN_TEST = 0,     /* bip court de test */
    AUDIO_PATTERN_VERY_LOW,     /* critique : triple bip aigu rapide */
    AUDIO_PATTERN_LOW,          /* double bip */
    AUDIO_PATTERN_HIGH,         /* bip long grave */
    AUDIO_PATTERN_VERY_HIGH,    /* deux bips longs */
    AUDIO_PATTERN_NO_DATA,      /* deux bips graves courts */
    AUDIO_PATTERN_SENSOR,       /* carillon montant */
    AUDIO_PATTERN_COUNT,
} audio_pattern_t;

esp_err_t audio_out_init(void);
bool audio_out_ready(void);

/* File d'attente (non bloquant) ; volume 0..100 */
esp_err_t audio_out_play_pattern(audio_pattern_t pattern, int volume_pct);
esp_err_t audio_out_play_tone(int freq_hz, int duration_ms, int volume_pct);
esp_err_t audio_out_play_pcm(const int16_t *samples, size_t count, int volume_pct); /* mono 16 kHz, memoire persistante */

/* Lecture preparee dans la tache audio (acces microSD permis, jamais dans l'interface ni sous un verrou d'alerte) :
 * fn(a, b) est appelee a son tour dans la file et joue ses sons avec les fonctions audio_out_render_*. */
typedef void (*audio_render_fn_t)(int a, int b);
esp_err_t audio_out_play_render(audio_render_fn_t fn, int a, int b, int volume_pct);

/* Uniquement depuis une fonction de rendu : false si la lecture a ete arretee (acquittement, rappel) */
bool audio_out_render_pcm(const int16_t *samples, size_t count); /* mono 16 kHz */
bool audio_out_render_silence(int ms);
bool audio_out_render_pattern(audio_pattern_t pattern);

/* Arrete la lecture en cours et vide la file */
void audio_out_stop(void);
bool audio_out_busy(void);

/* Volume numerique du DAC ES8311 (dB) correspondant a un volume general 0..100 % (-96 = muet) */
float audio_out_volume_db(int volume_pct);
void audio_out_print_info(void); /* console */

/* Micros ES7210 (BSP) reserves par les commandes vocales : NULL si deja reserves ou indisponibles. Pendant la
 * reservation, l'autotest et la mesure au micro sont refuses. */
void *audio_out_mic_acquire(void); /* esp_codec_dev_handle_t */
void audio_out_mic_release(void);

/* Autotest : joue une tonalite et verifie sa presence avec les micros de la carte (ES7210 via le BSP) */
esp_err_t audio_out_selftest(int volume_pct, bool *detected, float *gain_db);

/* Mesure comparative au micro de la carte (instrument relatif, meme position) : bip 1 kHz si play_voice est NULL,
 * sinon la lecture lancee par play_voice (TEST VOIX). Gain micro fixe pour comparer les volumes entre eux. */
typedef void (*audio_playback_fn_t)(int volume_pct);
esp_err_t audio_out_measure(int volume_pct, audio_playback_fn_t play_voice, float mic_gain_db);

/* Registres ES8311 et broche de l'amplificateur relus pendant un bip 1 kHz (console) */
void audio_out_dump_regs(int volume_pct);
