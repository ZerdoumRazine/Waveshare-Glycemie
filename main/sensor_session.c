/*
 * sensor_session.c: Sensor insertion dates and expected replacement time.
 * Design note: Keep insertion dates persistent across normal reboots.
 */

/*
 * Keeps sensor insertion time, expected expiration and one-time reminder flags.
 * Follows the date of installation of the sensor and the reminders already issued.
 */
#include "sensor_session.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i18n.h"
#include "nvs.h"

static const char *TAG = "capteur";

#define NVS_NS            "sensor"
#define KEY_START         "start"
#define KEY_REMINDERS     "remind2" /* bit 0 : rappel 24 h, bit 1 : rappel 2 h */
#define KEY_OLD_REMINDERS "remind"  /* former mask (48 h, 24 h, 12 h, grace, end of grace - 2 h): read, never written */

static SemaphoreHandle_t s_mutex;
static int64_t s_start_s;       /* 0 = no sensor */
static uint8_t s_done_mask;
static volatile uint32_t s_revision;

/* Compute an alert from the recorded insertion time, not from a reboot time. */
static int64_t reminder_time(int reminder, int64_t start_s)
{
    const int64_t due = start_s + SENSOR_SESSION_S;
    switch (reminder) {
    case SENSOR_REMIND_24H:
        return due - 24 * 3600;
    case SENSOR_REMIND_2H:
        return due - 2 * 3600;
    default:
        return INT64_MAX;
    }
}

/* Write session changes to NVS only after a user-confirmed update. */
static esp_err_t persist(void)
{
    s_revision++;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_i64(h, KEY_START, s_start_s);
        if (err == ESP_OK) {
            err = nvs_set_u8(h, KEY_REMINDERS, s_done_mask);
        }
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enregistrement NVS impossible (%s)", esp_err_to_name(err));
    }
    return err;
}

/* Reminders already passed the registration: marks as facts (no burst of reminders perimes) */
/* Avoid generating a burst of expired reminders after a clock adjustment. */
static void mark_past_reminders(int64_t now_s)
{
    s_done_mask = 0;
    for (int i = 0; i < SENSOR_REMIND_COUNT; i++) {
        if (now_s >= reminder_time(i, s_start_s)) {
            s_done_mask |= (uint8_t)(1u << i);
        }
    }
}

esp_err_t sensor_session_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_i64(h, KEY_START, &s_start_s) != ESP_OK) {
            s_start_s = 0;
        }
        if (nvs_get_u8(h, KEY_REMINDERS, &s_done_mask) != ESP_OK) {
            /* Older records lack reminder flags. Infer whether 24-hour and two-hour
 * alerts were already emitted from the stored legacy reminder timestamps.
 * Do not modify the original insertion date while migrating state. */
            uint8_t old = 0;
            if (nvs_get_u8(h, KEY_OLD_REMINDERS, &old) != ESP_OK) {
                old = 0;
            }
            s_done_mask = (uint8_t)(((old & 0x1E) ? 0x01 : 0) | ((old & 0x18) ? 0x02 : 0));
        }
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Suivi du capteur : %s (echeance pose + 10 jours, rappels 24 h et 2 h, deja signales 0x%02x)",
             s_start_s > 0 ? "heure de pose enregistree" : "aucun capteur enregistre", s_done_mask);
    return ESP_OK;
}

void sensor_session_get(int64_t now_s, sensor_status_t *out)
{
    memset(out, 0, sizeof(*out));
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int64_t start = s_start_s;
    xSemaphoreGive(s_mutex);
    if (start <= 0) {
        return;
    }
    out->known = true;
    out->start_s = start;
    out->due_s = start + SENSOR_SESSION_S; /* 10 days exactly: grace does not extend the meter */
    const int64_t elapsed = now_s - start;
    out->day = elapsed < 0 ? 1 : (int)(elapsed / (24 * 3600)) + 1;
    if (out->day > 10) {
        out->day = 10;
    }
    out->expired = now_s >= out->due_s;
    out->remaining_s = out->due_s - now_s;
    int64_t pct = elapsed * 100 / SENSOR_SESSION_S;
    out->progress_pct = (int)(pct < 0 ? 0 : (pct > 100 ? 100 : pct));
}

esp_err_t sensor_session_start(int64_t start_s, int64_t now_s)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_start_s = start_s;
    mark_past_reminders(now_s);
    esp_err_t err = persist();
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Nouveau capteur enregistre");
    return err;
}

esp_err_t sensor_session_adjust(int64_t delta_s, int64_t now_s)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_start_s <= 0) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    int64_t start = s_start_s + delta_s;
    /* never in the future, never older than the end of grace */
    if (start > now_s) {
        start = now_s;
    }
    if (start < now_s - (SENSOR_SESSION_S + SENSOR_GRACE_S)) {
        start = now_s - (SENSOR_SESSION_S + SENSOR_GRACE_S);
    }
    s_start_s = start;
    mark_past_reminders(now_s);
    esp_err_t err = persist();
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t sensor_session_clear(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_start_s = 0;
    s_done_mask = 0;
    esp_err_t err = persist();
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Suivi du capteur efface");
    return err;
}

int sensor_session_current_reminder(int64_t now_s, bool *done)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int current = -1;
    if (s_start_s > 0 && now_s < s_start_s + SENSOR_SESSION_S) {
        for (int i = 0; i < SENSOR_REMIND_COUNT; i++) {
            if (now_s >= reminder_time(i, s_start_s)) {
                current = i;
            }
        }
    }
    *done = current >= 0 && (s_done_mask & (1u << current)) != 0;
    xSemaphoreGive(s_mutex);
    return current;
}

void sensor_session_mark_reminder_done(int reminder)
{
    if (reminder < 0 || reminder >= SENSOR_REMIND_COUNT) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    /* older memories are also considered as reports; NVS writes only if the mask changes */
    uint8_t mask = s_done_mask;
    for (int i = 0; i <= reminder; i++) {
        mask |= (uint8_t)(1u << i);
    }
    if (mask != s_done_mask) {
        s_done_mask = mask;
        persist();
        ESP_LOGI(TAG, "Rappel %s du capteur signale : memorise en NVS (rappels 0x%02x)",
                 reminder == SENSOR_REMIND_24H ? "24 h" : "2 h", mask);
    }
    xSemaphoreGive(s_mutex);
}

void sensor_session_reset_reminders(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_done_mask = 0;
    persist();
    xSemaphoreGive(s_mutex);
}

uint32_t sensor_session_revision(void)
{
    return s_revision;
}

uint8_t sensor_session_reminder_mask(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const uint8_t mask = s_done_mask;
    xSemaphoreGive(s_mutex);
    return mask;
}

esp_err_t sensor_session_restore(int64_t start_s, uint8_t reminders_mask)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_start_s = start_s > 0 ? start_s : 0;
    s_done_mask = start_s > 0 ? reminders_mask : 0;
    esp_err_t err = persist();
    xSemaphoreGive(s_mutex);
    return err;
}

const char *sensor_reminder_text(int reminder)
{
    switch (reminder) {
    case SENSOR_REMIND_24H:
        return tr(S_REMIND_SENSOR_24H);
    case SENSOR_REMIND_2H:
        return tr(S_REMIND_SENSOR_2H);
    default:
        return "";
    }
}
