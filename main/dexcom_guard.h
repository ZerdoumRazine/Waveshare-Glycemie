/* dexcom_guard.h: Persistent Dexcom authentication state API.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Verrou d'authentification Dexcom, persistant en NVS.
 *
 * Objective: Never risk blocking the Dexcom account.
 * - No authentication until the identifiers are valid,
 *   sauf tentative unique armee explicitement (console "dexcom_auth").
 * - L'etat est enregistre AVANT sending IDs and then updated with the result:
 *   a reboot during or after a failure never restarts an attempt.
 * - Seul l'etat VALIDE allows automatic connections (restart, session expires).
 * The lock is linked to a fingerprint of the identifiers: new identifiers start from INCONNU.
 */
/* Public interface / Interface publique
 * Saved authentication decisions prevent repeated attempts against a blocked account.
 */
#pragma once

#include "esp_err.h"

typedef enum {
    DEX_GUARD_UNKNOWN = 0,      /* no attempt with these identifiers */
    DEX_GUARD_IN_PROGRESS = 1,  /* attempt starts, result never saves (starts up) */
    DEX_GUARD_VALIDATED = 2,    /* authentification reussie : fonctionnement normal */
    DEX_GUARD_REFUSED = 3,      /* identifiants refuses ou compte bloque : plus aucune tentative */
    DEX_GUARD_INCONCLUSIVE = 4, /* identifiers sent without verdict (reseau/serveur) : pas de nouvel essai */
} dex_guard_state_t;

esp_err_t dex_guard_init(const char *region, const char *username, const char *password);
dex_guard_state_t dex_guard_state(void);
const char *dex_guard_info(void);
esp_err_t dex_guard_set(dex_guard_state_t state, const char *info);
const char *dex_guard_state_name(dex_guard_state_t state);
