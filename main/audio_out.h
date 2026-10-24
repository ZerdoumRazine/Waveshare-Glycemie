/* audio_out.h: Audio playback settings and alert tone API.
 * Keep this header focused on the contracts used by other modules.
 */
/* Drive the ES8311 codec and I2S speaker through the Waveshare BSP.
 * A dedicated audio task queues synthesized tones and PCM clips so callers
 * do not block, and the codec opens only while audio is being played. */
/* Nonblocking playback and codec-diagnostic APIs. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define AUDIO_SAMPLE_RATE 16000

typedef enum {
    AUDIO_PATTERN_TEST = 0,     /* Short test beep. */
    AUDIO_PATTERN_VERY_LOW,     /* Critical alert: three quick high-pitched beeps. */
    AUDIO_PATTERN_LOW,          /* Two beeps. */
    AUDIO_PATTERN_HIGH,         /* One long low-pitched beep. */
    AUDIO_PATTERN_VERY_HIGH,    /* Two long beeps. */
    AUDIO_PATTERN_NO_DATA,      /* Two short low-pitched beeps. */
    AUDIO_PATTERN_SENSOR,       /* Rising chime. */
    AUDIO_PATTERN_COUNT,
} audio_pattern_t;

esp_err_t audio_out_init(void);
bool audio_out_ready(void);

/* Queue playback without blocking; volume ranges from 0 to 100. */
esp_err_t audio_out_play_pattern(audio_pattern_t pattern, int volume_pct);
esp_err_t audio_out_play_tone(int freq_hz, int duration_ms, int volume_pct);
esp_err_t audio_out_play_pcm(const int16_t *samples, size_t count, int volume_pct); /* Mono 16 kHz PCM kept in persistent memory. */

/* Prepare playback in the audio task, where microSD access is allowed;
 * never perform storage I/O from LVGL or while holding an alert lock. */
typedef void (*audio_render_fn_t)(int a, int b);
esp_err_t audio_out_play_render(audio_render_fn_t fn, int a, int b, int volume_pct);

/* Only from a rendering function: false if the reading has been stopped (acquittal, reminder) */
bool audio_out_render_pcm(const int16_t *samples, size_t count); /* mono 16 kHz */
bool audio_out_render_silence(int ms);
bool audio_out_render_pattern(audio_pattern_t pattern);

/* Stop current playback and clear the queued requests. */
void audio_out_stop(void);
bool audio_out_busy(void);

/* Digital volume of DAC ES8311 (dB) corresponding to a general volume 0.10% (-96 = mute) */
float audio_out_volume_db(int volume_pct);
void audio_out_print_info(void); /* console */

/* Microphones ES7210 (BSP) reserved by voice commands : NULL if already reserved or unavailable. During the
 * reservation, self-test and microphone measurement are refused. */
void *audio_out_mic_acquire(void); /* esp_codec_dev_handle_t */
void audio_out_mic_release(void);

/* Play a test tone and check its presence through the onboard microphone. */
esp_err_t audio_out_selftest(int volume_pct, bool *detected, float *gain_db);

/* Measure a 1 kHz beep or spoken test through the board microphone using
 * a fixed input gain, allowing repeatable relative volume comparisons. */
typedef void (*audio_playback_fn_t)(int volume_pct);
esp_err_t audio_out_measure(int volume_pct, audio_playback_fn_t play_voice, float mic_gain_db);

/* Registers ES8311 and pin of the amplifier rereaded during a beep 1 kHz (console) */
void audio_out_dump_regs(int volume_pct);
