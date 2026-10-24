/*
 * alerts.c: Supplemental glucose alerts and acknowledgement timing.
 * Design note: This display does not replace official Dexcom alarms.
 */

/*
 * Schedules alert priorities and repetitions; acknowledgements are stored separately from readings.
 * Plans the priorities, repetitions and acquittals of alerts.
 */
#include "alerts.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "app_state.h"
#include "audio_out.h"
#include "consumables.h"
#include "glucose_math.h"
#include "net_services.h"
#include "sd_storage.h"
#include "sensor_session.h"
#include "settings.h"
#include "i18n.h"
#include "voice.h"

static const char *TAG = "alertes";

typedef struct {
    audio_pattern_t pattern;
    int repeat_s;      /* interval between two bells as long as not acquitted */
    int max_repeats;   /* 0 = jusqu'a l'acquittement */
    int reraise_min;   /* new alert if condition persists after acquittal */
    bool critical;     /* volume croissant (RAMP) */
    str_id_t title_id; /* Text ID: go through tr() ou tr_fr(), never as */
} alert_cfg_t;

static const alert_cfg_t k_cfg[ALERT_COUNT] = {
    [ALERT_VERY_LOW] = {AUDIO_PATTERN_VERY_LOW, 20, 0, 15, true, S_ALERT_VERY_LOW},
    [ALERT_LOW] = {AUDIO_PATTERN_LOW, 60, 10, 30, false, S_ALERT_LOW},
    [ALERT_NO_DATA] = {AUDIO_PATTERN_NO_DATA, 120, 3, 60, false, S_ALERT_NO_DATA},
    [ALERT_VERY_HIGH] = {AUDIO_PATTERN_VERY_HIGH, 120, 5, 60, false, S_ALERT_VERY_HIGH},
    [ALERT_HIGH] = {AUDIO_PATTERN_HIGH, 300, 2, 120, false, S_ALERT_HIGH},
    [ALERT_SENSOR] = {AUDIO_PATTERN_SENSOR, 0, 1, 0, false, S_ALERT_SENSOR},
    [ALERT_CATHETER] = {AUDIO_PATTERN_SENSOR, 0, 1, 0, false, S_ALERT_CATHETER},
    [ALERT_TEST] = {AUDIO_PATTERN_TEST, 0, 1, 0, false, S_ALERT_TEST},
};

typedef struct {
    bool active;
    bool acked;
    int64_t snooze_until_us;
    int64_t reraise_us;
    int64_t last_sound_us;
    int repeats;
    uint32_t seq;      /* number of last levee */
} alert_slot_t;

static alert_slot_t s_slots[ALERT_COUNT];
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static alert_view_t s_view = {.type = ALERT_NONE, .style = ALERT_NONE, .ongoing = ALERT_NONE};
/* Sensor reminder [0] / catheter [1] during presentation (-1: none): guard until'a OK, jusqu'at the next reminder
 * or up'a l'even if he is already remembered in NVS as indicated */
static int s_reminder[2] = {-1, -1};
static bool s_test_active;
static alert_type_t s_test_as = ALERT_TEST; /* type simule par l'alerte de test */
static int s_test_value;
static uint32_t s_raise_count;

static int64_t now_epoch_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Translate the current alert into the title, value and buttons seen on screen. */
static void build_view(alert_type_t type, int value, alert_view_t *v)
{
    v->type = type;
    v->value = value;
    v->style = type;
    if (type == ALERT_NONE) {
        v->title[0] = '\0';
        v->message[0] = '\0';
        return;
    }
    v->seq = s_slots[type].seq;
    if (type == ALERT_TEST && s_test_as != ALERT_TEST) {
        /* simulation of a real type: same title, message and color; "(test)" in the message (title on a line) */
        build_view(s_test_as, value, v);
        v->type = ALERT_TEST;
        v->seq = s_slots[ALERT_TEST].seq;
        strlcat(v->message, tr(S_ALERT_TEST_SUFFIX), sizeof(v->message));
        return;
    }
    strlcpy(v->title, tr(k_cfg[type].title_id), sizeof(v->title));
    switch (type) {
    case ALERT_VERY_LOW:
    case ALERT_LOW:
    case ALERT_VERY_HIGH:
    case ALERT_HIGH:
        snprintf(v->message, sizeof(v->message), "%d mg/dL", value);
        break;
    case ALERT_NO_DATA:
        snprintf(v->message, sizeof(v->message), tr(S_ALERT_NO_DATA_MSG), value);
        break;
    case ALERT_SENSOR:
        strlcpy(v->message, sensor_reminder_text(value), sizeof(v->message));
        break;
    case ALERT_CATHETER:
        strlcpy(v->message, catheter_reminder_text(value), sizeof(v->message));
        break;
    case ALERT_TEST:
        strlcpy(v->message, tr(S_ALERT_TEST_NOTE), sizeof(v->message));
        break;
    default:
        v->message[0] = '\0';
        break;
    }
}

/* type: sound type (for simule test alert, simule type); test: trace marked as test */
/* Choose the matching audio pattern; a simulated alert is not a live glucose alarm. */
static void sound(alert_type_t type, int value, const settings_t *cfg, int repeats, bool test)
{
    const bool may_sound = cfg->alerts || (type == ALERT_VERY_LOW && cfg->very_low_always);
    if (!may_sound) {
        return;
    }
    int volume = cfg->volume_pct;
    if (volume == 0 && type == ALERT_VERY_LOW && cfg->very_low_always) {
        volume = settings_level_volume(VOLUME_LOW);
    }
    if (k_cfg[type].critical && cfg->ramp) {
        const int base = volume < 30 ? 30 : volume;
        volume = base + 10 * repeats;
        volume = volume > 100 ? 100 : volume;
    }
    if (volume <= 0) {
        return;
    }
    audio_out_play_pattern(k_cfg[type].pattern, volume);
    if (cfg->voice) {
        voice_announce_alert(type, value, volume);
    }
    ESP_LOGW(TAG, "Sonnerie %s%s (valeur %d, volume %d %%, repetition %d)", tr_fr(k_cfg[type].title_id), test ? " (TEST)" : "",
             value, volume, repeats + 1);
}

/* Evaluate current readings on a schedule while respecting snooze and repetition limits. */
static void alerts_task(void *arg)
{
    (void)arg;
    static glucose_point_t pts[2];
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        settings_t cfg;
        settings_get(&cfg);
        const int64_t now_us = esp_timer_get_time();
        const bool time_ok = net_time_is_valid();
        const int64_t now_ms = now_epoch_ms();
        uint32_t seq = 0;
        const int count = app_history_get(pts, 2, &seq);
        const int64_t age_s = gm_age_s(pts, count, now_ms, time_ok);
        const bool fresh = age_s >= 0 && age_s < (int64_t)GLY_AGE_STALE_MIN * 60;
        const int v = count > 0 ? pts[0].mgdl : 0;

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        bool cond[ALERT_COUNT] = {0};
        int value[ALERT_COUNT] = {0};

        /* Hysteresis: the end condition is stricter than the entry condition */
        cond[ALERT_VERY_LOW] = fresh && v < (int)cfg.thr_very_low + (s_slots[ALERT_VERY_LOW].active ? 3 : 0);
        cond[ALERT_LOW] = fresh && !cond[ALERT_VERY_LOW] && v < (int)cfg.thr_low + (s_slots[ALERT_LOW].active ? 5 : 0);
        cond[ALERT_VERY_HIGH] = fresh && v > (int)cfg.thr_very_high - (s_slots[ALERT_VERY_HIGH].active ? 10 : 0);
        cond[ALERT_HIGH] = fresh && !cond[ALERT_VERY_HIGH] &&
                           v > (int)cfg.thr_high - (s_slots[ALERT_HIGH].active ? 10 : 0);
        value[ALERT_VERY_LOW] = value[ALERT_LOW] = value[ALERT_VERY_HIGH] = value[ALERT_HIGH] = v;

        cond[ALERT_NO_DATA] = count > 0 && age_s >= (s_slots[ALERT_NO_DATA].active ? 10 : 15) * 60LL;
        value[ALERT_NO_DATA] = (int)(age_s / 60);

        /* Sensor / catheter recalls: each recall is presented only once (memorise in NVS ringing,
         * never replay after a restart); a new reminder replaces the previous with a new levee */
        for (int i = 0; i < 2; i++) {
            const alert_type_t t = i == 0 ? ALERT_SENSOR : ALERT_CATHETER;
            bool done = false;
            const int reminder = !time_ok ? -1
                                 : (i == 0 ? sensor_session_current_reminder(now_ms / 1000, &done)
                                           : catheter_current_reminder(now_ms / 1000, &done));
            const int shown = reminder < 0 ? -1 : (reminder == s_reminder[i] || !done ? reminder : -1);
            if (shown >= 0 && shown != s_reminder[i]) {
                memset(&s_slots[t], 0, sizeof(s_slots[t]));
            }
            s_reminder[i] = shown;
            cond[t] = shown >= 0;
            value[t] = shown;
        }

        cond[ALERT_TEST] = s_test_active;
        value[ALERT_TEST] = s_test_value;

        /* Persistent indicator: glycemic condition or absence of current data (order = priority) */
        alert_type_t ongoing = ALERT_NONE;
        for (int t = ALERT_VERY_LOW; t <= ALERT_HIGH && ongoing == ALERT_NONE; t++) {
            if (cond[t]) {
                ongoing = (alert_type_t)t;
            }
        }
        const bool ongoing_test = ongoing == ALERT_NONE && s_test_active && s_test_as <= ALERT_HIGH;
        if (ongoing_test) {
            ongoing = s_test_as; /* simulation: test condition persists until "alert_test off" */
        }

        alert_type_t current = ALERT_NONE;
        bool test_ready = false;
        for (int t = 0; t < ALERT_COUNT; t++) {
            alert_slot_t *slot = &s_slots[t];
            const bool log_sd = t != ALERT_TEST; /* the test alert does not appear in the microSD journal */
            if (!cond[t]) {
                if (slot->active || slot->acked) {
                    ESP_LOGI(TAG, "Fin d'alerte : %s", tr_fr(k_cfg[t].title_id));
                    if (log_sd) {
                        sd_storage_log_event("Fin d'alerte : %s", tr_fr(k_cfg[t].title_id));
                    }
                }
                memset(slot, 0, sizeof(*slot));
                continue;
            }
            if (slot->acked) {
                if (k_cfg[t].reraise_min > 0 && now_us >= slot->reraise_us) {
                    slot->acked = false;
                    slot->active = false;
                } else {
                    continue;
                }
            }
            if (now_us < slot->snooze_until_us) {
                continue;
            }
            if (!slot->active) {
                slot->active = true;
                slot->repeats = 0;
                slot->last_sound_us = 0;
                slot->seq = ++s_raise_count;
                if (t == ALERT_TEST && s_test_as != ALERT_TEST) {
                    ESP_LOGW(TAG, "ALERTE : %s simulant %s (valeur %d)", tr_fr(k_cfg[t].title_id), tr_fr(k_cfg[s_test_as].title_id),
                             value[t]);
                } else {
                    ESP_LOGW(TAG, "ALERTE : %s (valeur %d)", tr_fr(k_cfg[t].title_id), value[t]);
                }
                if (log_sd) {
                    sd_storage_log_event("Alerte : %s (valeur %d)", tr_fr(k_cfg[t].title_id), value[t]);
                }
            }
            if (t == ALERT_TEST) {
                test_ready = true;
            } else if (current == ALERT_NONE ||
                       (t == ALERT_CATHETER && current == ALERT_SENSOR && slot->seq > s_slots[ALERT_SENSOR].seq)) {
                /* reminder sensor already present but not acquit: it does not block a more recent catheter reminder */
                current = (alert_type_t)t;
            }
        }
        /* The test alert passes before non-critical alerts, never ahead BASSE ni TRES BASSE */
        if (test_ready && current != ALERT_VERY_LOW && current != ALERT_LOW) {
            current = ALERT_TEST;
        }

        alert_type_t presented = ALERT_NONE; /* sensor / catheter reminder that just ringed */
        if (current != ALERT_NONE) {
            alert_slot_t *slot = &s_slots[current];
            const alert_cfg_t *c = &k_cfg[current];
            const bool due = slot->last_sound_us == 0 || (c->repeat_s > 0 && now_us - slot->last_sound_us >=
                                                          (int64_t)c->repeat_s * 1000000);
            const bool allowed = c->max_repeats == 0 || slot->repeats < c->max_repeats;
            if (due && allowed && !audio_out_busy()) {
                const bool test = current == ALERT_TEST;
                sound(test ? s_test_as : current, value[current], &cfg, slot->repeats, test);
                slot->last_sound_us = now_us;
                slot->repeats++;
                if (current == ALERT_SENSOR || current == ALERT_CATHETER) {
                    presented = current;
                }
            }
        }
        const int presented_value = presented == ALERT_NONE ? -1 : value[presented];
        build_view(current, current == ALERT_NONE ? 0 : value[current], &s_view);
        s_view.ongoing = ongoing;
        s_view.ongoing_test = ongoing_test;
        xSemaphoreGive(s_mutex);

        /* Off lock: reminder present memorise in NVS (a reboot doesn't play it again) */
        if (presented == ALERT_SENSOR) {
            sensor_session_mark_reminder_done(presented_value);
        } else if (presented == ALERT_CATHETER) {
            catheter_mark_reminder_done(presented_value);
        }
    }
}

esp_err_t alerts_start(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    /* 6 Ko stack: writing NVS the reminder in addition to the bell */
    return xTaskCreate(alerts_task, "alertes", 6144, NULL, 4, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void alerts_get_view(alert_view_t *out)
{
    if (s_mutex == NULL) {
        memset(out, 0, sizeof(*out));
        out->type = ALERT_NONE;
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_view;
    xSemaphoreGive(s_mutex);
}

void alerts_acknowledge(void)
{
    if (s_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const alert_type_t t = s_view.type;
    if (t != ALERT_NONE) {
        alert_slot_t *slot = &s_slots[t];
        slot->active = false;
        slot->acked = true;
        slot->reraise_us = esp_timer_get_time() + (int64_t)k_cfg[t].reraise_min * 60 * 1000000;
        if (t == ALERT_SENSOR) {
            sensor_session_mark_reminder_done(s_view.value);
            s_reminder[0] = -1;
        } else if (t == ALERT_CATHETER) {
            catheter_mark_reminder_done(s_view.value);
            s_reminder[1] = -1;
        }
        ESP_LOGI(TAG, "Alerte acquittee : %s", tr_fr(k_cfg[t].title_id));
        if (t != ALERT_TEST) {
            sd_storage_log_event("Alerte acquittee : %s", tr_fr(k_cfg[t].title_id));
        }
        build_view(ALERT_NONE, 0, &s_view);
    }
    xSemaphoreGive(s_mutex);
    audio_out_stop();
}

void alerts_snooze(int minutes)
{
    if (s_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const alert_type_t t = s_view.type;
    if (t != ALERT_NONE) {
        alert_slot_t *slot = &s_slots[t];
        slot->active = false;
        slot->snooze_until_us = esp_timer_get_time() + (int64_t)minutes * 60 * 1000000;
        if (t == ALERT_SENSOR || t == ALERT_CATHETER) {
            /* a sensor recall or catheter report will return once */
            slot->acked = false;
        }
        char resume[16] = "--:--";
        if (net_time_is_valid()) {
            const time_t at = (time_t)(now_epoch_ms() / 1000) + (time_t)minutes * 60;
            struct tm tm_local;
            localtime_r(&at, &tm_local);
            snprintf(resume, sizeof(resume), "%02d:%02d", tm_local.tm_hour, tm_local.tm_min);
        }
        ESP_LOGI(TAG, "Alerte reportee de %d min : %s (reprise vers %s)", minutes, tr_fr(k_cfg[t].title_id), resume);
        if (t != ALERT_TEST) {
            sd_storage_log_event("Alerte reportee %d min : %s", minutes, tr_fr(k_cfg[t].title_id));
        }
        build_view(ALERT_NONE, 0, &s_view);
    }
    xSemaphoreGive(s_mutex);
    audio_out_stop();
}

const char *alerts_title(alert_type_t type)
{
    return type >= 0 && type < ALERT_COUNT ? tr(k_cfg[type].title_id) : "";
}

void alerts_test_sound(alert_type_t type, int value)
{
    if (type < 0 || type >= ALERT_COUNT) {
        return;
    }
    settings_t cfg;
    settings_get(&cfg);
    cfg.alerts = true;
    sound(type, value, &cfg, 0, true);
}

void alerts_test_start(alert_type_t as, int value)
{
    if (s_mutex == NULL || as < ALERT_VERY_LOW || as > ALERT_TEST ||
        (as == ALERT_SENSOR && (value < 0 || value >= SENSOR_REMIND_COUNT)) ||
        (as == ALERT_CATHETER && (value < 0 || value >= CATHETER_REMIND_COUNT))) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(&s_slots[ALERT_TEST], 0, sizeof(s_slots[ALERT_TEST]));
    s_test_as = as;
    s_test_value = as == ALERT_TEST ? 0 : value;
    s_test_active = true;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Alerte de test demandee : %s, valeur %d (vraies alertes et donnees Dexcom inchangees)",
             tr_fr(k_cfg[as].title_id), as == ALERT_TEST ? 0 : value);
}

void alerts_test_stop(void)
{
    if (s_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_test_active = false;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Alerte de test arretee");
}

void alerts_print_status(void)
{
    if (s_mutex == NULL) {
        return;
    }
    alert_slot_t slots[ALERT_COUNT];
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(slots, s_slots, sizeof(slots));
    const alert_type_t shown = s_view.type;
    const alert_type_t ongoing = s_view.ongoing;
    const bool test = s_test_active;
    const alert_type_t test_as = s_test_as;
    xSemaphoreGive(s_mutex);

    const int64_t now_us = esp_timer_get_time();
    printf("ALERT_INFO affichee=%s indicateur=%s test=%s (%s) pile_libre_min=%u\n",
           shown == ALERT_NONE ? "aucune" : tr_fr(k_cfg[shown].title_id),
           ongoing == ALERT_NONE ? "aucun" : tr_fr(k_cfg[ongoing].title_id),
           test ? "en cours" : "arrete", tr_fr(k_cfg[test_as].title_id),
           s_task != NULL ? (unsigned)uxTaskGetStackHighWaterMark(s_task) : 0u);
    for (int t = 0; t < ALERT_COUNT; t++) {
        const alert_slot_t *slot = &slots[t];
        const int64_t snooze_s = slot->snooze_until_us > now_us ? (slot->snooze_until_us - now_us) / 1000000 : 0;
        if (slot->active || slot->acked || snooze_s > 0) {
            printf("ALERT_INFO %s active=%d acquittee=%d rappel_dans_s=%lld sonneries=%d\n", tr_fr(k_cfg[t].title_id),
                   slot->active, slot->acked, (long long)snooze_s, slot->repeats);
        }
    }
}
