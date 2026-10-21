/* consumables.h: Consumables lifecycle and reminders interface.
 * FR: Interface de suivi et rappel des consommables.
 * Keep this header focused on the contracts used by other modules.
 */
/*
 * Consommables : capteur Dexcom G7 (suivi dans sensor_session.c) et site de perfusion / catheter.
 *
 * NOUVEAU CAPTEUR / NOUVEAU CATHETER (bouton avec confirmation, ou commande vocale confirmee) : l'heure actuelle
 * (SNTP) devient l'heure de depart, enregistree en NVS, sans aucune saisie de date ni d'heure. Une ligne est ajoutee
 * a /glycemie/logs/consumables.csv pour chaque confirmation reelle ("2026-09-15 16:42,G7,new") ; si la microSD est
 * indisponible, la ligne est ecrite a son retour (marqueur NVS, jamais de doublon ni de ligne pour une restauration).
 *
 * Catheter : intervalle 72 h par defaut (48 h possible), un seul rappel a usage unique 2 h avant l'echeance, puis
 * "A CHANGER" a l'echeance. C'est un rappel configure par l'utilisateur, pas une recommandation therapeutique.
 */
/* Public interface / Interface publique
 * EN: Catheter dates, reminders and their persistence contract.
 * FR: Dates, rappels et sauvegarde du cathéter.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    CONSUMABLE_SENSOR = 0,  /* capteur Dexcom G7 */
    CONSUMABLE_CATHETER,    /* site de perfusion / catheter */
    CONSUMABLE_COUNT,
} consumable_t;

typedef enum {
    CATHETER_REMIND_2H = 0, /* 2 h avant l'echeance (seul rappel) */
    CATHETER_REMIND_COUNT,
} catheter_reminder_t;

typedef struct {
    bool known;
    int64_t start_s;        /* heure du changement (epoch UTC) */
    int64_t due_s;          /* echeance = changement + intervalle */
    int interval_h;         /* 48 ou 72 */
    int day;                /* jour 1..days_total */
    int days_total;
    bool overdue;           /* echeance depassee */
    int64_t remaining_s;    /* avant l'echeance (negatif si depassee) */
    int progress_pct;       /* avancement sur l'intervalle */
} catheter_status_t;

esp_err_t consumables_init(void); /* apres sensor_session_init() */

/* Heure actuelle -> heure de depart (NVS + ligne CSV en attente). ESP_ERR_INVALID_STATE si l'heure SNTP n'est pas
 * valide. source : "bouton", "voix" ou "console" (journal uniquement). Pas depuis une tache a pile en PSRAM. */
esp_err_t consumable_start_now(consumable_t c, const char *source);
const char *consumable_name(consumable_t c); /* "NOUVEAU CAPTEUR"... */

void catheter_get(int64_t now_s, catheter_status_t *out);
int catheter_interval_h(void);
void catheter_set_interval_h(int hours); /* 48 ou 72 */

/* Incremente a chaque modification (capteur ou catheter) : rafraichissement de l'affichage */
uint32_t consumables_revision(void);

/* Rappel du catheter en cours (heure passee, echeance non atteinte), -1 sinon ; *done : deja presente (NVS) */
int catheter_current_reminder(int64_t now_s, bool *done);
void catheter_mark_reminder_done(int reminder); /* presente ou acquitte : memorise en NVS */
void catheter_reset_reminders(void);
const char *catheter_reminder_text(int reminder);

/* Tache microSD, carte montee : ecrit les lignes CSV en attente */
void consumables_sd_flush(void);

/* Console : etat, restauration exacte d'une date reelle apres un test (aucune ligne CSV), effacement */
void consumables_print(void);
esp_err_t consumable_restore(consumable_t c, int64_t start_s, int reminders_mask);
