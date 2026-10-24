/* credentials.h: Contracts for Wi-Fi/Dexcom trials and NVS persistence.
 * The declarations below are shared with other firmware modules.
 */
/*
 * User IDs (Wi-Fi and Dexcom Share account) entered SUR L'APPAREIL.
 *
 * Purpose: No one needs to recompile the firmware to put their own accesses.
 * - Storage: space NVS "ident", separates settings and Dexcom lock.
 *   The block is blurred by an OR exclusive with a key drive of the address MAC de la carte :
 *   It's not PAS encryption (the Flash Encryption is not active on this project), only
 *   to avoid a password reading plain in a rough copy of the memory.
 * - Passwords are not JAMAIS Journals and posters: only masked shapes come out
 *   d'ici (credentials_masked_*).
 * - Compatibilite : si la NVS is empty and that firmware has been compiled with private/device_secrets.h,
 *   These values serve as departure (the original device continues to operate without entering anything).
 */
/* Public interface / Interface publique
 * Read, mask, trial, confirm and erase user credentials with explicit error results.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define CRED_SSID_MAX   33  /* 32 caracteres + fin de chaine (norme Wi-Fi) */
#define CRED_PASS_MAX   65  /* 64 caracteres + fin de chaine */
#define CRED_USER_MAX   65
#define CRED_REGION_MAX 4   /* "ous", "us", "jp" */

typedef struct {
    char wifi_ssid[CRED_SSID_MAX];
    char wifi_password[CRED_PASS_MAX];
    char dexcom_user[CRED_USER_MAX];
    char dexcom_password[CRED_PASS_MAX];
    char dexcom_region[CRED_REGION_MAX];
} credentials_t;

/* To call just after settings_init() */
esp_err_t credentials_init(void);

/* Complete copy: only for plug-in modules (Wi-Fi, Dexcom). Never show. */
void credentials_get(credentials_t *out);

bool credentials_has_wifi(void);
bool credentials_has_dexcom(void);
bool credentials_configured(void);   /* Wi-Fi ET Dexcom renseignes */
bool credentials_from_build(void);   /* valeurs heritees de private/device_secrets.h */

/* Record immediately (after a successful test, or for a console resumption). */
esp_err_t credentials_set_wifi(const char *ssid, const char *password);
esp_err_t credentials_set_dexcom(const char *user, const char *password, const char *region);
esp_err_t credentials_clear(void);   /* efface tout : NVS, memoire et verrou d'authentification */

/*
 * ESSAI DE NOUVEAUX ACCES, without losing the elders.
 *
 * Regle appliquee partout (ecran tactile, console, assistant PC) :
 *   keep old accesses -> try new -> save only after success
 *   ET confirmation.
 *
 * During a trial, the new values are used as a living memory (the connection test).
 * porte donc bien sur elles) mais RIEN is written in the permanent memory. A cancellation,
 * an error, a power outage or a restart therefore brings back the old accesses: there is no
 * no intermediate state or device would end up without previous access.
 */
bool credentials_trial_active(void);
void credentials_trial_begin(void);  /* photograph current accesses; no effect if already in progress */
esp_err_t credentials_trial_set_wifi(const char *ssid, const char *password);
esp_err_t credentials_trial_set_dexcom(const char *user, const char *password, const char *region);
esp_err_t credentials_trial_commit(void);  /* record the test (after success and confirmation) */
void credentials_trial_cancel(void);       /* revient aux acces precedents, y compris en memoire */

/* Mode of demonstration: Simulated data, no account. Remember from start to start. */
bool credentials_demo(void);
esp_err_t credentials_set_demo(bool on);

/* Masked shapes for screen and console (never password):
 * "MonReseau", "a***@exemple.fr", "renseigne (12 caracteres)" / "non renseigne". */
void credentials_masked_wifi(char *buf, size_t size);
void credentials_masked_dexcom(char *buf, size_t size);
void credentials_masked_password(char *buf, size_t size, bool wifi);

/* Regions acceptees par Dexcom Share (indice 0..2) : "ous" Europe, "us" Amerique du Nord, "jp" Japon */
const char *credentials_region_code(int index);
const char *credentials_region_label(int index);
int credentials_region_count(void);
int credentials_region_index(const char *code);

void credentials_print(void); /* console: state mask, never secret */
