/* net_services.h: Network time and service initialization declarations.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Services reseau : heure (SNTP, fuseau France), test DNS + HTTPS neutre, RSSI.
 */
/* Public interface / Interface publique
 * Internet probing and clock synchronization service.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t net_services_start(void);

/* Vrai si l'heure systeme est fiable (synchronisee par SNTP ou deja valide) */
bool net_time_is_valid(void);
