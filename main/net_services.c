#include "net_services.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "app_config.h"
#include "app_state.h"
#include "wifi_manager.h"

static const char *TAG = "net";

#define MIN_VALID_EPOCH 1735689600 /* 2025-01-01 */

static volatile bool s_time_synced;

static void on_time_sync(struct timeval *tv)
{
    (void)tv;
    s_time_synced = true;
}

bool net_time_is_valid(void)
{
    return s_time_synced || time(NULL) > MIN_VALID_EPOCH;
}

static bool dns_check(void)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    int err = getaddrinfo(GLY_INTERNET_PROBE_HOST, "443", &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGW(TAG, "DNS : echec de resolution de %s (erreur %d)", GLY_INTERNET_PROBE_HOST, err);
        return false;
    }
    char addr[16];
    inet_ntoa_r(((struct sockaddr_in *)res->ai_addr)->sin_addr, addr, sizeof(addr));
    freeaddrinfo(res);
    ESP_LOGI(TAG, "DNS OK : %s -> %s", GLY_INTERNET_PROBE_HOST, addr);
    return true;
}

static bool https_check(void)
{
    esp_http_client_config_t cfg = {
        .url = GLY_INTERNET_PROBE_URL,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    int64_t t0 = esp_timer_get_time();
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return false;
    }
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    int elapsed_ms = (int)((esp_timer_get_time() - t0) / 1000);

    if (err == ESP_OK && (status == 204 || status == 200)) {
        ESP_LOGI(TAG, "Internet OK : HTTPS avec certificat verifie, HTTP %d en %d ms", status, elapsed_ms);
        return true;
    }
    ESP_LOGW(TAG, "Internet : test HTTPS en echec (%s, HTTP %d)", esp_err_to_name(err), status);
    return false;
}

static void net_task(void *arg)
{
    (void)arg;
    bool sntp_started = false;
    bool time_reported = false;
    bool was_connected = false;
    int failures = 0;
    int64_t next_check_us = 0;
    int64_t next_rssi_us = 0;

    for (;;) {
        if (!wifi_manager_wait_connected(pdMS_TO_TICKS(1000))) {
            if (was_connected) {
                was_connected = false;
                app_state_set_internet(LINK_PENDING);
            }
            continue;
        }
        if (!was_connected) {
            was_connected = true;
            next_check_us = 0; /* nouveau test apres chaque (re)connexion */
            failures = 0;
        }

        if (!sntp_started) {
            esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                                        ESP_SNTP_SERVER_LIST(GLY_SNTP_SERVER_1, GLY_SNTP_SERVER_2));
            cfg.sync_cb = on_time_sync;
            esp_err_t err = esp_netif_sntp_init(&cfg);
            if (err == ESP_OK) {
                sntp_started = true;
                app_state_set_time(LINK_PENDING);
                ESP_LOGI(TAG, "SNTP demarre (%s, %s)", GLY_SNTP_SERVER_1, GLY_SNTP_SERVER_2);
            } else {
                ESP_LOGW(TAG, "SNTP : initialisation impossible (%s)", esp_err_to_name(err));
            }
        }

        int64_t now_us = esp_timer_get_time();
        if (now_us >= next_check_us) {
            app_state_set_internet(LINK_PENDING);
            bool ok = dns_check() && https_check();
            app_state_set_internet(ok ? LINK_OK : LINK_ERROR);
            if (ok) {
                failures = 0;
                next_check_us = esp_timer_get_time() + 15LL * 60 * 1000000;
            } else {
                failures++;
                next_check_us = esp_timer_get_time() + (failures < 3 ? 30LL : 120LL) * 1000000;
            }
        }

        if (s_time_synced && !time_reported) {
            time_t t = time(NULL);
            struct tm tm_local;
            localtime_r(&t, &tm_local);
            char text[32];
            strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &tm_local);
            ESP_LOGI(TAG, "Heure OK (SNTP) : %s (heure de Paris)", text);
            app_state_set_time(LINK_OK);
            time_reported = true;
        }

        if (now_us >= next_rssi_us) {
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                app_state_set_rssi(ap.rssi);
            }
            next_rssi_us = now_us + 30LL * 1000000;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t net_services_start(void)
{
    setenv("TZ", GLY_TIMEZONE, 1);
    tzset();
    if (time(NULL) > MIN_VALID_EPOCH) {
        app_state_set_time(LINK_OK);
    }
    BaseType_t ok = xTaskCreate(net_task, "net_svc", 8192, NULL, 4, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
