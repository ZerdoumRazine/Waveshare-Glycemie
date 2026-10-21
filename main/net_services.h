/*
 * Services reseau : heure (SNTP, fuseau France), test DNS + HTTPS neutre, RSSI.
 */
/* Public interface / Interface publique
 * EN: Internet probing and clock synchronization service.
 * FR: Vérification d'Internet et synchronisation de l'horloge.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t net_services_start(void);

/* Vrai si l'heure systeme est fiable (synchronisee par SNTP ou deja valide) */
bool net_time_is_valid(void);
