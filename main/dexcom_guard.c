/*
 * dexcom_guard.c: Keeps a persistent guard against repeated Dexcom authentication failures.
 * FR: Évite les tentatives d'authentification Dexcom répétées.
 * Design decision: Write the attempt state before making network calls to prevent restart loops.
 *
 * Experimental supplementary display; official monitoring remains authoritative.
 */

/*
 * Persists one authentication outcome per account to prevent repeated rejected logins.
 * Mémorise le résultat de connexion pour éviter de bloquer le compte Dexcom.
 */
#include "dexcom_guard.h"

#include <string.h>
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

static const char *TAG = "dexcom_guard";

#define GUARD_NAMESPACE "dexcom"
#define KEY_CRED        "cred"
#define KEY_STATE       "state"
#define KEY_INFO        "info"
#define CRED_TAG_LEN    8

static uint8_t s_cred_tag[CRED_TAG_LEN];
static dex_guard_state_t s_state = DEX_GUARD_UNKNOWN;
static char s_info[128];

/* Use an account fingerprint to match guard state without storing the password in this record. */
static void compute_tag(const char *region, const char *username, const char *password, uint8_t out[CRED_TAG_LEN])
{
    uint8_t digest[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    static const char domain[] = "glycemie_display/dexcom_guard/v1";
    mbedtls_sha256_update(&ctx, (const unsigned char *)domain, sizeof(domain));
    mbedtls_sha256_update(&ctx, (const unsigned char *)region, strlen(region) + 1);
    mbedtls_sha256_update(&ctx, (const unsigned char *)username, strlen(username) + 1);
    mbedtls_sha256_update(&ctx, (const unsigned char *)password, strlen(password) + 1);
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    memcpy(out, digest, CRED_TAG_LEN);
    memset(digest, 0, sizeof(digest));
}

/* Recover the prior state only when the selected account has the same fingerprint. */
esp_err_t dex_guard_init(const char *region, const char *username, const char *password)
{
    compute_tag(region, username, password, s_cred_tag);
    s_state = DEX_GUARD_UNKNOWN;
    s_info[0] = '\0';

    nvs_handle_t h;
    esp_err_t err = nvs_open(GUARD_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK; /* jamais ecrit : aucune tentative passee */
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lecture NVS impossible (%s)", esp_err_to_name(err));
        return err;
    }

    uint8_t stored[CRED_TAG_LEN];
    size_t len = sizeof(stored);
    uint8_t state = 0;
    if (nvs_get_blob(h, KEY_CRED, stored, &len) == ESP_OK && len == CRED_TAG_LEN &&
        memcmp(stored, s_cred_tag, CRED_TAG_LEN) == 0 && nvs_get_u8(h, KEY_STATE, &state) == ESP_OK &&
        state <= DEX_GUARD_INCONCLUSIVE) {
        s_state = (dex_guard_state_t)state;
        size_t info_len = sizeof(s_info);
        if (nvs_get_str(h, KEY_INFO, s_info, &info_len) != ESP_OK) {
            s_info[0] = '\0';
        }
    }
    nvs_close(h);
    return ESP_OK;
}

dex_guard_state_t dex_guard_state(void)
{
    return s_state;
}

const char *dex_guard_info(void)
{
    return s_info;
}

/* Commit the guard state before subsequent network attempts are allowed. */
esp_err_t dex_guard_set(dex_guard_state_t state, const char *info)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(GUARD_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, KEY_CRED, s_cred_tag, CRED_TAG_LEN);
        if (err == ESP_OK) {
            err = nvs_set_u8(h, KEY_STATE, (uint8_t)state);
        }
        if (err == ESP_OK) {
            err = nvs_set_str(h, KEY_INFO, info ? info : "");
        }
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enregistrement du verrou impossible (%s)", esp_err_to_name(err));
        return err;
    }
    s_state = state;
    strlcpy(s_info, info ? info : "", sizeof(s_info));
    ESP_LOGI(TAG, "Verrou d'authentification Dexcom : %s", dex_guard_state_name(state));
    return ESP_OK;
}

const char *dex_guard_state_name(dex_guard_state_t state)
{
    switch (state) {
    case DEX_GUARD_UNKNOWN: return "INCONNU (aucune tentative)";
    case DEX_GUARD_IN_PROGRESS: return "TENTATIVE EN COURS";
    case DEX_GUARD_VALIDATED: return "VALIDE";
    case DEX_GUARD_REFUSED: return "REFUSE";
    case DEX_GUARD_INCONCLUSIVE: return "NON CONCLUANT";
    default: return "?";
    }
}
