/* wifi_manager.h: Wi-Fi scan and connection state interface.
 * FR: Interface de recherche et de connexion Wi-Fi.
 * Shared declarations for the implementation.
 */
/*
 * Wi-Fi station via l'ESP32-C6 (ESP-Hosted + esp_wifi_remote), d'apres l'exemple
 * officiel Waveshare 05_wifistation. Le firmware du C6 n'est jamais modifie.
 */
/* EN: Wi-Fi state and network scanning; setup requests must not expose passwords.
 * FR: État du Wi-Fi et recherche de réseaux sans divulguer les mots de passe.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t wifi_manager_start(void);

/* Assistant de configuration : liste des reseaux visibles (noms uniques, tries par signal).
 * Bloquant (quelques secondes) : a appeler hors de la tache LVGL. Retourne le nombre de reseaux ecrits. */
typedef struct {
    char ssid[33];
    int8_t rssi;      /* dBm */
    bool protege;     /* reseau avec mot de passe */
} wifi_reseau_t;
int wifi_manager_scan(wifi_reseau_t *out, int max);

/* Applique les identifiants enregistres (credentials.c) et relance une connexion.
 * true si l'association et l'adresse IP sont obtenues avant la fin du delai. */
bool wifi_manager_apply_credentials(TickType_t timeout);
bool wifi_manager_is_connected(void);
bool wifi_manager_wait_connected(TickType_t timeout);

/* Vrai quand le Wi-Fi heberge est demarre (liaison SDIO avec l'ESP32-C6 etablie) */
bool wifi_manager_wait_started(TickType_t timeout);

/* Test de fiabilite : coupe volontairement l'association (la reconnexion doit etre automatique) */
void wifi_manager_test_disconnect(void);
