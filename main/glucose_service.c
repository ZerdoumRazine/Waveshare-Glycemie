/*
 * glucose_service.c: Coordinates refreshing, ageing, and presenting glucose data.
 * FR: Coordonne l'actualisation, l'âge et la diffusion des mesures.
 * Design decision: Display the age of a reading instead of pretending it is current.
 *
 * Experimental supplementary display; official monitoring remains authoritative.
 */

/*
 * Polls Dexcom Share, deduplicates readings, and serves both live and demo modes.
 * Interroge Dexcom Share, évite les doublons et fournit le mode démonstration.
 * This is a supplementary display: official Dexcom alarms remain authoritative.
 */
#include "glucose_service.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "credentials.h"
#include "app_state.h"
#include "dexcom_guard.h"
#include "dexcom_share.h"
#include "glucose_math.h"
#include "net_services.h"
#include "wifi_manager.h"
#include "wifi_manager.h"

static const char *TAG = "glycemie";

/* Battement de coeur surveille par app_main (tache bloquee = redemarrage) */
static volatile int64_t s_heartbeat_us;
/* Autorisation de la tentative unique (console "dexcom_auth") */
static volatile bool s_arm_request;
/* Test de fiabilite : session invalide injectee a la prochaine lecture (console "dexcom_test_session") */
static volatile bool s_test_bad_session;
static int64_t s_last_request_us;

/* Mode demonstration (console uniquement, jamais persistant, jamais ecrit sur la microSD) */
typedef enum {
    DEMO_OFF = 0,
    DEMO_NORMAL,
    DEMO_LOW,
    DEMO_VERY_LOW,
    DEMO_HIGH,
    DEMO_VERY_HIGH,
    DEMO_GAP,
    DEMO_STALE,
    DEMO_SPARSE,
} demo_scenario_t;
static volatile demo_scenario_t s_demo;
static volatile bool s_demo_task_running;
static volatile bool s_demo_ended;

/* Nouvelles mesures reelles (ecriture microSD) */
static glucose_new_readings_cb_t s_new_readings_cb;

/* Cache de l'historique 24 h, de la plus recente a la plus ancienne */
static glucose_point_t *s_cache; /* PSRAM */
static int s_cache_count;

/* Delais croissants apres erreur reseau ou serveur (secondes), identifiants deja valides */
static const int32_t k_backoff_s[] = {30, 60, 120, 300};
#define BACKOFF_STEPS (sizeof(k_backoff_s) / sizeof(k_backoff_s[0]))

static void wait_seconds(int32_t seconds)
{
    for (int32_t i = 0; i < seconds; i++) {
        s_heartbeat_us = esp_timer_get_time();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    s_heartbeat_us = esp_timer_get_time();
}

/* Attente entre deux lectures, interrompue par une demande de test ou le mode demo */
static void wait_poll(int32_t seconds)
{
    for (int32_t i = 0; i < seconds && !s_test_bad_session && s_demo == DEMO_OFF && !s_demo_ended; i++) {
        s_heartbeat_us = esp_timer_get_time();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    s_heartbeat_us = esp_timer_get_time();
}

static void idle_forever(void)
{
    for (;;) {
        wait_seconds(60);
    }
}

static int64_t now_epoch_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Prochaine lecture : heure de la derniere mesure + 5 min + marge, jamais moins de 30 s */
static int32_t next_poll_delay_s(int64_t latest_wt_ms)
{
    if (!net_time_is_valid() || latest_wt_ms <= 0) {
        return 60;
    }
    const int64_t period_s = GLY_READING_PERIOD_S + GLY_READING_MARGIN_S;
    int64_t now_ms = now_epoch_ms();
    int64_t age_s = (now_ms - latest_wt_ms) / 1000;
    int64_t wait_s = (latest_wt_ms + period_s * 1000 - now_ms) / 1000;

    if (wait_s >= GLY_MIN_REQUEST_INTERVAL_S) {
        return (int32_t)(wait_s > period_s ? period_s : wait_s);
    }
    /* La mesure suivante est attendue mais pas encore publiee */
    if (age_s < 7 * 60) {
        return 30;
    }
    if (age_s < 15 * 60) {
        return 60;
    }
    return 90;
}

/* Attend le Wi-Fi, puis l'heure (au plus 60 s) */
static void wait_network_ready(void)
{
    int time_wait_s = 0;
    for (;;) {
        s_heartbeat_us = esp_timer_get_time();
        if (!wifi_manager_is_connected()) {
            time_wait_s = 0;
            wait_seconds(2);
            continue;
        }
        if (!net_time_is_valid() && time_wait_s < 60) {
            time_wait_s++;
            wait_seconds(1);
            continue;
        }
        return;
    }
}

/* Test DNS + TLS vers Dexcom sans identifiants, repete jusqu'au succes */
static void probe_until_ok(dexcom_client_t *client)
{
    for (;;) {
        wait_network_ready();
        int status = 0;
        if (dexcom_probe_tls(client, &status) == DEX_OK) {
            return;
        }
        wait_seconds(30);
    }
}

static void format_info(const dexcom_client_t *client, char *out, size_t size)
{
    const dex_error_info_t *e = &client->last_error;
    snprintf(out, size, "etape %s, HTTP %d, code %s%s%s", dex_step_name(e->step), e->http_status,
             e->code[0] ? e->code : "-", e->esp_error[0] ? ", " : "", e->esp_error);
}

static void log_failure_report(const char *title, const dexcom_client_t *client)
{
    const dex_error_info_t *e = &client->last_error;
    ESP_LOGE(TAG, "=== %s ===", title);
    ESP_LOGE(TAG, "Etape en echec : %s", dex_step_name(e->step));
    ESP_LOGE(TAG, "Code HTTP : %d%s", e->http_status, e->http_status == 0 ? " (aucune reponse HTTP)" : "");
    ESP_LOGE(TAG, "Code Dexcom : %s", e->code[0] ? e->code : "-");
    ESP_LOGE(TAG, "Message Dexcom (masque) : %s", e->message[0] ? e->message : "-");
    ESP_LOGE(TAG, "Erreur reseau/TLS : %s", e->esp_error[0] ? e->esp_error : "-");
    ESP_LOGE(TAG, "Requete envoyee au serveur : %s", e->request_sent ? "oui" : "non");
}

/* Arret definitif sur refus : aucune nouvelle tentative, meme apres redemarrage */
static void stop_on_refusal(dexcom_client_t *client, dex_result_t r)
{
    char info[128];
    format_info(client, info, sizeof(info));
    dexcom_invalidate_session(client);
    dex_guard_set(DEX_GUARD_REFUSED, info);
    app_state_set_dexcom(r == DEX_ERR_MAX_ATTEMPTS ? DEX_STATE_LOCKED : DEX_STATE_AUTH_REFUSED, 0);
    app_state_set_dexcom_info(info);
    log_failure_report(r == DEX_ERR_MAX_ATTEMPTS ? "COMPTE DEXCOM TEMPORAIREMENT BLOQUE" : "AUTHENTIFICATION DEXCOM REFUSEE",
                       client);
    ESP_LOGE(TAG, "Plus aucune tentative Dexcom (verrou NVS). Verifier l'identifiant et le mot de passe du compte qui partage.");
    idle_forever();
}

/* Tentative UNIQUE d'authentification pour des identifiants jamais valides. Vrai si reussie. */
static bool single_authentication_attempt(dexcom_client_t *client)
{
    probe_until_ok(client); /* aucun identifiant envoye */
    wait_network_ready();

    if (dex_guard_set(DEX_GUARD_IN_PROGRESS, "tentative unique en cours") != ESP_OK) {
        ESP_LOGE(TAG, "Verrou NVS impossible a enregistrer : tentative annulee (aucun identifiant envoye)");
        app_state_set_dexcom(DEX_STATE_AUTH_BLOCKED, 0);
        app_state_set_dexcom_info("verrou NVS indisponible");
        return false;
    }

    app_state_set_dexcom(DEX_STATE_CONNECTING, 0);
    ESP_LOGW(TAG, "=== TENTATIVE UNIQUE D'AUTHENTIFICATION DEXCOM ===");
    dex_result_t r = dexcom_login(client);
    s_heartbeat_us = esp_timer_get_time();

    if (r == DEX_OK) {
        dex_guard_set(DEX_GUARD_VALIDATED, "authentification reussie");
        app_state_set_dexcom_info("");
        ESP_LOGI(TAG, "=== AUTHENTIFICATION DEXCOM REUSSIE (etapes 1/2 et 2/2) ===");
        return true;
    }

    if (r == DEX_ERR_AUTH || r == DEX_ERR_MAX_ATTEMPTS) {
        stop_on_refusal(client, r); /* ne revient pas */
    }

    char info[128];
    format_info(client, info, sizeof(info));
    if (!client->last_error.request_sent) {
        /* La requete n'est jamais partie : ce n'etait pas une authentification */
        dex_guard_set(DEX_GUARD_UNKNOWN, "");
        app_state_set_dexcom(DEX_STATE_AWAITING_ARM, 0);
        app_state_set_dexcom_info(info);
        log_failure_report("CONNEXION DEXCOM IMPOSSIBLE AVANT L'ENVOI DES IDENTIFIANTS", client);
        ESP_LOGW(TAG, "Aucun identifiant transmis : nouvelle autorisation necessaire (console dexcom_auth)");
        return false;
    }

    dex_guard_set(DEX_GUARD_INCONCLUSIVE, info);
    app_state_set_dexcom(DEX_STATE_AUTH_BLOCKED, 0);
    app_state_set_dexcom_info(info);
    log_failure_report("TENTATIVE DEXCOM NON CONCLUANTE (identifiants envoyes, pas de verdict)", client);
    ESP_LOGE(TAG, "Aucune nouvelle tentative automatique (verrou NVS).");
    return false;
}

/* ------------------------------------------------------------------------- */
/* Cache 24 h */

static int cmp_points_desc(const void *a, const void *b)
{
    const glucose_point_t *pa = a;
    const glucose_point_t *pb = b;
    return (pa->wt_ms < pb->wt_ms) - (pa->wt_ms > pb->wt_ms);
}

/* Fusionne des mesures lues dans le cache ; renvoie le nombre de mesures plus recentes que l'ancien cache */
static int cache_merge(const glucose_point_t *fresh, int n, int64_t now_ms, bool time_ok)
{
    static glucose_point_t *merged;
    if (merged == NULL) {
        merged = heap_caps_malloc(2 * GLY_HISTORY_MAX * sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (merged == NULL) {
            return 0;
        }
    }
    const int64_t previous_newest = s_cache_count > 0 ? s_cache[0].wt_ms : 0;

    int total = 0;
    for (int i = 0; i < n && total < 2 * GLY_HISTORY_MAX; i++) {
        merged[total++] = fresh[i];
    }
    for (int i = 0; i < s_cache_count && total < 2 * GLY_HISTORY_MAX; i++) {
        merged[total++] = s_cache[i];
    }
    qsort(merged, (size_t)total, sizeof(glucose_point_t), cmp_points_desc);

    const int64_t oldest_allowed = time_ok ? now_ms - (int64_t)24 * 3600 * 1000 - 5 * 60 * 1000 : INT64_MIN;
    int out = 0;
    int newer = 0;
    for (int i = 0; i < total && out < GLY_HISTORY_MAX; i++) {
        if (merged[i].wt_ms < oldest_allowed) {
            break;
        }
        /* doublon : meme horodatage a moins d'une seconde pres (la version lue en premier est conservee) */
        if (out > 0 && llabs(s_cache[out - 1].wt_ms - merged[i].wt_ms) < 1000) {
            continue;
        }
        s_cache[out++] = merged[i];
        if (merged[i].wt_ms > previous_newest) {
            newer++;
        }
    }
    s_cache_count = out;
    return newer;
}

/* ------------------------------------------------------------------------- */
/* Mode demonstration */

static int demo_baseline(int64_t slot)
{
    const double t = (double)slot * 300.0;
    double v = 150.0 + 55.0 * sin(2.0 * M_PI * t / (5.5 * 3600.0)) + 18.0 * sin(2.0 * M_PI * t / (1.3 * 3600.0));
    uint32_t h = (uint32_t)(slot * 2654435761u);
    v += (double)((h >> 16) % 7) - 3.0;
    return (int)v;
}

static uint8_t demo_trend(int delta_5min)
{
    const double rate = delta_5min / 5.0;
    if (rate >= 3.0) {
        return TREND_DOUBLE_UP;
    }
    if (rate >= 2.0) {
        return TREND_SINGLE_UP;
    }
    if (rate >= 1.0) {
        return TREND_FORTY_FIVE_UP;
    }
    if (rate > -1.0) {
        return TREND_FLAT;
    }
    if (rate > -2.0) {
        return TREND_FORTY_FIVE_DOWN;
    }
    if (rate > -3.0) {
        return TREND_SINGLE_DOWN;
    }
    return TREND_DOUBLE_DOWN;
}

/* Produce clearly identified simulated readings without a Dexcom connection. */
static void demo_task(void *arg)
{
    (void)arg;
    glucose_point_t *pts = heap_caps_malloc(GLY_HISTORY_MAX * sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int64_t last_newest = 0;
    if (pts == NULL) {
        s_demo_task_running = false;
        vTaskDelete(NULL);
        return;
    }
    demo_scenario_t last_scenario = DEMO_OFF;

    while (s_demo != DEMO_OFF) {
        const demo_scenario_t sc = s_demo;
        const int64_t phase_ms = 37000;
        int64_t newest = ((now_epoch_ms() - phase_ms) / 300000) * 300000 + phase_ms;
        if (sc == DEMO_STALE) {
            newest -= 18 * 60 * 1000;
        }
        if (newest != last_newest || sc != last_scenario) {
            int target = 0;
            switch (sc) {
            case DEMO_LOW: target = 64; break;
            case DEMO_VERY_LOW: target = 48; break;
            case DEMO_HIGH: target = 265; break;
            case DEMO_VERY_HIGH: target = 330; break;
            default: break;
            }
            const int64_t newest_slot = newest / 300000;
            int n = 0;
            for (int k = 0; k < GLY_HISTORY_MAX; k++) {
                const int64_t slot = newest_slot - k;
                if (sc == DEMO_SPARSE && k >= 120) {
                    break; /* 10 h de mesures sur 24 h : couverture insuffisante */
                }
                if (sc == DEMO_GAP && (k == 1 || (k >= 30 && k < 42))) {
                    continue; /* trou d'une heure et mesure precedente manquante */
                }
                int v = demo_baseline(slot);
                if (target != 0 && k < 8) {
                    const int start = demo_baseline(newest_slot - 8);
                    v = target + (start - target) * k / 8;
                }
                pts[n].wt_ms = newest - (int64_t)k * 300000;
                pts[n].mgdl = (int16_t)(v < 40 ? 40 : (v > 400 ? 400 : v));
                pts[n].trend = TREND_FLAT;
                n++;
            }
            for (int i = 0; i + 1 < n; i++) {
                int d = 0;
                pts[i].trend = gm_delta_between(&pts[i], &pts[i + 1], &d) ? demo_trend(d) : TREND_NOT_COMPUTABLE;
            }
            app_history_set(pts, n);
            last_newest = newest;
            last_scenario = sc;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    free(pts);
    s_demo_task_running = false;
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------------- */

static void choose_window(bool full, bool backfill, int64_t now_ms, bool time_ok, int *minutes, int *max_count)
{
    if (full || s_cache_count == 0) {
        *minutes = GLY_HISTORY_FULL_MINUTES;
        *max_count = GLY_HISTORY_MAX;
        return;
    }
    if (backfill) {
        *minutes = GLY_BACKFILL_MINUTES;
        *max_count = GLY_BACKFILL_MINUTES / 5 + 2;
        return;
    }
    int64_t since_min = time_ok ? (now_ms - s_cache[0].wt_ms) / 60000 : 30;
    int m = (int)since_min + 15;
    m = m < 20 ? 20 : (m > GLY_HISTORY_FULL_MINUTES ? GLY_HISTORY_FULL_MINUTES : m);
    *minutes = m;
    int c = m / 5 + 3;
    *max_count = c > GLY_HISTORY_MAX ? GLY_HISTORY_MAX : c;
}

static void glucose_task(void *arg)
{
    (void)arg;
    s_heartbeat_us = esp_timer_get_time();

    glucose_point_t *points = heap_caps_malloc(GLY_HISTORY_MAX * sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_cache = heap_caps_calloc(GLY_HISTORY_MAX, sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    configASSERT(points != NULL && s_cache != NULL);
    static dexcom_client_t client;
    /* Identifiants saisis sur l'appareil (ou herites de la compilation) : jamais journalises */
    static credentials_t cred;
    credentials_get(&cred);
    dexcom_client_init(&client, cred.dexcom_region, cred.dexcom_user, cred.dexcom_password);

    if (!dexcom_client_configured(&client)) {
        ESP_LOGW(TAG, "Compte Dexcom non renseigne : service en attente (RÉGLAGES > CONNEXION)");
        app_state_set_dexcom(DEX_STATE_NO_CONFIG, 0);
        idle_forever();
    }

    if (dex_guard_init(cred.dexcom_region, cred.dexcom_user, cred.dexcom_password) != ESP_OK) {
        ESP_LOGE(TAG, "Verrou d'authentification illisible : Dexcom desactive par securite");
        app_state_set_dexcom(DEX_STATE_AUTH_BLOCKED, 0);
        app_state_set_dexcom_info("verrou NVS illisible");
        idle_forever();
    }

    const dex_guard_state_t guard = dex_guard_state();
    ESP_LOGI(TAG, "Service Dexcom Share : region %s, verrou d'authentification : %s",
             strcmp(cred.dexcom_region, "ous") == 0 ? "Europe (ous)" : cred.dexcom_region,
             dex_guard_state_name(guard));

    switch (guard) {
    case DEX_GUARD_VALIDATED:
        break;
    case DEX_GUARD_UNKNOWN:
        app_state_set_dexcom(DEX_STATE_AWAITING_ARM, 0);
        ESP_LOGW(TAG, "Identifiants Dexcom jamais valides : aucune authentification sans autorisation explicite");
        probe_until_ok(&client);
        app_state_set_dexcom(DEX_STATE_AWAITING_ARM, 0);
        ESP_LOGI(TAG, "Pret pour la tentative unique : en attente de l'autorisation (console : dexcom_auth)");
        for (;;) {
            while (!s_arm_request) {
                wait_seconds(1);
            }
            s_arm_request = false;
            if (single_authentication_attempt(&client)) {
                break;
            }
            if (dex_guard_state() != DEX_GUARD_UNKNOWN) {
                idle_forever(); /* tentative consommee sans succes */
            }
        }
        break;
    default:
        app_state_set_dexcom(guard == DEX_GUARD_REFUSED ? DEX_STATE_AUTH_REFUSED : DEX_STATE_AUTH_BLOCKED, 0);
        app_state_set_dexcom_info(dex_guard_info());
        ESP_LOGE(TAG, "Verrou Dexcom %s (%s) : aucune tentative automatique", dex_guard_state_name(guard),
                 dex_guard_info());
        idle_forever();
    }

    /* Identifiants valides : fonctionnement normal */
    unsigned backoff = 0;
    unsigned server_errors = 0;
    int64_t next_full_us = 0;
    int64_t next_backfill_us = 0;

    for (;;) {
        /* Mode demo : aucune requete Dexcom, l'historique reel est purge a la sortie */
        while (s_demo != DEMO_OFF) {
            wait_seconds(1);
        }
        if (s_demo_ended) {
            s_demo_ended = false;
            s_cache_count = 0;
            app_history_set(NULL, 0);
            next_full_us = 0;
            ESP_LOGW(TAG, "Fin du mode demo : donnees simulees effacees, relecture Dexcom 24 h");
        }

        wait_network_ready();

        if (s_test_bad_session && dexcom_has_session(&client)) {
            int64_t since_s = (esp_timer_get_time() - s_last_request_us) / 1000000;
            if (since_s < GLY_MIN_REQUEST_INTERVAL_S) {
                wait_seconds((int32_t)(GLY_MIN_REQUEST_INTERVAL_S - since_s));
            }
            strlcpy(client.session_id, "11111111-2222-3333-4444-555555555555", sizeof(client.session_id));
            s_test_bad_session = false;
            ESP_LOGW(TAG, "TEST : session Dexcom remplacee par une session invalide");
        }

        /* Jamais moins de 30 s entre deux requetes */
        if (s_last_request_us != 0) {
            int64_t since_s = (esp_timer_get_time() - s_last_request_us) / 1000000;
            if (since_s < GLY_MIN_REQUEST_INTERVAL_S) {
                wait_seconds((int32_t)(GLY_MIN_REQUEST_INTERVAL_S - since_s));
            }
        }

        const int64_t now_us = esp_timer_get_time();
        const bool time_ok = net_time_is_valid();
        const bool full = s_cache_count == 0 || now_us >= next_full_us;
        const bool backfill = !full && now_us >= next_backfill_us;
        int minutes = 0;
        int max_count = 0;
        choose_window(full, backfill, now_epoch_ms(), time_ok, &minutes, &max_count);

        dex_result_t r = DEX_OK;
        if (!dexcom_has_session(&client)) {
            app_state_set_dexcom(DEX_STATE_CONNECTING, 0);
            r = dexcom_login(&client);
        }

        int count = 0;
        if (r == DEX_OK) {
            r = dexcom_read_glucose(&client, minutes, max_count, points, GLY_HISTORY_MAX, &count);
            if (r == DEX_ERR_SESSION) {
                /* Session expiree : une seule reconnexion puis un seul nouvel essai */
                ESP_LOGI(TAG, "Session Dexcom expiree : reconnexion");
                r = dexcom_login(&client);
                if (r == DEX_OK) {
                    r = dexcom_read_glucose(&client, minutes, max_count, points, GLY_HISTORY_MAX, &count);
                }
            }
        }
        s_heartbeat_us = esp_timer_get_time();
        s_last_request_us = s_heartbeat_us;

        if (r == DEX_ERR_AUTH || r == DEX_ERR_MAX_ATTEMPTS) {
            stop_on_refusal(&client, r); /* ne revient pas */
        }
        if (s_demo != DEMO_OFF || s_demo_ended) {
            continue; /* resultat ignore : le mode demo a ete active pendant la requete */
        }

        if (r == DEX_OK) {
            backoff = 0;
            server_errors = 0;
            if (full) {
                next_full_us = s_last_request_us + (int64_t)GLY_FULL_RESYNC_PERIOD_S * 1000000;
                next_backfill_us = s_last_request_us + (int64_t)GLY_BACKFILL_PERIOD_S * 1000000;
            } else if (backfill) {
                next_backfill_us = s_last_request_us + (int64_t)GLY_BACKFILL_PERIOD_S * 1000000;
            }

            const int64_t previous_newest = s_cache_count > 0 ? s_cache[0].wt_ms : 0;
            int newer = cache_merge(points, count, now_epoch_ms(), net_time_is_valid());
            if (s_cache_count == 0) {
                ESP_LOGW(TAG, "Dexcom : aucune mesure sur les 24 dernieres heures");
                app_state_set_dexcom(DEX_STATE_NO_DATA, 120);
                app_history_set(NULL, 0);
                wait_poll(120);
                continue;
            }
            app_history_set(s_cache, s_cache_count);
            app_state_set_dexcom(DEX_STATE_OK, 0);

            if (newer > 0 && s_new_readings_cb != NULL) {
                /* du plus ancien au plus recent, uniquement les mesures reellement nouvelles */
                s_new_readings_cb(s_cache, s_cache_count, previous_newest);
            }

            const int64_t newest = s_cache[0].wt_ms;
            int64_t age_s = net_time_is_valid() ? (now_epoch_ms() - newest) / 1000 : -1;
            int32_t delay_s = next_poll_delay_s(newest);
            ESP_LOGI(TAG, "Lecture Dexcom OK (%s %d min) : %d lues, cache %d, derniere %d mg/dL, tendance %s, "
                     "age %" PRId64 " s%s, prochaine lecture dans %" PRId32 " s",
                     full ? "complete" : (backfill ? "rattrapage" : "increment"), minutes, count, s_cache_count,
                     s_cache[0].mgdl, trend_name(s_cache[0].trend), age_s, newer > 0 ? " (nouvelle)" : "", delay_s);
            wait_poll(delay_s);
            continue;
        }

        int32_t delay_s = k_backoff_s[backoff < BACKOFF_STEPS ? backoff : BACKOFF_STEPS - 1];
        if (r == DEX_ERR_RATE_LIMITED && delay_s < 300) {
            delay_s = 300;
        }
        if (backoff < BACKOFF_STEPS) {
            backoff++;
        }
        if (r == DEX_ERR_SERVER) {
            server_errors++;
        }
        if (r == DEX_ERR_SESSION || server_errors >= 2) {
            /* Session douteuse : nouvelle connexion (identifiants valides) au prochain essai */
            dexcom_invalidate_session(&client);
            server_errors = 0;
        }
        app_state_set_dexcom(DEX_STATE_RETRYING, delay_s);
        ESP_LOGW(TAG, "Dexcom indisponible (%s) : nouvel essai dans %" PRId32 " s, historique conserve",
                 dex_result_name(r), delay_s);
        wait_poll(delay_s);
    }
}

int64_t glucose_service_heartbeat_age_s(void)
{
    return (esp_timer_get_time() - s_heartbeat_us) / 1000000;
}

const char *glucose_service_request_auth(void)
{
    if (!credentials_has_dexcom()) {
        return "refuse : identifiants absents";
    }
    switch (dex_guard_state()) {
    case DEX_GUARD_UNKNOWN:
        s_arm_request = true;
        return "tentative unique autorisee";
    case DEX_GUARD_VALIDATED:
        return "inutile : identifiants deja valides";
    default:
        return "refuse : tentative deja consommee (verrou NVS)";
    }
}

const char *glucose_service_test_invalid_session(void)
{
    if (dex_guard_state() != DEX_GUARD_VALIDATED) {
        return "refuse : identifiants non valides";
    }
    s_test_bad_session = true;
    return "session invalide injectee a la prochaine lecture (30 s minimum entre requetes)";
}

const char *glucose_service_set_demo(const char *scenario)
{
    static const struct {
        const char *name;
        demo_scenario_t sc;
    } k_scenarios[] = {
        {"off", DEMO_OFF}, {"normal", DEMO_NORMAL}, {"low", DEMO_LOW}, {"verylow", DEMO_VERY_LOW},
        {"high", DEMO_HIGH}, {"veryhigh", DEMO_VERY_HIGH}, {"gap", DEMO_GAP}, {"stale", DEMO_STALE},
        {"sparse", DEMO_SPARSE},
    };
    for (size_t i = 0; i < sizeof(k_scenarios) / sizeof(k_scenarios[0]); i++) {
        if (strcmp(scenario, k_scenarios[i].name) != 0) {
            continue;
        }
        const demo_scenario_t sc = k_scenarios[i].sc;
        if (sc == DEMO_OFF) {
            if (s_demo != DEMO_OFF) {
                s_demo = DEMO_OFF;
                s_demo_ended = true;
                app_state_set_demo(false);
                app_history_set(NULL, 0);
                ESP_LOGW(TAG, "MODE DEMO desactive");
            }
            return "demo desactivee";
        }
        s_demo = sc;
        app_state_set_demo(true);
        ESP_LOGW(TAG, "MODE DEMO actif : scenario %s (donnees simulees, aucune requete Dexcom)", scenario);
        if (!s_demo_task_running) {
            s_demo_task_running = true;
            if (xTaskCreate(demo_task, "demo", 4096, NULL, 3, NULL) != pdPASS) {
                s_demo_task_running = false;
                return "erreur : tache demo";
            }
        }
        return "demo active";
    }
    return "scenario inconnu (off normal low verylow high veryhigh gap stale sparse)";
}

void glucose_service_set_new_readings_cb(glucose_new_readings_cb_t cb)
{
    s_new_readings_cb = cb;
}

esp_err_t glucose_service_start(void)
{
    s_heartbeat_us = esp_timer_get_time();
    BaseType_t ok = xTaskCreate(glucose_task, "glycemie", 12288, NULL, 4, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

/* ------------------------------------------------------------------------- */
/* Assistant de configuration : essai de connexion (tache dediee, pile interne pour TLS) */

static volatile gly_test_t s_test = GLY_TEST_INACTIF;
static char s_test_msg[96];
static portMUX_TYPE s_test_lock = portMUX_INITIALIZER_UNLOCKED;

static void test_set(gly_test_t etat, const char *msg)
{
    portENTER_CRITICAL(&s_test_lock);
    s_test = etat;
    strlcpy(s_test_msg, msg, sizeof(s_test_msg));
    portEXIT_CRITICAL(&s_test_lock);
}

/* Test candidate credentials without persisting them before user confirmation. */
static void test_task(void *arg)
{
    (void)arg;
    credentials_t cred;
    credentials_get(&cred);

    test_set(GLY_TEST_EN_COURS, "wifi");
    if (!wifi_manager_apply_credentials(pdMS_TO_TICKS(25000))) {
        test_set(GLY_TEST_ECHEC, "wifi");
        memset(&cred, 0, sizeof(cred));
        vTaskDelete(NULL);
        return;
    }
    test_set(GLY_TEST_EN_COURS, "internet");
    bool heure = false;
    for (int i = 0; i < 40 && !heure; i++) { /* l'heure SNTP prouve l'acces a Internet (20 s au plus) */
        heure = net_time_is_valid();
        if (!heure) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    if (!heure) {
        test_set(GLY_TEST_ECHEC, "internet");
        memset(&cred, 0, sizeof(cred));
        vTaskDelete(NULL);
        return;
    }

    test_set(GLY_TEST_EN_COURS, "dexcom");
    dexcom_client_t client;
    dexcom_client_init(&client, cred.dexcom_region, cred.dexcom_user, cred.dexcom_password);
    if (!dexcom_client_configured(&client)) {
        test_set(GLY_TEST_ECHEC, "dexcom_vide");
        memset(&cred, 0, sizeof(cred));
        vTaskDelete(NULL);
        return;
    }
    /* Le verrou suit ces identifiants : un essai demande par l'utilisateur est une autorisation explicite */
    dex_guard_init(cred.dexcom_region, cred.dexcom_user, cred.dexcom_password);
    dex_guard_set(DEX_GUARD_IN_PROGRESS, "essai depuis l'assistant");
    const dex_result_t r = dexcom_login(&client);
    switch (r) {
    case DEX_OK:
        dex_guard_set(DEX_GUARD_VALIDATED, "essai reussi depuis l'assistant");
        test_set(GLY_TEST_REUSSI, "ok");
        break;
    case DEX_ERR_AUTH:
        dex_guard_set(DEX_GUARD_REFUSED, "identifiants refuses (assistant)");
        test_set(GLY_TEST_ECHEC, "dexcom_refuse");
        break;
    case DEX_ERR_MAX_ATTEMPTS:
        dex_guard_set(DEX_GUARD_REFUSED, "compte bloque (assistant)");
        test_set(GLY_TEST_ECHEC, "dexcom_bloque");
        break;
    case DEX_ERR_NETWORK:
    case DEX_ERR_RATE_LIMITED:
        dex_guard_set(DEX_GUARD_UNKNOWN, "essai sans reponse (assistant)");
        test_set(GLY_TEST_ECHEC, "dexcom_reseau");
        break;
    default:
        dex_guard_set(DEX_GUARD_UNKNOWN, "essai sans reponse (assistant)");
        test_set(GLY_TEST_ECHEC, "dexcom_autre");
        break;
    }
    dexcom_invalidate_session(&client);
    memset(&client, 0, sizeof(client));
    memset(&cred, 0, sizeof(cred));
    vTaskDelete(NULL);
}

void glucose_service_test_request(void)
{
    if (s_test == GLY_TEST_EN_COURS) {
        return;
    }
    test_set(GLY_TEST_EN_COURS, "demarrage");
    if (xTaskCreate(test_task, "essai_cnx", 12288, NULL, 4, NULL) != pdPASS) {
        test_set(GLY_TEST_ECHEC, "memoire");
    }
}

gly_test_t glucose_service_test_state(char *msg, size_t size)
{
    portENTER_CRITICAL(&s_test_lock);
    const gly_test_t etat = s_test;
    if (msg != NULL && size > 0) {
        strlcpy(msg, s_test_msg, size);
    }
    portEXIT_CRITICAL(&s_test_lock);
    return etat;
}
