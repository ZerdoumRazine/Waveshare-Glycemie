/*
 * stock.c: Consumable stock counting and one-time decrements.
 * Design note: Repeated UI events must not consume the same item twice.
 */

/*
 * Tracks local stock levels and signals low quantities without creating negative values.
 * Follows consumables in stock and warns when they become rare.
 */
#include "stock.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "net_services.h"
#include "nvs.h"
#include "sd_storage.h"

static const char *TAG = "stock";

#define NVS_NS        "stock"
#define EVENT_MAX     4 /* file d'evenements STOCK_LOW (oldest are lost) */

static const char *const k_key_qty[STOCK_COUNT] = {"q_g7", "q_cath"};
static const char *const k_key_low[STOCK_COUNT] = {"s_g7", "s_cath"};
static const char *const k_key_change[STOCK_COUNT] = {"t_g7", "t_cath"};
static const int k_low_default[STOCK_COUNT] = {2, 3};

static SemaphoreHandle_t s_mutex;
static int s_qty[STOCK_COUNT] = {STOCK_UNKNOWN, STOCK_UNKNOWN};
static int s_low[STOCK_COUNT];
static int64_t s_change[STOCK_COUNT]; /* dernier changement deja decompte */
static volatile uint32_t s_revision;
static stock_event_t s_events[EVENT_MAX];
static int s_event_count;

/* Keep a quantity in the representable range even when an input is unexpected. */
static int clamp_qty(int value)
{
    if (value <= STOCK_UNKNOWN) {
        return STOCK_UNKNOWN;
    }
    return value > STOCK_MAX ? STOCK_MAX : value;
}

/* Verrou pris */
/* Persist the item that changed without resetting other stock counters. */
static esp_err_t persist(stock_item_t item)
{
    s_revision++;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_i16(h, k_key_qty[item], (int16_t)s_qty[item]);
        if (err == ESP_OK) {
            err = nvs_set_i16(h, k_key_low[item], (int16_t)s_low[item]);
        }
        if (err == ESP_OK) {
            err = nvs_set_i64(h, k_key_change[item], s_change[item]);
        }
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enregistrement NVS du stock impossible (%s)", esp_err_to_name(err));
    }
    return err;
}

/* Lock taken: local event if the stock information has just passed to low level */
/* Raise a stock reminder separately from medical glucose alerts. */
static void push_low_event(stock_item_t item)
{
    if (s_qty[item] == STOCK_UNKNOWN || s_qty[item] > s_low[item]) {
        return;
    }
    stock_event_t ev = {
        .item = item,
        .remaining = s_qty[item],
        .threshold = s_low[item],
        .time_s = net_time_is_valid() ? (int64_t)time(NULL) : 0,
    };
    if (s_event_count == EVENT_MAX) {
        memmove(&s_events[0], &s_events[1], sizeof(s_events[0]) * (EVENT_MAX - 1));
        s_event_count--;
    }
    s_events[s_event_count++] = ev;
}

esp_err_t stock_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    nvs_handle_t h;
    const bool open = nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK;
    for (int i = 0; i < STOCK_COUNT; i++) {
        int16_t qty = STOCK_UNKNOWN;
        int16_t low = (int16_t)k_low_default[i];
        int64_t change = 0;
        if (open) {
            if (nvs_get_i16(h, k_key_qty[i], &qty) != ESP_OK) {
                qty = STOCK_UNKNOWN; /* first start of this version: not provided */
            }
            if (nvs_get_i16(h, k_key_low[i], &low) != ESP_OK || low < 0 || low > STOCK_MAX) {
                low = (int16_t)k_low_default[i];
            }
            if (nvs_get_i64(h, k_key_change[i], &change) != ESP_OK) {
                change = 0;
            }
        }
        s_qty[i] = clamp_qty(qty);
        s_low[i] = low;
        s_change[i] = change;
    }
    if (open) {
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Stock : capteurs G7 %d (seuil %d), catheters %d (seuil %d) [-1 = non renseigne]",
             s_qty[STOCK_SENSOR], s_low[STOCK_SENSOR], s_qty[STOCK_CATHETER], s_low[STOCK_CATHETER]);
    return ESP_OK;
}

const char *stock_name(stock_item_t item)
{
    return item == STOCK_SENSOR ? "capteurs G7" : "cathéters";
}

int stock_get(stock_item_t item)
{
    if (item >= STOCK_COUNT || s_mutex == NULL) {
        return STOCK_UNKNOWN;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int value = s_qty[item];
    xSemaphoreGive(s_mutex);
    return value;
}

esp_err_t stock_set(stock_item_t item, int value)
{
    if (item >= STOCK_COUNT || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    value = clamp_qty(value);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    const bool changed = s_qty[item] != value;
    if (changed) {
        s_qty[item] = value;
        push_low_event(item);
        err = persist(item);
    }
    xSemaphoreGive(s_mutex);
    if (changed && value == STOCK_UNKNOWN) {
        ESP_LOGI(TAG, "Stock %s : non renseigne", stock_name(item));
        sd_storage_log_event("Stock %s : non renseigne", stock_name(item));
    } else if (changed) {
        ESP_LOGI(TAG, "Stock %s : %d", stock_name(item), value);
        sd_storage_log_event("Stock %s : %d", stock_name(item), value);
    }
    return err;
}

esp_err_t stock_add(stock_item_t item, int delta)
{
    if (item >= STOCK_COUNT || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int current = s_qty[item];
    xSemaphoreGive(s_mutex);
    if (current == STOCK_UNKNOWN) {
        return ESP_ERR_INVALID_STATE; /* Nothing to increment until the stock is clear */
    }
    int value = current + delta;
    if (value < 0) {
        value = 0; /* Never negative */
    }
    return stock_set(item, value);
}

bool stock_consume(stock_item_t item, int64_t change_s)
{
    if (item >= STOCK_COUNT || s_mutex == NULL) {
        return false;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int before = s_qty[item];
    const bool already = change_s > 0 && s_change[item] == change_s;
    bool consumed = false;
    if (!already) {
        s_change[item] = change_s; /* marker: this change is decounting (or not accounting) */
        if (before > 0) {
            s_qty[item] = before - 1;
            consumed = true;
            push_low_event(item);
        }
        persist(item);
    }
    const int after = s_qty[item];
    const int low = s_low[item];
    xSemaphoreGive(s_mutex);

    if (already) {
        ESP_LOGI(TAG, "Stock %s inchange : changement %" PRId64 " deja decompte", stock_name(item), change_s);
        return false;
    }
    if (consumed) {
        ESP_LOGI(TAG, "Stock %s : %d -> %d", stock_name(item), before, after);
        sd_storage_log_event("Stock %s : %d restant(s)", stock_name(item), after);
        if (after <= low) {
            ESP_LOGW(TAG, "Stock %s faible : %d restant(s) (seuil %d)", stock_name(item), after, low);
            sd_storage_log_event("Stock %s faible : %d restant(s)", stock_name(item), after);
        }
    } else if (before == 0) {
        ESP_LOGW(TAG, "Stock %s deja a zero : rien a decompter", stock_name(item));
        sd_storage_log_event("Stock %s deja a zero", stock_name(item));
    } else {
        ESP_LOGI(TAG, "Stock %s non renseigne : aucune decrementation", stock_name(item));
    }
    return consumed;
}

int stock_low_threshold(stock_item_t item)
{
    if (item >= STOCK_COUNT || s_mutex == NULL) {
        return 0;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int value = s_low[item];
    xSemaphoreGive(s_mutex);
    return value;
}

esp_err_t stock_set_low_threshold(stock_item_t item, int value)
{
    if (item >= STOCK_COUNT || s_mutex == NULL || value < 0 || value > STOCK_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    if (s_low[item] != value) {
        s_low[item] = value;
        push_low_event(item);
        err = persist(item);
    }
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Seuil de stock faible %s : %d", stock_name(item), value);
    return err;
}

bool stock_is_low(stock_item_t item)
{
    if (item >= STOCK_COUNT || s_mutex == NULL) {
        return false;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool low = s_qty[item] != STOCK_UNKNOWN && s_qty[item] <= s_low[item];
    xSemaphoreGive(s_mutex);
    return low;
}

uint32_t stock_revision(void)
{
    return s_revision;
}

bool stock_take_event(stock_event_t *out)
{
    if (s_mutex == NULL || out == NULL) {
        return false;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const bool available = s_event_count > 0;
    if (available) {
        *out = s_events[0];
        memmove(&s_events[0], &s_events[1], sizeof(s_events[0]) * (size_t)(--s_event_count));
    }
    xSemaphoreGive(s_mutex);
    return available;
}

void stock_print(void)
{
    if (s_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const int qty[STOCK_COUNT] = {s_qty[0], s_qty[1]};
    const int low[STOCK_COUNT] = {s_low[0], s_low[1]};
    const int64_t change[STOCK_COUNT] = {s_change[0], s_change[1]};
    const int events = s_event_count;
    xSemaphoreGive(s_mutex);
    printf("STOCK capteur quantite=%d seuil=%d faible=%d dernier_changement=%" PRId64 "\n", qty[STOCK_SENSOR],
           low[STOCK_SENSOR], qty[STOCK_SENSOR] != STOCK_UNKNOWN && qty[STOCK_SENSOR] <= low[STOCK_SENSOR],
           change[STOCK_SENSOR]);
    printf("STOCK catheter quantite=%d seuil=%d faible=%d dernier_changement=%" PRId64 "\n", qty[STOCK_CATHETER],
           low[STOCK_CATHETER], qty[STOCK_CATHETER] != STOCK_UNKNOWN && qty[STOCK_CATHETER] <= low[STOCK_CATHETER],
           change[STOCK_CATHETER]);
    printf("STOCK evenements_en_attente=%d (STOCK_LOW, usage local)\n", events);
}
