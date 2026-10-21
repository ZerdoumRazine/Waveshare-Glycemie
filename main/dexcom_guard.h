/*
 * Verrou d'authentification Dexcom, persistant en NVS.
 *
 * Objectif : ne jamais risquer de bloquer le compte Dexcom.
 * - Aucune authentification tant que les identifiants n'ont pas ete valides,
 *   sauf tentative unique armee explicitement (console "dexcom_auth").
 * - L'etat est enregistre AVANT l'envoi des identifiants puis mis a jour avec le resultat :
 *   un redemarrage pendant ou apres un echec ne relance jamais de tentative.
 * - Seul l'etat VALIDE autorise les connexions automatiques (redemarrage, session expiree).
 * Le verrou est lie a une empreinte des identifiants : de nouveaux identifiants repartent de INCONNU.
 */
/* Public interface / Interface publique
 * EN: Saved authentication decisions prevent repeated attempts against a blocked account.
 * FR: Contrôle des tentatives d'authentification Dexcom.
 */
#pragma once

#include "esp_err.h"

typedef enum {
    DEX_GUARD_UNKNOWN = 0,      /* aucune tentative avec ces identifiants */
    DEX_GUARD_IN_PROGRESS = 1,  /* tentative commencee, resultat jamais enregistre (redemarrage) */
    DEX_GUARD_VALIDATED = 2,    /* authentification reussie : fonctionnement normal */
    DEX_GUARD_REFUSED = 3,      /* identifiants refuses ou compte bloque : plus aucune tentative */
    DEX_GUARD_INCONCLUSIVE = 4, /* identifiants envoyes sans verdict (reseau/serveur) : pas de nouvel essai */
} dex_guard_state_t;

esp_err_t dex_guard_init(const char *region, const char *username, const char *password);
dex_guard_state_t dex_guard_state(void);
const char *dex_guard_info(void);
esp_err_t dex_guard_set(dex_guard_state_t state, const char *info);
const char *dex_guard_state_name(dex_guard_state_t state);
