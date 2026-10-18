/* Remplacant minimal de FreeRTOS pour les tests sur PC (execution mono-tache). */
#pragma once
#include <stdint.h>

typedef uint32_t TickType_t;
#define portMAX_DELAY ((TickType_t)0xFFFFFFFFu)
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

#include <assert.h>
#define configASSERT(x) assert(x)
