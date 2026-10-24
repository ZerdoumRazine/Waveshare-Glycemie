/*
 * voice_cmd_stub.c: Inactive voice-command implementation for the public build.
 * Design note: This stub must not activate microphone listening.
 */

/* The public build disables experimental local speech-command recognition.
 * ESP-SR's available model and tested phonetic approximations were not
 * reliable enough to promise six-language recognition without using paid
 * remote services. See docs/LIMITATIONS_EN.md.
 * This does not disable the six-language microSD speech announcements or
 * the standard audible alerts. All commands remain available by touch.
 * This stub preserves the voice_cmd.c API without opening microphones or
 * loading speech recognition models. */
/* Keep the public build's interface stable without enabling microphones. */
#include "voice_cmd.h"

#include <stdio.h>
#include "esp_log.h"
#include "i18n.h"

static const char *TAG = "voixcmd";

/* Report unavailable recognition without opening microphone hardware. */
esp_err_t voice_cmd_init(void)
{
    ESP_LOGI(TAG, "Commandes vocales absentes de cette compilation : micros jamais ouverts");
    return ESP_OK;
}

void voice_cmd_apply_settings(void)
{
}

bool voice_cmd_running(void)
{
    return false;
}

/* Tell the UI that voice commands are unavailable in this build. */
const char *voice_cmd_status_text(void)
{
    return tr(S_UNAVAILABLE);
}

void voice_cmd_ui_hint(char *buf, size_t size)
{
    if (buf != NULL && size > 0) {
        buf[0] = '\0';
    }
}

void voice_cmd_print_info(void)
{
    printf("VCMD absentes de cette compilation (GLY_VOICE_CMD=OFF)\n");
}

void voice_cmd_simulate(int mn_id, float prob)
{
    (void)mn_id;
    (void)prob;
    printf("VCMD absentes de cette compilation\n");
}

bool voice_cmd_inject(const uint8_t *pcm, size_t bytes)
{
    (void)pcm;
    (void)bytes;
    return false;
}

void voice_cmd_set_level_log(bool on)
{
    (void)on;
}

void voice_cmd_set_decode_log(bool on)
{
    (void)on;
}

void voice_cmd_add_phrase(int mn_id, const char *phonemes)
{
    (void)mn_id;
    (void)phonemes;
    printf("VCMD absentes de cette compilation\n");
}
