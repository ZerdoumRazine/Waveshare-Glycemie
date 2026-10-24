/* voice_cmd.h: Experimental voice-command recognition API.
 * Shared declarations for the implementation.
 */
/* Experimental ESP-SR voice recognition on the device's local microphones.
 * The standard public firmware disables this subsystem.
 *
 * MultiNet7 recognizes a small set of phonetic command candidates, including
 * glucose readback and changes to sensor or infusion-set dates. A sensor
 * replacement is a two-stage operation: the first detection announces
 * the pending action, and a different confirmation command is required
 * within ten seconds. Infusion-set replacement likewise needs an explicit
 * second detection. Unconfirmed requests never change NVS state.
 *
 * The recognition models are loaded from the dedicated flash partition.
 * Recognition is suspended during device audio playback to reduce false
 * triggers. Recognition tasks never write NVS directly: confirmed actions
 * are handed to the LVGL task for execution.
 *
 * Historical language and scoring experiments are not evidence that voice
 * commands are safe for clinical decisions. All public functions are
 * accessible through touch controls without recognition enabled. */
/* Optional recognition control and diagnostics; disabled in public builds. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    VOICE_CMD_NONE = 0,
    VOICE_CMD_GLUCOSE,   /* "Glycemie" */
    VOICE_CMD_SENSOR,    /* A sensor change requires two different consecutive command detections. */
    VOICE_CMD_CATHETER,  /* Infusion-set change requires a confirmation window. */
} voice_cmd_id_t;

/* At the start, after audio_out_init() : start listening if it is active in the settings */
esp_err_t voice_cmd_init(void);

/* Apply recognition settings from Settings or serial diagnostics. */
void voice_cmd_apply_settings(void);

bool voice_cmd_running(void);

/* Text of the upper headband tablet ("" if no current order) ; spot LVGL */
void voice_cmd_ui_hint(char *buf, size_t size);

/* Human-readable recognition status for the interface. */
const char *voice_cmd_status_text(void);

/* Console */
void voice_cmd_print_info(void);
/* Inject a simulated command through the ordinary confidence and confirmation filters. */
void voice_cmd_simulate(int mn_id, float prob);
/* Inject 16-bit mono PCM test audio at 16 kHz into the microphone path. */
bool voice_cmd_inject(const uint8_t *pcm, size_t bytes);
void voice_cmd_set_level_log(bool on);        /* microphone level every second */
void voice_cmd_set_decode_log(bool on);       /* MultiNet decode string (phone fixes) */
void voice_cmd_add_phrase(int mn_id, const char *phonemes); /* variante temporaire (perdue au redemarrage) */
