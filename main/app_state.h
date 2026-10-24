/* app_state.h: Application-wide glucose and connection snapshots.
 * Keep this header focused on the contracts used by other modules.
 */
/*
 * State sharing between the spots (network, Dexcom, storage, audio, alerts) and the interface.
 * The spots publish under mutex; the interface reads one copy once a second.
 * The history (up to 24 h) is stored separately and is only copied when it changes.
 */
/* Public interface / Interface publique
 * Thread-safe snapshots of link state, glucose data and history.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

typedef enum {
    LINK_UNKNOWN = 0,
    LINK_PENDING,
    LINK_OK,
    LINK_ERROR,
} link_state_t;

typedef enum {
    DEX_STATE_WAITING_NETWORK = 0,
    DEX_STATE_NO_CONFIG,
    DEX_STATE_CONNECTING,
    DEX_STATE_OK,
    DEX_STATE_NO_DATA,
    DEX_STATE_RETRYING,     /* Network or server error: retry according to the connection policy. */
    DEX_STATE_AUTH_REFUSED, /* identifiants refuses : tentatives arretees */
    DEX_STATE_LOCKED,       /* compte temporairement bloque par Dexcom */
    DEX_STATE_AWAITING_ARM, /* Never valid identifiers: single attempt pending authorization */
    DEX_STATE_AUTH_BLOCKED, /* unique attempt already consumed without verdict: verification required */
} dexcom_state_t;

typedef enum {
    SD_STATE_UNKNOWN = 0,   /* not yet initialized */
    SD_STATE_ABSENT,        /* no detecte map */
    SD_STATE_OK,            /* montee, ecriture possible */
    SD_STATE_UNSUPPORTED,   /* Unrecognized file system: no automatic formatting */
    SD_STATE_ERROR,         /* Authentication error: preserve normal local display operation. */
} sd_state_t;

/* Codes de tendance Dexcom Share (ordre de pydexcom DEXCOM_TREND_DIRECTIONS) */
typedef enum {
    TREND_NONE = 0,
    TREND_DOUBLE_UP,
    TREND_SINGLE_UP,
    TREND_FORTY_FIVE_UP,
    TREND_FLAT,
    TREND_FORTY_FIVE_DOWN,
    TREND_SINGLE_DOWN,
    TREND_DOUBLE_DOWN,
    TREND_NOT_COMPUTABLE,
    TREND_RATE_OUT_OF_RANGE,
    TREND_COUNT,
} trend_t;

typedef struct {
    int64_t wt_ms;   /* time stamping UTC Measurement in ms (WT field) */
    int16_t mgdl;
    uint8_t trend;   /* trend_t */
} glucose_point_t;

typedef struct {
    link_state_t wifi;
    link_state_t internet;
    link_state_t time_sync;
    bool wifi_configured;
    int8_t rssi;
    char ip[16];
    char coproc_fw[24];

    dexcom_state_t dexcom;
    int32_t dexcom_retry_s;
    char dexcom_info[128];      /* diagnostic non sensible (etape, HTTP, code Dexcom) */
    bool demo_mode;             /* simule data (development console only) */

    sd_state_t sd;
    char sd_type[12];
    uint32_t sd_capacity_mb;
    uint32_t sd_free_mb;
    uint32_t sd_rows_written;

    bool audio_ok;
} app_state_t;

void app_state_init(void);
void app_state_get(app_state_t *out);

void app_state_set_wifi(link_state_t st, int8_t rssi, const char *ip);
void app_state_set_wifi_configured(bool configured);
void app_state_set_rssi(int8_t rssi);
void app_state_set_internet(link_state_t st);
void app_state_set_time(link_state_t st);
void app_state_set_coproc_fw(const char *version);
void app_state_set_dexcom(dexcom_state_t st, int32_t retry_s);
void app_state_set_dexcom_info(const char *info);
void app_state_set_demo(bool demo);
void app_state_set_sd(sd_state_t st, const char *type, uint32_t capacity_mb, uint32_t free_mb);
void app_state_add_sd_rows(uint32_t rows);
void app_state_set_audio_ok(bool ok);

/* History: measures sorted from the most recent to the oldest */
void app_history_set(const glucose_point_t *points, int count);
int app_history_get(glucose_point_t *out, int max, uint32_t *seq);
uint32_t app_history_seq(void);

const char *trend_name(uint8_t trend);
