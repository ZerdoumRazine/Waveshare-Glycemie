/*
 * app_state.c: Shared application snapshot and live connection state.
 * Design note: Consumers should see coherent data snapshots.
 */

/*
 * Protects shared connection status and the glucose history with a mutex.
 * Protects connection states and shared history.
 */
#include "app_state.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

static app_state_t s_state;
static SemaphoreHandle_t s_mutex;

static glucose_point_t *s_history; /* PSRAM : la RAM interne est reservee aux tampons DMA */
static int s_history_count;
static uint32_t s_history_seq;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

/* Allocate shared state and history before background tasks begin publishing updates. */
void app_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.dexcom = DEX_STATE_WAITING_NETWORK;
    strlcpy(s_state.coproc_fw, "?", sizeof(s_state.coproc_fw));
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);
    s_history = heap_caps_calloc(GLY_HISTORY_MAX, sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    configASSERT(s_history);
}

/* Return a consistent snapshot for the UI instead of exposing mutable state. */
void app_state_get(app_state_t *out)
{
    LOCK();
    memcpy(out, &s_state, sizeof(*out));
    UNLOCK();
}

void app_state_set_wifi(link_state_t st, int8_t rssi, const char *ip)
{
    LOCK();
    s_state.wifi = st;
    s_state.rssi = rssi;
    strlcpy(s_state.ip, ip ? ip : "", sizeof(s_state.ip));
    if (st != LINK_OK) {
        s_state.internet = (s_state.internet == LINK_OK) ? LINK_PENDING : s_state.internet;
    }
    UNLOCK();
}

void app_state_set_wifi_configured(bool configured)
{
    LOCK();
    s_state.wifi_configured = configured;
    UNLOCK();
}

void app_state_set_rssi(int8_t rssi)
{
    LOCK();
    s_state.rssi = rssi;
    UNLOCK();
}

void app_state_set_internet(link_state_t st)
{
    LOCK();
    s_state.internet = st;
    UNLOCK();
}

void app_state_set_time(link_state_t st)
{
    LOCK();
    s_state.time_sync = st;
    UNLOCK();
}

void app_state_set_coproc_fw(const char *version)
{
    LOCK();
    strlcpy(s_state.coproc_fw, version ? version : "?", sizeof(s_state.coproc_fw));
    UNLOCK();
}

/* Record connectivity separately from the last glucose value. */
void app_state_set_dexcom(dexcom_state_t st, int32_t retry_s)
{
    LOCK();
    s_state.dexcom = st;
    s_state.dexcom_retry_s = retry_s;
    UNLOCK();
}

void app_state_set_dexcom_info(const char *info)
{
    LOCK();
    strlcpy(s_state.dexcom_info, info ? info : "", sizeof(s_state.dexcom_info));
    UNLOCK();
}

void app_state_set_demo(bool demo)
{
    LOCK();
    s_state.demo_mode = demo;
    UNLOCK();
}

void app_state_set_sd(sd_state_t st, const char *type, uint32_t capacity_mb, uint32_t free_mb)
{
    LOCK();
    s_state.sd = st;
    strlcpy(s_state.sd_type, type ? type : "", sizeof(s_state.sd_type));
    s_state.sd_capacity_mb = capacity_mb;
    s_state.sd_free_mb = free_mb;
    UNLOCK();
}

void app_state_add_sd_rows(uint32_t rows)
{
    LOCK();
    s_state.sd_rows_written += rows;
    UNLOCK();
}

void app_state_set_audio_ok(bool ok)
{
    LOCK();
    s_state.audio_ok = ok;
    UNLOCK();
}

void app_history_set(const glucose_point_t *points, int count)
{
    if (count < 0 || points == NULL) {
        count = 0;
    }
    if (count > GLY_HISTORY_MAX) {
        count = GLY_HISTORY_MAX;
    }
    LOCK();
    if (count > 0) {
        memcpy(s_history, points, (size_t)count * sizeof(glucose_point_t));
    }
    s_history_count = count;
    s_history_seq++;
    UNLOCK();
}

int app_history_get(glucose_point_t *out, int max, uint32_t *seq)
{
    LOCK();
    int n = s_history_count < max ? s_history_count : max;
    if (n > 0 && out != NULL) {
        memcpy(out, s_history, (size_t)n * sizeof(glucose_point_t));
    }
    if (seq != NULL) {
        *seq = s_history_seq;
    }
    UNLOCK();
    return n;
}

uint32_t app_history_seq(void)
{
    LOCK();
    uint32_t seq = s_history_seq;
    UNLOCK();
    return seq;
}

const char *trend_name(uint8_t trend)
{
    static const char *const names[TREND_COUNT] = {
        "None", "DoubleUp", "SingleUp", "FortyFiveUp", "Flat",
        "FortyFiveDown", "SingleDown", "DoubleDown", "NotComputable", "RateOutOfRange",
    };
    return trend < TREND_COUNT ? names[trend] : "?";
}
