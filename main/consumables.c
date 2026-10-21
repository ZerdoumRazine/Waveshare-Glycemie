/*
 * consumables.c: Consumable replacement lifecycle and reminders.
 * FR: Cycle de remplacement et rappels des consommables.
 * Design note: A reboot does not indicate that a new item was installed.
 */

/*
 * Maintains infusion-set dates, configurable intervals and reminders in persistent storage.
 * Conserve les dates et rappels du cathéter en mémoire persistante.
 */
#include "consumables.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "i18n.h"
#include "nvs.h"
#include "net_services.h"
#include "sd_storage.h"
#include "sensor_session.h"
#include "stock.h"

static const char *TAG = "consommables";

#define NVS_NS_CATHETER   "catheter"
#define NVS_NS_CONSO      "conso"
#define KEY_START         "start"
#define KEY_REMINDERS     "remind2" /* bit 0 : rappel 2 h */
#define KEY_OLD_REMINDERS "remind"  /* ancien masque (24 h, 6 h, 2 h, echeance) : lu, jamais ecrit */
#define KEY_HOURS         "hours"
#define KEY_LOGGED_G7     "log_g7"
#define KEY_LOGGED_CATH   "log_cath"
#define CATHETER_DEFAULT_H 72
#define CSV_PATH          SD_MOUNT_POINT "/glycemie/logs/consumables.csv"
#define CSV_HEADER        "timestamp,type,event\n"

static SemaphoreHandle_t s_mutex;
static int64_t s_cath_start_s; /* 0 = aucun changement enregistre */
static uint8_t s_cath_done;
static uint8_t s_cath_hours = CATHETER_DEFAULT_H;
static volatile uint32_t s_revision;
/* Derniere heure de depart deja ecrite dans consumables.csv (par consommable) */
static int64_t s_logged[CONSUMABLE_COUNT];

static const char *const k_csv_type[CONSUMABLE_COUNT] = {"G7", "infusion_set"};
static const char *const k_logged_key[CONSUMABLE_COUNT] = {KEY_LOGGED_G7, KEY_LOGGED_CATH};

/* Build the deadline relative to the saved replacement time, not the current clock. */
static int64_t catheter_reminder_time(int reminder, int64_t start_s, int hours)
{
    const int64_t due = start_s + (int64_t)hours * 3600;
    switch (reminder) {
    case CATHETER_REMIND_2H:
        return due - 2 * 3600;
    default:
        return INT64_MAX;
    }
}

/* Verrou pris */
/* Persist a confirmed catheter change so an ordinary reboot preserves the date. */
static esp_err_t persist_catheter(void)
{
    s_revision++;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_CATHETER, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_i64(h, KEY_START, s_cath_start_s);
        if (err == ESP_OK) {
            err = nvs_set_u8(h, KEY_REMINDERS, s_cath_done);
        }
        if (err == ESP_OK) {
            err = nvs_set_u8(h, KEY_HOURS, s_cath_hours);
        }
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enregistrement NVS du catheter impossible (%s)", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t persist_logged(consumable_t c)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_CONSO, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_i64(h, k_logged_key[c], s_logged[c]);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Marqueur NVS de l'historique impossible (%s)", esp_err_to_name(err));
    }
    return err;
}

/* Rappels deja depasses a l'enregistrement : marques comme faits (pas de rafale de rappels perimes) */
/* Skip overdue historical reminders rather than replaying them after every restart. */
static void mark_past_catheter_reminders(int64_t now_s)
{
    s_cath_done = 0;
    for (int i = 0; i < CATHETER_REMIND_COUNT; i++) {
        if (now_s >= catheter_reminder_time(i, s_cath_start_s, s_cath_hours)) {
            s_cath_done |= (uint8_t)(1u << i);
        }
    }
}

esp_err_t consumables_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    nvs_handle_t h;
    if (nvs_open(NVS_NS_CATHETER, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_i64(h, KEY_START, &s_cath_start_s) != ESP_OK) {
            s_cath_start_s = 0;
        }
        if (nvs_get_u8(h, KEY_REMINDERS, &s_cath_done) != ESP_OK) {
            /* masque absent (versions precedentes) : rappel 2 h deja signale si l'ancien 2 h ou l'echeance l'a ete */
            uint8_t old = 0;
            if (nvs_get_u8(h, KEY_OLD_REMINDERS, &old) != ESP_OK) {
                old = 0;
            }
            s_cath_done = (old & 0x0C) ? 0x01 : 0;
        }
        if (nvs_get_u8(h, KEY_HOURS, &s_cath_hours) != ESP_OK || (s_cath_hours != 48 && s_cath_hours != 72)) {
            s_cath_hours = CATHETER_DEFAULT_H;
        }
        nvs_close(h);
    }

    /* Marqueurs de l'historique : absents au premier demarrage de cette version -> la date deja enregistree est
     * consideree comme journalisee (aucune ligne CSV sans confirmation reelle) */
    sensor_status_t sensor;
    sensor_session_get((int64_t)time(NULL), &sensor);
    const int64_t current[CONSUMABLE_COUNT] = {sensor.known ? sensor.start_s : 0, s_cath_start_s};
    bool found[CONSUMABLE_COUNT] = {false, false};
    if (nvs_open(NVS_NS_CONSO, NVS_READONLY, &h) == ESP_OK) {
        for (int c = 0; c < CONSUMABLE_COUNT; c++) {
            found[c] = nvs_get_i64(h, k_logged_key[c], &s_logged[c]) == ESP_OK;
        }
        nvs_close(h);
    }
    for (int c = 0; c < CONSUMABLE_COUNT; c++) {
        if (!found[c]) {
            s_logged[c] = current[c];
            persist_logged((consumable_t)c);
        }
    }
    ESP_LOGI(TAG, "Catheter : %s, intervalle %u h, rappel 2 h deja signale 0x%02x ; historique consumables.csv %s",
             s_cath_start_s > 0 ? "date enregistree" : "aucun changement enregistre", s_cath_hours, s_cath_done,
             (current[0] != s_logged[0] || current[1] != s_logged[1]) ? ": ligne(s) en attente" : "a jour");
    return ESP_OK;
}

const char *consumable_name(consumable_t c)
{
    return c == CONSUMABLE_SENSOR ? tr(S_NEW_SENSOR) : tr(S_NEW_CATHETER);
}

esp_err_t consumable_start_now(consumable_t c, const char *source)
{
    if (c >= CONSUMABLE_COUNT || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!net_time_is_valid()) {
        ESP_LOGW(TAG, "%s refuse (%s) : heure non synchronisee", consumable_name(c), source);
        return ESP_ERR_INVALID_STATE;
    }
    const int64_t now = (int64_t)time(NULL);
    esp_err_t err;
    if (c == CONSUMABLE_SENSOR) {
        err = sensor_session_start(now, now);
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_revision++;
        xSemaphoreGive(s_mutex);
    } else {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_cath_start_s = now;
        mark_past_catheter_reminders(now);
        err = persist_catheter();
        xSemaphoreGive(s_mutex);
    }
    struct tm tm_local;
    const time_t t = (time_t)now;
    localtime_r(&t, &tm_local);
    ESP_LOGI(TAG, "%s enregistre (%s) : depart %02d/%02d a %02d:%02d", consumable_name(c), source, tm_local.tm_mday,
             tm_local.tm_mon + 1, tm_local.tm_hour, tm_local.tm_min);
    sd_storage_log_event("%s enregistre (%s)", c == CONSUMABLE_SENSOR ? "Nouveau capteur" : "Nouveau catheter", source);
    /* Stock : une seule unite par changement reellement enregistre (l'heure de depart sert de marqueur) */
    stock_consume(c == CONSUMABLE_SENSOR ? STOCK_SENSOR : STOCK_CATHETER, now);
    return err;
}

void catheter_get(int64_t now_s, catheter_status_t *out)
{
    memset(out, 0, sizeof(*out));
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int64_t start = s_cath_start_s;
    const int hours = s_cath_hours;
    xSemaphoreGive(s_mutex);
    out->interval_h = hours;
    out->days_total = hours / 24;
    if (start <= 0) {
        return;
    }
    const int64_t span = (int64_t)hours * 3600;
    const int64_t elapsed = now_s - start;
    out->known = true;
    out->start_s = start;
    out->due_s = start + span;
    out->day = elapsed < 0 ? 1 : (int)(elapsed / (24 * 3600)) + 1;
    if (out->day > out->days_total) {
        out->day = out->days_total;
    }
    out->overdue = now_s >= out->due_s;
    out->remaining_s = out->due_s - now_s;
    const int64_t pct = elapsed * 100 / span;
    out->progress_pct = (int)(pct < 0 ? 0 : (pct > 100 ? 100 : pct));
}

int catheter_interval_h(void)
{
    return s_cath_hours;
}

void catheter_set_interval_h(int hours)
{
    if ((hours != 48 && hours != 72) || s_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (hours != s_cath_hours) {
        s_cath_hours = (uint8_t)hours;
        if (s_cath_start_s > 0 && net_time_is_valid()) {
            mark_past_catheter_reminders((int64_t)time(NULL));
        }
        persist_catheter();
        ESP_LOGI(TAG, "Intervalle du catheter : %d h", hours);
    }
    xSemaphoreGive(s_mutex);
}

uint32_t consumables_revision(void)
{
    return s_revision + sensor_session_revision();
}

int catheter_current_reminder(int64_t now_s, bool *done)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int current = -1;
    if (s_cath_start_s > 0 && now_s < s_cath_start_s + (int64_t)s_cath_hours * 3600) {
        for (int i = 0; i < CATHETER_REMIND_COUNT; i++) {
            if (now_s >= catheter_reminder_time(i, s_cath_start_s, s_cath_hours)) {
                current = i;
            }
        }
    }
    *done = current >= 0 && (s_cath_done & (1u << current)) != 0;
    xSemaphoreGive(s_mutex);
    return current;
}

void catheter_mark_reminder_done(int reminder)
{
    if (reminder < 0 || reminder >= CATHETER_REMIND_COUNT) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    /* les rappels plus anciens sont aussi consideres comme signales ; NVS ecrite seulement si le masque change */
    uint8_t mask = s_cath_done;
    for (int i = 0; i <= reminder; i++) {
        mask |= (uint8_t)(1u << i);
    }
    if (mask != s_cath_done) {
        s_cath_done = mask;
        persist_catheter();
        ESP_LOGI(TAG, "Rappel 2 h du catheter signale : memorise en NVS (rappels 0x%02x)", mask);
    }
    xSemaphoreGive(s_mutex);
}

void catheter_reset_reminders(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_cath_done = 0;
    persist_catheter();
    xSemaphoreGive(s_mutex);
}

const char *catheter_reminder_text(int reminder)
{
    switch (reminder) {
    case CATHETER_REMIND_2H:
        return tr(S_REMIND_CATHETER_2H);
    default:
        return "";
    }
}

void consumables_sd_flush(void)
{
    if (s_mutex == NULL) {
        return;
    }
    sensor_status_t sensor;
    sensor_session_get((int64_t)time(NULL), &sensor);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int64_t current[CONSUMABLE_COUNT] = {sensor.known ? sensor.start_s : 0, s_cath_start_s};
    int64_t pending[CONSUMABLE_COUNT] = {0, 0};
    for (int c = 0; c < CONSUMABLE_COUNT; c++) {
        pending[c] = current[c] > 0 && current[c] != s_logged[c] ? current[c] : 0;
    }
    xSemaphoreGive(s_mutex);

    for (int c = 0; c < CONSUMABLE_COUNT; c++) {
        if (pending[c] == 0) {
            continue;
        }
        const time_t t = (time_t)pending[c];
        struct tm tm_local;
        localtime_r(&t, &tm_local);
        char line[64];
        snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d,%s,new\n", tm_local.tm_year + 1900, tm_local.tm_mon + 1,
                 tm_local.tm_mday, tm_local.tm_hour, tm_local.tm_min, k_csv_type[c]);
        if (sd_storage_append_line(CSV_PATH, CSV_HEADER, line) != ESP_OK) {
            return; /* nouvel essai au prochain passage (carte indisponible) */
        }
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_logged[c] = pending[c];
        persist_logged((consumable_t)c);
        xSemaphoreGive(s_mutex);
        ESP_LOGI(TAG, "consumables.csv : %.*s", (int)strlen(line) - 1, line);
    }
}

void consumables_print(void)
{
    const int64_t now = (int64_t)time(NULL);
    sensor_status_t s;
    sensor_session_get(now, &s);
    catheter_status_t cath;
    catheter_get(now, &cath);
    bool sensor_done = false;
    const int sensor_current = sensor_session_current_reminder(now, &sensor_done);
    bool cath_current_done = false;
    const int cath_current = catheter_current_reminder(now, &cath_current_done);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const uint8_t cath_done = s_cath_done;
    const int64_t logged_g7 = s_logged[CONSUMABLE_SENSOR];
    const int64_t logged_cath = s_logged[CONSUMABLE_CATHETER];
    xSemaphoreGive(s_mutex);
    printf("CONSO capteur connu=%d depart=%" PRId64 " echeance=%" PRId64 " jour=%d a_changer=%d restant_s=%" PRId64
           " rappels=0x%02x rappel_en_cours=%d deja_signale=%d csv=%" PRId64 "\n", s.known, s.known ? s.start_s : 0,
           s.known ? s.due_s : 0, s.day, s.expired, s.remaining_s, sensor_session_reminder_mask(), sensor_current,
           sensor_done, logged_g7);
    printf("CONSO catheter connu=%d depart=%" PRId64 " intervalle_h=%d echeance=%" PRId64 " jour=%d/%d a_changer=%d"
           " restant_s=%" PRId64 " rappels=0x%02x rappel_en_cours=%d deja_signale=%d csv=%" PRId64 "\n", cath.known,
           cath.start_s, cath.interval_h, cath.due_s, cath.day, cath.days_total, cath.overdue, cath.remaining_s,
           cath_done, cath_current, cath_current_done, logged_cath);
}

esp_err_t consumable_restore(consumable_t c, int64_t start_s, int reminders_mask)
{
    if (c >= CONSUMABLE_COUNT || start_s < 0 || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err;
    if (c == CONSUMABLE_SENSOR) {
        err = sensor_session_restore(start_s, (uint8_t)reminders_mask);
    } else {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_cath_start_s = start_s;
        s_cath_done = start_s > 0 ? (uint8_t)reminders_mask : 0;
        err = persist_catheter();
        xSemaphoreGive(s_mutex);
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_logged[c] = start_s; /* restauration : jamais de ligne dans l'historique */
    persist_logged(c);
    s_revision++;
    xSemaphoreGive(s_mutex);
    ESP_LOGW(TAG, "%s : date restauree par la console (depart %" PRId64 ", rappels 0x%02x), aucune ligne CSV",
             c == CONSUMABLE_SENSOR ? "Capteur" : "Catheter", start_s, reminders_mask & 0xFF);
    return err;
}
