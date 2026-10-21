/* Implementation des remplacants : NVS en memoire, mutex neutres, allocations, noms d'erreurs. */
/* EN: Implement deterministic fake NVS, mutexes and error responses.
 * FR: Implémente les remplacements NVS et les erreurs simulées.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

const char *esp_err_to_name(esp_err_t code)
{
    switch (code) {
    case ESP_OK: return "ESP_OK";
    case ESP_FAIL: return "ESP_FAIL";
    case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
    case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_NVS_NOT_FOUND: return "ESP_ERR_NVS_NOT_FOUND";
    default: return "ESP_ERR_?";
    }
}

void *heap_caps_malloc(size_t size, uint32_t caps) { (void)caps; return malloc(size); }
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) { (void)caps; return calloc(n, size); }

size_t strlcpy(char *dst, const char *src, size_t size)
{
    const size_t len = strlen(src);
    if (size > 0) {
        const size_t n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = 0;
    }
    return len;
}

static int s_mutex_token;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &s_mutex_token; }
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s; (void)t; return pdTRUE; }
int xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }

/* ---- NVS simulee -------------------------------------------------------- */

#define NVS_MAX_ENTRIES 64
#define NVS_MAX_HANDLES 16

#define NVS_MAX_BLOB 320

typedef struct {
    char ns[20];
    char key[20];
    int is_u8;
    int is_blob;
    int64_t i64;
    uint8_t u8;
    uint8_t blob[NVS_MAX_BLOB];
    size_t blob_len;
    int used;
} nvs_entry_t;

static nvs_entry_t s_entries[NVS_MAX_ENTRIES];
static char s_handle_ns[NVS_MAX_HANDLES][20];
static int s_handle_used[NVS_MAX_HANDLES];
static int s_writes;
static char s_fail_next_open_ns[20]; /* injection de panne reservee aux tests */

void fake_nvs_reset(void)
{
    memset(s_entries, 0, sizeof(s_entries));
    memset(s_handle_used, 0, sizeof(s_handle_used));
    s_writes = 0;
    s_fail_next_open_ns[0] = '\0';
}

void fake_nvs_fail_next_open(const char *namespace_name)
{
    snprintf(s_fail_next_open_ns, sizeof(s_fail_next_open_ns), "%s", namespace_name);
}

int fake_nvs_write_count(void) { return s_writes; }

static nvs_entry_t *find(const char *ns, const char *key, int create)
{
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_entries[i].used && strcmp(s_entries[i].ns, ns) == 0 && strcmp(s_entries[i].key, key) == 0) {
            return &s_entries[i];
        }
    }
    if (!create) {
        return NULL;
    }
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (!s_entries[i].used) {
            s_entries[i].used = 1;
            snprintf(s_entries[i].ns, sizeof(s_entries[i].ns), "%s", ns);
            snprintf(s_entries[i].key, sizeof(s_entries[i].key), "%s", key);
            return &s_entries[i];
        }
    }
    return NULL;
}

esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *out)
{
    (void)mode;
    if (s_fail_next_open_ns[0] != '\0' && strcmp(s_fail_next_open_ns, ns) == 0) {
        s_fail_next_open_ns[0] = '\0';
        return ESP_FAIL;
    }
    for (int i = 1; i < NVS_MAX_HANDLES; i++) {
        if (!s_handle_used[i]) {
            s_handle_used[i] = 1;
            snprintf(s_handle_ns[i], sizeof(s_handle_ns[i]), "%s", ns);
            *out = (nvs_handle_t)i;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

void nvs_close(nvs_handle_t h) { if (h < NVS_MAX_HANDLES) { s_handle_used[h] = 0; } }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }

esp_err_t nvs_get_i64(nvs_handle_t h, const char *key, int64_t *out)
{
    nvs_entry_t *e = find(s_handle_ns[h], key, 0);
    if (e == NULL || e->is_u8) { return ESP_ERR_NVS_NOT_FOUND; }
    *out = e->i64;
    return ESP_OK;
}

esp_err_t nvs_set_i64(nvs_handle_t h, const char *key, int64_t value)
{
    nvs_entry_t *e = find(s_handle_ns[h], key, 1);
    if (e == NULL) { return ESP_ERR_NO_MEM; }
    e->is_u8 = 0; e->i64 = value; s_writes++;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *out)
{
    nvs_entry_t *e = find(s_handle_ns[h], key, 0);
    if (e == NULL || !e->is_u8) { return ESP_ERR_NVS_NOT_FOUND; }
    *out = e->u8;
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t value)
{
    nvs_entry_t *e = find(s_handle_ns[h], key, 1);
    if (e == NULL) { return ESP_ERR_NO_MEM; }
    e->is_u8 = 1; e->u8 = value; s_writes++;
    return ESP_OK;
}

esp_err_t nvs_get_i16(nvs_handle_t h, const char *key, int16_t *out)
{
    int64_t v = 0;
    esp_err_t err = nvs_get_i64(h, key, &v);
    if (err == ESP_OK) {
        *out = (int16_t)v;
    }
    return err;
}

esp_err_t nvs_set_i16(nvs_handle_t h, const char *key, int16_t value)
{
    return nvs_set_i64(h, key, (int64_t)value);
}

esp_err_t nvs_erase_key(nvs_handle_t h, const char *key)
{
    nvs_entry_t *e = find(s_handle_ns[h], key, 0);
    if (e == NULL) { return ESP_ERR_NVS_NOT_FOUND; }
    e->used = 0;
    return ESP_OK;
}

esp_err_t nvs_erase_all(nvs_handle_t h)
{
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_entries[i].used && strcmp(s_entries[i].ns, s_handle_ns[h]) == 0) {
            memset(&s_entries[i], 0, sizeof(s_entries[i]));
        }
    }
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size)
{
    nvs_entry_t *e = find(s_handle_ns[h], key, 0);
    if (e == NULL || !e->is_blob) { return ESP_ERR_NVS_NOT_FOUND; }
    if (out == NULL) { *size = e->blob_len; return ESP_OK; }
    if (*size < e->blob_len) { *size = e->blob_len; return ESP_ERR_NVS_INVALID_LENGTH; }
    memcpy(out, e->blob, e->blob_len);
    *size = e->blob_len;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t size)
{
    if (size > NVS_MAX_BLOB) { return ESP_ERR_NO_MEM; }
    nvs_entry_t *e = find(s_handle_ns[h], key, 1);
    if (e == NULL) { return ESP_ERR_NO_MEM; }
    e->is_u8 = 0; e->is_blob = 1;
    memcpy(e->blob, value, size);
    e->blob_len = size;
    s_writes++;
    return ESP_OK;
}

/* Contenu brut tel qu'il serait ecrit en flash : sert a verifier qu'aucun mot de passe n'y est en clair. */
const void *fake_nvs_blob(const char *ns, const char *key, size_t *len)
{
    for (int i = 0; i < NVS_MAX_ENTRIES; i++) {
        if (s_entries[i].used && s_entries[i].is_blob && strcmp(s_entries[i].ns, ns) == 0 &&
                strcmp(s_entries[i].key, key) == 0) {
            if (len != NULL) { *len = s_entries[i].blob_len; }
            return s_entries[i].blob;
        }
    }
    if (len != NULL) { *len = 0; }
    return NULL;
}

/* ---- Adresse MAC simulee (esp_mac.h) ------------------------------------ */

esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type)
{
    (void)type;
    static const uint8_t k_mac[6] = {0x3C, 0x84, 0x27, 0x11, 0x22, 0x33};
    if (mac == NULL) { return ESP_ERR_INVALID_ARG; }
    memcpy(mac, k_mac, sizeof(k_mac));
    return ESP_OK;
}
