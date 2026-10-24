/*
 * voice_cmd_stub.c: Inactive voice-command implementation for the public build.
 * Design note: This stub must not activate microphone listening.
 */

/*
 * Commandes vocales ABSENTES de cette compilation (GLY_VOICE_CMD=OFF).
 *
 * Raison : la reconnaissance locale (ESP-SR MultiNet) exists only in English and Chinese. The sentences
 * French were approached with English phones, which gave scores barely above the
 * "New sensor" recognized in an English sentence of the
 * television). Aucun modele espagnol, allemand, italien ou portugais n'existe : la promesse "six langues"
 * could not be held properly without paid remote service. See docs/LIMITATIONS_FR.md.
 *
 * The ANNONCES PARLEES (microSD voice bank, six languages) and ALERTES SONORES ne sont pas concernees :
 * they continue to work. All actions are accessible to the touch.
 *
 * This file replaces voice_cmd.c : meme interface, aucune ecoute, aucun micro ouvert, aucun modele charge.
 */
/*
 * Public-build stubs keep touch and audio features while disabling voice-command listening.
 * Public compilation replacements: no listening to voice commands.
 */
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
