#include "dexcom_share.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "dexcom";

/* Constantes du protocole Dexcom Share, reprises de pydexcom/const.py
 * (https://github.com/gagebenne/pydexcom, Copyright (c) Gage Benne, licence MIT).
 * Ce sont des valeurs imposees par le service : tout client doit employer les memes. */
#define DEX_BASE_URL_US        "https://share2.dexcom.com/ShareWebServices/Services/"
#define DEX_BASE_URL_OUS       "https://shareous1.dexcom.com/ShareWebServices/Services/"
#define DEX_BASE_URL_JP        "https://share.dexcom.jp/ShareWebServices/Services/"
#define DEX_APP_ID_US          "d89443d2-327c-4a6f-89e5-496bbb0317db"
#define DEX_APP_ID_OUS         DEX_APP_ID_US
#define DEX_APP_ID_JP          "d8665ade-9673-4e27-9ff6-92db4ce13d13"
#define DEX_EP_AUTHENTICATE    "General/AuthenticatePublisherAccount"
#define DEX_EP_LOGIN_ID        "General/LoginPublisherAccountById"
#define DEX_EP_READINGS        "Publisher/ReadPublisherLatestGlucoseValues"
#define DEX_DEFAULT_UUID       "00000000-0000-0000-0000-000000000000"

#define DEX_HTTP_TIMEOUT_MS    15000
#define DEX_RESP_CAPACITY      (64 * 1024)
#define DEX_URL_MAX            256

typedef enum {
    CTX_AUTH,
    CTX_READ,
} request_ctx_t;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    bool overflow;
    bool headers_sent;
} resp_buf_t;

/* Vocabulaire de tendance defini par l'API Dexcom Share : ce sont les chaines que le service
 * renvoie. Tout client emploie donc la meme liste, a l'identique. */
static const char *const k_trend_names[TREND_COUNT] = {
    "None", "DoubleUp", "SingleUp", "FortyFiveUp", "Flat",
    "FortyFiveDown", "SingleDown", "DoubleDown", "NotComputable", "RateOutOfRange",
};

/* ------------------------------------------------------------------------- */

static bool is_uuid_at(const char *s)
{
    for (int i = 0; i < 36; i++) {
        if (s[i] == '\0') {
            return false;
        }
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') {
                return false;
            }
        } else if (!isxdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return true;
}

static bool is_uuid(const char *s)
{
    return s != NULL && strlen(s) == 36 && is_uuid_at(s);
}

static void secure_wipe(char *s)
{
    if (s != NULL) {
        memset(s, 0, strlen(s));
    }
}

static void append(char *out, size_t size, size_t *pos, const char *text, size_t len)
{
    while (len-- > 0 && *pos + 1 < size) {
        out[(*pos)++] = *text++;
    }
    out[*pos] = '\0';
}

/* Message Dexcom sans identifiant, mot de passe, adresse e-mail ni UUID */
static void sanitize_message(const dexcom_client_t *c, const char *in, char *out, size_t size)
{
    char tmp[256];
    size_t pos = 0;
    size_t ulen = strlen(c->username);
    size_t plen = strlen(c->password);
    tmp[0] = '\0';
    for (size_t i = 0; in[i] != '\0' && pos + 1 < sizeof(tmp);) {
        if (ulen >= 3 && strncasecmp(in + i, c->username, ulen) == 0) {
            append(tmp, sizeof(tmp), &pos, "[identifiant]", 13);
            i += ulen;
        } else if (plen >= 3 && strncmp(in + i, c->password, plen) == 0) {
            append(tmp, sizeof(tmp), &pos, "[mot de passe]", 14);
            i += plen;
        } else if (is_uuid_at(in + i)) {
            append(tmp, sizeof(tmp), &pos, "[uuid]", 6);
            i += 36;
        } else {
            append(tmp, sizeof(tmp), &pos, in + i, 1);
            i++;
        }
    }

    pos = 0;
    out[0] = '\0';
    for (size_t i = 0; tmp[i] != '\0' && pos + 1 < size;) {
        if (isspace((unsigned char)tmp[i])) {
            append(out, size, &pos, tmp + i, 1);
            i++;
            continue;
        }
        size_t j = i;
        bool has_at = false;
        while (tmp[j] != '\0' && !isspace((unsigned char)tmp[j])) {
            has_at |= (tmp[j] == '@');
            j++;
        }
        if (has_at) {
            append(out, size, &pos, "[email]", 7);
        } else {
            append(out, size, &pos, tmp + i, j - i);
        }
        i = j;
    }
}

static void error_reset(dexcom_client_t *c, dex_step_t step)
{
    memset(&c->last_error, 0, sizeof(c->last_error));
    c->last_error.step = step;
}

static bool resp_alloc(resp_buf_t *rb)
{
    rb->data = heap_caps_malloc(DEX_RESP_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (rb->data == NULL) {
        rb->data = malloc(DEX_RESP_CAPACITY);
    }
    rb->cap = rb->data ? DEX_RESP_CAPACITY : 0;
    rb->len = 0;
    rb->overflow = false;
    rb->headers_sent = false;
    if (rb->data) {
        rb->data[0] = '\0';
    }
    return rb->data != NULL;
}

static void resp_free(resp_buf_t *rb)
{
    if (rb->data) {
        memset(rb->data, 0, rb->len);
        free(rb->data);
    }
    rb->data = NULL;
    rb->len = rb->cap = 0;
}

static esp_err_t http_event(esp_http_client_event_t *evt)
{
    resp_buf_t *rb = evt->user_data;
    if (rb == NULL) {
        return ESP_OK;
    }
    if (evt->event_id == HTTP_EVENT_HEADERS_SENT) {
        rb->headers_sent = true;
    } else if (evt->event_id == HTTP_EVENT_ON_DATA && rb->data != NULL && evt->data_len > 0) {
        size_t room = rb->cap - 1 - rb->len;
        size_t n = (size_t)evt->data_len;
        if (n > room) {
            n = room;
            rb->overflow = true;
        }
        memcpy(rb->data + rb->len, evt->data, n);
        rb->len += n;
        rb->data[rb->len] = '\0';
    }
    return ESP_OK;
}

/* POST JSON en HTTPS. Ni l'URL (sessionId) ni le corps (mot de passe) ne sont journalises. */
static dex_result_t http_post_json(dexcom_client_t *c, const char *url, const char *body, resp_buf_t *rb)
{
    rb->len = 0;
    rb->overflow = false;
    rb->headers_sent = false;
    rb->data[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = DEX_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event,
        .user_data = rb,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return DEX_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "User-Agent", "glycemie-display/0.1 (ESP32-P4; ESP-IDF)");
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    c->last_error.http_status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    c->last_error.request_sent = rb->headers_sent;

    if (err != ESP_OK) {
        strlcpy(c->last_error.esp_error, esp_err_to_name(err), sizeof(c->last_error.esp_error));
        ESP_LOGW(TAG, "Etape %s : echec reseau/TLS %s (requete envoyee : %s)", dex_step_name(c->last_error.step),
                 c->last_error.esp_error, rb->headers_sent ? "oui" : "non");
        return DEX_ERR_NETWORK;
    }
    if (rb->overflow) {
        ESP_LOGW(TAG, "Reponse trop volumineuse (> %u octets)", (unsigned)DEX_RESP_CAPACITY);
        return DEX_ERR_SERVER;
    }
    return DEX_OK;
}

/* Correspondance des erreurs Dexcom, d'apres pydexcom/dexcom.py _handle_error_code */
static dex_result_t classify_http_error(dexcom_client_t *c, const resp_buf_t *rb, request_ctx_t ctx)
{
    const int status = c->last_error.http_status;
    dex_result_t result = (status == 429) ? DEX_ERR_RATE_LIMITED : DEX_ERR_SERVER;

    cJSON *root = cJSON_Parse(rb->data);
    const cJSON *code = cJSON_IsObject(root) ? cJSON_GetObjectItemCaseSensitive(root, "Code") : NULL;
    const cJSON *message = cJSON_IsObject(root) ? cJSON_GetObjectItemCaseSensitive(root, "Message") : NULL;
    const char *code_s = cJSON_IsString(code) ? code->valuestring : NULL;
    const char *msg_s = cJSON_IsString(message) ? message->valuestring : "";

    if (code_s != NULL) {
        strlcpy(c->last_error.code, code_s, sizeof(c->last_error.code));
        sanitize_message(c, msg_s, c->last_error.message, sizeof(c->last_error.message));
        if (strcmp(code_s, "SessionIdNotFound") == 0 || strcmp(code_s, "SessionNotValid") == 0) {
            result = DEX_ERR_SESSION;
        } else if (strcmp(code_s, "AccountPasswordInvalid") == 0) {
            result = DEX_ERR_AUTH;
        } else if (strcmp(code_s, "SSO_AuthenticateMaxAttemptsExceeded") == 0) {
            result = DEX_ERR_MAX_ATTEMPTS;
        } else if (strcmp(code_s, "SSO_InternalError") == 0 &&
                   (strstr(msg_s, "Cannot Authenticate by AccountName") != NULL ||
                    strstr(msg_s, "Cannot Authenticate by AccountId") != NULL)) {
            result = DEX_ERR_AUTH;
        } else if (strcmp(code_s, "InvalidArgument") == 0) {
            /* Lecture : sessionId invalide -> nouvelle session. Connexion : identifiant/mot de passe invalide. */
            result = (ctx == CTX_READ) ? DEX_ERR_SESSION : DEX_ERR_AUTH;
        } else if (ctx == CTX_AUTH && (strstr(code_s, "Account") != NULL || strstr(code_s, "Password") != NULL ||
                                       strstr(code_s, "Authenticate") != NULL)) {
            /* Code inconnu mais clairement lie au compte : traite comme un refus (aucun nouvel essai) */
            result = DEX_ERR_AUTH;
        }
        ESP_LOGW(TAG, "Etape %s : HTTP %d, code Dexcom %s, message \"%s\" -> %s", dex_step_name(c->last_error.step),
                 status, c->last_error.code, c->last_error.message, dex_result_name(result));
    } else {
        ESP_LOGW(TAG, "Etape %s : reponse inattendue HTTP %d (%u octets)", dex_step_name(c->last_error.step), status,
                 (unsigned)rb->len);
    }
    cJSON_Delete(root);
    return result;
}

static char *build_credentials_body(const char *id_key, const char *id_value, const char *password,
                                    const char *application_id)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(obj, id_key, id_value);
    cJSON_AddStringToObject(obj, "password", password);
    cJSON_AddStringToObject(obj, "applicationId", application_id);
    char *body = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return body;
}

/* Reponse attendue : un UUID JSON entre guillemets, different de l'UUID nul */
static bool parse_uuid_response(const resp_buf_t *rb, char out[37], bool *is_default)
{
    bool ok = false;
    *is_default = false;
    cJSON *root = cJSON_Parse(rb->data);
    if (cJSON_IsString(root) && is_uuid(root->valuestring)) {
        if (strcmp(root->valuestring, DEX_DEFAULT_UUID) == 0) {
            *is_default = true;
        } else {
            memcpy(out, root->valuestring, 36);
            out[36] = '\0';
            ok = true;
        }
    }
    cJSON_Delete(root);
    return ok;
}

static dex_result_t post_credentials(dexcom_client_t *c, dex_step_t step, const char *endpoint, const char *id_key,
                                     const char *id_value, char out_uuid[37])
{
    error_reset(c, step);
    char url[DEX_URL_MAX];
    snprintf(url, sizeof(url), "%s%s", c->base_url, endpoint);

    char *body = build_credentials_body(id_key, id_value, c->password, c->application_id);
    if (body == NULL) {
        return DEX_ERR_NO_MEM;
    }
    resp_buf_t rb;
    if (!resp_alloc(&rb)) {
        secure_wipe(body);
        cJSON_free(body);
        return DEX_ERR_NO_MEM;
    }

    dex_result_t result = http_post_json(c, url, body, &rb);
    secure_wipe(body);
    cJSON_free(body);

    if (result == DEX_OK) {
        if (c->last_error.http_status == 200) {
            bool is_default = false;
            if (!parse_uuid_response(&rb, out_uuid, &is_default)) {
                if (is_default) {
                    /* pydexcom : ACCOUNT_ID_DEFAULT / SESSION_ID_DEFAULT = refus */
                    strlcpy(c->last_error.code, "UUID_NUL", sizeof(c->last_error.code));
                    strlcpy(c->last_error.message, "identifiant nul renvoye par Dexcom", sizeof(c->last_error.message));
                    ESP_LOGW(TAG, "Etape %s : HTTP 200 mais identifiant nul -> refus", dex_step_name(step));
                    result = DEX_ERR_AUTH;
                } else {
                    strlcpy(c->last_error.code, "REPONSE_INVALIDE", sizeof(c->last_error.code));
                    ESP_LOGW(TAG, "Etape %s : HTTP 200 mais reponse inattendue (%u octets)", dex_step_name(step),
                             (unsigned)rb.len);
                    result = DEX_ERR_SERVER;
                }
            }
        } else {
            result = classify_http_error(c, &rb, CTX_AUTH);
        }
    }
    resp_free(&rb);
    return result;
}

static bool parse_dexcom_date(const cJSON *item, int64_t *ms)
{
    if (!cJSON_IsString(item)) {
        return false;
    }
    const char *p = strstr(item->valuestring, "Date(");
    if (p == NULL) {
        return false;
    }
    p += 5;
    char *end = NULL;
    errno = 0;
    long long value = strtoll(p, &end, 10);
    if (end == p || errno != 0 || value <= 0) {
        return false;
    }
    *ms = (int64_t)value;
    return true;
}

static uint8_t parse_trend(const cJSON *item)
{
    if (cJSON_IsString(item)) {
        for (int i = 0; i < TREND_COUNT; i++) {
            if (strcmp(item->valuestring, k_trend_names[i]) == 0) {
                return (uint8_t)i;
            }
        }
        return TREND_NOT_COMPUTABLE;
    }
    if (cJSON_IsNumber(item) && item->valueint >= 0 && item->valueint < TREND_COUNT) {
        return (uint8_t)item->valueint; /* ancien format numerique */
    }
    return TREND_NONE;
}

static int compare_points_desc(const void *a, const void *b)
{
    const glucose_point_t *pa = a;
    const glucose_point_t *pb = b;
    if (pa->wt_ms == pb->wt_ms) {
        return 0;
    }
    return pa->wt_ms < pb->wt_ms ? 1 : -1;
}

/* ------------------------------------------------------------------------- */

void dexcom_client_init(dexcom_client_t *c, const char *region, const char *username, const char *password)
{
    memset(c, 0, sizeof(*c));
    if (region != NULL && strcmp(region, "us") == 0) {
        c->base_url = DEX_BASE_URL_US;
        c->application_id = DEX_APP_ID_US;
    } else if (region != NULL && strcmp(region, "jp") == 0) {
        c->base_url = DEX_BASE_URL_JP;
        c->application_id = DEX_APP_ID_JP;
    } else {
        c->base_url = DEX_BASE_URL_OUS; /* Europe (defaut) */
        c->application_id = DEX_APP_ID_OUS;
    }
    c->username = username ? username : "";
    c->password = password ? password : "";
    /* Comme pydexcom : un identifiant de compte (UUID) peut remplacer le nom d'utilisateur */
    if (is_uuid(c->username)) {
        memcpy(c->account_id, c->username, 36);
        c->account_id[36] = '\0';
    }
}

bool dexcom_client_configured(const dexcom_client_t *c)
{
    return c->username[0] != '\0' && c->password[0] != '\0';
}

bool dexcom_has_session(const dexcom_client_t *c)
{
    return c->session_id[0] != '\0';
}

void dexcom_invalidate_session(dexcom_client_t *c)
{
    memset(c->session_id, 0, sizeof(c->session_id));
}

dex_result_t dexcom_probe_tls(dexcom_client_t *c, int *http_status)
{
    *http_status = 0;
    esp_http_client_config_t cfg = {
        .url = c->base_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = DEX_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return DEX_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "User-Agent", "glycemie-display/0.1 (ESP32-P4; ESP-IDF)");
    esp_err_t err = esp_http_client_perform(client);
    *http_status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Test TLS Dexcom (sans identifiants) : echec %s", esp_err_to_name(err));
        return DEX_ERR_NETWORK;
    }
    ESP_LOGI(TAG, "Test TLS Dexcom (sans identifiants) : certificat verifie, HTTP %d", *http_status);
    return DEX_OK;
}

dex_result_t dexcom_login(dexcom_client_t *c)
{
    if (!dexcom_client_configured(c)) {
        return DEX_ERR_CONFIG;
    }
    dexcom_invalidate_session(c);

    if (c->account_id[0] == '\0') {
        dex_result_t r = post_credentials(c, DEX_STEP_AUTHENTICATE, DEX_EP_AUTHENTICATE, "accountName", c->username,
                                          c->account_id);
        if (r != DEX_OK) {
            memset(c->account_id, 0, sizeof(c->account_id));
            return r;
        }
        ESP_LOGI(TAG, "Connexion Dexcom 1/2 (AuthenticatePublisherAccount) : compte authentifie");
    }

    char session[37] = {0};
    dex_result_t r = post_credentials(c, DEX_STEP_LOGIN, DEX_EP_LOGIN_ID, "accountId", c->account_id, session);
    if (r != DEX_OK) {
        return r;
    }
    memcpy(c->session_id, session, sizeof(c->session_id));
    memset(session, 0, sizeof(session));
    ESP_LOGI(TAG, "Connexion Dexcom 2/2 (LoginPublisherAccountById) : session ouverte");
    return DEX_OK;
}

dex_result_t dexcom_read_glucose(dexcom_client_t *c, int minutes, int max_count,
                                 glucose_point_t *out, int max_out, int *out_count)
{
    *out_count = 0;
    error_reset(c, DEX_STEP_READ);
    if (!dexcom_has_session(c)) {
        return DEX_ERR_SESSION;
    }
    if (minutes < 1) {
        minutes = 1;
    } else if (minutes > 1440) {
        minutes = 1440;
    }
    if (max_count < 1) {
        max_count = 1;
    } else if (max_count > 288) {
        max_count = 288;
    }

    char url[DEX_URL_MAX];
    snprintf(url, sizeof(url), "%s%s?sessionId=%s&minutes=%d&maxCount=%d",
             c->base_url, DEX_EP_READINGS, c->session_id, minutes, max_count);

    resp_buf_t rb;
    if (!resp_alloc(&rb)) {
        return DEX_ERR_NO_MEM;
    }
    dex_result_t result = http_post_json(c, url, "{}", &rb);
    memset(url, 0, sizeof(url));

    if (result == DEX_OK && c->last_error.http_status != 200) {
        result = classify_http_error(c, &rb, CTX_READ);
    }
    if (result != DEX_OK) {
        resp_free(&rb);
        return result;
    }

    cJSON *root = cJSON_Parse(rb.data);
    if (!cJSON_IsArray(root)) {
        ESP_LOGW(TAG, "Lecture Dexcom : JSON inattendu (%u octets)", (unsigned)rb.len);
        cJSON_Delete(root);
        resp_free(&rb);
        return DEX_ERR_SERVER;
    }

    static bool fields_logged;
    const cJSON *first = cJSON_GetArrayItem(root, 0);
    if (!fields_logged && cJSON_IsObject(first)) {
        /* Recherche de metadonnees (duree du capteur...) : noms des champs seulement, jamais les valeurs */
        char names[160] = "";
        const cJSON *field = NULL;
        cJSON_ArrayForEach(field, first) {
            if (field->string != NULL && strlen(names) + strlen(field->string) + 3 < sizeof(names)) {
                strcat(names, names[0] ? ", " : "");
                strcat(names, field->string);
            }
        }
        ESP_LOGI(TAG, "Champs d'une mesure Dexcom Share : %s", names);
        fields_logged = true;
    }

    int n = 0;
    int skipped = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (n >= max_out) {
            break;
        }
        int64_t wt = 0;
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(item, "Value");
        bool has_time = parse_dexcom_date(cJSON_GetObjectItemCaseSensitive(item, "WT"), &wt) ||
                        parse_dexcom_date(cJSON_GetObjectItemCaseSensitive(item, "ST"), &wt) ||
                        parse_dexcom_date(cJSON_GetObjectItemCaseSensitive(item, "DT"), &wt);
        if (!has_time || !cJSON_IsNumber(value) || value->valuedouble < 1 || value->valuedouble > 1000) {
            skipped++;
            continue;
        }
        out[n].wt_ms = wt;
        out[n].mgdl = (int16_t)value->valueint;
        out[n].trend = parse_trend(cJSON_GetObjectItemCaseSensitive(item, "Trend"));
        n++;
    }
    cJSON_Delete(root);
    resp_free(&rb);

    if (skipped > 0) {
        ESP_LOGW(TAG, "Lecture Dexcom : %d mesure(s) ignoree(s) (format inattendu)", skipped);
    }
    qsort(out, (size_t)n, sizeof(glucose_point_t), compare_points_desc);
    *out_count = n;
    return DEX_OK;
}

const char *dex_result_name(dex_result_t result)
{
    switch (result) {
    case DEX_OK: return "OK";
    case DEX_ERR_CONFIG: return "CONFIG";
    case DEX_ERR_NETWORK: return "RESEAU";
    case DEX_ERR_SERVER: return "SERVEUR";
    case DEX_ERR_SESSION: return "SESSION";
    case DEX_ERR_AUTH: return "IDENTIFIANTS_REFUSES";
    case DEX_ERR_MAX_ATTEMPTS: return "COMPTE_BLOQUE";
    case DEX_ERR_RATE_LIMITED: return "TROP_DE_REQUETES";
    case DEX_ERR_NO_MEM: return "MEMOIRE";
    default: return "?";
    }
}

const char *dex_step_name(dex_step_t step)
{
    switch (step) {
    case DEX_STEP_AUTHENTICATE: return "1/2 AuthenticatePublisherAccount";
    case DEX_STEP_LOGIN: return "2/2 LoginPublisherAccountById";
    case DEX_STEP_READ: return "ReadPublisherLatestGlucoseValues";
    default: return "-";
    }
}
