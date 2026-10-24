/*
 * credentials.c: Wi-Fi/Dexcom settings, trial changes and persistence.
 * Design note: Do not overwrite working credentials until the replacement passes verification.
 */

/*
 * Manages saved Wi-Fi/Dexcom access separately from unconfirmed trial settings.
 * Separate registered identifiers from new unconfirmed accesses.
 * This is a supplementary display: official Dexcom alarms remain authoritative.
 */
#include "credentials.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "app_secrets.h"

static const char *TAG = "identifiants";

#define NVS_NS      "ident"
#define NVS_KEY     "v1"
#define NVS_KEY_DEMO "demo"

static SemaphoreHandle_t s_mutex;
static credentials_t s_cred;
static bool s_from_build;
static bool s_demo;

/* Test in progress: copy of previous accesses, return if the test is cancelled or failed.
 * s_cred then contains the values tested, but the permanent memory keeps the old ones. */
static credentials_t s_avant;
static bool s_avant_from_build;
static bool s_essai;

static const struct {
    const char *code;
    const char *label;
} k_regions[] = {
    {"ous", "Europe / hors Amérique"},
    {"us", "Amérique du Nord"},
    {"jp", "Japon"},
};
#define REGION_COUNT (int)(sizeof(k_regions) / sizeof(k_regions[0]))

/* Interference: OR exclusive with a key drive of the address MAC. It's not encryption.
 * (see credentials.h): it only avoids reading a plain password in a flash dump. */
/* Obfuscate NVS bytes, but do not mistake this reversible step for encryption. */
static void scramble(uint8_t *data, size_t len)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    for (size_t i = 0; i < len; i++) {
        data[i] ^= (uint8_t)(mac[i % sizeof(mac)] + (uint8_t)(i * 31u) + 0x5Au);
    }
}

/* Commit a copy of the active settings to NVS; callers need the real error code. */
static esp_err_t persist(void)
{
    credentials_t copy;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    copy = s_cred;
    const bool demo = s_demo;
    xSemaphoreGive(s_mutex);

    scramble((uint8_t *)&copy, sizeof(copy));
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS indisponible (%s) : identifiants non enregistres", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(h, NVS_KEY, &copy, sizeof(copy));
    if (err == ESP_OK) {
        err = nvs_set_u8(h, NVS_KEY_DEMO, demo ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    memset(&copy, 0, sizeof(copy));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enregistrement impossible (%s)", esp_err_to_name(err));
    }
    return err;
}

/* Restore saved settings without reapplying obsolete defaults when a valid record exists. */
/* Load the saved configuration first; fall back to optional build defaults only when no saved block exists.
 */
esp_err_t credentials_init(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memset(&s_cred, 0, sizeof(s_cred));
    strlcpy(s_cred.dexcom_region, "ous", sizeof(s_cred.dexcom_region));
    memset(&s_avant, 0, sizeof(s_avant));
    s_essai = false; /* an equivalent restart'annulation d'one test not confirmed */

    nvs_handle_t h;
    bool lu = false;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        credentials_t stored;
        size_t size = sizeof(stored);
        if (nvs_get_blob(h, NVS_KEY, &stored, &size) == ESP_OK && size == sizeof(stored)) {
            scramble((uint8_t *)&stored, sizeof(stored));
            /* guaranteed chain ends: a NVS abime shall not cause reading outside the area */
            stored.wifi_ssid[sizeof(stored.wifi_ssid) - 1] = '\0';
            stored.wifi_password[sizeof(stored.wifi_password) - 1] = '\0';
            stored.dexcom_user[sizeof(stored.dexcom_user) - 1] = '\0';
            stored.dexcom_password[sizeof(stored.dexcom_password) - 1] = '\0';
            stored.dexcom_region[sizeof(stored.dexcom_region) - 1] = '\0';
            if (credentials_region_index(stored.dexcom_region) < 0) {
                strlcpy(stored.dexcom_region, "ous", sizeof(stored.dexcom_region));
            }
            s_cred = stored;
            lu = true;
            memset(&stored, 0, sizeof(stored));
        }
        uint8_t demo = 0;
        if (nvs_get_u8(h, NVS_KEY_DEMO, &demo) == ESP_OK) {
            s_demo = demo != 0;
        }
        nvs_close(h);
    }

    if (!lu) {
        /* First start, or device already configures to the compilation (original device) */
        strlcpy(s_cred.wifi_ssid, GLY_WIFI_SSID, sizeof(s_cred.wifi_ssid));
        strlcpy(s_cred.wifi_password, GLY_WIFI_PASSWORD, sizeof(s_cred.wifi_password));
        strlcpy(s_cred.dexcom_user, GLY_DEXCOM_USERNAME, sizeof(s_cred.dexcom_user));
        strlcpy(s_cred.dexcom_password, GLY_DEXCOM_PASSWORD, sizeof(s_cred.dexcom_password));
        strlcpy(s_cred.dexcom_region, GLY_DEXCOM_REGION, sizeof(s_cred.dexcom_region));
        s_from_build = s_cred.wifi_ssid[0] != '\0' || s_cred.dexcom_user[0] != '\0';
    }
    ESP_LOGI(TAG, "Identifiants : Wi-Fi %s, Dexcom %s (%s)%s",
             credentials_has_wifi() ? "renseigne" : "a saisir",
             credentials_has_dexcom() ? "renseigne" : "a saisir",
             s_cred.dexcom_region, s_from_build ? ", herites de la compilation" : "");
    return ESP_OK;
}

void credentials_get(credentials_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_cred;
    xSemaphoreGive(s_mutex);
}

bool credentials_has_wifi(void)
{
    return s_cred.wifi_ssid[0] != '\0';
}

bool credentials_has_dexcom(void)
{
    return s_cred.dexcom_user[0] != '\0' && s_cred.dexcom_password[0] != '\0';
}

bool credentials_configured(void)
{
    return credentials_has_wifi() && credentials_has_dexcom();
}

bool credentials_from_build(void)
{
    return s_from_build;
}

esp_err_t credentials_set_wifi(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL || strlen(ssid) >= CRED_SSID_MAX ||
            strlen(password) >= CRED_PASS_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strlcpy(s_cred.wifi_ssid, ssid, sizeof(s_cred.wifi_ssid));
    strlcpy(s_cred.wifi_password, password, sizeof(s_cred.wifi_password));
    s_from_build = false;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Reseau Wi-Fi enregistre (%d caracteres de mot de passe)", (int)strlen(password));
    return persist();
}

esp_err_t credentials_set_dexcom(const char *user, const char *password, const char *region)
{
    if (user == NULL || password == NULL || region == NULL || strlen(user) >= CRED_USER_MAX ||
            strlen(password) >= CRED_PASS_MAX || credentials_region_index(region) < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strlcpy(s_cred.dexcom_user, user, sizeof(s_cred.dexcom_user));
    strlcpy(s_cred.dexcom_password, password, sizeof(s_cred.dexcom_password));
    strlcpy(s_cred.dexcom_region, region, sizeof(s_cred.dexcom_region));
    s_from_build = false;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Compte Dexcom enregistre (region %s)", region);
    return persist();
}

/* ---- Essai de nouveaux acces (voir credentials.h) ----------------------------------------- */

bool credentials_trial_active(void)
{
    return s_essai;
}

void credentials_trial_begin(void)
{
    if (s_essai) {
        return; /* a trial is already underway: the original accesses are already rated */
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_avant = s_cred;
    s_avant_from_build = s_from_build;
    s_essai = true;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Essai de nouveaux acces : les acces actuels sont conserves");
}

/* Test values: used to connect, never saved before confirmation. */
esp_err_t credentials_trial_set_wifi(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL || strlen(ssid) >= CRED_SSID_MAX ||
            strlen(password) >= CRED_PASS_MAX || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    credentials_trial_begin();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strlcpy(s_cred.wifi_ssid, ssid, sizeof(s_cred.wifi_ssid));
    strlcpy(s_cred.wifi_password, password, sizeof(s_cred.wifi_password));
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Reseau Wi-Fi a l'essai (%d caracteres de mot de passe)", (int)strlen(password));
    return ESP_OK;
}

esp_err_t credentials_trial_set_dexcom(const char *user, const char *password, const char *region)
{
    if (user == NULL || password == NULL || region == NULL || user[0] == '\0' ||
            strlen(user) >= CRED_USER_MAX || strlen(password) >= CRED_PASS_MAX ||
            credentials_region_index(region) < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    credentials_trial_begin();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strlcpy(s_cred.dexcom_user, user, sizeof(s_cred.dexcom_user));
    strlcpy(s_cred.dexcom_password, password, sizeof(s_cred.dexcom_password));
    strlcpy(s_cred.dexcom_region, region, sizeof(s_cred.dexcom_region));
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Compte Dexcom a l'essai (region %s)", region);
    return ESP_OK;
}

/* Replace saved credentials only after a successful connection test. */
esp_err_t credentials_trial_commit(void)
{
    if (!s_essai) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_from_build = false; /* these are now accesses seized by the user */
    xSemaphoreGive(s_mutex);
    const esp_err_t err = persist();
    if (err != ESP_OK) {
        /* Nothing could be written: we keep the values in memory (the connection in
         * and the test remains open to try again. */
        ESP_LOGE(TAG, "Enregistrement impossible : les acces precedents restent en memoire permanente");
        return err;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(&s_avant, 0, sizeof(s_avant));
    s_essai = false;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Nouveaux acces enregistres apres essai reussi");
    return ESP_OK;
}

void credentials_trial_cancel(void)
{
    if (!s_essai) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_cred = s_avant;              /* back to previous accesses, never replaced in flash */
    s_from_build = s_avant_from_build;
    memset(&s_avant, 0, sizeof(s_avant));
    s_essai = false;
    xSemaphoreGive(s_mutex);
    ESP_LOGW(TAG, "Essai annule : retour aux acces precedents");
}

/* Store an explicitly empty record before removing auxiliary account state. */
/* Persist an explicitly empty configuration before removing the old Dexcom guard state.
 */
esp_err_t credentials_clear(void)
{
    /* First delete storage identify NVS. Lock
     * Dexcom is in another space NVS : the two spaces cannot
     * not to be modified in an atomic manner. So it must never be
     * erase AVANT confirmation of the recording of a configuration
     * empty, otherwise allow attempts to connect with
     * old accesses after a mistake NVS. */
    esp_err_t err;

    /* Keep l'ancien etat en RAM tant que l'configuration writing
     * empty was not confirmed. Clearing the Key NVS would be insufficient:
     * on an old customised firmware, compile IDs would be
     * automatically resumed at the next start. */
    credentials_t precedent, essai_precedent;
    bool origine_precedente, demo_precedent, essai_actif_precedent, origine_essai_precedente;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    precedent = s_cred;
    essai_precedent = s_avant;
    origine_precedente = s_from_build;
    demo_precedent = s_demo;
    essai_actif_precedent = s_essai;
    origine_essai_precedente = s_avant_from_build;
    memset(&s_cred, 0, sizeof(s_cred));
    strlcpy(s_cred.dexcom_region, "ous", sizeof(s_cred.dexcom_region));
    s_from_build = false;
    s_demo = false;
    xSemaphoreGive(s_mutex);

    /* explicitly enter an empty block in NVS to prevent any return
     * implicit in identifiers embedded by an old compilation. */
    err = persist();
    if (err != ESP_OK) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_cred = precedent;
        s_avant = essai_precedent;
        s_from_build = origine_precedente;
        s_demo = demo_precedent;
        s_essai = essai_actif_precedent;
        s_avant_from_build = origine_essai_precedente;
        xSemaphoreGive(s_mutex);
        memset(&precedent, 0, sizeof(precedent));
        memset(&essai_precedent, 0, sizeof(essai_precedent));
        ESP_LOGE(TAG, "Identifiants non effaces : enregistrement NVS impossible (%s)", esp_err_to_name(err));
        return err;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(&s_avant, 0, sizeof(s_avant));
    s_avant_from_build = false;
    s_essai = false;
    xSemaphoreGive(s_mutex);
    memset(&precedent, 0, sizeof(precedent));
    memset(&essai_precedent, 0, sizeof(essai_precedent));

    /* Espace reel du verrou : "dexcom" (dexcom_guard.c), et non "dexguard".
     * Its erasure is secondary: it contains only a short print
     * of former identifiers and will not allow to reconstitute the
     * configuration qui vient d'etre retiree.
     * A breakdown here does not turn an effective erasure into a failure. */
    nvs_handle_t h;
    err = nvs_open("dexcom", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_erase_all(h);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Identifiants retires, mais nettoyage du verrou Dexcom impossible (%s)",
                 esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Identifiants retires de la configuration active et NVS vide enregistree");
    return ESP_OK;
}

bool credentials_demo(void)
{
    return s_demo;
}

esp_err_t credentials_set_demo(bool on)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_demo = on;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Mode demonstration %s", on ? "actif au demarrage" : "desactive");
    return persist();
}

void credentials_masked_wifi(char *buf, size_t size)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    snprintf(buf, size, "%s", s_cred.wifi_ssid[0] ? s_cred.wifi_ssid : "-");
    xSemaphoreGive(s_mutex);
}

/* "antoine.dupont@exemple.fr" -> "a***@exemple.fr" ; "utilisateur" -> "u***" */
void credentials_masked_dexcom(char *buf, size_t size)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const char *user = s_cred.dexcom_user;
    if (user[0] == '\0') {
        snprintf(buf, size, "-");
    } else {
        const char *at = strchr(user, '@');
        if (at != NULL) {
            snprintf(buf, size, "%c***%s", user[0], at);
        } else {
            snprintf(buf, size, "%c***", user[0]);
        }
    }
    xSemaphoreGive(s_mutex);
}

void credentials_masked_password(char *buf, size_t size, bool wifi)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const size_t len = strlen(wifi ? s_cred.wifi_password : s_cred.dexcom_password);
    xSemaphoreGive(s_mutex);
    if (len == 0) {
        snprintf(buf, size, "-");
    } else {
        snprintf(buf, size, "•••••••• (%d)", (int)len);
    }
}

const char *credentials_region_code(int index)
{
    return index >= 0 && index < REGION_COUNT ? k_regions[index].code : "ous";
}

const char *credentials_region_label(int index)
{
    return index >= 0 && index < REGION_COUNT ? k_regions[index].label : k_regions[0].label;
}

int credentials_region_count(void)
{
    return REGION_COUNT;
}

int credentials_region_index(const char *code)
{
    for (int i = 0; code != NULL && i < REGION_COUNT; i++) {
        if (strcmp(code, k_regions[i].code) == 0) {
            return i;
        }
    }
    return -1;
}

void credentials_print(void)
{
    char wifi[CRED_SSID_MAX + 8];
    char user[CRED_USER_MAX + 8];
    char mdp_wifi[32];
    char mdp_dex[32];
    credentials_masked_wifi(wifi, sizeof(wifi));
    credentials_masked_dexcom(user, sizeof(user));
    credentials_masked_password(mdp_wifi, sizeof(mdp_wifi), true);
    credentials_masked_password(mdp_dex, sizeof(mdp_dex), false);
    printf("IDENT wifi=\"%s\" mdp_wifi=%s dexcom=\"%s\" mdp_dexcom=%s region=%s configure=%d demo=%d "
           "origine=%s essai=%d\n",
           wifi, mdp_wifi, user, mdp_dex, s_cred.dexcom_region, credentials_configured() ? 1 : 0,
           credentials_demo() ? 1 : 0, s_from_build ? "compilation" : "saisie", s_essai ? 1 : 0);
}
