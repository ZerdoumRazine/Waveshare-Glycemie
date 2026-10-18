/* Mutex sans effet : les tests sont mono-tache. */
#pragma once
#include "freertos/FreeRTOS.h"

typedef void *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks);
int xSemaphoreGive(SemaphoreHandle_t s);
