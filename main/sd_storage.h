/* sd_storage.h: microSD storage and history access interface.
 * FR: Interface de stockage et d'historique microSD.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Carte microSD (optionnelle) : historique CSV quotidien et journal d'evenements.
 *
 * - Montage APRES le demarrage du Wi-Fi heberge (le controleur SDMMC est partage avec l'ESP32-C6).
 * - Jamais de formatage automatique : un systeme de fichiers non reconnu arrete l'ecriture ; seule la commande
 *   console "sd_format EFFACER" formate (FAT32, cluster 32 Ko) une carte au systeme de fichiers non reconnu.
 * - Le fonctionnement principal ne depend jamais de la carte : retrait ou erreur -> demontage, etat
 *   "indisponible", nouvelles tentatives espacees, puis reprise de l'historique sans doublon.
 * - Dossiers : /glycemie/history (AAAA-MM-JJ.csv), /glycemie/audio (voice : banque vocale WAV),
 *   /glycemie/logs (AAAA-MM-JJ.log).
 */
/* Public interface / Interface publique
 * EN: FAT32 history storage, status reporting and strictly scoped maintenance APIs.
 * FR: Stockage FAT32 de l'historique et maintenance limitée.
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

/* Acces aux fichiers depuis une autre tache (audio, console), jamais depuis l'interface : protege contre un
 * demontage simultane de la carte. ESP_ERR_NOT_FOUND : carte indisponible ou fichier absent. */
esp_err_t sd_storage_read_file(const char *path, uint8_t *buf, size_t max, size_t *len);
/* Console (lecture seule) : affiche un fichier de /glycemie/logs (journal AAAA-MM-JJ.log, consumables.csv) */
void sd_storage_print_log(const char *name);
/* Relecture exacte d'un fichier de /glycemie par la console (base64 + CRC-32), sans rien modifier :
 * sert a sauvegarder la carte quand elle ne peut pas etre sortie de l'appareil. */
void sd_storage_dump_file(const char *rel);
esp_err_t sd_storage_write_file(const char *path, const uint8_t *data, size_t len); /* .tmp puis renommage */
/* Cree un dossier s'il n'existe pas (banque vocale d'une langue). ESP_OK s'il existe deja. */
esp_err_t sd_storage_mkdir(const char *path);
/* Ajoute une ligne (terminee par \n) a un fichier texte ; l'en-tete est ecrit d'abord si le fichier est vide */
esp_err_t sd_storage_append_line(const char *path, const char *header, const char *line);

/* Banque vocale : nombre de fichiers WAV dans /glycemie/audio/voice (-1 si carte indisponible), relu au montage */
int sd_storage_voice_files(void);
void sd_storage_refresh_voice_files(void);

/* Parcours de /glycemie/audio/voice (console), ou de son sous-dossier de langue si subdir est renseigne :
 * cb est appele pour chaque FICHIER (les sous-dossiers sont ignores) ; retourne le nombre de fichiers,
 * -1 si la carte est indisponible. free_mb recoit l'espace libre de la carte. */
typedef void (*sd_voice_file_cb_t)(const char *name, long size, void *ctx);
int sd_storage_list_voice(const char *subdir, sd_voice_file_cb_t cb, void *ctx, uint32_t *free_mb);

/* Rappel du service glycemie apres chaque nouvelle mesure : copie du cache Dexcom reel (plus recente en premier) */
void sd_storage_record_readings(const glucose_point_t *cache, int count, int64_t previous_newest_ms);

/* Journal d'evenements techniques (jamais de secret, jamais en mode demonstration) */
void sd_storage_log_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Console de maintenance */
const char *sd_storage_request_format(const char *confirmation);
void sd_storage_request_check(void);
void sd_storage_request_repair(void); /* supprime des CSV les doublons exacts uniquement (sinon fichier intact) */
void sd_storage_test_eject(int seconds);
void sd_storage_test_remount(int cycles);
/* Nettoyage apres un test : retire de logs/consumables.csv les lignes datees de from_s (epoch) ou plus tard */
void sd_storage_request_consumables_purge(int64_t from_s);
