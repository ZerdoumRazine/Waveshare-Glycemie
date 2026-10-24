/* voice.h: Speech clip selection and fallback interface.
 * Shared definitions for the firmware implementation.
 */
/* Speech announcements use optional prerecorded WAV files stored on microSD.
 * The French bank lives directly in /glycemie/audio/voice; English, Spanish,
 * German, Italian and Portuguese use language-code subfolders. A missing
 * selected-language bank falls back to French, while absent WAV files leave
 * the supplementary audible alert beeps operational. Files use 16-bit mono
 * PCM at 16 kHz and are loaded by the dedicated audio task, never LVGL.
 * The old embedded voice-bank experiment is restricted to diagnostics. */
/* Speech-clip lookup, playback and language fallback APIs. */
#pragma once


#include <stdbool.h>
#include "alerts.h"

/* Changing language: the voice bank of the new language will be searched again */
void voice_lang_changed(void);

/* Former integrated bank present in firmware */
bool voice_available(void);

/* Speak an alert phrase, then an optional glucose value after its tone. */
void voice_announce_alert(alert_type_t type, int value, int volume_pct);

/* Voice test reads a sample glucose phrase; missing clips fall back to a beep. */
void voice_test(int number, int volume_pct);

/* Responses to voice commands (vc *.wav); test beep if missing file */
typedef enum {
    VOICE_REPLY_SENSOR_CONFIRM = 0, /* "New sensor. Repeat to confirm." */
    VOICE_REPLY_SENSOR_SAVED,       /* Confirmation response after saving a sensor replacement. */
    VOICE_REPLY_CATHETER_CONFIRM,   /* "New catheter. Repeat to confirm." */
    VOICE_REPLY_CATHETER_SAVED,     /* Confirmation response after saving an infusion-set replacement. */
    VOICE_REPLY_CANCELLED,          /* Cancellation response for an abandoned voice operation. */
    VOICE_REPLY_TIME_UNAVAILABLE,   /* An unsynchronized clock prevents a dated replacement from being saved. */
    VOICE_REPLY_SENSOR_ASK_CONFIRM, /* First stage of the historical sensor-change confirmation sequence. */
    VOICE_REPLY_COUNT,
} voice_reply_t;
void voice_reply(voice_reply_t reply, int volume_pct);

/* Speak the latest glucose value and trend; for stale data, identify
 * its age and omit any misleading current-trend announcement. */
void voice_say_glucose(int mgdl, uint8_t trend, bool stale, bool available, int volume_pct);

/* Historical embedded voice test, available only through diagnostics. */
void voice_old_announce(alert_type_t type, int value, int volume_pct);
