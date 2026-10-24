/* Mock ESP-IDF error codes used by host tests.
 * Test and configuration interface only; no live patient data is included.
 */
/* Remplacant minimal d'esp_err.h for PC tests (no ESP-IDF requis). */
/* Subset of ESP-IDF error codes required by isolated tests.
 */
#pragma once
#include <stdint.h>

#ifdef _MSC_VER
#define __attribute__(x)
#endif

typedef int esp_err_t;

#define ESP_OK                  0
#define ESP_FAIL               -1
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_NVS_NOT_FOUND   0x1102
#define ESP_ERR_NVS_INVALID_LENGTH 0x110A

const char *esp_err_to_name(esp_err_t code);
