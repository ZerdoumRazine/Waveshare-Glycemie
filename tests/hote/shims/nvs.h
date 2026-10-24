/* Simulated NVS persistence and failure injection.
 * Used exclusively by hardware-independent regression tests.
 */
/* NVS Simulate in memory: allows to verify persistence and simulate a restart. */
/* In-memory NVS interface plus explicit fault injection for regression tests.
 */
#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY = 0, NVS_READWRITE = 1 } nvs_open_mode_t;

esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *out);
void nvs_close(nvs_handle_t h);
esp_err_t nvs_commit(nvs_handle_t h);
esp_err_t nvs_get_i64(nvs_handle_t h, const char *key, int64_t *out);
esp_err_t nvs_set_i64(nvs_handle_t h, const char *key, int64_t value);
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *out);
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t value);
esp_err_t nvs_get_i16(nvs_handle_t h, const char *key, int16_t *out);
esp_err_t nvs_set_i16(nvs_handle_t h, const char *key, int16_t value);
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key);
esp_err_t nvs_erase_all(nvs_handle_t h);
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t size);

/* Utilitaires reserves aux tests */
void fake_nvs_reset(void);        /* efface tout : carte neuve */
void fake_nvs_fail_next_open(const char *namespace_name); /* failure injection, once only */
int  fake_nvs_write_count(void);  /* nombre d'ecritures effectives */
const void *fake_nvs_blob(const char *ns, const char *key, size_t *len); /* octets bruts, tels qu'en flash */
