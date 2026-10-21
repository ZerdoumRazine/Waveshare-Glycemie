/* Journalisation neutralisee pour les tests sur PC (definir TEST_LOGS=1 pour voir les messages). */
/* EN: Suppress device logging dependencies in the host test build.
 * FR: Remplace les journaux ESP-IDF pendant les tests.
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
