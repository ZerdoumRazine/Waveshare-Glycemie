/* dexcom_guard.h: Persistent Dexcom authentication state API.
 * The declarations below are shared with other firmware modules.
 */
/* Persist the Dexcom Share authentication guard in NVS.
 * Unknown or rejected credentials must not be retried automatically:
 * otherwise repeated failures could lock the user's Dexcom account.
 * Record an attempt before sending credentials so a restart cannot reset
 * this protection. Only verified credentials permit automatic sessions.
 * A single explicitly armed attempt may be requested through dexcom_auth.
 * The guard is tied to a fingerprint of the submitted credentials. */
/* Read and persist login state to protect against repeated rejected attempts. */
#pragma once

#include "esp_err.h"

typedef enum {
    DEX_GUARD_UNKNOWN = 0,      /* no attempt with these identifiers */
    DEX_GUARD_IN_PROGRESS = 1,  /* attempt starts, result never saves (starts up) */
    DEX_GUARD_VALIDATED = 2,    /* Authentication succeeded; normal connection retries are permitted. */
    DEX_GUARD_REFUSED = 3,      /* Credentials rejected or account blocked; do not retry automatically. */
    DEX_GUARD_INCONCLUSIVE = 4, /* Request reached the server without a definitive verdict; require manual retry. */
} dex_guard_state_t;

esp_err_t dex_guard_init(const char *region, const char *username, const char *password);
dex_guard_state_t dex_guard_state(void);
const char *dex_guard_info(void);
esp_err_t dex_guard_set(dex_guard_state_t state, const char *info);
const char *dex_guard_state_name(dex_guard_state_t state);
