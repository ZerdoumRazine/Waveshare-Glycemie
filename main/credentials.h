/* credentials.h: Contracts for Wi-Fi/Dexcom trials and NVS persistence.
 * The declarations below are shared with other firmware modules.
 */
/* User-supplied Wi-Fi and Dexcom Share credentials can be configured directly
 * on the touchscreen or through the Windows USB tool without recompiling.
 * Credentials are stored in the "ident" NVS namespace, separate from
 * application preferences and Dexcom's authentication guard. The MAC-derived
 * XOR obfuscation is reversible; it is NOT encryption.
 * Never print raw passwords or place them in public firmware builds.
 * Legacy build defaults are read only if no saved credential record exists. */
/* Read, mask, trial, confirm and erase account details with explicit errors. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define CRED_SSID_MAX   33  /* Maximum 32-byte Wi-Fi SSID plus trailing NUL. */
#define CRED_PASS_MAX   65  /* Maximum 64-byte password plus trailing NUL. */
#define CRED_USER_MAX   65
#define CRED_REGION_MAX 4   /* "ous", "us", "jp" */

typedef struct {
    char wifi_ssid[CRED_SSID_MAX];
    char wifi_password[CRED_PASS_MAX];
    char dexcom_user[CRED_USER_MAX];
    char dexcom_password[CRED_PASS_MAX];
    char dexcom_region[CRED_REGION_MAX];
} credentials_t;

/* Call after settings_init(). */
esp_err_t credentials_init(void);

/* Complete copy: only for plug-in modules (Wi-Fi, Dexcom). Never show. */
void credentials_get(credentials_t *out);

bool credentials_has_wifi(void);
bool credentials_has_dexcom(void);
bool credentials_configured(void);   /* Both Wi-Fi and Dexcom settings are present. */
bool credentials_from_build(void);   /* Values originated from legacy build-time defaults. */

/* Record immediately (after a successful test, or for a console resumption). */
esp_err_t credentials_set_wifi(const char *ssid, const char *password);
esp_err_t credentials_set_dexcom(const char *user, const char *password, const char *region);
esp_err_t credentials_clear(void);   /* Clear everything: NVS, memory and authentication lock */

/* Credential trials are reversible across the touchscreen, serial and PC paths.
 * Keep the previous credentials; test the new ones without persisting them;
 * commit only on success and explicit user confirmation. A failed test,
 * cancellation or reboot must never discard known working credentials. */
bool credentials_trial_active(void);
void credentials_trial_begin(void);  /* photograph current accesses; no effect if already in progress */
esp_err_t credentials_trial_set_wifi(const char *ssid, const char *password);
esp_err_t credentials_trial_set_dexcom(const char *user, const char *password, const char *region);
esp_err_t credentials_trial_commit(void);  /* record the test (after success and confirmation) */
void credentials_trial_cancel(void);       /* Restore previous trial snapshot, including in-memory values. */

/* Mode of demonstration: Simulated data, no account. Remember from start to start. */
bool credentials_demo(void);
esp_err_t credentials_set_demo(bool on);

/* Only masked account names and status strings may be displayed;
 * never expose the Wi-Fi or Dexcom passwords. */
void credentials_masked_wifi(char *buf, size_t size);
void credentials_masked_dexcom(char *buf, size_t size);
void credentials_masked_password(char *buf, size_t size, bool wifi);

/* Dexcom Share regions: ous (Europe), us (North America), jp (Japan). */
const char *credentials_region_code(int index);
const char *credentials_region_label(int index);
int credentials_region_count(void);
int credentials_region_index(const char *code);

void credentials_print(void); /* console: state mask, never secret */
