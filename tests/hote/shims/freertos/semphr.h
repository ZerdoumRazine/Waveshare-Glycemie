/* Host substitutes for FreeRTOS synchronization.
 * FR: Substituts de synchronisation FreeRTOS.
 * Used exclusively by hardware-independent regression tests.
 */
/* Mutex sans effet : les tests sont mono-tache. */
/* EN: Simulate the mutex interface without real scheduling.
 * FR: Simule les mutex sans ordonnanceur matériel.
 */
#pragma once
#include "freertos/FreeRTOS.h"

typedef void *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks);
int xSemaphoreGive(SemaphoreHandle_t s);
