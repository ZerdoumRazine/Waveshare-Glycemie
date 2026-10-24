/* Deterministic mock MAC-address interface for tests.
 * Used exclusively by hardware-independent regression tests.
 */
/* Adresse MAC simule for PC tests: fixed value, therefore reproducible results. */
/* Provide a fixed fake MAC address for repeatable test data.
 */
#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef enum {
    ESP_MAC_WIFI_STA = 0,
    ESP_MAC_WIFI_SOFTAP = 1,
} esp_mac_type_t;

esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type);
