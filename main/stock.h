/*
 * Stock local des consommables : capteurs Dexcom G7 et catheters (sites de perfusion).
 *
 * Logique pure (NVS + evenements), sans LVGL : l'interface (ui_consumables.c), la console et une future couche
 * Nova appellent exactement les memes fonctions.
 *  - quantite INCONNUE tant que l'utilisateur ne l'a pas renseignee (STOCK_UNKNOWN) : aucune decrementation ne la
 *    transforme en 0 ;
 *  - jamais negative, bornee a STOCK_MAX ;
 *  - une seule decrementation par changement reellement enregistre : l'heure du changement sert de marqueur en NVS
 *    (un redemarrage, une double confirmation ou un rejeu ne consomment jamais deux unites) ;
 *  - seuil de stock faible par consommable (2 et 3 par defaut, modifiables) : etat consultable + evenement local
 *    STOCK_LOW (file en memoire, aucun reseau).
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
    int64_t time_s;     /* epoch UTC, 0 si l'heure n'est pas synchronisee */
} stock_event_t;

esp_err_t stock_init(void);

int stock_get(stock_item_t item);                       /* STOCK_UNKNOWN si non renseigne */
esp_err_t stock_set(stock_item_t item, int value);      /* 0..STOCK_MAX, ou STOCK_UNKNOWN pour effacer */
esp_err_t stock_add(stock_item_t item, int delta);      /* borne 0..STOCK_MAX ; sans effet si non renseigne */

/* Changement reellement enregistre (heure de depart du consommable) : retire une unite, une seule fois pour un meme
 * changement. Sans effet si le stock n'est pas renseigne ou est deja a zero. true = une unite retiree. */
bool stock_consume(stock_item_t item, int64_t change_s);

int stock_low_threshold(stock_item_t item);
esp_err_t stock_set_low_threshold(stock_item_t item, int value); /* 0..STOCK_MAX */
bool stock_is_low(stock_item_t item);                   /* renseigne et <= seuil */

uint32_t stock_revision(void);                          /* incremente a chaque changement (rafraichissement) */
const char *stock_name(stock_item_t item);              /* "capteurs G7", "cathéters" */

/* Evenement local STOCK_LOW (file courte) : destine a une future couche Nova, jamais envoye par le firmware */
bool stock_take_event(stock_event_t *out);

void stock_print(void); /* console */
