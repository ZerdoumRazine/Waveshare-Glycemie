/*
 * audio_out.c: Audio playback and alert tones.
 * Design note: The audible beep fallback works without prerecorded WAVs.
 */

/*
 * Owns the codec and serialises tones, alerts and spoken clips through one worker task.
 * Pilot the codec and serialize the sounds in a dedicated task.
 */
#include "audio_out.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_vol.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "app_state.h"

static const char *TAG = "audio";

#define CHUNK_FRAMES   320  /* 20 ms a 16 kHz */
#define IDLE_CLOSE_MS  3000
#define TONE_AMPLITUDE 0.5f /* -6 dBFS: has 100 % of the amp sature adu-dela (micro measure: H3 -11 dB with -1.5 dBFS) */
#define ES8311_REG_DAC_VOLUME 0x32 /* 0xBF = 0 dB, pas de 0,5 dB */

/*
 * General Volume -> Digital Volume of DAC ES8311 (dB). The curve by defaultesp_codec_dev is linear
 * Of -50 dB at 0 dB (45% = -27.5 dB: voice too low). Law close to an amplitude proportional to the square
 * of the percentage (40 log10): 50% = -12 dB, 70% = -6 dB, 85% = -3 dB, 100% = 0 dB. Never gain
 * positive digital (it would saturate normalized extracts a -1.5 dBFS).
 */
static const struct {
    int pct;
    float dac_db;
} k_volume_curve[] = {
    {1, -50.0f}, {10, -40.0f}, {20, -28.0f}, {35, -18.0f}, {50, -12.0f},
    {60, -9.0f}, {70, -6.0f},  {85, -3.0f},  {100, 0.0f},
};
#define VOLUME_POINTS (int)(sizeof(k_volume_curve) / sizeof(k_volume_curve[0]))

/* Same compensation as the pilot ES8311 with values of BSP 5 V, DAC 3.3 V): approximately -3.6 dB */
static esp_codec_dev_hw_gain_t s_hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3};

typedef enum {
    REQ_PATTERN,
    REQ_TONE,
    REQ_PCM,
    REQ_RENDER,
} req_type_t;

typedef struct {
    req_type_t type;
    int volume;
    audio_pattern_t pattern;
    int freq;
    int ms;
    const int16_t *samples;
    size_t count;
    audio_render_fn_t render;
    int a;
    int b;
} audio_req_t;

typedef struct {
    uint16_t freq;
    uint16_t on_ms;
    uint16_t off_ms;
} note_t;

static const note_t k_test[] = {{1000, 250, 0}};
static const note_t k_very_low[] = {{1760, 140, 70}, {1760, 140, 70}, {1760, 140, 260},
                                     {1760, 140, 70}, {1760, 140, 70}, {1760, 140, 0}};
static const note_t k_low[] = {{1320, 220, 120}, {1320, 220, 0}};
static const note_t k_high[] = {{660, 650, 0}};
static const note_t k_very_high[] = {{660, 450, 180}, {880, 450, 0}};
static const note_t k_no_data[] = {{520, 170, 120}, {520, 170, 0}};
static const note_t k_sensor[] = {{784, 170, 40}, {988, 170, 40}, {1175, 320, 0}};

static const struct {
    const note_t *notes;
    size_t count;
} k_patterns[AUDIO_PATTERN_COUNT] = {
    [AUDIO_PATTERN_TEST] = {k_test, sizeof(k_test) / sizeof(k_test[0])},
    [AUDIO_PATTERN_VERY_LOW] = {k_very_low, sizeof(k_very_low) / sizeof(k_very_low[0])},
    [AUDIO_PATTERN_LOW] = {k_low, sizeof(k_low) / sizeof(k_low[0])},
    [AUDIO_PATTERN_HIGH] = {k_high, sizeof(k_high) / sizeof(k_high[0])},
    [AUDIO_PATTERN_VERY_HIGH] = {k_very_high, sizeof(k_very_high) / sizeof(k_very_high[0])},
    [AUDIO_PATTERN_NO_DATA] = {k_no_data, sizeof(k_no_data) / sizeof(k_no_data[0])},
    [AUDIO_PATTERN_SENSOR] = {k_sensor, sizeof(k_sensor) / sizeof(k_sensor[0])},
};

static esp_codec_dev_handle_t s_speaker;
static esp_codec_dev_handle_t s_mic;
static volatile bool s_mic_reserved; /* microphones used continuously by voice commands */
static bool s_open;
static int s_volume_set = -1;
static QueueHandle_t s_queue;
static volatile bool s_stop;
static volatile bool s_busy;
static int16_t s_buf[CHUNK_FRAMES * 2];

/* Prepare the codec before submitting samples to its audio path. */
static esp_err_t codec_open(int volume)
{
    if (!s_open) {
        esp_codec_dev_sample_info_t fs = {
            .sample_rate = AUDIO_SAMPLE_RATE,
            .channel = 2,
            .bits_per_sample = 16,
        };
        if (esp_codec_dev_open(s_speaker, &fs) != ESP_CODEC_DEV_OK) {
            return ESP_FAIL;
        }
        s_open = true;
        s_volume_set = -1;
    }
    if (volume != s_volume_set) {
        esp_codec_dev_set_out_vol(s_speaker, volume);
        s_volume_set = volume;
        ESP_LOGD(TAG, "Volume %d %% : DAC ES8311 %.1f dB", volume, audio_out_volume_db(volume));
    }
    return ESP_OK;
}

static void codec_close(void)
{
    if (s_open) {
        esp_codec_dev_close(s_speaker);
        s_open = false;
    }
}

static bool write_frames(int frames)
{
    const int err = esp_codec_dev_write(s_speaker, s_buf, frames * 4);
    if (err != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Ecriture vers le codec ES8311 impossible (code %d) : son interrompu", err);
        return false;
    }
    return true;
}

static void play_tone(int freq, int ms)
{
    const int total = AUDIO_SAMPLE_RATE * ms / 1000;
    const int ramp = AUDIO_SAMPLE_RATE * 6 / 1000;
    const double step = 2.0 * M_PI * freq / AUDIO_SAMPLE_RATE;
    double phase = 0.0;
    int done = 0;
    while (done < total && !s_stop) {
        const int n = total - done < CHUNK_FRAMES ? total - done : CHUNK_FRAMES;
        for (int i = 0; i < n; i++) {
            const int idx = done + i;
            float env = 1.0f;
            if (idx < ramp) {
                env = (float)idx / ramp;
            } else if (total - idx < ramp) {
                env = (float)(total - idx) / ramp;
            }
            const int16_t v = (int16_t)(sin(phase) * TONE_AMPLITUDE * env * 32767.0);
            phase += step;
            if (phase > 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
            s_buf[2 * i] = v;
            s_buf[2 * i + 1] = v;
        }
        if (!write_frames(n)) {
            break;
        }
        done += n;
    }
}

static void play_silence(int ms)
{
    memset(s_buf, 0, sizeof(s_buf));
    const int total = AUDIO_SAMPLE_RATE * ms / 1000;
    for (int done = 0; done < total && !s_stop;) {
        const int n = total - done < CHUNK_FRAMES ? total - done : CHUNK_FRAMES;
        if (!write_frames(n)) {
            break;
        }
        done += n;
    }
}

static void play_pcm(const int16_t *samples, size_t count)
{
    for (size_t done = 0; done < count && !s_stop;) {
        const int n = count - done < CHUNK_FRAMES ? (int)(count - done) : CHUNK_FRAMES;
        for (int i = 0; i < n; i++) {
            s_buf[2 * i] = samples[done + i];
            s_buf[2 * i + 1] = samples[done + i];
        }
        if (!write_frames(n)) {
            break;
        }
        done += (size_t)n;
    }
}

static void play_pattern(audio_pattern_t pattern)
{
    for (size_t i = 0; i < k_patterns[pattern].count && !s_stop; i++) {
        const note_t *note = &k_patterns[pattern].notes[i];
        play_tone(note->freq, note->on_ms);
        if (note->off_ms > 0) {
            play_silence(note->off_ms);
        }
    }
}

/* Handle the audio queue in one place to avoid overlapping writes to the codec. */
static void audio_task(void *arg)
{
    (void)arg;
    audio_req_t req;
    for (;;) {
        const TickType_t wait = s_open ? pdMS_TO_TICKS(IDLE_CLOSE_MS) : portMAX_DELAY;
        if (xQueueReceive(s_queue, &req, wait) != pdTRUE) {
            codec_close(); /* repos : amplificateur coupe */
            continue;
        }
        s_busy = true;
        s_stop = false;
        if (codec_open(req.volume) != ESP_OK) {
            ESP_LOGE(TAG, "Ouverture du codec ES8311 impossible");
            s_busy = false;
            continue;
        }
        switch (req.type) {
        case REQ_PATTERN:
            play_pattern(req.pattern);
            break;
        case REQ_TONE:
            play_tone(req.freq, req.ms);
            break;
        case REQ_PCM:
            play_pcm(req.samples, req.count);
            break;
        case REQ_RENDER:
            req.render(req.a, req.b);
            break;
        }
        play_silence(60);
        s_busy = uxQueueMessagesWaiting(s_queue) > 0;
    }
}

/* Submit a request asynchronously instead of blocking the UI or glucose service. */
static esp_err_t enqueue(const audio_req_t *req)
{
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (req->volume <= 0) {
        return ESP_OK; /* muet */
    }
    s_busy = true;
    return xQueueSend(s_queue, req, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t audio_out_init(void)
{
    s_speaker = bsp_audio_codec_speaker_init();
    if (s_speaker == NULL) {
        ESP_LOGE(TAG, "Initialisation du codec haut-parleur (BSP) impossible");
        app_state_set_audio_ok(false);
        return ESP_FAIL;
    }
    /* Convert user-facing volume to the codec's dB curve, compensating for the driver. */
    const float hw_gain = esp_codec_dev_col_calc_hw_gain(&s_hw_gain);
    esp_codec_dev_vol_map_t map[VOLUME_POINTS];
    for (int i = 0; i < VOLUME_POINTS; i++) {
        map[i].vol = k_volume_curve[i].pct;
        /* +0.25 dB: the truncated pilot at 0.5 dB of the register, rounded to the nearest (100% = 0xBF) */
        map[i].db_value = k_volume_curve[i].dac_db + hw_gain + 0.25f;
    }
    esp_codec_dev_vol_curve_t curve = {.vol_map = map, .count = VOLUME_POINTS};
    if (esp_codec_dev_set_vol_curve(s_speaker, &curve) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Courbe de volume refusee : courbe par defaut d'esp_codec_dev");
    }
    s_queue = xQueueCreate(24, sizeof(audio_req_t));
    /* 6 KB stack: rendering functions read microSD (FatFs, driver) SDMMC) */
    if (s_queue == NULL || xTaskCreate(audio_task, "audio", 6144, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    app_state_set_audio_ok(true);
    ESP_LOGI(TAG, "Audio pret : ES8311 via le BSP, 16 kHz stereo 16 bits");
    /* The codec driver emits informational logs and occasionally a harmless
 * "i2s_channel_disable ... has not been enabled yet" diagnostic. Report real
 * audio open/write failures separately so operators can distinguish them. */
    esp_log_level_set("I2S_IF", ESP_LOG_WARN);
    esp_log_level_set("Adev_Codec", ESP_LOG_WARN);
    esp_log_level_set("i2s_common", ESP_LOG_NONE);
    return ESP_OK;
}

bool audio_out_ready(void)
{
    return s_queue != NULL;
}

esp_err_t audio_out_play_pattern(audio_pattern_t pattern, int volume_pct)
{
    if (pattern >= AUDIO_PATTERN_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    const audio_req_t req = {.type = REQ_PATTERN, .volume = volume_pct, .pattern = pattern};
    return enqueue(&req);
}

esp_err_t audio_out_play_tone(int freq_hz, int duration_ms, int volume_pct)
{
    const audio_req_t req = {.type = REQ_TONE, .volume = volume_pct, .freq = freq_hz, .ms = duration_ms};
    return enqueue(&req);
}

esp_err_t audio_out_play_pcm(const int16_t *samples, size_t count, int volume_pct)
{
    const audio_req_t req = {.type = REQ_PCM, .volume = volume_pct, .samples = samples, .count = count};
    return enqueue(&req);
}

esp_err_t audio_out_play_render(audio_render_fn_t fn, int a, int b, int volume_pct)
{
    if (fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const audio_req_t req = {.type = REQ_RENDER, .volume = volume_pct, .render = fn, .a = a, .b = b};
    return enqueue(&req);
}

bool audio_out_render_pcm(const int16_t *samples, size_t count)
{
    play_pcm(samples, count);
    return !s_stop;
}

bool audio_out_render_silence(int ms)
{
    play_silence(ms);
    return !s_stop;
}

bool audio_out_render_pattern(audio_pattern_t pattern)
{
    if (pattern < AUDIO_PATTERN_COUNT) {
        play_pattern(pattern);
    }
    return !s_stop;
}

void audio_out_stop(void)
{
    if (s_queue != NULL) {
        xQueueReset(s_queue);
    }
    s_stop = true;
}

bool audio_out_busy(void)
{
    return s_busy;
}

float audio_out_volume_db(int volume_pct)
{
    if (volume_pct <= 0) {
        return -96.0f;
    }
    if (volume_pct >= 100) {
        return k_volume_curve[VOLUME_POINTS - 1].dac_db;
    }
    for (int i = 0; i < VOLUME_POINTS - 1; i++) {
        if (volume_pct < k_volume_curve[i + 1].pct) {
            const float span = (float)(k_volume_curve[i + 1].pct - k_volume_curve[i].pct);
            const float ratio = volume_pct < k_volume_curve[i].pct ? 0.0f : (volume_pct - k_volume_curve[i].pct) / span;
            return k_volume_curve[i].dac_db + ratio * (k_volume_curve[i + 1].dac_db - k_volume_curve[i].dac_db);
        }
    }
    return 0.0f;
}

void audio_out_print_info(void)
{
    const float hw_gain = esp_codec_dev_col_calc_hw_gain(&s_hw_gain);
    printf("AUDIO_INFO codec=ES8311 (BSP) 16 kHz stereo 16 bits ; ampli : broche du BSP, gain fixe, coupe au repos\n");
    printf("AUDIO_INFO compensation pilote hw_gain=%.2f dB (ampli 5 V, DAC 3,3 V) ; bips %.1f dBFS ; voix -1,5 dBFS crete\n",
           hw_gain, 20.0 * log10(TONE_AMPLITUDE));
    for (int i = 0; i < VOLUME_POINTS; i++) {
        printf("AUDIO_INFO courbe %3d %% -> DAC %.1f dB (esp_codec_dev %.1f dB)\n", k_volume_curve[i].pct,
               k_volume_curve[i].dac_db, k_volume_curve[i].dac_db + hw_gain);
    }
    printf("AUDIO_INFO codec %s, dernier volume applique %d %%\n", s_open ? "ouvert" : "ferme (repos)", s_volume_set);
    const TaskHandle_t task = xTaskGetHandle("audio");
    if (task != NULL) {
        printf("AUDIO_INFO tache audio : pile libre minimale %u octets\n", (unsigned)uxTaskGetStackHighWaterMark(task));
    }
}

/* Amplitude of component a `freq` (Goertzel algorithm) on the left channel */
static double goertzel(const int16_t *stereo, int frames, int freq)
{
    const double coeff = 2.0 * cos(2.0 * M_PI * freq / AUDIO_SAMPLE_RATE);
    double s1 = 0.0;
    double s2 = 0.0;
    for (int i = 0; i < frames; i++) {
        const double s = stereo[2 * i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return sqrt(power > 0 ? power : 0) / frames;
}

/* Micro (console) measuring stamp: 100 ms stereo, in PSRAM the first measure (the RAM internal
 * tampons DMA) */
#define MIC_CHUNK 1600
static int16_t *s_rx;

static int16_t *mic_buffer(void)
{
    if (s_rx == NULL) {
        s_rx = heap_caps_malloc(MIC_CHUNK * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    return s_rx;
}

/* Measure speaker test signals; this diagnostic does not certify alarm audibility. */
static double measure(int freq, int chunks)
{
    int16_t *rx = mic_buffer();
    double sum = 0.0;
    int ok = 0;
    for (int c = 0; c < chunks && rx != NULL; c++) {
        if (esp_codec_dev_read(s_mic, rx, MIC_CHUNK * 2 * sizeof(int16_t)) == ESP_CODEC_DEV_OK) {
            sum += goertzel(rx, MIC_CHUNK, freq);
            ok++;
        }
    }
    return ok > 0 ? sum / ok : 0.0;
}

void *audio_out_mic_acquire(void)
{
    if (s_speaker == NULL || s_mic_reserved) {
        return NULL;
    }
    if (s_mic == NULL) {
        s_mic = bsp_audio_codec_microphone_init();
    }
    s_mic_reserved = s_mic != NULL;
    return s_mic;
}

void audio_out_mic_release(void)
{
    s_mic_reserved = false;
}

esp_err_t audio_out_selftest(int volume_pct, bool *detected, float *gain_db)
{
    *detected = false;
    *gain_db = 0.0f;
    if (s_speaker == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mic_reserved) {
        ESP_LOGW(TAG, "Autotest refuse : micros utilises par les commandes vocales (vcmd_off pour tester)");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mic == NULL) {
        s_mic = bsp_audio_codec_microphone_init();
        if (s_mic == NULL) {
            ESP_LOGW(TAG, "Autotest : micros (ES7210) indisponibles");
            return ESP_FAIL;
        }
    }
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = AUDIO_SAMPLE_RATE,
        .channel = 2,
        .bits_per_sample = 16,
    };
    if (esp_codec_dev_open(s_mic, &fs) != ESP_CODEC_DEV_OK) {
        return ESP_FAIL;
    }
    esp_codec_dev_set_in_gain(s_mic, 30.0f);

    measure(1000, 2); /* stabilisation */
    const double silence = measure(1000, 3);
    audio_out_play_tone(1000, 1000, volume_pct);
    vTaskDelay(pdMS_TO_TICKS(300));
    const double tone = measure(1000, 4);
    esp_codec_dev_close(s_mic);

    *gain_db = (float)(20.0 * log10((tone + 0.01) / (silence + 0.01)));
    *detected = *gain_db >= 12.0f;
    ESP_LOGI(TAG, "Autotest : composante 1 kHz au micro, silence %.2f, tonalite %.2f (%+.1f dB) -> son %s",
             silence, tone, *gain_db, *detected ? "DETECTE" : "NON DETECTE");
    return ESP_OK;
}

static double dbfs(double amplitude)
{
    return 20.0 * log10((amplitude > 1e-3 ? amplitude : 1e-3) / 32768.0);
}

esp_err_t audio_out_measure(int volume_pct, audio_playback_fn_t play_voice, float mic_gain_db)
{
    enum { CHUNK = MIC_CHUNK, FRAME = 320 }; /* 100 ms lus par bloc, energie par trame de 20 ms */
    const size_t rx_bytes = CHUNK * 2 * sizeof(int16_t);
    int16_t *rx = mic_buffer();
    if (rx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (s_speaker == NULL || s_mic_reserved) {
        printf("AUDIO_MESURE refusee : %s\n", s_speaker == NULL ? "audio indisponible" : "micros utilises par les "
                                                                                          "commandes vocales");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mic == NULL && (s_mic = bsp_audio_codec_microphone_init()) == NULL) {
        return ESP_FAIL;
    }
    esp_codec_dev_sample_info_t fs = {.sample_rate = AUDIO_SAMPLE_RATE, .channel = 2, .bits_per_sample = 16};
    if (esp_codec_dev_open(s_mic, &fs) != ESP_CODEC_DEV_OK) {
        return ESP_FAIL;
    }
    esp_codec_dev_set_in_gain(s_mic, mic_gain_db);
    for (int i = 0; i < 3; i++) {
        esp_codec_dev_read(s_mic, rx, rx_bytes); /* stabilisation */
    }

    /* Bruit de fond (0,5 s) */
    double noise_energy = 0.0;
    int noise_samples = 0;
    for (int c = 0; c < 5; c++) {
        if (esp_codec_dev_read(s_mic, rx, rx_bytes) == ESP_CODEC_DEV_OK) {
            for (int i = 0; i < CHUNK; i++) {
                noise_energy += (double)rx[2 * i] * rx[2 * i];
            }
            noise_samples += CHUNK;
        }
    }
    const double noise_rms = noise_samples > 0 ? sqrt(noise_energy / noise_samples) : 0.0;

    const bool voice = play_voice != NULL;
    if (voice) {
        play_voice(volume_pct);
    } else {
        audio_out_play_tone(1000, 1800, volume_pct);
    }
    const int chunks = voice ? 55 : 12;       /* vote: 5.5 seconds; bip: 1.2 s */
    int skip = voice ? 1 : 3;                 /* ouverture du codec et montee du bip */
    double active_energy = 0.0;
    int active_frames = 0;
    double tone_sum = 0.0;
    double h2_sum = 0.0;
    double h3_sum = 0.0;
    int tone_chunks = 0;
    int peak = 0;
    int clipped = 0;
    for (int c = 0; c < chunks + skip; c++) {
        if (esp_codec_dev_read(s_mic, rx, rx_bytes) != ESP_CODEC_DEV_OK || c < skip) {
            continue;
        }
        for (int f = 0; f < CHUNK; f += FRAME) {
            double energy = 0.0;
            for (int i = f; i < f + FRAME; i++) {
                const int v = rx[2 * i];
                energy += (double)v * v;
                const int a = v < 0 ? -v : v;
                peak = a > peak ? a : peak;
                clipped += a >= 32500;
            }
            const double rms = sqrt(energy / FRAME);
            if (!voice || rms > noise_rms * 3.16) { /* voice: frames at least 10 dB above the noise */
                active_energy += energy;
                active_frames++;
            }
        }
        if (!voice) {
            tone_sum += goertzel(rx, CHUNK, 1000);
            h2_sum += goertzel(rx, CHUNK, 2000); /* Harmonic distortion can indicate amplifier or speaker saturation. */
            h3_sum += goertzel(rx, CHUNK, 3000);
            tone_chunks++;
        }
    }
    esp_codec_dev_close(s_mic);

    const double active_rms = active_frames > 0 ? sqrt(active_energy / ((double)active_frames * FRAME)) : 0.0;
    if (voice) {
        printf("AUDIO_MESURE voix volume=%d%% gain_micro=%.0f dB : niveau actif=%.1f dBFS (%d trames) crete=%.1f dBFS "
               "ecretage_micro=%d bruit=%.1f dBFS\n", volume_pct, mic_gain_db, dbfs(active_rms), active_frames,
               dbfs(peak), clipped, dbfs(noise_rms));
    } else {
        const double tone_amplitude = tone_chunks > 0 ? 2.0 * tone_sum / tone_chunks : 0.0;
        const double h2 = h2_sum > 0 && tone_sum > 0 ? 20.0 * log10(h2_sum / tone_sum) : -99.0;
        const double h3 = h3_sum > 0 && tone_sum > 0 ? 20.0 * log10(h3_sum / tone_sum) : -99.0;
        printf("AUDIO_MESURE bip volume=%d%% gain_micro=%.0f dB : 1 kHz=%.1f dBFS efficace_total=%.1f dBFS "
               "crete=%.1f dBFS H2=%.1f dB H3=%.1f dB ecretage_micro=%d bruit=%.1f dBFS\n", volume_pct, mic_gain_db,
               dbfs(tone_amplitude / sqrt(2.0)), dbfs(active_rms), dbfs(peak), h2, h3, clipped, dbfs(noise_rms));
    }
    return ESP_OK;
}

void audio_out_dump_regs(int volume_pct)
{
    const int pa_bit = 53 - 32; /* BSP_POWER_AMP_IO = GPIO 53 (registres GPIO_OUT1 / GPIO_ENABLE1) */
    audio_out_play_tone(1000, 2500, volume_pct);
    vTaskDelay(pdMS_TO_TICKS(700));
    printf("AUDIO_REGS pendant un bip 1 kHz a %d %% (codec %s)\n", volume_pct, s_open ? "ouvert" : "ferme");
    for (int base = 0x00; base <= 0x40; base += 0x10) {
        char line[96];
        int used = snprintf(line, sizeof(line), "AUDIO_REGS ES8311 0x%02X :", base);
        for (int reg = base; reg < base + 0x10 && reg <= 0x45; reg++) {
            int value = -1;
            esp_codec_dev_read_reg(s_speaker, reg, &value);
            used += snprintf(line + used, sizeof(line) - (size_t)used, " %02X", (unsigned)(value & 0xFF));
        }
        printf("%s\n", line);
    }
    const uint32_t out_on = REG_READ(GPIO_OUT1_REG);
    const uint32_t en_on = REG_READ(GPIO_ENABLE1_REG);
    printf("AUDIO_REGS ampli (GPIO 53) pendant la lecture : niveau=%" PRIu32 " sortie_activee=%" PRIu32 "\n",
           (out_on >> pa_bit) & 1, (en_on >> pa_bit) & 1);
    vTaskDelay(pdMS_TO_TICKS(2000 + IDLE_CLOSE_MS + 500));
    const uint32_t out_off = REG_READ(GPIO_OUT1_REG);
    printf("AUDIO_REGS ampli (GPIO 53) au repos : niveau=%" PRIu32 " (codec %s)\n", (out_off >> pa_bit) & 1,
           s_open ? "ouvert" : "ferme");
}
