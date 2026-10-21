/* Minimal FreeRTOS types for host compilation.
 * FR: Types FreeRTOS minimum pour la compilation PC.
 * Used exclusively by hardware-independent regression tests.
 */
/* Remplacant minimal de FreeRTOS pour les tests sur PC (execution mono-tache). */
/* EN: Minimal FreeRTOS types needed when running C tests on a PC.
 * FR: Types FreeRTOS réduits pour les tests PC.
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
