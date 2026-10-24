/* wifi_manager.h: Wi-Fi scan and connection state interface.
 * Shared declarations for the implementation.
 */
/*
 * Wi-Fi station via l'ESP32-C6 (ESP-Hosted + esp_wifi_remote), according to the example
 * official Waveshare 05 wifistation. The firmware of C6 is never changed.
 */
/* Wi-Fi state and network scanning; setup requests must not expose passwords.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t wifi_manager_start(void);

/* Configuration assistant: list of visible networks (single names, sort by signal).
 * Blocking (a few seconds): a call out of the spot LVGL. Returns the number of networks written. */
typedef struct {
    char ssid[33];
    int8_t rssi;      /* dBm */
    bool protege;     /* network with password */
} wifi_reseau_t;
int wifi_manager_scan(wifi_reseau_t *out, int max);

/* Apply the saved identifiers (credentials.c) and restart a connection.
 * true if the association and IP address are obtained before the end of the delay. */
bool wifi_manager_apply_credentials(TickType_t timeout);
bool wifi_manager_is_connected(void);
bool wifi_manager_wait_connected(TickType_t timeout);

/* True when Wi-Fi heberge is started (linking SDIO withESP32-C6 etablie) */
bool wifi_manager_wait_started(TickType_t timeout);

/* Fiability test: Voluntaryly cuts the association (reconnection must be automatic) */
void wifi_manager_test_disconnect(void);
