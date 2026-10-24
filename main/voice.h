/* voice.h: Speech clip selection and fallback interface.
 * Shared definitions for the firmware implementation.
 */
/*
 * French Voice Announcements LOCALES (no Internet dependence, no Google calls from the map):
 *
 * - main bank: files WAV MicroSD. The French is at the root,
 *   /glycemie/audio/voice/ ; other languages in a subfolder by code ISO
 *   (/glycemie/audio/voice/en/, /es/, /de/, /it/, /pt/). If the bank of the chosen language
 *   is not installed, the French bank takes over rather than silence.
 *   (PCM 16 bits mono 16 kHz, Google voice
 *   en-FR-Chirp3-HD-Aoede generee once on PC by tools/voice/gen_voice_google.py). An alert plays
 *   the warning sentence and then the value: alert_low.wav + glucose_078.wav ("Warning, low glycemia. Glycemie
 *   seventy-eight."). File missing or card unavailable: no voices, the alert beep already plays serves as
 *   Help. The files are read in the audio spot, never in the interface.
 * - old bank integrated with firmware (Windows voice en-FR, numbers composed word to word): preserved
 *   temporarily until validation of the new voice, played only by the console (voice_old).
 */
/* Public interface / Interface publique
 * PCM announcement playback and microSD language fallback.
 */
#pragma once


#include <stdbool.h>
#include "alerts.h"

/* Changing language: the voice bank of the new language will be searched again */
void voice_lang_changed(void);

/* Former integrated bank present in firmware */
bool voice_available(void);

/* Advertisement associated with an alert (phrase + potential value), play after the alert sound */
void voice_announce_alert(alert_type_t type, int value, int volume_pct);

/* TEST VOIX : "Glycemie <nombre>." (glucose_NNN.wav, 120 for the button); test beep if missing file */
void voice_test(int number, int volume_pct);

/* Responses to voice commands (vc *.wav); test beep if missing file */
typedef enum {
    VOICE_REPLY_SENSOR_CONFIRM = 0, /* "New sensor. Repeat to confirm." */
    VOICE_REPLY_SENSOR_SAVED,       /* "Nouveau capteur enregistre." */
    VOICE_REPLY_CATHETER_CONFIRM,   /* "New catheter. Repeat to confirm." */
    VOICE_REPLY_CATHETER_SAVED,     /* "Nouveau catheter enregistre." */
    VOICE_REPLY_CANCELLED,          /* "Commande annulee." */
    VOICE_REPLY_TIME_UNAVAILABLE,   /* "Heure non synchronisee. Enregistrement impossible." */
    VOICE_REPLY_SENSOR_ASK_CONFIRM, /* K19 : "Nouveau capteur. Dites confirmer capteur." */
    VOICE_REPLY_COUNT,
} voice_reply_t;
void voice_reply(voice_reply_t reply, int volume_pct);

/* Glycemie has the request: "Glycemie cent quinze." then the trend ("Stable.", "En hausse."...) old data:
 * "Old data. Last measure:" then the value, without trend; no action: vc_no_data.wav */
void voice_say_glucose(int mgdl, uint8_t trend, bool stale, bool available, int volume_pct);

/* Former integree voice (console): warning phrase, or type ALERT_TEST Volume test. Glycemie <value>
 * milligrammes par decilitre." */
void voice_old_announce(alert_type_t type, int value, int volume_pct);
