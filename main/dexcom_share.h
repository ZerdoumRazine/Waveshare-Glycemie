/* dexcom_share.h: Read-only Dexcom Share client API.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Client Dexcom Share natif ESP-IDF (esp_http_client + TLS verifie par le bundle de certificats).
 *
 * Protocol after pydexcom (MIT, gagebenne/pydexcom, commit 3447aa08) :
 *   1. POST General/AuthenticatePublisherAccount  {accountName, password, applicationId} -> accountId
 *   2. POST General/LoginPublisherAccountById     {accountId, password, applicationId}   -> sessionId
 *   3. POST Publisher/ReadPublisherLatestGlucoseValues?sessionId=..&minutes=..&maxCount=..  corps {}
 * Europe = region "ous" : https://shareous1.dexcom.com/ShareWebServices/Services/
 * Identifiant d'application US/OUS : d89443d2-327c-4a6f-89e5-496bbb0317db
 * (pydexcom const.py : DEXCOM_APPLICATION_ID_OUS = DEXCOM_APPLICATION_ID_US ; l'ID d8665ade... est celui du Japon).
 *
 * API non officielle : ce module isole entierement le service distant.
 */
/* Public interface / Interface publique
 * Dexcom Share HTTPS client types, timestamps and result codes.
 */
#pragma once

#include <stdbool.h>
#include "app_state.h"

typedef enum {
    DEX_OK = 0,
    DEX_ERR_CONFIG,        /* identifiants absents */
    DEX_ERR_NETWORK,       /* DNS, TCP, TLS, delai depasse */
    DEX_ERR_SERVER,        /* HTTP inattendu, JSON invalide, code inconnu */
    DEX_ERR_SESSION,       /* SessionIdNotFound / SessionNotValid */
    DEX_ERR_AUTH,          /* identifiant ou mot de passe refuse : ne pas reessayer */
    DEX_ERR_MAX_ATTEMPTS,  /* SSO_AuthenticateMaxAttemptsExceeded : compte bloque */
    DEX_ERR_RATE_LIMITED,  /* HTTP 429 */
    DEX_ERR_NO_MEM,
} dex_result_t;

typedef enum {
    DEX_STEP_NONE = 0,
    DEX_STEP_AUTHENTICATE = 1, /* General/AuthenticatePublisherAccount */
    DEX_STEP_LOGIN = 2,        /* General/LoginPublisherAccountById */
    DEX_STEP_READ = 3,         /* Publisher/ReadPublisherLatestGlucoseValues */
} dex_step_t;

/* Diagnostic de la derniere requete en echec (aucune donnee sensible) */
typedef struct {
    dex_step_t step;
    int http_status;       /* 0 si aucune reponse HTTP */
    bool request_sent;     /* the request (and therefore the identifiers) may have left */
    char esp_error[40];    /* erreur ESP-IDF reseau/TLS */
    char code[64];         /* champ Code de Dexcom */
    char message[160];     /* champ Message de Dexcom, identifiants masques */
} dex_error_info_t;

typedef struct {
    const char *base_url;
    const char *application_id;
    const char *username;
    const char *password;
    char account_id[37];
    char session_id[37];
    dex_error_info_t last_error;
} dexcom_client_t;

void dexcom_client_init(dexcom_client_t *client, const char *region, const char *username, const char *password);
bool dexcom_client_configured(const dexcom_client_t *client);
bool dexcom_has_session(const dexcom_client_t *client);
void dexcom_invalidate_session(dexcom_client_t *client);

/* Test DNS + TLS du serveur Dexcom SANS identifiants (aucune authentification) */
dex_result_t dexcom_probe_tls(dexcom_client_t *client, int *http_status);

/* 2 step connection (Step 1 is skipped if the account ID is already known) */
dex_result_t dexcom_login(dexcom_client_t *client);

/* Reading measurements (more recent first) */
dex_result_t dexcom_read_glucose(dexcom_client_t *client, int minutes, int max_count,
                                 glucose_point_t *out, int max_out, int *out_count);

const char *dex_result_name(dex_result_t result);
const char *dex_step_name(dex_step_t step);
