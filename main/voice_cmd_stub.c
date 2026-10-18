/*
 * Commandes vocales ABSENTES de cette compilation (GLY_VOICE_CMD=OFF).
 *
 * Raison : la reconnaissance locale (ESP-SR MultiNet) n'existe qu'en anglais et en chinois. Les phrases
 * francaises etaient approchees avec des phonemes anglais, ce qui donnait des scores a peine au-dessus du
 * plancher et de vrais declenchements intempestifs ("New sensor" reconnu dans une phrase anglaise de la
 * television). Aucun modele espagnol, allemand, italien ou portugais n'existe : la promesse "six langues"
 * ne pouvait pas etre tenue proprement sans service distant payant. Voir docs/LIMITATIONS_FR.md.
 *
 * Les ANNONCES PARLEES (banque vocale microSD, six langues) et les ALERTES SONORES ne sont pas concernees :
 * elles continuent de fonctionner. Toutes les actions sont accessibles au toucher.
 *
 * Ce fichier remplace voice_cmd.c : meme interface, aucune ecoute, aucun micro ouvert, aucun modele charge.
 */
#include "voice_cmd.h"

#include <stdio.h>
#include "esp_log.h"
#include "i18n.h"

static const char *TAG = "voixcmd";

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
