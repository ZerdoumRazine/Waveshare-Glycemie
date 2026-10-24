/*
 * voice.c: Playback of prerecorded WAV announcements by language.
 * Design note: Without the voice bank, the alert beeps remain available.
 */

/*
 * Combines stored PCM WAV clips for six languages with a French and then beep fallback.
 * Assemble announcements WAV of the six languages with an audio fold.
 */
#include "voice.h"

#include "i18n.h"

#include <stdio.h>
#include <string.h>
#include "app_state.h"
#include "audio_out.h"
#include "consumables.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sd_storage.h"
#include "sensor_session.h"
#include "voice_clips.h"

static const char *TAG = "voix";

#ifndef GLY_VOICE_OLD_PCM
#define GLY_VOICE_OLD_PCM 0 /* ancienne voix Windows absente par defaut (droits a clarifier) */
#endif

#if GLY_VOICE_OLD_PCM
extern const uint8_t voice_pcm_start[] asm("_binary_voice_fr_pcm_start");
extern const uint8_t voice_pcm_end[] asm("_binary_voice_fr_pcm_end");
#endif

#define MAX_WORDS 8

/* ------------------------------------------------------------------------- */
/* Banque microSD (voix Google Chirp 3 HD) */

#define VOICE_FILE_MAX (256 * 1024) /* 8 s at 16 kHz: sentences last less than 4 s */
#define VOICE_LEAD_MS  150          /* pause entre le bip d'alerte et la voix */
#define VOICE_GAP_MS   300          /* pause entre la phrase d'alerte et la valeur */

typedef struct {
    const int16_t *samples;
    size_t count;
} voice_pcm_t;

static uint8_t *s_file_buf; /* 2 x VOICE_FILE_MAX en PSRAM, tache audio uniquement */

static uint32_t rd16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return rd16(p) | (rd16(p + 2) << 16);
}

/* WAV PCM 16 bits mono 16 kHz: samples of the data block (allines: blocks RIFF have a pair size) */
static bool parse_wav(const uint8_t *buf, size_t len, voice_pcm_t *out)
{
    if (len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        return false;
    }
    bool format_ok = false;
    size_t pos = 12;
    while (pos + 8 <= len) {
        const size_t size = rd32(buf + pos + 4);
        const size_t body = pos + 8;
        if (size > len - body) {
            return false;
        }
        if (memcmp(buf + pos, "fmt ", 4) == 0 && size >= 16) {
            format_ok = rd16(buf + body) == 1 && rd16(buf + body + 2) == 1 && rd32(buf + body + 4) == AUDIO_SAMPLE_RATE &&
                        rd16(buf + body + 14) == 16;
        } else if (memcmp(buf + pos, "data", 4) == 0) {
            if (!format_ok || body % 2 != 0 || size < 2) {
                return false;
            }
            out->samples = (const int16_t *)(buf + body);
            out->count = size / 2;
            return true;
        }
        pos = body + size + (size & 1);
    }
    return false;
}

/* Audio cache: load a file from the bank into the slot buffer (0 or 1) */
/* Un seul avertissement par changement de langue si la banque correspondante manque */
static bool s_lang_missing_logged;

void voice_lang_changed(void)
{
    s_lang_missing_logged = false;
}

/* Look in the selected language folder; missing recordings must not crash the display. */
static bool load_voice_file(const char *name, int slot, voice_pcm_t *out)
{
    if (s_file_buf == NULL) {
        s_file_buf = heap_caps_malloc(2 * VOICE_FILE_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_file_buf == NULL) {
            ESP_LOGE(TAG, "Voix : memoire PSRAM indisponible");
            return false;
        }
    }
    char path[96];
    uint8_t *buf = s_file_buf + (size_t)slot * VOICE_FILE_MAX;
    size_t len = 0;
    esp_err_t err = ESP_ERR_NOT_FOUND;

    /* Banque de la langue choisie : /glycemie/audio/voice/<code>/<file> (in, es, de, it, pt).
     * The francais remains at the root, or it has always been: no copy of the original bank.
     * If the bank of a language is not installed on the map, we fall back on the French rather
     * que de rester muet ; si le francais manque aussi, le bip d'alerte sert de secours. */
    const lang_t lang = i18n_lang();
    if (lang != LANG_FR) {
        snprintf(path, sizeof(path), SD_VOICE_DIR "/%s/%s", lang_code(lang), name);
        err = sd_storage_read_file(path, buf, VOICE_FILE_MAX, &len);
        if (err != ESP_OK && !s_lang_missing_logged) {
            s_lang_missing_logged = true;
            ESP_LOGW(TAG, "Voix : banque %s absente de la microSD, repli sur la banque francaise",
                     lang_code(lang));
        }
    }
    if (err != ESP_OK) {
        snprintf(path, sizeof(path), SD_VOICE_DIR "/%s", name);
        err = sd_storage_read_file(path, buf, VOICE_FILE_MAX, &len);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Voix : %s %s : bip d'alerte seul", name,
                 err == ESP_ERR_NOT_FOUND ? "absent (fichier manquant ou microSD indisponible)" : "illisible");
        return false;
    }
    if (!parse_wav(buf, len, out)) {
        ESP_LOGW(TAG, "Voix : %s n'est pas un WAV PCM 16 bits mono 16 kHz : ignore", name);
        return false;
    }
    return true;
}

static void glucose_file_name(int value, char *name, size_t size)
{
    if (value < 40) {
        strlcpy(name, "glucose_low.wav", size);
    } else if (value > 400) {
        strlcpy(name, "glucose_high.wav", size);
    } else {
        snprintf(name, size, "glucose_%03d.wav", value);
    }
}

/* Tache audio : phrase d'alerte puis valeur */
static void render_alert(int type, int value)
{
    /* "Call back. The sensor must be changed tomorrow." / "... in two hours." / "Recall. The catheter must..." */
    static const char *const k_sensor[SENSOR_REMIND_COUNT] = {
        [SENSOR_REMIND_24H] = "sensor_rappel_24h.wav",
        [SENSOR_REMIND_2H] = "sensor_rappel_2h.wav",
    };
    static const char *const k_catheter[CATHETER_REMIND_COUNT] = {
        [CATHETER_REMIND_2H] = "catheter_rappel_2h.wav",
    };
    const char *phrase = NULL;
    bool with_value = true;
    switch (type) {
    case ALERT_VERY_LOW:
        phrase = "alert_urgent_low.wav";
        break;
    case ALERT_LOW:
        phrase = "alert_low.wav";
        break;
    case ALERT_HIGH:
        phrase = "alert_high.wav";
        break;
    case ALERT_VERY_HIGH:
        phrase = "alert_urgent_high.wav";
        break;
    case ALERT_NO_DATA:
        phrase = "alert_no_data.wav";
        with_value = false;
        break;
    case ALERT_SENSOR:
        phrase = value >= 0 && value < SENSOR_REMIND_COUNT ? k_sensor[value] : NULL;
        with_value = false;
        break;
    case ALERT_CATHETER:
        phrase = value >= 0 && value < CATHETER_REMIND_COUNT ? k_catheter[value] : NULL;
        with_value = false;
        break;
    default:
        break;
    }
    voice_pcm_t first;
    if (phrase == NULL || !load_voice_file(phrase, 0, &first)) {
        return; /* aucune voix : le bip d'alerte deja joue sert de secours */
    }
    char name[24] = "";
    voice_pcm_t second = {0};
    if (with_value) {
        glucose_file_name(value, name, sizeof(name));
        if (!load_voice_file(name, 1, &second)) {
            name[0] = '\0';
        }
    }
    ESP_LOGD(TAG, "Voix microSD : %s (%.2f s)%s%s%s", phrase, first.count / (double)AUDIO_SAMPLE_RATE,
             name[0] ? " puis " : "", name, name[0] ? "" : (with_value ? " (valeur absente)" : ""));
    if (audio_out_render_silence(VOICE_LEAD_MS) && audio_out_render_pcm(first.samples, first.count) && name[0] &&
        audio_out_render_silence(VOICE_GAP_MS)) {
        audio_out_render_pcm(second.samples, second.count);
    }
}

/* Tache audio : TEST VOIX */
static void render_test(int number, int unused)
{
    (void)unused;
    char name[24];
    glucose_file_name(number, name, sizeof(name));
    voice_pcm_t clip;
    if (load_voice_file(name, 0, &clip)) {
        ESP_LOGD(TAG, "Voix microSD : %s (%.2f s)", name, clip.count / (double)AUDIO_SAMPLE_RATE);
        audio_out_render_pcm(clip.samples, clip.count);
    } else {
        audio_out_render_pattern(AUDIO_PATTERN_TEST); /* secours : bip de test */
    }
}

/* Audio cache: response of a voice command (test beep if the file is missing) */
static void render_reply(int reply, int unused)
{
    static const char *const k_files[VOICE_REPLY_COUNT] = {
        [VOICE_REPLY_SENSOR_CONFIRM] = "vc_sensor_confirm.wav",
        [VOICE_REPLY_SENSOR_SAVED] = "vc_sensor_saved.wav",
        [VOICE_REPLY_CATHETER_CONFIRM] = "vc_catheter_confirm.wav",
        [VOICE_REPLY_CATHETER_SAVED] = "vc_catheter_saved.wav",
        [VOICE_REPLY_CANCELLED] = "vc_cancelled.wav",
        [VOICE_REPLY_TIME_UNAVAILABLE] = "vc_time_unavailable.wav",
        [VOICE_REPLY_SENSOR_ASK_CONFIRM] = "vc_sensor_ask.wav",
    };
    (void)unused;
    voice_pcm_t clip = {0};
    if (reply >= 0 && reply < VOICE_REPLY_COUNT && load_voice_file(k_files[reply], 0, &clip)) {
        audio_out_render_pcm(clip.samples, clip.count);
    } else {
        audio_out_render_pattern(AUDIO_PATTERN_TEST);
    }
}

/* Tache audio : glycemie a la demande. b = tendance | (ancienne << 8) | (disponible << 9) */
static void render_glucose(int value, int b)
{
    static const char *const k_trend[TREND_COUNT] = {
        [TREND_DOUBLE_UP] = "trend_up_fast.wav",
        [TREND_SINGLE_UP] = "trend_up.wav",
        [TREND_FORTY_FIVE_UP] = "trend_up_slow.wav",
        [TREND_FLAT] = "trend_flat.wav",
        [TREND_FORTY_FIVE_DOWN] = "trend_down_slow.wav",
        [TREND_SINGLE_DOWN] = "trend_down.wav",
        [TREND_DOUBLE_DOWN] = "trend_down_fast.wav",
    };
    const int trend = b & 0xFF;
    const bool stale = (b & 0x100) != 0;
    const bool available = (b & 0x200) != 0;
    voice_pcm_t first = {0};
    voice_pcm_t second = {0};
    if (!available) {
        if (load_voice_file("vc_no_data.wav", 0, &first)) {
            audio_out_render_pcm(first.samples, first.count);
        } else {
            audio_out_render_pattern(AUDIO_PATTERN_TEST);
        }
        return;
    }
    char name[24];
    glucose_file_name(value, name, sizeof(name));
    if (stale) {
        /* Never presented as current: "Old data. Last measure:" then the value */
        if (!load_voice_file("vc_data_old.wav", 0, &first) || !load_voice_file(name, 1, &second)) {
            audio_out_render_pattern(AUDIO_PATTERN_TEST);
            return;
        }
        if (audio_out_render_pcm(first.samples, first.count) && audio_out_render_silence(250)) {
            audio_out_render_pcm(second.samples, second.count);
        }
        return;
    }
    if (!load_voice_file(name, 0, &first)) {
        audio_out_render_pattern(AUDIO_PATTERN_TEST);
        return;
    }
    const char *trend_file = trend >= 0 && trend < TREND_COUNT ? k_trend[trend] : NULL;
    const bool with_trend = trend_file != NULL && load_voice_file(trend_file, 1, &second);
    if (audio_out_render_pcm(first.samples, first.count) && with_trend && audio_out_render_silence(150)) {
        audio_out_render_pcm(second.samples, second.count);
    }
}

void voice_announce_alert(alert_type_t type, int value, int volume_pct)
{
    if (volume_pct > 0) {
        audio_out_play_render(render_alert, type, value, volume_pct);
    }
}

void voice_reply(voice_reply_t reply, int volume_pct)
{
    if (volume_pct > 0) {
        audio_out_play_render(render_reply, (int)reply, 0, volume_pct);
    }
}

void voice_say_glucose(int mgdl, uint8_t trend, bool stale, bool available, int volume_pct)
{
    if (volume_pct > 0) {
        audio_out_play_render(render_glucose, mgdl, trend | (stale ? 0x100 : 0) | (available ? 0x200 : 0), volume_pct);
    }
}

void voice_test(int number, int volume_pct)
{
    ESP_LOGD(TAG, "Test du volume : voix a %d %% (glycemie %d)", volume_pct, number);
    if (volume_pct > 0) {
        audio_out_play_render(render_test, number, 0, volume_pct);
    }
}

/* ------------------------------------------------------------------------- */
/* Ancienne banque integree (voix Windows), facultative : absente, tout le reste fonctionne */

#if !GLY_VOICE_OLD_PCM

bool voice_available(void)
{
    return false;
}

void voice_old_announce(alert_type_t type, int value, int volume_pct)
{
    (void)type;
    (void)value;
    (void)volume_pct;
    ESP_LOGW(TAG, "Ancienne voix embarquee absente de cette compilation (main/voice/voice_fr.pcm)");
}

#else

static void say(voice_clip_id_t id, int volume)
{
    if (id >= VOICE_CLIP_COUNT) {
        return;
    }
    const voice_clip_t *clip = &k_voice_clips[id];
    const int16_t *base = (const int16_t *)voice_pcm_start;
    audio_out_play_pcm(base + clip->offset, clip->samples, volume);
}

/* Nombre en mots francais (1..999) : "deux cent soixante-quatre", "quatre-vingt-onze"... */
static int number_words(int n, voice_clip_id_t *out, int max)
{
    static const voice_clip_id_t k_units[17] = {
        VOICE_CLIP_COUNT, VOICE_W_UN, VOICE_W_DEUX, VOICE_W_TROIS, VOICE_W_QUATRE, VOICE_W_CINQ,
        VOICE_W_SIX, VOICE_W_SEPT, VOICE_W_HUIT, VOICE_W_NEUF, VOICE_W_DIX, VOICE_W_ONZE,
        VOICE_W_DOUZE, VOICE_W_TREIZE, VOICE_W_QUATORZE, VOICE_W_QUINZE, VOICE_W_SEIZE,
    };
    static const voice_clip_id_t k_tens[7] = {
        VOICE_CLIP_COUNT, VOICE_CLIP_COUNT, VOICE_W_VINGT, VOICE_W_TRENTE, VOICE_W_QUARANTE,
        VOICE_W_CINQUANTE, VOICE_W_SOIXANTE,
    };
    int k = 0;
#define ADD(id)                                                                                                        \
    do {                                                                                                               \
        if (k < max) {                                                                                                 \
            out[k++] = (id);                                                                                           \
        }                                                                                                              \
    } while (0)

    if (n <= 0 || n > 999) {
        return 0;
    }
    const int hundreds = n / 100;
    int rest = n % 100;
    if (hundreds > 0) {
        if (hundreds > 1) {
            ADD(k_units[hundreds]);
        }
        ADD(hundreds > 1 && rest == 0 ? VOICE_W_CENTS : VOICE_W_CENT);
    }
    if (rest == 0) {
        return k;
    }
    if (rest <= 16) {
        ADD(k_units[rest]);
    } else if (rest < 20) {
        ADD(VOICE_W_DIX);
        ADD(k_units[rest - 10]);
    } else if (rest < 70) {
        const int unit = rest % 10;
        ADD(k_tens[rest / 10]);
        if (unit == 1) {
            ADD(VOICE_W_ET);
            ADD(VOICE_W_UN);
        } else if (unit > 0) {
            ADD(k_units[unit]);
        }
    } else if (rest < 80) {
        const int r = rest - 60; /* 10..19 */
        ADD(VOICE_W_SOIXANTE);
        if (r == 11) {
            ADD(VOICE_W_ET);
            ADD(VOICE_W_ONZE);
        } else if (r <= 16) {
            ADD(k_units[r]);
        } else {
            ADD(VOICE_W_DIX);
            ADD(k_units[r - 10]);
        }
    } else {
        const int r = rest - 80; /* 0..19 */
        if (r == 0) {
            ADD(VOICE_W_QUATRE_VINGTS);
        } else {
            ADD(VOICE_W_QUATRE_VINGT);
            if (r <= 16) {
                ADD(k_units[r]);
            } else {
                ADD(VOICE_W_DIX);
                ADD(k_units[r - 10]);
            }
        }
    }
#undef ADD
    return k;
}

/* Read pre-generated number clips; speech is not synthesized on the board. */
static void say_number(int n, int volume)
{
    voice_clip_id_t words[MAX_WORDS];
    const int count = number_words(n, words, MAX_WORDS);
    for (int i = 0; i < count; i++) {
        say(words[i], volume);
    }
}

bool voice_available(void)
{
    return (size_t)((const uint8_t *)voice_pcm_end - (const uint8_t *)voice_pcm_start) > 0;
}

void voice_old_announce(alert_type_t type, int value, int volume_pct)
{
    if (!voice_available() || volume_pct <= 0) {
        return;
    }
    switch (type) {
    case ALERT_VERY_LOW:
        say(VOICE_P_TRES_BASSE, volume_pct);
        say_number(value, volume_pct);
        break;
    case ALERT_LOW:
        say(VOICE_P_BASSE, volume_pct);
        say_number(value, volume_pct);
        break;
    case ALERT_HIGH:
        say(VOICE_P_ELEVEE, volume_pct);
        say_number(value, volume_pct);
        break;
    case ALERT_VERY_HIGH:
        say(VOICE_P_TRES_ELEVEE, volume_pct);
        say_number(value, volume_pct);
        break;
    case ALERT_NO_DATA:
        say(VOICE_P_AUCUNE_DONNEE, volume_pct);
        say_number(value, volume_pct);
        say(VOICE_P_MINUTES, volume_pct);
        break;
    case ALERT_SENSOR: {
        static const voice_clip_id_t k_sensor[SENSOR_REMIND_COUNT] = {
            [SENSOR_REMIND_24H] = VOICE_P_CAPTEUR_24H,
            [SENSOR_REMIND_2H] = VOICE_P_CAPTEUR_2H,
        };
        if (value >= 0 && value < SENSOR_REMIND_COUNT) {
            say(k_sensor[value], volume_pct);
        }
        break;
    }
    case ALERT_TEST:
        say(VOICE_P_TEST, volume_pct);     /* "Test du volume." */
        say(VOICE_P_GLYCEMIE, volume_pct); /* "Glycemie" */
        say_number(value, volume_pct);
        say(VOICE_P_MG_DL, volume_pct);    /* "milligrammes par decilitre." */
        break;
    default:
        break;
    }
}

#endif /* GLY_VOICE_OLD_PCM */
