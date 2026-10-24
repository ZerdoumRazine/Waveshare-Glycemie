/* glucose_service.h: Lifecycle and freshness of glucose requests.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Glycemy service: plans readings Dexcom Share, gere session, errors and delays,
 * and maintains a cache of the last 24 hours (incremental scans, without reauthentication).
 * Never valid identify only triggerAn attempt, upon explicit authorization.
 */
/* Public interface / Interface publique
 * Polling, demo mode and uncommitted setup-test public APIs.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "app_state.h"

/*
 * Called after each reading providing new real measures.
 * `cache` is sorted from the most recent to the oldest; `previous_newest_ms` is the time stamp
 * the most recent measurement before reading (0 at start).
 * Never called in demonstration mode.
 */
typedef void (*glucose_new_readings_cb_t)(const glucose_point_t *cache, int count, int64_t previous_newest_ms);

esp_err_t glucose_service_start(void);

/* Configuration Assistant: test connection with the saved identifiers.
 * The test rotates in its own spot; the interface questions the state without ever blocking. */
typedef enum {
    GLY_TEST_INACTIF = 0,
    GLY_TEST_EN_COURS,
    GLY_TEST_REUSSI,
    GLY_TEST_ECHEC,
} gly_test_t;
void glucose_service_test_request(void);                    /* run a trial (does not know if already underway) */
gly_test_t glucose_service_test_state(char *msg, size_t size); /* message ready to display (without secret) */
void glucose_service_set_new_readings_cb(glucose_new_readings_cb_t cb);

/* Seconds since the last sign of life of the spot (monitoring by app_main) */
int64_t glucose_service_heartbeat_age_s(void);

/* Authorizes the single attempt to authenticate; returns a textual record */
const char *glucose_service_request_auth(void);

/* Fiability Test: Force an expired session to verify automatic renewal */
const char *glucose_service_test_invalid_session(void);

/* Mode demonstration (console) : off, normal, low, verylow, high, veryhigh, gap, stale */
const char *glucose_service_set_demo(const char *scenario);
