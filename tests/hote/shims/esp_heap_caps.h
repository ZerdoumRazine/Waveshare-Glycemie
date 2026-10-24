/* Host-memory replacements for ESP heap allocation.
 * Test and configuration interface only; no live patient data is included.
 */
/* Allocations: ordinary malloc on PC. */
/* Replace ESP32 special-purpose allocation with host memory.
 */
#pragma once
#include <stdlib.h>

#define MALLOC_CAP_8BIT    (1 << 2)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_SPIRAM  (1 << 10)
#define MALLOC_CAP_DMA     (1 << 3)

void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
