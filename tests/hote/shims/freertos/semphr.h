/* Host substitutes for FreeRTOS synchronization.
 * Used exclusively by hardware-independent regression tests.
 */
/* Mutex has no effect: the tests are mono-tache. */
/* Simulate the mutex interface without real scheduling.
 */
#pragma once
#include "freertos/FreeRTOS.h"

typedef void *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks);
int xSemaphoreGive(SemaphoreHandle_t s);
