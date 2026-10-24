/* net_services.h: Network time and service initialization declarations.
 * The declarations below are shared with other firmware modules.
 */
/* Network services: France-local SNTP time, DNS/HTTPS probe and Wi-Fi RSSI. */
/* Connectivity probing and clock-synchronization APIs. */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t net_services_start(void);

/* True if system time has been synchronized or otherwise validated. */
bool net_time_is_valid(void);
