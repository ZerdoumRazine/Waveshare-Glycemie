/*
 * settings.c: Persistent preferences, thresholds and display options.
 * Design note: Updating one setting must not reset unrelated values.
 */

/*
 * Validates UI options loaded from NVS and batches non-urgent writes.
 * Validates saved settings and avoids writing NVS inutiles.
 */
#include "settings.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

static const char *TAG = "reglages";

#define NVS_NS        "settings"
#define NVS_KEY       "v1"
#define NVS_KEY_NIGHT "nuit"
#define NVS_KEY_VCMD  "vcmd"
#define NVS_KEY_LANG  "lang" /* un seul octet, separe du bloc "v1" */

static SemaphoreHandle_t s_mutex;
static settings_t s_settings;
static night_settings_t s_night;
static voice_cmd_settings_t s_vcmd;

static const voice_cmd_settings_t k_vcmd_defaults = {
    .enabled = false, /* after-memory activation (REGULATIONS or console vcmd_on) */
    .wake_word = false,
    .mic_gain_db = 30,
};
static volatile uint32_t s_revision;
static TaskHandle_t s_saver;

static const night_settings_t k_night_defaults = {
    .enabled = false,
    .black = false,
    .start_min = 22 * 60,
    .end_min = 7 * 60,
    .brightness = 10,
};

static const settings_t k_defaults = {
    .volume_level = VOLUME_LOW, /* premier demarrage : volume faible */
    .volume_pct = 35,
    .voice = true,
    .alerts = true,
    .ramp = true,
    .very_low_always = true,
    .thr_very_low = 55,
    .thr_low = 70,
    .thr_high = 250,
    .thr_very_high = 300,
    .brightness = 100,
};

int settings_level_volume(volume_level_t level)
{
    switch (level) {
    case VOLUME_MUTE:
        return 0;
    case VOLUME_LOW:
        return 35;
    case VOLUME_MEDIUM:
        return 60;
    case VOLUME_HIGH:
        return 85;
    case VOLUME_MAX:
        return 100;
    default:
        return -1;
    }
}

/* Clamp persisted values so an invalid record cannot break a control. */
static void sanitize(settings_t *s)
{
    if (s->volume_pct > 100) {
        s->volume_pct = 100;
    }
    /* Le pourcentage fait foi : bouton allume seulement s'il correspond exactement a un prereglage */
    s->volume_level = VOLUME_CUSTOM;
    for (int level = VOLUME_MUTE; level <= VOLUME_MAX; level++) {
        if (settings_level_volume((volume_level_t)level) == s->volume_pct) {
            s->volume_level = (uint8_t)level;
        }
    }
    if (s->brightness < 10 || s->brightness > 100) {
        s->brightness = 100;
    }
    if (s->thr_very_low < 40 || s->thr_very_low > 70) {
        s->thr_very_low = k_defaults.thr_very_low;
    }
    if (s->thr_low <= s->thr_very_low || s->thr_low > 100) {
        s->thr_low = k_defaults.thr_low > s->thr_very_low ? k_defaults.thr_low : s->thr_very_low + 5;
    }
    if (s->thr_high < 120 || s->thr_high > 350) {
        s->thr_high = k_defaults.thr_high;
    }
    if (s->thr_very_high <= s->thr_high || s->thr_very_high > 400) {
        s->thr_very_high = s->thr_high + 50 > 400 ? 400 : s->thr_high + 50;
    }
}

/* Keep the night interval and brightness within allowed ranges. */
static void sanitize_night(night_settings_t *n)
{
    n->enabled = n->enabled != 0;
    n->black = n->black != 0;
    if (n->start_min >= 24 * 60) {
        n->start_min = k_night_defaults.start_min;
    }
    if (n->end_min >= 24 * 60) {
        n->end_min = k_night_defaults.end_min;
    }
    if (n->brightness < 1 || n->brightness > 100) {
        n->brightness = k_night_defaults.brightness;
    }
}

/* Save changes outside the UI callback to keep touch input responsive. */
static void saver_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* group rapid changes (cursor): a writing NVS not more than 2 s */
        while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000)) != 0) {
        }
        settings_t copy;
        settings_get(&copy);
        night_settings_t night;
        settings_night_get(&night);
        voice_cmd_settings_t vcmd;
        settings_vcmd_get(&vcmd);
        const int64_t t0 = esp_timer_get_time();
        nvs_handle_t h;
        esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_blob(h, NVS_KEY, &copy, sizeof(copy));
            if (err == ESP_OK) {
                err = nvs_set_blob(h, NVS_KEY_NIGHT, &night, sizeof(night));
            }
            if (err == ESP_OK) {
                err = nvs_set_blob(h, NVS_KEY_VCMD, &vcmd, sizeof(vcmd));
            }
            if (err == ESP_OK) {
                err = nvs_set_u8(h, NVS_KEY_LANG, (uint8_t)i18n_lang());
            }
            if (err == ESP_OK) {
                err = nvs_commit(h);
            }
            nvs_close(h);
        }
        const int64_t nvs_ms = (esp_timer_get_time() - t0) / 1000;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Enregistrement des reglages impossible (%s)", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Reglages enregistres en %" PRId64 " ms (tache reglages) : volume %u %%, seuils %u/%u/%u/%u, "
                     "mode nuit %s %02u:%02u-%02u:%02u %u %%, commandes vocales %s%s", nvs_ms, copy.volume_pct,
                     copy.thr_very_low, copy.thr_low, copy.thr_high, copy.thr_very_high, night.enabled ? "oui" : "non",
                     night.start_min / 60, night.start_min % 60, night.end_min / 60, night.end_min % 60,
                     night.brightness, vcmd.enabled ? "oui" : "non", vcmd.wake_word ? " (mot d'eveil)" : "");
        }
    }
}

esp_err_t settings_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_settings = k_defaults;
    s_night = k_night_defaults;
    s_vcmd = k_vcmd_defaults;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        settings_t stored;
        size_t size = sizeof(stored);
        if (nvs_get_blob(h, NVS_KEY, &stored, &size) == ESP_OK && size == sizeof(stored)) {
            s_settings = stored;
        }
        night_settings_t night;
        size = sizeof(night);
        if (nvs_get_blob(h, NVS_KEY_NIGHT, &night, &size) == ESP_OK && size == sizeof(night)) {
            s_night = night;
        }
        voice_cmd_settings_t vcmd;
        size = sizeof(vcmd);
        if (nvs_get_blob(h, NVS_KEY_VCMD, &vcmd, &size) == ESP_OK && size == sizeof(vcmd)) {
            s_vcmd = vcmd;
        }
        uint8_t lang = (uint8_t)LANG_FR;
        if (nvs_get_u8(h, NVS_KEY_LANG, &lang) == ESP_OK && lang < (uint8_t)LANG_COUNT) {
            i18n_set_lang((lang_t)lang);
        }
        nvs_close(h);
    }
    sanitize(&s_settings);
    sanitize_night(&s_night);
    s_vcmd.enabled = s_vcmd.enabled != 0;
    s_vcmd.wake_word = s_vcmd.wake_word != 0;
    if (s_vcmd.mic_gain_db > 42) {
        s_vcmd.mic_gain_db = k_vcmd_defaults.mic_gain_db;
    }
    xTaskCreate(saver_task, "reglages", 3072, NULL, 2, &s_saver);
    ESP_LOGI(TAG, "Reglages : volume %u %%, voix %s, alertes %s, seuils %u/%u/%u/%u mg/dL, langue %s",
             s_settings.volume_pct, s_settings.voice ? "oui" : "non", s_settings.alerts ? "oui" : "non",
             s_settings.thr_very_low, s_settings.thr_low, s_settings.thr_high, s_settings.thr_very_high,
             lang_code(i18n_lang()));
    return ESP_OK;
}

void settings_get(settings_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_mutex);
}

void settings_set(const settings_t *in)
{
    settings_t copy = *in;
    sanitize(&copy);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_settings = copy;
    s_revision++;
    xSemaphoreGive(s_mutex);
    if (s_saver != NULL) {
        xTaskNotifyGive(s_saver);
    }
}

void settings_night_get(night_settings_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_night;
    xSemaphoreGive(s_mutex);
}

void settings_night_set(const night_settings_t *in)
{
    night_settings_t copy = *in;
    sanitize_night(&copy);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_night = copy;
    s_revision++;
    xSemaphoreGive(s_mutex);
    if (s_saver != NULL) {
        xTaskNotifyGive(s_saver);
    }
}

void settings_vcmd_get(voice_cmd_settings_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_vcmd;
    xSemaphoreGive(s_mutex);
}

void settings_vcmd_set(const voice_cmd_settings_t *in)
{
    voice_cmd_settings_t copy = *in;
    copy.enabled = copy.enabled != 0;
    copy.wake_word = copy.wake_word != 0;
    copy.mic_gain_db = copy.mic_gain_db > 42 ? 42 : copy.mic_gain_db;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_vcmd = copy;
    s_revision++;
    xSemaphoreGive(s_mutex);
    if (s_saver != NULL) {
        xTaskNotifyGive(s_saver);
    }
}

lang_t settings_lang_get(void)
{
    return i18n_lang();
}

void settings_lang_set(lang_t lang)
{
    if (lang < 0 || lang >= LANG_COUNT || lang == i18n_lang()) {
        return;
    }
    i18n_set_lang(lang);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_revision++;
    xSemaphoreGive(s_mutex);
    if (s_saver != NULL) {
        xTaskNotifyGive(s_saver); /* recording different, like other settings */
    }
    ESP_LOGI(TAG, "Langue de l'interface : %s", lang_code(lang));
}

uint32_t settings_revision(void)
{
    return s_revision;
}

void settings_print(void)
{
    settings_t s;
    settings_get(&s);
    night_settings_t n;
    settings_night_get(&n);
    printf("REGLAGES volume=%u%% voix=%d alertes=%d volume_croissant=%d tres_basse_meme_en_mute=%d "
           "seuils tres_basse<%u basse<%u elevee>%u tres_elevee>%u luminosite=%u%%\n", s.volume_pct, s.voice, s.alerts,
           s.ramp, s.very_low_always, s.thr_very_low, s.thr_low, s.thr_high, s.thr_very_high, s.brightness);
    printf("REGLAGES mode_nuit=%d debut=%02u:%02u fin=%02u:%02u luminosite_nuit=%u%%\n", n.enabled, n.start_min / 60,
           n.start_min % 60, n.end_min / 60, n.end_min % 60, n.brightness);
    voice_cmd_settings_t v;
    settings_vcmd_get(&v);
    printf("REGLAGES commandes_vocales=%d mot_eveil=%d gain_micro=%u dB\n", v.enabled, v.wake_word, v.mic_gain_db);
}
