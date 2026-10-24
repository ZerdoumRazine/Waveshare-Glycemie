/* Minimal FreeRTOS types for host compilation.
 * Used exclusively by hardware-independent regression tests.
 */
/* Minimum FreeRTO filler for PC tests (mono-tache execution). */
/* Minimal FreeRTOS types needed when running C tests on a PC.
 */
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
