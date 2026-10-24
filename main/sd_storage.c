/*
 * sd_storage.c: microSD history, files and storage checks.
 * Design note: Never automatically format the user's card.
 */

/*
 * Mounts FAT32 microSD storage and manages append-only glucose history with repair tools.
 * Manage microSD FAT32, l'historique CSV and consistency checks.
 */
#include "sd_storage.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "ff.h" /* before diskio_sdmmc.h (type BYTE) */
#include "diskio_sdmmc.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sd_protocol_defs.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"
#include "consumables.h"
#include "glucose_math.h"
#include "net_services.h"
#include "wifi_manager.h"

static const char *TAG = "microsd";

/* Montage identique au BSP Waveshare (bsp_sdcard_mount) : slot SDMMC 0 en 4 bits, LDO interne n° 4,
 * but without automatic formatting and with a power supply created only once. */
#define SD_LDO_CHANNEL    4
#define SD_CLUSTER_SIZE   (32 * 1024) /* FAT32 : 32 KB cluster for a large capacity map */
#define EVENT_QUEUE_LEN   16
#define EVENT_LEN         112
#define PROBE_PERIOD_S    30          /* map presence (CMD13, pilot protected control) */
#define RETRY_IDLE_S      3600        /* card missing or not recognized since start */
#define CHECK_MAX_LINES   1024
#define HISTORY_DIR       SD_MOUNT_POINT "/glycemie/history"
#define LOGS_DIR          SD_MOUNT_POINT "/glycemie/logs"
#define CSV_HEADER        "timestamp,glucose,trend,delta\n"

/* After a withdrawal: new and more spatial attempts. Each mount reconfigures
 * l'horloge du controleur SDMMC sharing with theESP32-C6 : leur nombre est volontairement limite. */
static const int32_t k_retry_after_loss_s[] = {30, 60, 120, 300, 600, 1800, 3600};
#define RETRY_STEPS (int)(sizeof(k_retry_after_loss_s) / sizeof(k_retry_after_loss_s[0]))

/* Traces of SD components masked during automatic attempts (not those of the link ESP32-C6) */
static const char *const k_quiet_tags[] = {"vfs_fat_sdmmc", "sdmmc_common", "sdmmc_init", "sdmmc_sd", "sdmmc_periph"};
#define QUIET_TAGS (int)(sizeof(k_quiet_tags) / sizeof(k_quiet_tags[0]))

typedef enum {
    MOUNT_BOOT = 0, /* start: complete traces, retest in 1 h if absent */
    MOUNT_RETRY,    /* automatic recovery: masked traces, growing times */
    MOUNT_CHECK,    /* After formatting or testing: complete traces */
} mount_reason_t;

typedef struct {
    int64_t uptime_us;
    char text[EVENT_LEN];
} sd_event_t;

static TaskHandle_t s_task;
static QueueHandle_t s_events;
static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr;
static volatile bool s_mounted;
static volatile sd_state_t s_state = SD_STATE_UNKNOWN;
/* Life cycle of the map (mounting) against the lectures/ecritures other stains (voice, console);
 * recursive: the console can read a file during the path of the voice bank folder */
static SemaphoreHandle_t s_card_mutex;
static volatile int s_voice_files = -1;

/* Retain only the last real Dexcom data snapshot, never synthetic demo readings. */
static glucose_point_t *s_snapshot;
static glucose_point_t *s_work;
static int s_snapshot_count;
static SemaphoreHandle_t s_snapshot_mutex;
static volatile bool s_snapshot_pending;

static int64_t s_last_written_ms;
static bool s_last_written_known;

static int s_retry_step;
static int64_t s_next_retry_us;
static int64_t s_next_probe_us;

static volatile bool s_req_format;
static volatile bool s_req_check;
static volatile bool s_req_repair;
static volatile int32_t s_req_eject_s;
static volatile int32_t s_req_remount_cycles;
static volatile bool s_req_purge;
static int64_t s_purge_from_s;

/* ------------------------------------------------------------------------- */

static int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + doe - 719468;
}

static void format_iso(int64_t epoch_ms, char *buf, size_t size)
{
    time_t t = (time_t)(epoch_ms / 1000);
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    char tz[8] = "+0000";
    strftime(tz, sizeof(tz), "%z", &tm_local);
    snprintf(buf, size, "%04d-%02d-%02dT%02d:%02d:%02d%.3s:%.2s", tm_local.tm_year + 1900, tm_local.tm_mon + 1,
             tm_local.tm_mday, tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec, tz, tz + 3);
}

static bool parse_iso(const char *s, int64_t *epoch_ms)
{
    int y, mo, d, h, mi, se, oh, om;
    char sign;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%c%2d:%2d", &y, &mo, &d, &h, &mi, &se, &sign, &oh, &om) != 9) {
        return false;
    }
    int64_t epoch = days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + se;
    const int64_t offset = (int64_t)oh * 3600 + om * 60;
    epoch -= sign == '-' ? -offset : offset;
    *epoch_ms = epoch * 1000;
    return true;
}

/* Local day file of epoch_ms of day_offset days (calendar calculation, time-sensitive) */
static void day_file(const char *dir, const char *ext, int64_t epoch_ms, int day_offset, char *path, size_t size)
{
    time_t t = (time_t)(epoch_ms / 1000);
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    if (day_offset != 0) {
        tm_local.tm_mday += day_offset;
        tm_local.tm_hour = 12;
        tm_local.tm_isdst = -1;
        t = mktime(&tm_local);
        localtime_r(&t, &tm_local);
    }
    snprintf(path, size, "%s/%04d-%02d-%02d.%s", dir, tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
             ext);
}

static void set_state(sd_state_t st, const char *type, uint32_t capacity_mb, uint32_t free_mb)
{
    s_state = st;
    app_state_set_sd(st, type, capacity_mb, free_mb);
}

static const char *card_type(const sdmmc_card_t *card, uint64_t bytes)
{
    if (card->is_sdio) {
        return "SDIO";
    }
    if (card->is_mmc) {
        return "MMC";
    }
    if (card->ocr & SD_OCR_SDHC_CAP) {
        return bytes > 32ULL * 1024 * 1024 * 1024 ? "SDXC" : "SDHC";
    }
    return "SDSC";
}

static bool make_dir(const char *path)
{
    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return true;
    }
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* ------------------------------------------------------------------------- */
/* Montage */

static bool make_host(sdmmc_host_t *host, sdmmc_slot_config_t *slot_config)
{
    *host = (sdmmc_host_t)SDMMC_HOST_DEFAULT();
    host->slot = SDMMC_HOST_SLOT_0;
    host->max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    if (s_pwr == NULL) {
        const sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = SD_LDO_CHANNEL,
        };
        if (sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &s_pwr) != ESP_OK) {
            return false;
        }
    }
    host->pwr_ctrl_handle = s_pwr;
    *slot_config = (sdmmc_slot_config_t){
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };
    return true;
}

static void quiet_logs(bool quiet, esp_log_level_t saved[QUIET_TAGS])
{
    for (int i = 0; i < QUIET_TAGS; i++) {
        if (quiet) {
            saved[i] = esp_log_level_get(k_quiet_tags[i]);
            esp_log_level_set(k_quiet_tags[i], ESP_LOG_NONE);
        } else {
            esp_log_level_set(k_quiet_tags[i], saved[i]);
        }
    }
}

/* format : formatage FAT32 if the file system is not recognized (explicit command only) */
static esp_err_t mount_fs(bool format, bool quiet)
{
    sdmmc_host_t host;
    sdmmc_slot_config_t slot_config;
    if (!make_host(&host, &slot_config)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = format,
        .max_files = 4,
        .allocation_unit_size = SD_CLUSTER_SIZE,
    };
    esp_log_level_t saved[QUIET_TAGS];
    if (quiet) {
        quiet_logs(true, saved);
    }
    const esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (quiet) {
        quiet_logs(false, saved);
    }
    if (err != ESP_OK) {
        s_card = NULL;
    }
    return err;
}

/* Present card but not mounted: read-only identification (registers) OCR et CSD), no writing */
static void identify_card(void)
{
    sdmmc_host_t host;
    sdmmc_slot_config_t slot_config;
    sdmmc_card_t *card = calloc(1, sizeof(sdmmc_card_t));
    if (card == NULL || !make_host(&host, &slot_config)) {
        free(card);
        return;
    }
    if (host.init() == ESP_OK && sdmmc_host_init_slot(host.slot, &slot_config) == ESP_OK) {
        if (sdmmc_card_init(&host, card) == ESP_OK) {
            const uint64_t capacity = (uint64_t)card->csd.capacity * card->csd.sector_size;
            const char *type = card_type(card, capacity);
            ESP_LOGW(TAG, "Type : %s, capacite : %" PRIu64 " Mo, espace libre : inconnu (carte non montee)", type,
                     capacity / (1024 * 1024));
            set_state(SD_STATE_UNSUPPORTED, type, (uint32_t)(capacity / (1024 * 1024)), 0);
        }
        host.deinit_p(host.slot); /* slot 0 only: the link SDIO of theESP32-C6 remains active */
    }
    free(card);
}

static const char *fs_details(uint32_t *cluster_kb)
{
    *cluster_kb = 0;
    const BYTE pdrv = ff_diskio_get_pdrv_card(s_card);
    if (pdrv == 0xFF) {
        return "?";
    }
    const char drv[3] = {(char)('0' + pdrv), ':', 0};
    FATFS *fs = NULL;
    DWORD free_clusters = 0;
    if (f_getfree(drv, &free_clusters, &fs) != FR_OK || fs == NULL) {
        return "?";
    }
    *cluster_kb = (uint32_t)fs->csize * fs->ssize / 1024;
    switch (fs->fs_type) {
    case FS_FAT12: return "FAT12";
    case FS_FAT16: return "FAT16";
    case FS_FAT32: return "FAT32";
    default: return "?";
    }
}

static void unmount_card(void)
{
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    if (s_mounted && s_card != NULL) {
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    }
    s_card = NULL;
    s_mounted = false;
    s_voice_files = -1;
    xSemaphoreGiveRecursive(s_card_mutex);
    s_last_written_known = false;
}

static bool is_wav_name(const char *name)
{
    const size_t len = strlen(name);
    return len > 4 && strcasecmp(name + len - 4, ".wav") == 0;
}

/* Carte montee, verrou pris */
static int count_voice_files_locked(void)
{
    DIR *dir = opendir(SD_VOICE_DIR);
    if (dir == NULL) {
        return 0;
    }
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        count += is_wav_name(entry->d_name);
    }
    closedir(dir);
    return count;
}

static void update_free_space(void)
{
    uint64_t total = 0;
    uint64_t free_bytes = 0;
    if (esp_vfs_fat_info(SD_MOUNT_POINT, &total, &free_bytes) == ESP_OK) {
        const uint64_t capacity = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
        set_state(SD_STATE_OK, card_type(s_card, capacity), (uint32_t)(capacity / (1024 * 1024)),
                  (uint32_t)(free_bytes / (1024 * 1024)));
    }
}

static void schedule_retry(int32_t delay_s)
{
    s_next_retry_us = esp_timer_get_time() + (int64_t)delay_s * 1000000;
}

/* Mounted map: verification, folders, historical recovery */
static bool on_mounted(mount_reason_t reason)
{
    s_mounted = true;
    const uint64_t capacity = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
    uint64_t total = 0;
    uint64_t free_bytes = 0;
    esp_vfs_fat_info(SD_MOUNT_POINT, &total, &free_bytes);
    uint32_t cluster_kb = 0;
    const char *fs_type = fs_details(&cluster_kb);
    const char *type = card_type(s_card, capacity);
    ESP_LOGI(TAG, "Carte detectee : OUI%s", reason == MOUNT_RETRY ? " (de nouveau disponible)" : "");
    ESP_LOGI(TAG, "Type : %s, capacite : %" PRIu64 " Mo, %s %" PRIu64 " Mo, espace libre : %" PRIu64
             " Mo, cluster %" PRIu32 " Ko", type, capacity / (1024 * 1024), fs_type, total / (1024 * 1024),
             free_bytes / (1024 * 1024), cluster_kb);

    if (!make_dir(SD_MOUNT_POINT "/glycemie") || !make_dir(HISTORY_DIR) || !make_dir(SD_AUDIO_DIR) ||
        !make_dir(SD_VOICE_DIR) || !make_dir(LOGS_DIR)) {
        ESP_LOGW(TAG, "microSD : creation des dossiers impossible, nouvel essai automatique");
        unmount_card();
        set_state(SD_STATE_ERROR, "", 0, 0);
        s_retry_step = 1;
        schedule_retry(k_retry_after_loss_s[0]);
        return false;
    }
    ESP_LOGI(TAG, "Dossiers /glycemie/history, /glycemie/audio/voice, /glycemie/logs prets");
    sd_storage_refresh_voice_files();
    ESP_LOGI(TAG, "Banque vocale : %d fichier(s) WAV dans /glycemie/audio/voice%s", s_voice_files,
             s_voice_files > 0 ? "" : " (voix absente : bip d'alerte seul)");
    set_state(SD_STATE_OK, type, (uint32_t)(capacity / (1024 * 1024)), (uint32_t)(free_bytes / (1024 * 1024)));
    s_last_written_known = false;
    s_snapshot_pending = true;
    s_retry_step = 0;
    s_next_probe_us = esp_timer_get_time() + (int64_t)PROBE_PERIOD_S * 1000000;
    return true;
}

static void try_mount(mount_reason_t reason)
{
    const sd_state_t previous = s_state;
    const esp_err_t err = mount_fs(false, reason == MOUNT_RETRY);
    if (err == ESP_OK) {
        if (on_mounted(reason)) {
            sd_storage_log_event("%s", reason == MOUNT_BOOT    ? "Demarrage de glycemie_display"
                                       : reason == MOUNT_RETRY ? "Carte microSD de nouveau disponible"
                                                               : "Carte microSD remontee");
        }
        return;
    }
    if (err == ESP_FAIL) {
        if (previous != SD_STATE_UNSUPPORTED) {
            ESP_LOGW(TAG, "Carte detectee : OUI, mais systeme de fichiers NON RECONNU (FAT/FAT32 requis)");
            ESP_LOGW(TAG, "Aucun formatage automatique : ecriture microSD arretee (Dexcom et affichage non affectes)");
            set_state(SD_STATE_UNSUPPORTED, "", 0, 0);
            identify_card();
        }
    } else {
        if (previous != SD_STATE_ABSENT) {
            ESP_LOGW(TAG, "Carte detectee : NON (%s) : microSD indisponible, nouvel essai automatique",
                     esp_err_to_name(err));
        }
        set_state(SD_STATE_ABSENT, "", 0, 0);
    }
    if (reason == MOUNT_BOOT) {
        s_retry_step = RETRY_STEPS - 1;
        schedule_retry(RETRY_IDLE_S);
    } else {
        const int step = s_retry_step < RETRY_STEPS ? s_retry_step : RETRY_STEPS - 1;
        schedule_retry(k_retry_after_loss_s[step]);
        s_retry_step = step + 1;
    }
}

/* Card removed or in error: clean assembly, the rest of the system continues */
static void lose_card(const char *why)
{
    if (!s_mounted) {
        return;
    }
    ESP_LOGW(TAG, "microSD indisponible : %s (affichage, Dexcom, Wi-Fi, alertes et voix non affectes)", why);
    unmount_card();
    set_state(SD_STATE_ABSENT, "", 0, 0);
    xQueueReset(s_events);
    s_retry_step = 1;
    schedule_retry(k_retry_after_loss_s[0]);
}

/* ------------------------------------------------------------------------- */
/* Historique CSV */

/* Time stamping of the last complete line of a CSV (0 if no measurement). An incomplete line
 * (card removed or cut during writing) is removed. Returns false in case of access error. */
static bool last_line_time(const char *path, int64_t *out_ms)
{
    *out_ms = 0;
    struct stat st;
    if (stat(path, &st) != 0) {
        return errno == ENOENT;
    }
    if (st.st_size == 0) {
        return true;
    }
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }
    char buf[257];
    const long start = st.st_size > 256 ? (long)st.st_size - 256 : 0;
    size_t n = 0;
    if (fseek(f, start, SEEK_SET) == 0) {
        n = fread(buf, 1, sizeof(buf) - 1, f);
    }
    fclose(f);
    if (n != (size_t)(st.st_size - start)) {
        return false;
    }
    buf[n] = '\0';

    int last_nl = (int)n - 1;
    while (last_nl >= 0 && buf[last_nl] != '\n') {
        last_nl--;
    }
    if (last_nl < (int)n - 1 && (last_nl >= 0 || start == 0)) {
        const long keep = start + last_nl + 1;
        ESP_LOGW(TAG, "Ligne incomplete supprimee a la fin de %s (%ld octets)", path, (long)st.st_size - keep);
        if (truncate(path, keep) != 0) {
            return false;
        }
    }
    if (last_nl < 0) {
        return true;
    }
    buf[last_nl] = '\0';
    char *line = strrchr(buf, '\n');
    line = line != NULL ? line + 1 : buf;
    int64_t ms = 0;
    if (parse_iso(line, &ms)) {
        *out_ms = ms;
    }
    return true;
}

/* Last measure already written: file of the day of the most recent measure, if not the day before */
static bool find_last_written(int64_t newest_ms, int64_t *out_ms)
{
    *out_ms = 0;
    for (int back = 0; back < 2; back++) {
        char path[64];
        day_file(HISTORY_DIR, "csv", newest_ms, -back, path, sizeof(path));
        int64_t ms = 0;
        if (!last_line_time(path, &ms)) {
            return false;
        }
        if (ms > 0) {
            *out_ms = ms;
            return true;
        }
    }
    return true;
}

static FILE *open_csv(const char *path)
{
    struct stat st;
    const bool has_content = stat(path, &st) == 0 && st.st_size > 0;
    FILE *f = fopen(path, "a");
    if (f != NULL && !has_content && fputs(CSV_HEADER, f) < 0) {
        fclose(f);
        return NULL;
    }
    return f;
}

/* Writes once each measure more recent than the last line of the CSV, from the oldest to the most
 * recent. Only one fclose (draining and updating of the FAT) per batch : about one writing every 5 minutes. */
/* Merge delayed readings with the last stored day and avoid duplicate CSV rows. */
static void sync_history(void)
{
    xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY);
    const int count = s_snapshot_count;
    if (count > 0) {
        memcpy(s_work, s_snapshot, (size_t)count * sizeof(glucose_point_t));
    }
    s_snapshot_pending = false;
    xSemaphoreGive(s_snapshot_mutex);
    if (count == 0) {
        return;
    }

    if (!s_last_written_known) {
        if (!find_last_written(s_work[0].wt_ms, &s_last_written_ms)) {
            lose_card("lecture de l'historique impossible");
            return;
        }
        s_last_written_known = true;
    }

    FILE *f = NULL;
    char open_path[64] = "";
    uint32_t written = 0;
    for (int i = count - 1; i >= 0; i--) {
        const glucose_point_t *p = &s_work[i];
        if (p->wt_ms / 1000 <= s_last_written_ms / 1000) {
            continue; /* CSV timestamps have second-level precision; compare at that granularity. */
        }
        char path[64];
        day_file(HISTORY_DIR, "csv", p->wt_ms, 0, path, sizeof(path));
        if (strcmp(path, open_path) != 0) {
            if (f != NULL && fclose(f) != 0) {
                lose_card("fermeture du fichier impossible");
                return;
            }
            f = open_csv(path);
            if (f == NULL) {
                lose_card("ecriture impossible");
                return;
            }
            strlcpy(open_path, path, sizeof(open_path));
        }
        char iso[64];
        format_iso(p->wt_ms, iso, sizeof(iso));
        char delta[8] = "";
        int d = 0;
        if (i + 1 < count && gm_delta_between(p, &s_work[i + 1], &d)) {
            snprintf(delta, sizeof(delta), "%d", d);
        }
        if (fprintf(f, "%s,%d,%s,%s\n", iso, p->mgdl, trend_name(p->trend), delta) < 0) {
            fclose(f);
            lose_card("ecriture impossible");
            return;
        }
        s_last_written_ms = p->wt_ms;
        written++;
    }
    if (f != NULL && fclose(f) != 0) {
        lose_card("fermeture du fichier impossible");
        return;
    }
    if (written > 0) {
        app_state_add_sd_rows(written);
        update_free_space();
        ESP_LOGI(TAG, "Historique : %" PRIu32 " mesure(s) ajoutee(s)", written);
    }
}

static bool write_events(void)
{
    if (!net_time_is_valid() || uxQueueMessagesWaiting(s_events) == 0) {
        return true; /* reliable time stamping required: events await the time SNTP */
    }
    FILE *f = NULL;
    sd_event_t ev;
    while (xQueueReceive(s_events, &ev, 0) == pdTRUE) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        const int64_t epoch_ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000 -
                                 (esp_timer_get_time() - ev.uptime_us) / 1000;
        if (f == NULL) {
            char path[64];
            day_file(LOGS_DIR, "log", epoch_ms, 0, path, sizeof(path));
            f = fopen(path, "a");
            if (f == NULL) {
                return false;
            }
        }
        char iso[64];
        format_iso(epoch_ms, iso, sizeof(iso));
        if (fprintf(f, "%s %s\n", iso, ev.text) < 0) {
            fclose(f);
            return false;
        }
    }
    return f == NULL || fclose(f) == 0;
}

/* ------------------------------------------------------------------------- */
/* Maintenance (console) */

static void format_card(void)
{
    if (s_mounted || s_state != SD_STATE_UNSUPPORTED) {
        ESP_LOGW(TAG, "Formatage refuse : seule une carte presente au systeme de fichiers non reconnu est formatee");
        return;
    }
    ESP_LOGW(TAG, "FORMATAGE FAT32 de la microSD (autorise par l'utilisateur, cluster %d Ko) : contenu efface",
             SD_CLUSTER_SIZE / 1024);
    const int64_t t0 = esp_timer_get_time();
    esp_err_t err = mount_fs(true, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Formatage impossible (%s) : microSD hors service", esp_err_to_name(err));
        set_state(SD_STATE_ERROR, "", 0, 0);
        return;
    }
    s_mounted = true;
    uint32_t cluster_kb = 0;
    const char *fs_type = fs_details(&cluster_kb);
    ESP_LOGW(TAG, "Formatage termine en %" PRId64 " s : %s, cluster %" PRIu32 " Ko",
             (esp_timer_get_time() - t0) / 1000000, fs_type, cluster_kb);

    /* Demounting then normal winding (without formatting) to check the map */
    unmount_card();
    ESP_LOGI(TAG, "Carte demontee apres formatage : remontage de verification");
    vTaskDelay(pdMS_TO_TICKS(500));
    set_state(SD_STATE_UNKNOWN, "", 0, 0);
    s_retry_step = 0;
    try_mount(MOUNT_CHECK);
    if (s_mounted) {
        sd_storage_log_event("Carte microSD formatee en %s (cluster %" PRIu32 " Ko)", fs_type, cluster_kb);
    }
}

static int cmp_int64(const void *a, const void *b)
{
    const int64_t x = *(const int64_t *)a;
    const int64_t y = *(const int64_t *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static void check_csv(const char *path, const char *name, int64_t *times)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("SD_CSV %s illisible\n", name);
        return;
    }
    char line[128];
    char first[128] = "";
    char last[128] = "";
    int lines = 0;
    int invalid = 0;
    int disorder = 0;
    int n = 0;
    bool header = false;
    int64_t previous = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        lines++;
        if (lines == 1 && strcmp(line, "timestamp,glucose,trend,delta") == 0) {
            header = true;
            continue;
        }
        int64_t ms = 0;
        if (!parse_iso(line, &ms)) {
            invalid++;
            continue;
        }
        if (n > 0 && ms <= previous) {
            disorder++;
        }
        previous = ms;
        if (n == 0) {
            strlcpy(first, line, sizeof(first));
        }
        strlcpy(last, line, sizeof(last));
        if (n < CHECK_MAX_LINES) {
            times[n] = ms;
        }
        n++;
    }
    fclose(f);
    const int stored = n < CHECK_MAX_LINES ? n : CHECK_MAX_LINES;
    qsort(times, (size_t)stored, sizeof(int64_t), cmp_int64);
    int duplicates = 0;
    for (int i = 1; i < stored; i++) {
        if (times[i] / 1000 == times[i - 1] / 1000) {
            duplicates++;
        }
    }
    printf("SD_CSV %s entete=%s mesures=%d doublons=%d desordre=%d invalides=%d\n", name, header ? "oui" : "NON", n,
           duplicates, disorder, invalid);
    printf("SD_CSV %s premiere=%s\n", name, first);
    printf("SD_CSV %s derniere=%s\n", name, last);
}

/* Report consistency errors rather than assuming that old data can be discarded. */
static void check_card(void)
{
    if (!s_mounted) {
        printf("SD_CHECK carte non montee (etat %d)\n", (int)s_state);
        return;
    }
    const uint64_t capacity = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
    uint64_t total = 0;
    uint64_t free_bytes = 0;
    esp_vfs_fat_info(SD_MOUNT_POINT, &total, &free_bytes);
    uint32_t cluster_kb = 0;
    const char *fs_type = fs_details(&cluster_kb);
    printf("SD_CHECK type=%s capacite_mo=%" PRIu64 " format=%s cluster_ko=%" PRIu32 " volume_mo=%" PRIu64
           " libre_mo=%" PRIu64 " pile_libre=%u\n", card_type(s_card, capacity), capacity / (1024 * 1024), fs_type,
           cluster_kb, total / (1024 * 1024), free_bytes / (1024 * 1024),
           (unsigned)uxTaskGetStackHighWaterMark(NULL));

    static const char *const dirs[] = {HISTORY_DIR, SD_AUDIO_DIR, LOGS_DIR};
    int64_t *times = heap_caps_calloc(CHECK_MAX_LINES, sizeof(int64_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (dir == NULL) {
            printf("SD_DIR %s ABSENT\n", dirs[d] + strlen(SD_MOUNT_POINT));
            continue;
        }
        int files = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            char path[300];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], entry->d_name);
            struct stat st;
            const long size = stat(path, &st) == 0 ? (long)st.st_size : -1;
            printf("SD_FILE %s/%s %ld octets\n", dirs[d] + strlen(SD_MOUNT_POINT), entry->d_name, size);
            const size_t len = strlen(entry->d_name);
            if (times != NULL && d == 0 && len > 4 && strcasecmp(entry->d_name + len - 4, ".csv") == 0) {
                check_csv(path, entry->d_name, times);
            }
            files++;
        }
        closedir(dir);
        printf("SD_DIR %s %d fichier(s)\n", dirs[d] + strlen(SD_MOUNT_POINT), files);
    }
    free(times);
}

/* Rewrite a CSV without its exact duplicates.
 * If another line disappears (order, invalid or different), the file remains intact (-2):
 * no single measure is ever removed. */
static int repair_csv(const char *path, const char *name)
{
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= 0) {
        return 0;
    }
    const size_t size = (size_t)st.st_size;
    char *in = heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *out = heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    FILE *f = in != NULL && out != NULL ? fopen(path, "r") : NULL;
    const size_t n = f != NULL ? fread(in, 1, size, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    int removed = -1;
    if (n == size) {
        in[size] = '\0';
        removed = 0;
        size_t used = 0;
        int64_t previous_s = 0;
        const char *kept = NULL;
        size_t kept_len = 0;
        bool first = true;
        bool unsafe = false;
        for (char *line = in; *line != '\0';) {
            char *end = strchr(line, '\n');
            const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
            const char saved = line[len];
            line[len] = '\0';
            int64_t ms = 0;
            bool keep = false;
            if (first && strcmp(line, "timestamp,glucose,trend,delta") == 0) {
                keep = true;
            } else if (end != NULL && parse_iso(line, &ms) && ms / 1000 > previous_s) {
                keep = true;
                previous_s = ms / 1000;
            }
            if (keep) {
                kept = out + used;
                kept_len = len;
                memcpy(out + used, line, len);
                used += len;
                out[used++] = '\n';
            } else {
                const bool exact = kept != NULL && end != NULL && ms / 1000 == previous_s && len == kept_len &&
                                   memcmp(line, kept, len) == 0;
                printf("SD_REPAIR %s %s %s\n", name, exact ? "doublon_exact" : "NON_DOUBLON", line);
                unsafe = unsafe || !exact;
                removed++;
            }
            line[len] = saved;
            first = false;
            line = end != NULL ? end + 1 : line + len;
        }
        if (unsafe) {
            removed = -2; /* file left intact */
        } else if (removed > 0) {
            char tmp[80];
            snprintf(tmp, sizeof(tmp), "%.*s.tmp", (int)(strlen(path) - 4), path);
            FILE *w = fopen(tmp, "w");
            const bool ok = w != NULL && fwrite(out, 1, used, w) == used;
            if (w == NULL || fclose(w) != 0 || !ok || unlink(path) != 0 || rename(tmp, path) != 0) {
                removed = -1;
            }
        }
    }
    free(in);
    free(out);
    return removed;
}

/* Repair known CSV defects conservatively; keep a separate backup of the card. */
static void repair_history(void)
{
    if (!s_mounted) {
        printf("SD_REPAIR carte non montee\n");
        return;
    }
    DIR *dir = opendir(HISTORY_DIR);
    if (dir == NULL) {
        printf("SD_REPAIR dossier historique absent\n");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const size_t len = strlen(entry->d_name);
        if (len > 4 && strcasecmp(entry->d_name + len - 4, ".csv") == 0) {
            char path[300];
            snprintf(path, sizeof(path), "%s/%s", HISTORY_DIR, entry->d_name);
            const int removed = repair_csv(path, entry->d_name);
            if (removed == -2) {
                printf("SD_REPAIR %s refus : ligne non doublon exact, fichier intact\n", entry->d_name);
            } else {
                printf("SD_REPAIR %s lignes_supprimees=%d\n", entry->d_name, removed);
            }
            if (removed > 0) {
                ESP_LOGW(TAG, "Historique %s : %d doublon(s) exact(s) supprime(s)", entry->d_name, removed);
            }
        }
    }
    closedir(dir);
    s_last_written_known = false;
}

/* Remove consumables.csv test rows from the supplied local timestamp onward. */
static void purge_consumables(int64_t from_s)
{
    static const char path[] = LOGS_DIR "/consumables.csv";
    struct stat st;
    if (!s_mounted || stat(path, &st) != 0 || st.st_size <= 0 || st.st_size > 256 * 1024) {
        printf("CONSO_PURGE fichier absent ou vide\n");
        return;
    }
    const size_t size = (size_t)st.st_size;
    char *in = heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *out = heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    FILE *f = in != NULL && out != NULL ? fopen(path, "r") : NULL;
    const size_t n = f != NULL ? fread(in, 1, size, f) : 0;
    if (f != NULL) {
        fclose(f);
    }
    int removed = 0;
    int kept = 0;
    if (n == size) {
        in[size] = '\0';
        size_t used = 0;
        for (char *line = in; *line != '\0';) {
            char *end = strchr(line, '\n');
            const size_t len = end != NULL ? (size_t)(end - line) : strlen(line);
            struct tm tm_line = {0};
            bool drop = false;
            if (sscanf(line, "%4d-%2d-%2d %2d:%2d", &tm_line.tm_year, &tm_line.tm_mon, &tm_line.tm_mday,
                       &tm_line.tm_hour, &tm_line.tm_min) == 5) {
                tm_line.tm_year -= 1900;
                tm_line.tm_mon -= 1;
                tm_line.tm_isdst = -1;
                drop = (int64_t)mktime(&tm_line) >= from_s / 60 * 60; /* lines date a minute */
            }
            if (drop) {
                printf("CONSO_PURGE retiree : %.*s\n", (int)len, line);
                removed++;
            } else {
                memcpy(out + used, line, len);
                used += len;
                out[used++] = '\n';
                kept++;
            }
            line = end != NULL ? end + 1 : line + len;
        }
        if (removed > 0 && sd_storage_write_file(path, (const uint8_t *)out, used) != ESP_OK) {
            removed = -1;
        }
    }
    free(in);
    free(out);
    printf("CONSO_PURGE lignes_retirees=%d lignes_gardees=%d\n", removed, kept);
}

static void remount_test(int cycles)
{
    ESP_LOGW(TAG, "TEST : %d cycles demontage/remontage de la microSD (lien ESP32-C6 surveille)", cycles);
    const size_t int_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const int64_t t0 = esp_timer_get_time();
    int ok = 0;
    int failed = 0;
    for (int i = 0; i < cycles; i++) {
        unmount_card();
        if (mount_fs(false, true) == ESP_OK) {
            s_mounted = true;
            if (sdmmc_get_status(s_card) == ESP_OK) {
                ok++;
            } else {
                failed++;
            }
        } else {
            failed++;
        }
        if ((i + 1) % 50 == 0) {
            ESP_LOGI(TAG, "TEST remontage : %d/%d (ok %d, echecs %d)", i + 1, cycles, ok, failed);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    unmount_card();
    ESP_LOGW(TAG, "TEST remontage termine : %d cycles en %" PRId64 " s, ok %d, echecs %d, RAM interne %+d o, PSRAM %+d o",
             cycles, (esp_timer_get_time() - t0) / 1000000, ok, failed,
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) - (int)int_before,
             (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) - (int)psram_before);
    set_state(SD_STATE_UNKNOWN, "", 0, 0);
    s_retry_step = 0;
    try_mount(MOUNT_CHECK);
}

/* ------------------------------------------------------------------------- */

static void sd_task(void *arg)
{
    (void)arg;
    /* The Wi-Fi heberge must be initialized before the card (controversy SDMMC sharing with theESP32-C6) */
    const bool wifi_started = wifi_manager_wait_started(pdMS_TO_TICKS(90000));
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "Initialisation de la microSD (Wi-Fi heberge %s)", wifi_started ? "deja demarre" : "non demarre");
    try_mount(MOUNT_BOOT);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (s_req_format) {
            s_req_format = false;
            format_card();
        }
        if (s_req_remount_cycles > 0) {
            const int cycles = s_req_remount_cycles;
            s_req_remount_cycles = 0;
            remount_test(cycles);
        }
        if (s_req_eject_s > 0) {
            const int32_t seconds = s_req_eject_s;
            s_req_eject_s = 0;
            if (s_mounted) {
                lose_card("TEST : retrait simule depuis la console");
                schedule_retry(seconds);
            }
        }

        const int64_t now_us = esp_timer_get_time();
        if (s_req_repair) {
            s_req_repair = false;
            repair_history();
            s_snapshot_pending = s_mounted;
        }
        if (s_mounted) {
            if (s_snapshot_pending) {
                sync_history();
            }
            if (s_mounted && !write_events()) {
                lose_card("journal inaccessible");
            }
            if (s_mounted && s_req_purge) {
                s_req_purge = false;
                purge_consumables(s_purge_from_s);
            }
            if (s_mounted) {
                consumables_sd_flush(); /* lignes de consumables.csv en attente (confirmations reelles) */
            }
            if (s_mounted && now_us >= s_next_probe_us) {
                s_next_probe_us = now_us + (int64_t)PROBE_PERIOD_S * 1000000;
                if (sdmmc_get_status(s_card) != ESP_OK) {
                    lose_card("carte retiree ou ne repond plus");
                }
            }
        } else if (now_us >= s_next_retry_us) {
            try_mount(MOUNT_RETRY);
        }

        if (s_req_check) {
            s_req_check = false;
            check_card();
        }
    }
}

esp_err_t sd_storage_start(void)
{
    s_events = xQueueCreateWithCaps(EVENT_QUEUE_LEN, sizeof(sd_event_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_snapshot = heap_caps_calloc(GLY_HISTORY_MAX, sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_work = heap_caps_calloc(GLY_HISTORY_MAX, sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_snapshot_mutex = xSemaphoreCreateMutex();
    s_card_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_events == NULL || s_snapshot == NULL || s_work == NULL || s_snapshot_mutex == NULL || s_card_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    set_state(SD_STATE_UNKNOWN, "", 0, 0);
    return xTaskCreate(sd_task, "microsd", 6144, NULL, 3, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool sd_storage_ready(void)
{
    return s_mounted;
}

void sd_storage_print_log(const char *name)
{
    enum { LOG_READ_MAX = 128 * 1024 };
    if (name[0] == '\0' || strlen(name) > 40 || strchr(name, '/') != NULL || strchr(name, '\\') != NULL ||
            strstr(name, "..") != NULL) {
        printf("SD_LIRE refuse : nom de fichier de /glycemie/logs attendu\n");
        return;
    }
    char path[96];
    snprintf(path, sizeof(path), LOGS_DIR "/%s", name);
    char *buf = heap_caps_malloc(LOG_READ_MAX + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t len = 0;
    const esp_err_t err = buf != NULL ? sd_storage_read_file(path, (uint8_t *)buf, LOG_READ_MAX, &len) : ESP_ERR_NO_MEM;
    if (err != ESP_OK) {
        printf("SD_LIRE %s : %s\n", name, esp_err_to_name(err));
        free(buf);
        return;
    }
    buf[len] = '\0';
    int lines = 0;
    for (char *line = buf, *next; line != NULL && *line != '\0'; line = next) {
        next = strchr(line, '\n');
        if (next != NULL) {
            *next++ = '\0';
        }
        printf("SD_LIRE| %s\n", line);
        lines++;
        if ((lines & 15) == 0) {
            vTaskDelay(pdMS_TO_TICKS(5)); /* console series: letting the other tracks pass */
        }
    }
    printf("SD_LIRE fin %s : %d lignes, %u octets\n", name, lines, (unsigned)len);
    free(buf);
}

/*
 * sd_dump : Reread a card file by the console, exactly, without changing it.
 *
 * Why: the device does not have an accessible card reader, and exiting the microSD is not
 * Always possible. Content is sent to base64 (lines "SD_DUMP| "), followed by size and
 * of a print CRC-32: The PC can therefore verify that the copy is faithful, byte for byte.
 *
 * Play-only: no files are created, modified, or erased. Only subfolders of
 * /glycemie sont accessibles, et ni ".." ni "\" ne sont acceptes.
 */
void sd_storage_dump_file(const char *rel)
{
    enum { MORCEAU = 48 };   /* 48 octets -> 64 caracteres base64 par ligne */
    if (rel == NULL || rel[0] == '\0' || strlen(rel) > 80 || strstr(rel, "..") != NULL ||
            strchr(rel, '\\') != NULL) {
        printf("SD_DUMP refuse : chemin invalide\n");
        return;
    }
    char path[160];
    if (strncmp(rel, SD_MOUNT_POINT "/glycemie/", strlen(SD_MOUNT_POINT "/glycemie/")) == 0) {
        snprintf(path, sizeof(path), "%s", rel);
    } else if (strncmp(rel, "/glycemie/", 10) == 0) {
        snprintf(path, sizeof(path), SD_MOUNT_POINT "%s", rel);
    } else {
        snprintf(path, sizeof(path), SD_MOUNT_POINT "/glycemie/%s", rel);
    }

    if (s_card_mutex == NULL) {
        printf("SD_DUMP %s : microSD indisponible\n", rel);
        return;
    }
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    FILE *f = s_mounted ? fopen(path, "rb") : NULL;
    if (f == NULL) {
        xSemaphoreGiveRecursive(s_card_mutex);
        printf("SD_DUMP %s : introuvable\n", rel);
        return;
    }
    uint8_t brut[MORCEAU];
    unsigned char code[((MORCEAU + 2) / 3) * 4 + 2];
    uint32_t crc = 0;
    size_t total = 0;
    int lignes = 0;
    size_t n;
    while ((n = fread(brut, 1, sizeof(brut), f)) > 0) {
        size_t sortie = 0;
        if (mbedtls_base64_encode(code, sizeof(code), &sortie, brut, n) != 0) {
            printf("SD_DUMP %s : encodage impossible\n", rel);
            break;
        }
        code[sortie] = '\0';
        printf("SD_DUMP| %s\n", (const char *)code);
        crc = esp_rom_crc32_le(crc, brut, (uint32_t)n);  /* CRC-32 IEEE, identique a zlib.crc32 */
        total += n;
        if ((++lignes & 15) == 0) {
            vTaskDelay(pdMS_TO_TICKS(5)); /* Let other traces of the console pass */
        }
    }
    const bool erreur = ferror(f) != 0;
    fclose(f);
    xSemaphoreGiveRecursive(s_card_mutex);
    if (erreur) {
        printf("SD_DUMP %s : erreur de lecture apres %u octets\n", rel, (unsigned)total);
        return;
    }
    printf("SD_DUMP fin %s octets=%u crc=%08" PRIx32 "\n", rel, (unsigned)total, crc);
}

esp_err_t sd_storage_read_file(const char *path, uint8_t *buf, size_t max, size_t *len)
{
    *len = 0;
    if (s_card_mutex == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (s_mounted) {
        FILE *f = fopen(path, "rb");
        if (f != NULL) {
            const size_t n = fread(buf, 1, max, f);
            if (ferror(f)) {
                err = ESP_FAIL;
            } else if (n == max && fgetc(f) != EOF) {
                err = ESP_ERR_INVALID_SIZE;
            } else {
                *len = n;
                err = ESP_OK;
            }
            fclose(f);
        }
    }
    xSemaphoreGiveRecursive(s_card_mutex);
    return err;
}

esp_err_t sd_storage_mkdir(const char *path)
{
    if (s_card_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (s_mounted) {
        err = make_dir(path) ? ESP_OK : ESP_FAIL;
    }
    xSemaphoreGiveRecursive(s_card_mutex);
    return err;
}

esp_err_t sd_storage_write_file(const char *path, const uint8_t *data, size_t len)
{
    char tmp[96];
    if (s_card_mutex == NULL || snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (s_mounted) {
        err = ESP_FAIL;
        FILE *f = fopen(tmp, "wb");
        if (f != NULL) {
            const bool written = fwrite(data, 1, len, f) == len;
            if (fclose(f) == 0 && written) {
                unlink(path); /* rename() FatFs refuses an existing destination */
                if (rename(tmp, path) == 0) {
                    err = ESP_OK;
                }
            }
        }
        if (err != ESP_OK) {
            unlink(tmp);
        }
    }
    xSemaphoreGiveRecursive(s_card_mutex);
    return err;
}

esp_err_t sd_storage_append_line(const char *path, const char *header, const char *line)
{
    if (s_card_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (s_mounted) {
        struct stat st;
        const bool has_content = stat(path, &st) == 0 && st.st_size > 0;
        FILE *f = fopen(path, "a");
        err = ESP_FAIL;
        if (f != NULL) {
            const bool ok = (has_content || header == NULL || fputs(header, f) >= 0) && fputs(line, f) >= 0;
            if (fclose(f) == 0 && ok) {
                err = ESP_OK;
            }
        }
    }
    xSemaphoreGiveRecursive(s_card_mutex);
    return err;
}

int sd_storage_voice_files(void)
{
    return s_voice_files;
}

void sd_storage_refresh_voice_files(void)
{
    if (s_card_mutex == NULL) {
        return;
    }
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    s_voice_files = s_mounted ? count_voice_files_locked() : -1;
    xSemaphoreGiveRecursive(s_card_mutex);
}

int sd_storage_list_voice(const char *subdir, sd_voice_file_cb_t cb, void *ctx, uint32_t *free_mb)
{
    *free_mb = 0;
    if (s_card_mutex == NULL) {
        return -1;
    }
    char base[96];
    if (subdir != NULL && subdir[0] != '\0') {
        snprintf(base, sizeof(base), SD_VOICE_DIR "/%s", subdir);
    } else {
        snprintf(base, sizeof(base), SD_VOICE_DIR);
    }
    xSemaphoreTakeRecursive(s_card_mutex, portMAX_DELAY);
    int count = -1;
    if (s_mounted) {
        count = 0;
        DIR *dir = opendir(base);
        if (dir != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                char path[sizeof(base) + sizeof(entry->d_name) + 2]; /* Never truncate: -Werror=format-truncation */
                snprintf(path, sizeof(path), "%s/%s", base, entry->d_name);
                struct stat st;
                const bool ok = stat(path, &st) == 0;
                if (ok && S_ISDIR(st.st_mode)) {
                    continue; /* language subfolders: never accounts like bank files */
                }
                const long size = ok ? (long)st.st_size : -1;
                count++;
                if (cb != NULL) {
                    cb(entry->d_name, size, ctx);
                }
            }
            closedir(dir);
        }
        uint64_t total = 0;
        uint64_t free_bytes = 0;
        if (esp_vfs_fat_info(SD_MOUNT_POINT, &total, &free_bytes) == ESP_OK) {
            *free_mb = (uint32_t)(free_bytes / (1024 * 1024));
        }
    }
    xSemaphoreGiveRecursive(s_card_mutex);
    return count;
}

void sd_storage_record_readings(const glucose_point_t *cache, int count, int64_t previous_newest_ms)
{
    (void)previous_newest_ms; /* cover: each measure is written only once */
    if (s_task == NULL || cache == NULL || count <= 0) {
        return;
    }
    const int n = count < GLY_HISTORY_MAX ? count : GLY_HISTORY_MAX;
    xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY);
    memcpy(s_snapshot, cache, (size_t)n * sizeof(glucose_point_t));
    s_snapshot_count = n;
    s_snapshot_pending = true;
    xSemaphoreGive(s_snapshot_mutex);
    xTaskNotifyGive(s_task);
}

void sd_storage_log_event(const char *fmt, ...)
{
    if (s_events == NULL || !s_mounted) {
        return;
    }
    app_state_t st;
    app_state_get(&st);
    if (st.demo_mode) {
        return; /* demonstration mode: no writing on the map */
    }
    sd_event_t ev = {.uptime_us = esp_timer_get_time()};
    va_list args;
    va_start(args, fmt);
    vsnprintf(ev.text, sizeof(ev.text), fmt, args);
    va_end(args);
    xQueueSend(s_events, &ev, 0);
}

const char *sd_storage_request_format(const char *confirmation)
{
    if (confirmation == NULL || strcmp(confirmation, "EFFACER") != 0) {
        return "refuse : confirmation attendue (sd_format EFFACER)";
    }
    if (s_task == NULL || s_mounted || s_state != SD_STATE_UNSUPPORTED) {
        return "refuse : seule une carte au systeme de fichiers non reconnu peut etre formatee";
    }
    s_req_format = true;
    xTaskNotifyGive(s_task);
    return "formatage FAT32 demande";
}

void sd_storage_request_check(void)
{
    if (s_task != NULL) {
        s_req_check = true;
        xTaskNotifyGive(s_task);
    }
}

void sd_storage_request_repair(void)
{
    if (s_task != NULL) {
        s_req_repair = true;
        xTaskNotifyGive(s_task);
    }
}

void sd_storage_test_eject(int seconds)
{
    if (s_task != NULL) {
        s_req_eject_s = seconds > 0 ? seconds : 30;
        xTaskNotifyGive(s_task);
    }
}

void sd_storage_test_remount(int cycles)
{
    if (s_task != NULL) {
        s_req_remount_cycles = cycles > 0 ? cycles : 1;
        xTaskNotifyGive(s_task);
    }
}

void sd_storage_request_consumables_purge(int64_t from_s)
{
    if (s_task != NULL) {
        s_purge_from_s = from_s;
        s_req_purge = true;
        xTaskNotifyGive(s_task);
    }
}
