/* sd_storage.h: microSD storage and history access interface.
 * The declarations below are shared with other firmware modules.
 */
/* Optional FAT32 microSD storage for daily CSV glucose history and logs.
 * Initialize after the ESP-Hosted Wi-Fi coprocessor because it shares the
 * SDMMC controller. Never format a card automatically: only the explicit
 * diagnostic sd_format command with its confirmation token may format.
 * Card removal must not prevent glucose monitoring; mark storage unavailable
 * and retry mounting later, resuming history without duplicate samples.
 * Files reside beneath /glycemie/history, /glycemie/logs and
 * /glycemie/audio/voice. */
/* FAT32 history, status reporting and guarded maintenance APIs. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "app_state.h"

#define SD_MOUNT_POINT "/sdcard"
#define SD_AUDIO_DIR   SD_MOUNT_POINT "/glycemie/audio"
#define SD_VOICE_DIR   SD_AUDIO_DIR "/voice"

esp_err_t sd_storage_start(void);
bool sd_storage_ready(void);

/* Protect files from concurrent mount/unmount operations when used by
 * audio or serial worker tasks. ESP_ERR_NOT_FOUND means unavailable or absent. */
esp_err_t sd_storage_read_file(const char *path, uint8_t *buf, size_t max, size_t *len);
/* Read an allowed file under /glycemie/logs through the serial console. */
void sd_storage_print_log(const char *name);
/* Export exact file contents through base64 plus CRC32 without modifying the card. */
void sd_storage_dump_file(const char *rel);
esp_err_t sd_storage_write_file(const char *path, const uint8_t *data, size_t len); /* Write to a temporary file and rename after successful completion. */
/* Create an audio-bank directory if needed; existing directories are valid. */
esp_err_t sd_storage_mkdir(const char *path);
/* Add a line (finished by \n) to a text file; the header is written first if the file is empty */
esp_err_t sd_storage_append_line(const char *path, const char *header, const char *line);

/* Count WAV clips in the voice-bank directory; -1 means card unavailable. */
int sd_storage_voice_files(void);
void sd_storage_refresh_voice_files(void);

/* Enumerate files in the selected language's voice directory.
 * Invoke the callback once per file; return its count, or -1 on a storage
 * failure. Optionally report free space in megabytes. */
typedef void (*sd_voice_file_cb_t)(const char *name, long size, void *ctx);
int sd_storage_list_voice(const char *subdir, sd_voice_file_cb_t cb, void *ctx, uint32_t *free_mb);

/* Append a real Dexcom reading from the newest-first cache, never demo data. */
void sd_storage_record_readings(const glucose_point_t *cache, int count, int64_t previous_newest_ms);

/* Record technical events without secrets or synthetic demonstration data. */
void sd_storage_log_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Console de maintenance */
const char *sd_storage_request_format(const char *confirmation);
void sd_storage_request_check(void);
void sd_storage_request_repair(void); /* remove CSV exact duplicates only (if not intact file) */
void sd_storage_test_eject(int seconds);
void sd_storage_test_remount(int cycles);
/* Cleaning after a test: remove from logs/consumables.csv the date lines of from_s (epoch) ou plus tard */
void sd_storage_request_consumables_purge(int64_t from_s);
