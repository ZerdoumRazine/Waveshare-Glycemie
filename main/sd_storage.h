/* sd_storage.h: microSD storage and history access interface.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Carte microSD (optionnelle) : historique CSV quotidien et journal d'evenements.
 *
 * - Montage APRES le demarrage du Wi-Fi heberge (le controleur SDMMC is shared with theESP32-C6).
 * - Never automatic formatting: an unrecognized file system stops writing; only the command
 *   console "sd_format EFFACER" formate (FAT32, cluster 32 KB) a map to the unrecognized file system.
 * - The main operation never depends on the card: removal or error -> demontage, etat
 *   "unavailable", new spatial attempts, then resumption of history without duplicate.
 * - Dossiers : /glycemie/history (AAAA-MM-JJ.csv), /glycemie/audio (voice : banque vocale WAV),
 *   /glycemie/logs (AAAA-MM-JJ.log).
 */
/* Public interface / Interface publique
 * FAT32 history storage, status reporting and strictly scoped maintenance APIs.
 */
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

/* Access to files from another spot (audio, console), never from the interface: protect against a
 * demontage simultane de la carte. ESP_ERR_NOT_FOUND : card unavailable or file missing. */
esp_err_t sd_storage_read_file(const char *path, uint8_t *buf, size_t max, size_t *len);
/* Console (read only): displays a file of /glycemie/logs (journal AAAA-MM-JJ.log, consumables.csv) */
void sd_storage_print_log(const char *name);
/* Precise reading of a /glycemia file by the console (base64 + CRC-32), without modifying anything:
 * sert a sauvegarder la carte quand elle ne peut pas etre sortie de l'appareil. */
void sd_storage_dump_file(const char *rel);
esp_err_t sd_storage_write_file(const char *path, const uint8_t *data, size_t len); /* .tmp puis renommage */
/* Create a folder if it does not exist (voice bank of a language). ESP_OK s'il existe deja. */
esp_err_t sd_storage_mkdir(const char *path);
/* Add a line (finished by \n) to a text file; the header is written first if the file is empty */
esp_err_t sd_storage_append_line(const char *path, const char *header, const char *line);

/* Voice bank: number of files WAV in /glycemie/audio/voice (-1 si carte indisponible), relu au montage */
int sd_storage_voice_files(void);
void sd_storage_refresh_voice_files(void);

/* Parcours de /glycemie/audio/voice (console), ou de son sous-dossier de langue si subdir est renseigne :
 * cb is called for each FICHIER (subfolders are not known); returns the number of files,
 * -1 si la carte est indisponible. free_mb recoit l'espace libre de la carte. */
typedef void (*sd_voice_file_cb_t)(const char *name, long size, void *ctx);
int sd_storage_list_voice(const char *subdir, sd_voice_file_cb_t cb, void *ctx, uint32_t *free_mb);

/* Reminder of the Glycemie service after each new measurement: copy of the Dexcom reel cache (more recent first) */
void sd_storage_record_readings(const glucose_point_t *cache, int count, int64_t previous_newest_ms);

/* Journal of technical events (never secret, never in demonstration mode) */
void sd_storage_log_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Console de maintenance */
const char *sd_storage_request_format(const char *confirmation);
void sd_storage_request_check(void);
void sd_storage_request_repair(void); /* remove CSV exact duplicates only (if not intact file) */
void sd_storage_test_eject(int seconds);
void sd_storage_test_remount(int cycles);
/* Cleaning after a test: remove from logs/consumables.csv the date lines of from_s (epoch) ou plus tard */
void sd_storage_request_consumables_purge(int64_t from_s);
