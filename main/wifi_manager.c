/*
 * wifi_manager.c: Wi-Fi scan, association and reconnection.
 * Design note: Network failure must not destroy the saved configuration.
 */

/*
 * Coordinates the ESP32-C6 network driver, reconnection and Wi-Fi scanning.
 * Coordinates the Wi-Fi driver, its reconnections and network search.
 */
#include "wifi_manager.h"

/*
 * The start of Wi-Fi (recording of events managers, waiting by group)
 * d'evenements) suit le squelette de l'exemple station d'Espressif, repris tel quel par Waveshare
 * (examples/esp-idf/05_wifistation, SPDX-License-Identifier: Apache-2.0). Tout le reste - liaison
 * par ESP-Hosted, reconnection, search for networks, hot application of identifiers - is
 * propre a ce projet.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "app_state.h"
#include "credentials.h"

static const char *TAG = "wifi";

#define BIT_STA_STARTED  BIT0
#define BIT_CONNECTED    BIT1
#define BIT_DISCONNECTED BIT2

static EventGroupHandle_t s_events;
static esp_netif_t *s_sta_netif;
static volatile bool s_connected;

/* Progressive reconnection: never aggressive loop */
static const uint32_t k_retry_delays_ms[] = {2000, 5000, 10000, 30000, 60000};
#define RETRY_STEPS (sizeof(k_retry_delays_ms) / sizeof(k_retry_delays_ms[0]))

static bool wifi_configured(void)
{
    return credentials_has_wifi();
}

/* Station configuration from user input identifiers (never logs) */
static esp_err_t apply_config(void)
{
    credentials_t cred;
    credentials_get(&cred);
    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, cred.wifi_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, cred.wifi_password, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = cred.wifi_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    const esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wc);
    memset(&wc, 0, sizeof(wc));
    memset(&cred, 0, sizeof(cred));
    return err;
}

/* Convert asynchronous events into a stable connection state. */
static void on_network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            xEventGroupSetBits(s_events, BIT_STA_STARTED);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Association Wi-Fi OK (attente DHCP)");
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            const wifi_event_sta_disconnected_t *d = data;
            s_connected = false;
            /* Never log SSID : seulement la raison et le signal */
            ESP_LOGW(TAG, "Wi-Fi deconnecte (raison %d, RSSI %d dBm)", d ? d->reason : -1, d ? d->rssi : 0);
            app_state_set_wifi(LINK_ERROR, 0, "");
            xEventGroupClearBits(s_events, BIT_CONNECTED);
            xEventGroupSetBits(s_events, BIT_DISCONNECTED);
            break;
        }
        default:
            break;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            const ip_event_got_ip_t *e = data;
            char ip[16];
            snprintf(ip, sizeof(ip), IPSTR, IP2STR(&e->ip_info.ip));
            ESP_LOGI(TAG, "DHCP OK : IP %s, passerelle " IPSTR, ip, IP2STR(&e->ip_info.gw));
            s_connected = true;
            app_state_set_wifi(LINK_OK, 0, ip);
            xEventGroupClearBits(s_events, BIT_DISCONNECTED);
            xEventGroupSetBits(s_events, BIT_CONNECTED);
        } else if (id == IP_EVENT_STA_LOST_IP) {
            ESP_LOGW(TAG, "Adresse IP perdue");
        }
    }
}

static void read_coprocessor_version(void)
{
    esp_hosted_coprocessor_fwver_t ver = {0};
    char text[24];
    esp_err_t err = esp_hosted_get_coprocessor_fwversion(&ver);
    if (err == ESP_OK) {
        snprintf(text, sizeof(text), "%" PRIu32 ".%" PRIu32 ".%" PRIu32, ver.major1, ver.minor1, ver.patch1);
        ESP_LOGI(TAG, "ESP32-C6 : firmware ESP-Hosted esclave version %s", text);
    } else {
        strlcpy(text, "inconnue", sizeof(text));
        ESP_LOGW(TAG, "ESP32-C6 : version du firmware esclave non lisible (%s)", esp_err_to_name(err));
    }
    app_state_set_coproc_fw(text);
}

static void scan_networks(void)
{
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Scan Wi-Fi via l'ESP32-C6 impossible : %s", esp_err_to_name(err));
        return;
    }
    uint16_t total = 0;
    esp_wifi_scan_get_ap_num(&total);
    uint16_t count = total > 40 ? 40 : total;
    wifi_ap_record_t *records = heap_caps_calloc(count ? count : 1, sizeof(wifi_ap_record_t),
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (records == NULL) {
        ESP_LOGW(TAG, "Scan Wi-Fi : memoire insuffisante");
        return;
    }
    if (esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) {
        count = 0;
    }

    char attendu[CRED_SSID_MAX] = "";
    if (wifi_configured()) {
        credentials_masked_wifi(attendu, sizeof(attendu)); /* the name of the network is not a secret */
    }
    int best_rssi = -127;
    bool found = false;
    int found_rssi = -127;
    int found_channel = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (records[i].rssi > best_rssi) {
            best_rssi = records[i].rssi;
        }
        if (wifi_configured() &&
            strncmp((const char *)records[i].ssid, attendu, sizeof(records[i].ssid)) == 0 &&
            records[i].rssi > found_rssi) {
            found = true;
            found_rssi = records[i].rssi;
            found_channel = records[i].primary;
        }
    }
    free(records);

    ESP_LOGI(TAG, "Scan Wi-Fi via l'ESP32-C6 : %u reseaux visibles (meilleur signal %d dBm)", total, best_rssi);
    if (wifi_configured()) {
        if (found) {
            ESP_LOGI(TAG, "Reseau configure visible : RSSI %d dBm, canal %d", found_rssi, found_channel);
        } else {
            ESP_LOGW(TAG, "Reseau configure absent du scan (verifier le nom et la bande 2,4 GHz)");
        }
    }
}

/* Retry network connection without blocking the LVGL touchscreen task. */
static void wifi_task(void *arg)
{
    xEventGroupWaitBits(s_events, BIT_STA_STARTED, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi heberge demarre (ESP32-C6 via SDIO)");
    read_coprocessor_version();
    scan_networks();

    if (!wifi_configured()) {
        ESP_LOGW(TAG, "Aucun reseau Wi-Fi configure (private/device_secrets.h)");
        app_state_set_wifi(LINK_UNKNOWN, 0, "");
        vTaskDelete(NULL);
        return;
    }

    unsigned attempt = 0;
    app_state_set_wifi(LINK_PENDING, 0, "");
    xEventGroupClearBits(s_events, BIT_DISCONNECTED);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect : %s", esp_err_to_name(err));
        xEventGroupSetBits(s_events, BIT_DISCONNECTED);
    }

    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(s_events, BIT_CONNECTED | BIT_DISCONNECTED,
                                               pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & BIT_CONNECTED) {
            attempt = 0;
            xEventGroupWaitBits(s_events, BIT_DISCONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
            continue;
        }

        uint32_t delay_ms = k_retry_delays_ms[attempt < RETRY_STEPS ? attempt : RETRY_STEPS - 1];
        attempt++;
        ESP_LOGI(TAG, "Nouvelle tentative Wi-Fi dans %" PRIu32 " s (essai %u)", delay_ms / 1000, attempt);
        app_state_set_wifi(LINK_PENDING, 0, "");
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        xEventGroupClearBits(s_events, BIT_DISCONNECTED);
        err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_connect : %s", esp_err_to_name(err));
            xEventGroupSetBits(s_events, BIT_DISCONNECTED);
        }
    }
}

esp_err_t wifi_manager_start(void)
{
    s_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_events, ESP_ERR_NO_MEM, TAG, "event group");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    s_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_sta_netif, ESP_FAIL, TAG, "netif STA");
    esp_netif_set_hostname(s_sta_netif, "glycemie-p4");

    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_network_event, NULL, NULL),
                        TAG, "handler WIFI_EVENT");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, on_network_event, NULL, NULL),
                        TAG, "handler IP_EVENT");

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "esp_wifi_init");

    /* Configuration gardee en RAM : nothing recorded in the memory of the C6 */
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_storage(RAM) : %s", esp_err_to_name(err));
    }
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "esp_wifi_set_mode");

    app_state_set_wifi_configured(wifi_configured());
    if (wifi_configured()) {
        ESP_RETURN_ON_ERROR(apply_config(), TAG, "esp_wifi_set_config");
    }

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start");

    BaseType_t ok = xTaskCreate(wifi_task, "wifi_mgr", 6144, NULL, 5, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "tache wifi");
    return ESP_OK;
}

/* Assistant: list of visible networks, without duplicate, the strongest first */
int wifi_manager_scan(wifi_reseau_t *out, int max)
{
    if (out == NULL || max <= 0 || !wifi_manager_wait_started(pdMS_TO_TICKS(5000))) {
        return 0;
    }
    if (esp_wifi_scan_start(NULL, true) != ESP_OK) {
        return 0;
    }
    uint16_t total = 0;
    esp_wifi_scan_get_ap_num(&total);
    uint16_t count = total > 40 ? 40 : total;
    wifi_ap_record_t *records = heap_caps_calloc(count ? count : 1, sizeof(wifi_ap_record_t),
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (records == NULL) {
        return 0;
    }
    if (esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) {
        count = 0;
    }
    int n = 0;
    for (uint16_t i = 0; i < count && n < max; i++) {
        const char *ssid = (const char *)records[i].ssid;
        if (ssid[0] == '\0') {
            continue; /* reseau masque : saisie manuelle du nom */
        }
        int deja = -1;
        for (int j = 0; j < n; j++) {
            if (strncmp(out[j].ssid, ssid, sizeof(out[j].ssid)) == 0) {
                deja = j;
                break;
            }
        }
        if (deja >= 0) {
            if (records[i].rssi > out[deja].rssi) {
                out[deja].rssi = records[i].rssi;
            }
            continue;
        }
        strlcpy(out[n].ssid, ssid, sizeof(out[n].ssid));
        out[n].rssi = records[i].rssi;
        out[n].protege = records[i].authmode != WIFI_AUTH_OPEN;
        n++;
    }
    free(records);
    for (int i = 0; i < n - 1; i++) { /* tri par signal decroissant (liste courte) */
        for (int j = i + 1; j < n; j++) {
            if (out[j].rssi > out[i].rssi) {
                const wifi_reseau_t tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    ESP_LOGI(TAG, "Assistant : %d reseau(x) propose(s) a l'utilisateur", n);
    return n;
}

/* Assistant: Applies Save IDs and waits for IP address */
bool wifi_manager_apply_credentials(TickType_t timeout)
{
    if (!wifi_manager_wait_started(pdMS_TO_TICKS(5000))) {
        return false;
    }
    s_connected = false;
    xEventGroupClearBits(s_events, BIT_CONNECTED);
    esp_wifi_disconnect();
    app_state_set_wifi_configured(wifi_configured());
    if (!wifi_configured() || apply_config() != ESP_OK) {
        return false;
    }
    ESP_LOGI(TAG, "Assistant : connexion au reseau choisi");
    if (esp_wifi_connect() != ESP_OK) {
        return false;
    }
    return wifi_manager_wait_connected(timeout);
}

void wifi_manager_test_disconnect(void)
{
    ESP_LOGW(TAG, "TEST : deconnexion Wi-Fi volontaire (verification de la reconnexion automatique)");
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_disconnect : %s", esp_err_to_name(err));
    }
}

bool wifi_manager_wait_started(TickType_t timeout)
{
    if (s_events == NULL) {
        vTaskDelay(timeout);
        return false;
    }
    EventBits_t bits = xEventGroupWaitBits(s_events, BIT_STA_STARTED, pdFALSE, pdTRUE, timeout);
    return (bits & BIT_STA_STARTED) != 0;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

bool wifi_manager_wait_connected(TickType_t timeout)
{
    if (s_events == NULL) {
        vTaskDelay(timeout);
        return false;
    }
    EventBits_t bits = xEventGroupWaitBits(s_events, BIT_CONNECTED, pdFALSE, pdTRUE, timeout);
    return (bits & BIT_CONNECTED) != 0;
}
