/* stock.h: Stock counts and replacement bookkeeping interface.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Local consumable stock: Dexcom G7 sensors and catheters (infusion sites).
 *
 * Logique pure (NVS + events), without LVGL : l'interface (ui_consumables.c), the console and a future layer
 * Nova calls exactly the same functions.
 *  - quantite INCONNUE until the user has provided information (STOCK_UNKNOWN) : aucune decrementation ne la
 *    transforme en 0 ;
 *  - Never negative, limit to STOCK_MAX ;
 *  - a single decrementation by real change recorded: the time of change serves as a marker in NVS
 *    (a restart, double confirmation or replay never consume two units);
 *  - seuil de stock faible par consommable (2 et 3 par defaut, modifiables) : etat consultable + evenement local
 *    STOCK_LOW (file en memoire, aucun reseau).
 */
/* Public interface / Interface publique
 * Inventory counts and item-level low-stock notifications.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    STOCK_SENSOR = 0,   /* capteurs Dexcom G7 */
    STOCK_CATHETER,     /* catheters / sites de perfusion */
    STOCK_COUNT,
} stock_item_t;

#define STOCK_UNKNOWN (-1) /* non renseigne */
#define STOCK_MAX     99

typedef struct {
    stock_item_t item;
    int remaining;      /* quantite restante au moment de l'evenement */
    int threshold;      /* seuil qui a declenche l'evenement */
    int64_t time_s;     /* epoch UTC, 0 if the time is not synchronized */
} stock_event_t;

esp_err_t stock_init(void);

int stock_get(stock_item_t item);                       /* STOCK_UNKNOWN si non renseigne */
esp_err_t stock_set(stock_item_t item, int value);      /* 0..STOCK_MAX, ou STOCK_UNKNOWN to erase */
esp_err_t stock_add(stock_item_t item, int delta);      /* borne 0..STOCK_MAX ; no effect if not given */

/* Change actually recorded (time of departure of consumable): remove one unit, only once for the same
 * change. No effect if the stock is not informative or already zero. True = a unit removed. */
bool stock_consume(stock_item_t item, int64_t change_s);

int stock_low_threshold(stock_item_t item);
esp_err_t stock_set_low_threshold(stock_item_t item, int value); /* 0..STOCK_MAX */
bool stock_is_low(stock_item_t item);                   /* renseigne et <= seuil */

uint32_t stock_revision(void);                          /* incremente a chaque changement (rafraichissement) */
const char *stock_name(stock_item_t item);              /* "G7 sensors", "catheters" */

/* Evenement local STOCK_LOW (short line): Destiny has a future Nova layer, never sent by firmware */
bool stock_take_event(stock_event_t *out);

void stock_print(void); /* console */
