/* No-hardware replacement for firmware logging.
 * Used exclusively by hardware-independent regression tests.
 */
/* Neutralized logging for PC tests (define TEST_LOGS=1 to see the messages). */
/* Suppress device logging dependencies in the host test build.
 */
#pragma once
#include <stdio.h>

#ifndef TEST_LOGS
#define TEST_LOGS 0
#endif

#if TEST_LOGS
#define TEST_LOG(tag, fmt, ...) ((void)fprintf(stderr, "[%s] " fmt "\n", tag, ##__VA_ARGS__))
#else
#define TEST_LOG(tag, fmt, ...) ((void)0)
#endif

#define ESP_LOGE(tag, fmt, ...) TEST_LOG(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) TEST_LOG(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) TEST_LOG(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) TEST_LOG(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) TEST_LOG(tag, fmt, ##__VA_ARGS__)
