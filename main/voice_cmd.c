/*
 * voice_cmd.c: Optional experimental voice command recognition.
 * FR: Reconnaissance vocale expérimentale et facultative.
 * Design note: Disabled in the standard public build to avoid false triggers.
 */

/*
 * Optional experimental voice-command recognizer; excluded from the public firmware.
 * Reconnaissance vocale expérimentale, absente du firmware distribué.
 */
#include "voice_cmd.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_timer.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "model_path.h"
#include "app_state.h"
#include "audio_out.h"
#include "consumables.h"
#include "glucose_math.h"
#include "net_services.h"
#include "settings.h"
#include "ui_main.h"
#include "voice.h"

static const char *TAG = "voixcmd";

/* Modeles officiels d'esp-sr 2.5.3 (partition "model", srmodels.bin genere et flashe par le composant) */
#define MODEL_PARTITION    "model"
#define WN_MODEL           "wn9l_fr_bonjouresp_tts3"
#define MN_MODEL           "mn7_en"
#define MN_WINDOW_MS       6000                 /* ecoute d'une commande (apres le mot d'eveil, ou fenetre glissante) */
#define CONFIRM_WINDOW_US  (10 * 1000000LL)     /* deuxieme detection attendue apres la fin de "Repetez pour confirmer" */
#define PLAYBACK_TAIL_US   (600 * 1000LL)       /* detection reprise 0,6 s apres la fin de toute lecture audio */
#define RESULT_TIMEOUT_US  (5 * 1000000LL)      /* enregistrement par la tache LVGL (rafraichissement 1 s) */
#define TASK_STACK         (16 * 1024)
#define TASK_PRIORITY      3
#define BOOT_DELAY_US      (60 * 1000000LL)     /* premiere ecoute 60 s apres la mise sous tension */
#define MN_ID_GLUCOSE_Z    4                    /* "Glucose" GLoKbZ : meme action que "Glycemie", seuil propre */
#define GLYCEMIE_MIN_PROB  0.155f               /* accepte si prob >= 0.155f : score affiche >= 0,16 */
#define GLUCOSE_Z_MIN_PROB 0.205f               /* accepte si prob > 0.205f : score affiche >= 0,21 */
/* K19 (2026-09-21) : capteur en deux phrases differentes, identifiants MultiNet propres. L'identifiant 2 (anciennes
 * "Nouveau capteur" / "New sensor") n'a plus aucune phrase : il ne sert qu'aux variantes de test (vcmd_ajout 2),
 * journalisees sans aucune action. */
#define MN_ID_LEGACY_SENSOR   2
#define MN_ID_SENSOR_REGISTER 5                 /* "Enregistrer nouveau capteur" : annonce seulement */
#define MN_ID_SENSOR_CONFIRM  6                 /* "Confirmer capteur" : seule phrase qui enregistre */
#define MN_ID_MAX             6
#define SENSOR_REGISTER_MIN_PROB 0.180f         /* seuils propres, choisis sur les scores releves (detection_accepted) */
#define SENSOR_CONFIRM_MIN_PROB  0.150f
#define SENSOR_QUIET_US    (5 * 60 * 1000000LL) /* annonce capteur non confirmee : plus d'annonce pendant 5 min */
#define LEVEL_FRAMES       64                   /* niveau des micros sur les ~2 dernieres secondes (trames de 32 ms) */
#define INJECT_MAX_BYTES   (256 * 1024)         /* test vrx_inject : 8 s de son au plus */

/* Phonemes de MultiNet7 anglais (alphabet de tools/multinet_g2p.py d'esp-sr) ecrits pour la prononciation francaise :
 * "glycemie" /gli.se.mi/, "nouveau capteur" /nu.vo kap.toer/, "nouveau catheter" /nu.vo ka.te.ter/.
 * Deux variantes par commande (voyelles proches) : meme identifiant de commande. */
static const struct {
    int id;
    const char *text;
    const char *phonemes;
} k_phrases[] = {
    {VOICE_CMD_GLUCOSE, "GLYCEMIE", "GLmSdMm"},
    {VOICE_CMD_GLUCOSE, "GLYCEMIE B", "GLgSfMm"},
    /* K19 : "enregistrer nouveau capteur" /an.re.jis.tre nu.vo kap.teur/ puis "confirmer capteur" /kon.fir.me kap.teur/
     * (anciennes "NoVb KaPTk" et "No SfNSk" retirees : les faux declenchements venaient tous de "New sensor") */
    /* Voyelles nasales francaises "en" / "con" : entendues par ce modele anglais avec ou sans N. "Enregistrer" : avec
     * le schwa ("an-re-gis-tre") ou sans ("an-r'gis-tre"). Variantes de "confirmer" : la seule entendue lors des
     * essais par injection de voix de synthese (2026-09-21, logs 350-359 : Hortense, 0,1427) est KbFmMd. Peu de
     * variantes : chaque phrase ajoutee elargit le graphe (un graphe charge a degrade toute la reconnaissance). */
    {MN_ID_SENSOR_REGISTER, "ENREGISTRER NOUVEAU CAPTEUR", "nNRcrmSTRd NoVb KaPTk"},
    {MN_ID_SENSOR_REGISTER, "ENREGISTRER NOUVEAU CAPTEUR B", "nNRrmSTRd NoVb KaPTk"},
    {MN_ID_SENSOR_REGISTER, "ENREGISTRER NOUVEAU CAPTEUR C", "nRcrmSTRd NoVb KaPTk"},
    {MN_ID_SENSOR_CONFIRM, "CONFIRMER CAPTEUR", "KbNFmRMd KaPTk"},
    {MN_ID_SENSOR_CONFIRM, "CONFIRMER CAPTEUR B", "KeNFmRMd KaPTk"},
    {MN_ID_SENSOR_CONFIRM, "CONFIRMER CAPTEUR C", "KbFmRMd KaPTk"},
    {MN_ID_SENSOR_CONFIRM, "CONFIRMER CAPTEUR D", "KbFmMd KaPTk"},
    {VOICE_CMD_CATHETER, "NOUVEAU CATHETER", "NoVb KaTdTfR"},
    {VOICE_CMD_CATHETER, "NOUVEAU CATHETER B", "NoVb KcTdTk"},
    /* 2026-09-15 : "Nouveau catheter" jamais reconnu avec les deux variantes ci-dessus (log 194) ; la variante G
     * ("ka-te-te", sans R final) a ete decodee au premier essai (log 203) */
    {VOICE_CMD_CATHETER, "NOUVEAU CATHETER C", "NoVb KaTdTf"},
    {VOICE_CMD_CATHETER, "NOUVEAU CATHETER D", "NoVb KaTmTfR"},
    {VOICE_CMD_CATHETER, "NOUVEAU CATHETER E", "NoVb KaTfTfR"},
    {VOICE_CMD_CATHETER, "NOUVEAU CATHETER F", "NoVb KaTdTkR"},
    {VOICE_CMD_CATHETER, "NOUVEAU CATHETER G", "NoVb KcTfTf"},
    /* Commandes anglaises gardees en meme temps que le francais (demande de l'utilisateur) : meilleure tenue a
     * distance avec ce modele anglais (log 203 : "new sensor" p=0,24, "glucose" reconnu a chaque essai). Phonemes du
     * dictionnaire anglais (CMU) : GLUCOSE G L UW K OW S, NEW N (Y) UW, SENSOR S EH N S ER, CATHETER K AE TH AH T ER.
     * 2026-09-17 (log 221) : variante GLoKbS retiree (vraies detections 0,16-0,19 et faux declenchements 0,14-0,20
     * indissociables) ; GLoKbZ a son propre identifiant pour le filtre de detection_accepted() */
    {MN_ID_GLUCOSE_Z, "GLUCOSE Z", "GLoKbZ"},
    {VOICE_CMD_CATHETER, "NEW CATHETER", "No KavcTk"},
    {VOICE_CMD_CATHETER, "NEW CATHETER Y", "NYo KavcTk"},
};
#define PHRASE_COUNT (int)(sizeof(k_phrases) / sizeof(k_phrases[0]))

typedef enum {
    RUN_STOPPED = 0,
    RUN_STARTING,
    RUN_RUNNING,
    RUN_STOPPING,
    RUN_ERROR,
} run_state_t;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile run_state_t s_run = RUN_STOPPED;
static volatile bool s_stop;
static bool s_boot_hold; /* redemarrage apres panique ou watchdog : pas de demarrage automatique (jamais de boucle) */
static const char *volatile s_error = "";
static srmodel_list_t *s_models;          /* partition projetee une seule fois (pointeurs dans la flash) */

/* Etat de la reconnaissance (tache voixcmd ; lectures pour l'interface) */
static volatile voice_cmd_id_t s_pending = VOICE_CMD_NONE; /* commande a confirmer */
static volatile int64_t s_confirm_until_us;                /* 0 : fenetre pas encore ouverte (phrase en cours) */
static volatile bool s_listening;                          /* mot d'eveil entendu : commande attendue */
static volatile int64_t s_listen_until_us;
static volatile uint32_t s_await_seq;                      /* enregistrement demande a la tache LVGL */
static int64_t s_await_since_us;
static volatile int s_sim_id;                /* console vcmd_sim : identifiant MultiNet simule (0 = aucun) */
static volatile float s_sim_prob;
static volatile int64_t s_sensor_quiet_until_us; /* anti-repetition de l'annonce capteur (K19) */
static float s_frame_ms[LEVEL_FRAMES];       /* energie moyenne des dernieres trames (niveau lors d'une detection) */
static int s_frame_idx;
static int16_t *s_inj_pcm;                   /* test vrx_inject (PSRAM, alloue au premier essai) */
static volatile size_t s_inj_len;            /* echantillons */
static volatile size_t s_inj_pos;
static volatile bool s_inj_restart;          /* test : fenetre MultiNet alignee sur le debut du son injecte */
static volatile bool s_level_log;
static volatile bool s_decode_log;          /* console : chaine decodee par MultiNet a chaque detection ou fin de fenetre */
/* Console : variante de phonemes ajoutee pendant l'ecoute (reglage fin, perdue au redemarrage) */
static char s_add_phonemes[64];
static volatile int s_add_id;
static int s_extra_count;

/* Mesures */
static size_t s_int_before;
static size_t s_psram_before;
static size_t s_int_after;
static size_t s_psram_after;
static volatile int s_chunk;
static volatile uint32_t s_detections[4];
static volatile uint32_t s_ignored;        /* detections sous le seuil de detection_accepted() */
static volatile uint32_t s_wake_count;
static volatile uint32_t s_read_errors;
static int64_t s_proc_us;                 /* temps de calcul cumule (WakeNet + MultiNet) */
static int64_t s_audio_us;                /* duree audio traitee */
static int64_t s_wn_us;
static int64_t s_mn_us;
static volatile float s_cpu_pct;
static volatile float s_wn_ms;
static volatile float s_mn_ms;
static volatile float s_level_dbfs;

static const char *cmd_name(voice_cmd_id_t cmd)
{
    switch (cmd) {
    case VOICE_CMD_GLUCOSE:
        return "GLYCEMIE";
    case VOICE_CMD_SENSOR:
        return "NOUVEAU CAPTEUR";
    case VOICE_CMD_CATHETER:
        return "NOUVEAU CATHETER";
    default:
        return "?";
    }
}

/* Phrase MultiNet (journal et variantes de test) */
static const char *phrase_name(int mn_id)
{
    switch (mn_id) {
    case VOICE_CMD_GLUCOSE:
        return "GLYCEMIE";
    case MN_ID_LEGACY_SENSOR:
        return "ANCIEN CAPTEUR (TEST)";
    case VOICE_CMD_CATHETER:
        return "NOUVEAU CATHETER";
    case MN_ID_GLUCOSE_Z:
        return "GLUCOSE";
    case MN_ID_SENSOR_REGISTER:
        return "ENREGISTRER NOUVEAU CAPTEUR";
    case MN_ID_SENSOR_CONFIRM:
        return "CONFIRMER CAPTEUR";
    default:
        return "?";
    }
}

static int volume(void)
{
    settings_t s;
    settings_get(&s);
    return s.volume_pct;
}

/* ------------------------------------------------------------------------- */
/* Actions */

static void say_glucose(void)
{
    glucose_point_t pts[1];
    uint32_t seq = 0;
    const int count = app_history_get(pts, 1, &seq);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    const int64_t now_ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    const int64_t age_s = gm_age_s(pts, count, now_ms, net_time_is_valid());
    const gm_freshness_t fresh = gm_freshness(age_s);
    const bool available = count > 0;
    const bool stale = available && (fresh == GM_FRESH_STALE || fresh == GM_FRESH_NONE);
    ESP_LOGI(TAG, "Reponse GLYCEMIE : %s%d mg/dL, tendance %s, age %lld s", stale ? "donnee ancienne, " : "",
             available ? pts[0].mgdl : 0, available ? trend_name(pts[0].trend) : "-", (long long)age_s);
    voice_say_glucose(available ? pts[0].mgdl : 0, available ? pts[0].trend : TREND_NONE, stale, available, volume());
}

static void reply(voice_reply_t r)
{
    voice_reply(r, volume());
}

/* Discard unconfirmed requests rather than persisting an accidental detection. */
static void cancel_pending(const char *why, bool announce)
{
    if (s_pending != VOICE_CMD_NONE) {
        ESP_LOGI(TAG, "%s : confirmation annulee (%s), aucun enregistrement", cmd_name(s_pending), why);
        if (s_pending == VOICE_CMD_SENSOR) {
            /* demande non confirmee (souvent un faux declenchement) : aucune nouvelle annonce pendant 5 min */
            s_sensor_quiet_until_us = esp_timer_get_time() + SENSOR_QUIET_US;
            ESP_LOGI(TAG, "Anti-repetition : \"Enregistrer nouveau capteur\" ignore pendant 5 min (bouton tactile "
                     "toujours disponible)");
        }
        s_pending = VOICE_CMD_NONE;
        s_confirm_until_us = 0;
        if (announce) {
            reply(VOICE_REPLY_CANCELLED);
        }
    }
}

/* Filtre logiciel par commande (2026-09-17, logs 203, 208, 220, 221). MultiNet7 n'emet une commande que si
 * exp(logP / (jetons + 2)) > exp(-2) ~ 0,135 (test strict de la bibliotheque) ; scores relus a 2 decimales.
 * "Glycemie" : aucun faux declenchement observe, vraies detections >= 0,16 -> prob >= 0.155f (plus petit float
 * affiche 0,16). "Glucose" GLoKbZ : faux <= 0,16, vraies >= 0,24, faux de l'ancienne variante GLoKbS <= 0,20
 * -> prob > 0.205f (0.205f s'affiche 0,20). Catheter non filtre : double confirmation.
 * K19 (2026-09-21) : faux declenchements capteur releves du 18 au 19/09 (journal 226) : tous "New sensor",
 * 0,1390 / 0,1399 / 0,1422 / 0,1429 / 0,1656 ; vraies "Nouveau capteur" 0,16 a 0,36 (logs 194, 203, 226).
 * "Enregistrer nouveau capteur" (seule phrase qui provoque une annonce) : prob >= 0.180f, au-dessus de tous les
 * faux releves avec une marge. "Confirmer capteur" n'agit que dans les 10 s suivant l'annonce : prob >= 0.150f,
 * au-dessus du plancher de la bibliotheque sans rendre la confirmation impossible. */
static bool detection_accepted(int mn_id, float prob)
{
    if (mn_id == VOICE_CMD_GLUCOSE) {
        return prob >= GLYCEMIE_MIN_PROB;
    }
    if (mn_id == MN_ID_GLUCOSE_Z) {
        return prob > GLUCOSE_Z_MIN_PROB;
    }
    if (mn_id == MN_ID_SENSOR_REGISTER) {
        return prob >= SENSOR_REGISTER_MIN_PROB;
    }
    if (mn_id == MN_ID_SENSOR_CONFIRM) {
        return prob >= SENSOR_CONFIRM_MIN_PROB;
    }
    return true;
}

/* Handle confirmation as a distinct step before changing consumable dates. */
static void handle_command(voice_cmd_id_t cmd, float prob, int64_t now_us)
{
    if (cmd < VOICE_CMD_GLUCOSE || cmd > VOICE_CMD_CATHETER) {
        return;
    }
    s_detections[cmd]++;
    ESP_LOGI(TAG, "Commande entendue : %s (probabilite %.2f)%s", cmd_name(cmd), prob,
             s_pending != VOICE_CMD_NONE ? " pendant une confirmation" : "");
    if (s_await_seq != 0) {
        return; /* enregistrement deja en cours : reponse attendue avant toute autre commande */
    }
    if (cmd == VOICE_CMD_GLUCOSE) {
        cancel_pending("autre commande", false);
        say_glucose();
        return;
    }
    if (s_pending == cmd && s_confirm_until_us != 0 && now_us <= s_confirm_until_us) {
        /* deuxieme detection dans la fenetre : enregistrement de l'heure actuelle par la tache LVGL */
        ESP_LOGW(TAG, "%s CONFIRME par une deuxieme detection : enregistrement de l'heure actuelle", cmd_name(cmd));
        s_await_since_us = now_us;
        s_await_seq = ui_request_consumable_new(cmd == VOICE_CMD_SENSOR ? CONSUMABLE_SENSOR : CONSUMABLE_CATHETER);
        return;
    }
    if (s_pending != VOICE_CMD_NONE && s_pending != cmd) {
        cancel_pending("autre commande", false);
    }
    /* premiere detection : rien n'est modifie */
    s_pending = cmd;
    s_confirm_until_us = 0; /* ouverte a la fin de la phrase "Repetez pour confirmer" */
    ESP_LOGI(TAG, "%s : premiere detection, aucun changement ; \"Repetez pour confirmer\"", cmd_name(cmd));
    reply(cmd == VOICE_CMD_SENSOR ? VOICE_REPLY_SENSOR_CONFIRM : VOICE_REPLY_CATHETER_CONFIRM);
}

/* K19 : premiere phrase du capteur. Ne modifie jamais rien ; annonce puis attente de "Confirmer capteur". */
static void sensor_register(int64_t now_us)
{
    s_detections[VOICE_CMD_SENSOR]++;
    if (s_await_seq != 0) {
        return; /* enregistrement deja en cours */
    }
    if (s_pending == VOICE_CMD_SENSOR) {
        ESP_LOGI(TAG, "ENREGISTRER NOUVEAU CAPTEUR pendant l'attente : ignore (seul \"Confirmer capteur\" valide)");
        return;
    }
    if (now_us < s_sensor_quiet_until_us) {
        ESP_LOGW(TAG, "ENREGISTRER NOUVEAU CAPTEUR ignore : anti-repetition (%lld s restantes), aucune annonce",
                 (long long)((s_sensor_quiet_until_us - now_us) / 1000000));
        return;
    }
    if (s_pending != VOICE_CMD_NONE) {
        cancel_pending("autre commande", false);
    }
    s_pending = VOICE_CMD_SENSOR;
    s_confirm_until_us = 0; /* ouverte a la fin de l'annonce */
    ESP_LOGI(TAG, "NOUVEAU CAPTEUR : premiere phrase entendue, aucun changement ; \"Dites confirmer capteur\"");
    reply(VOICE_REPLY_SENSOR_ASK_CONFIRM);
}

/* K19 : seule phrase qui enregistre un capteur, et seulement dans la fenetre ouverte par la premiere */
static void sensor_confirm(int64_t now_us)
{
    if (s_await_seq != 0) {
        return;
    }
    if (s_pending != VOICE_CMD_SENSOR || s_confirm_until_us == 0 || now_us > s_confirm_until_us) {
        ESP_LOGI(TAG, "CONFIRMER CAPTEUR sans demande en cours : ignore, aucun changement");
        return;
    }
    ESP_LOGW(TAG, "NOUVEAU CAPTEUR CONFIRME par \"Confirmer capteur\" : enregistrement de l'heure actuelle");
    s_await_since_us = now_us;
    s_await_seq = ui_request_consumable_new(CONSUMABLE_SENSOR);
}

static float energy_dbfs(double mean_square)
{
    const double rms = sqrt(mean_square);
    return (float)(20.0 * log10((rms > 1.0 ? rms : 1.0) / 32768.0));
}

/* Toute detection (reelle ou simulee) : journal (score exact, phonemes, niveau des micros), seuil, action.
 * Retourne true si la detection est acceptee. */
static bool on_detection(int mn_id, float prob, const char *phonemes, int64_t now_us)
{
    double sum = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < LEVEL_FRAMES; i++) {
        sum += s_frame_ms[i];
        peak = s_frame_ms[i] > peak ? s_frame_ms[i] : peak;
    }
    const bool accepted = detection_accepted(mn_id, prob);
    ESP_LOGI(TAG, "Detection id %d, probabilite %.4f, phonemes \"%s\" : %s (%s ; micro crete %.1f dBFS, moyen %.1f "
             "dBFS sur 2 s)", mn_id, prob, phonemes, accepted ? "acceptee" : "IGNOREE (sous le seuil)", phrase_name(mn_id),
             energy_dbfs(peak), energy_dbfs(sum / LEVEL_FRAMES));
    if (!accepted) {
        s_ignored++;
        return false;
    }
    switch (mn_id) {
    case VOICE_CMD_GLUCOSE:
    case MN_ID_GLUCOSE_Z:
        handle_command(VOICE_CMD_GLUCOSE, prob, now_us);
        break;
    case VOICE_CMD_CATHETER:
        handle_command(VOICE_CMD_CATHETER, prob, now_us);
        break;
    case MN_ID_SENSOR_REGISTER:
        sensor_register(now_us);
        break;
    case MN_ID_SENSOR_CONFIRM:
        sensor_confirm(now_us);
        break;
    default:
        ESP_LOGI(TAG, "Phrase de test sans action (id %d)", mn_id);
        break;
    }
    return true;
}

/* Test vrx_inject : son ajoute au signal des micros (meme chemin que le son de la piece) */
static void inject_mix(int16_t *mono, int n)
{
    const size_t len = s_inj_len;
    const size_t pos = s_inj_pos;
    if (s_inj_pcm == NULL || pos >= len) {
        return;
    }
    const size_t count = len - pos < (size_t)n ? len - pos : (size_t)n;
    for (size_t i = 0; i < count; i++) {
        const int v = (int)mono[i] + (int)s_inj_pcm[pos + i];
        mono[i] = (int16_t)(v > INT16_MAX ? INT16_MAX : (v < INT16_MIN ? INT16_MIN : v));
    }
    s_inj_pos = pos + count;
    if (pos + count >= len) {
        ESP_LOGI(TAG, "TEST : son injecte joue en entier (%u ms)", (unsigned)(len * 1000 / AUDIO_SAMPLE_RATE));
    }
}

/* Resultat de l'enregistrement demande a la tache LVGL */
static void poll_registration(int64_t now_us)
{
    if (s_await_seq == 0) {
        return;
    }
    const int result = ui_consumable_new_result(s_await_seq);
    if (result == 0 && now_us - s_await_since_us < RESULT_TIMEOUT_US) {
        return;
    }
    const voice_cmd_id_t cmd = s_pending;
    s_await_seq = 0;
    s_pending = VOICE_CMD_NONE;
    s_confirm_until_us = 0;
    if (result > 0) {
        ESP_LOGW(TAG, "%s enregistre par commande vocale confirmee", cmd_name(cmd));
        reply(cmd == VOICE_CMD_SENSOR ? VOICE_REPLY_SENSOR_SAVED : VOICE_REPLY_CATHETER_SAVED);
    } else {
        ESP_LOGW(TAG, "%s : enregistrement impossible (%s)", cmd_name(cmd), result < 0 ? "heure non synchronisee"
                                                                                       : "interface indisponible");
        reply(VOICE_REPLY_TIME_UNAVAILABLE);
    }
}

/* ------------------------------------------------------------------------- */
/* Tache */

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* La partition "model" est au-dela de 16 Mo : une flash Quad ne peut pas la projeter en memoire (adressage 24 bits,
 * esp_srmodel_init() echouerait). srmodels.bin (format pack_model.py : nombre de modeles, puis pour chacun nom[32],
 * nombre de fichiers, et pour chaque fichier nom[32], debut, taille) est donc lu en PSRAM puis charge par l'API
 * officielle srmodel_load(). Lecture une seule fois, par une tache temporaire a pile en RAM interne : une lecture de
 * la flash desactive le cache, interdit depuis la tache voixcmd dont la pile est en PSRAM. */
typedef struct {
    TaskHandle_t waiter;
    uint8_t *data;
    size_t size;
} model_read_t;

static void read_model_partition_task(void *arg)
{
    model_read_t *ctx = arg;
    ctx->data = NULL;
    ctx->size = 0;
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                                           MODEL_PARTITION);
    uint8_t hdr[40];
    size_t pos = 4;
    size_t end = 0;
    bool ok = part != NULL && esp_partition_read(part, 0, hdr, 4) == ESP_OK;
    const uint32_t models = ok ? rd_u32(hdr) : 0;
    ok = ok && models > 0 && models <= 16;
    for (uint32_t m = 0; ok && m < models; m++) {
        ok = esp_partition_read(part, pos, hdr, 36) == ESP_OK;
        const uint32_t files = ok ? rd_u32(hdr + 32) : 0;
        pos += 36;
        ok = ok && files > 0 && files <= 32;
        for (uint32_t f = 0; ok && f < files; f++) {
            ok = esp_partition_read(part, pos, hdr, 40) == ESP_OK;
            const size_t file_end = (size_t)rd_u32(hdr + 32) + rd_u32(hdr + 36);
            end = file_end > end ? file_end : end;
            pos += 40;
        }
    }
    ok = ok && end > pos && end <= part->size;
    uint8_t *data = ok ? heap_caps_malloc(end, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
    if (data != NULL && esp_partition_read(part, 0, data, end) == ESP_OK) {
        ctx->data = data;
        ctx->size = end;
    } else {
        free(data);
    }
    xTaskNotifyGive(ctx->waiter);
    vTaskDelete(NULL);
}

static srmodel_list_t *load_model_partition(void)
{
    model_read_t ctx = {.waiter = xTaskGetCurrentTaskHandle()};
    const int64_t t0 = esp_timer_get_time();
    if (xTaskCreate(read_model_partition_task, "vcmd_lecture", 3072, &ctx, TASK_PRIORITY, NULL) != pdPASS) {
        return NULL;
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (ctx.data == NULL) {
        ESP_LOGE(TAG, "Partition model absente ou invalide");
        return NULL;
    }
    ESP_LOGI(TAG, "Modeles ESP-SR lus en PSRAM : %u Ko en %lld ms", (unsigned)(ctx.size / 1024),
             (long long)((esp_timer_get_time() - t0) / 1000));
    return srmodel_load(ctx.data); /* donnees gardees en memoire pendant toute la vie du programme */
}

static void log_step(const char *step, size_t *int_prev, size_t *psram_prev)
{
    const size_t int_now = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t psram_now = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "%s : RAM interne %+d o, PSRAM %+d Ko", step, (int)((int64_t)int_now - (int64_t)*int_prev),
             (int)((int64_t)psram_now - (int64_t)*psram_prev) / 1024);
    *int_prev = int_now;
    *psram_prev = psram_now;
}

/* WakeNet n'est cree que si le mot d'eveil est demande (moins de memoire en commandes directes) */
static bool load_models(bool with_wakenet, const esp_wn_iface_t **wn, model_iface_data_t **wn_data,
                        esp_mn_iface_t **mn, model_iface_data_t **mn_data)
{
    size_t int_prev = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psram_prev = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (s_models == NULL) {
        s_models = load_model_partition();
        log_step("Lecture des modeles", &int_prev, &psram_prev);
    }
    if (s_models == NULL || esp_srmodel_exists(s_models, MN_MODEL) < 0 ||
            (with_wakenet && esp_srmodel_exists(s_models, WN_MODEL) < 0)) {
        s_error = "modeles absents de la partition model";
        return false;
    }
    if (with_wakenet) {
        *wn = esp_wn_handle_from_name(WN_MODEL);
        *wn_data = *wn != NULL ? (*wn)->create(WN_MODEL, DET_MODE_95) : NULL;
        if (*wn_data == NULL) {
            s_error = "WakeNet indisponible";
            return false;
        }
        log_step("WakeNet9L \"Bonjour ESP\" cree", &int_prev, &psram_prev);
    }
    *mn = esp_mn_handle_from_name(MN_MODEL);
    *mn_data = *mn != NULL ? (*mn)->create(MN_MODEL, MN_WINDOW_MS) : NULL;
    if (*mn_data == NULL) {
        s_error = "MultiNet indisponible";
        return false;
    }
    log_step("MultiNet7 cree", &int_prev, &psram_prev);
    if (esp_mn_commands_alloc(*mn, *mn_data) != ESP_OK) {
        s_error = "commandes MultiNet";
        return false;
    }
    for (int i = 0; i < PHRASE_COUNT; i++) {
        if (esp_mn_commands_phoneme_add(k_phrases[i].id, k_phrases[i].text, k_phrases[i].phonemes) != ESP_OK) {
            ESP_LOGW(TAG, "Commande refusee : %s", k_phrases[i].text);
        }
    }
    const esp_mn_error_t *errors = esp_mn_commands_update();
    if (errors != NULL) {
        for (int i = 0; i < errors->num; i++) {
            ESP_LOGW(TAG, "Phrase non prise en compte par MultiNet : %s", errors->phrases[i]->string);
        }
    }
    log_step("Commandes MultiNet (phonemes)", &int_prev, &psram_prev);
    return true;
}

/* Process microphone frames only in builds where voice recognition is explicitly enabled. */
static void voice_task(void *arg)
{
    (void)arg;
    const esp_wn_iface_t *wn = NULL;
    model_iface_data_t *wn_data = NULL;
    esp_mn_iface_t *mn = NULL;
    model_iface_data_t *mn_data = NULL;
    int16_t *stereo = NULL;
    int16_t *mono = NULL;
    esp_codec_dev_handle_t mic = NULL;
    bool mic_open = false;

    /* Au demarrage de la carte : ecoute apres la connexion Wi-Fi / Dexcom et la lecture complete de l'historique */
    while (esp_timer_get_time() < BOOT_DELAY_US && !s_stop) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    s_int_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s_psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    voice_cmd_settings_t cfg;
    settings_vcmd_get(&cfg);
    uint32_t settings_rev = settings_revision();
    const bool use_wn = cfg.wake_word; /* changement de mode : ecoute redemarree avec les bons modeles */
    ESP_LOGI(TAG, "Demarrage (%s) : RAM interne %u Ko, PSRAM %u Ko avant ESP-SR",
             use_wn ? "mot d'eveil" : "commandes directes", (unsigned)(s_int_before / 1024),
             (unsigned)(s_psram_before / 1024));

    bool ok = load_models(use_wn, &wn, &wn_data, &mn, &mn_data);
    int chunk = 0;
    if (ok) {
        chunk = mn->get_samp_chunksize(mn_data);
        const int wn_chunk = use_wn ? wn->get_samp_chunksize(wn_data) : chunk;
        if (chunk <= 0 || chunk != wn_chunk || mn->get_samp_rate(mn_data) != AUDIO_SAMPLE_RATE) {
            ESP_LOGE(TAG, "Trames incompatibles : MultiNet %d, WakeNet %d echantillons", chunk, wn_chunk);
            s_error = "trames incompatibles";
            ok = false;
        }
    }
    if (ok) {
        size_t int_prev = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t psram_prev = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        stereo = heap_caps_malloc((size_t)chunk * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        mono = heap_caps_malloc((size_t)chunk * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        mic = audio_out_mic_acquire();
        esp_codec_dev_sample_info_t fs = {.sample_rate = AUDIO_SAMPLE_RATE, .channel = 2, .bits_per_sample = 16};
        mic_open = mic != NULL && esp_codec_dev_open(mic, &fs) == ESP_CODEC_DEV_OK;
        if (stereo == NULL || mono == NULL || !mic_open) {
            s_error = mic_open ? "memoire" : "micros ES7210 indisponibles";
            ok = false;
        } else {
            esp_codec_dev_set_in_gain(mic, (float)cfg.mic_gain_db);
        }
        log_step("Micros ES7210 ouverts", &int_prev, &psram_prev);
    }

    s_int_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s_psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (ok) {
        s_chunk = chunk;
        s_run = RUN_RUNNING;
        s_error = "";
        ESP_LOGI(TAG, "A l'ecoute : %s%s, trame %d echantillons (%d ms) ; RAM interne %u Ko (%+d Ko), PSRAM %u Ko "
                 "(%+d Ko)", use_wn ? WN_MODEL " + " : "", MN_MODEL, chunk, chunk * 1000 / AUDIO_SAMPLE_RATE,
                 (unsigned)(s_int_after / 1024), (int)((int64_t)s_int_after - (int64_t)s_int_before) / 1024,
                 (unsigned)(s_psram_after / 1024), (int)((int64_t)s_psram_after - (int64_t)s_psram_before) / 1024);
    } else {
        ESP_LOGE(TAG, "Commandes vocales indisponibles : %s", s_error);
    }

    /* Modele deja alimente : remise a zero possible. Jamais de clean() sur un modele encore jamais utilise (acces
     * memoire nul dans la bibliotheque : plantage constate le 2026-09-15, log 189). */
    bool wn_fed = false;
    bool mn_fed = false;
    int64_t muted_until_us = 0;
    bool need_clean = false;
    int64_t level_energy = 0;
    int level_samples = 0;
    int64_t level_next_us = esp_timer_get_time() + 1000000;
    int64_t stats_start_us = esp_timer_get_time();

    while (ok && !s_stop) {
        if (esp_codec_dev_read(mic, stereo, chunk * 4) != ESP_CODEC_DEV_OK) {
            s_read_errors++;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        const int64_t now_us = esp_timer_get_time();
        if (settings_revision() != settings_rev) {
            settings_rev = settings_revision();
            const uint8_t old_gain = cfg.mic_gain_db;
            settings_vcmd_get(&cfg);
            if (cfg.mic_gain_db != old_gain) {
                esp_codec_dev_set_in_gain(mic, (float)cfg.mic_gain_db);
                ESP_LOGI(TAG, "Gain des micros : %u dB", cfg.mic_gain_db);
            }
            if (cfg.wake_word != use_wn && s_pending == VOICE_CMD_NONE && s_await_seq == 0) {
                ESP_LOGI(TAG, "Mot d'eveil %s : redemarrage de l'ecoute", cfg.wake_word ? "requis" : "non requis");
                break; /* liberation puis nouvelle ecoute (voice_cmd_apply_settings en fin de tache) */
            }
        }

        /* Canaux gauche et droit (deux micros) moyennes, son de test eventuel, niveau */
        for (int i = 0; i < chunk; i++) {
            mono[i] = (int16_t)(((int)stereo[2 * i] + (int)stereo[2 * i + 1]) / 2);
        }
        if (s_inj_restart) {
            s_inj_restart = false;
            if (mn_fed) {
                mn->clean(mn_data); /* test : la fenetre de 6 s commence avec le son injecte (mesures repetables) */
            }
        }
        inject_mix(mono, chunk);
        int64_t frame_energy = 0;
        for (int i = 0; i < chunk; i++) {
            frame_energy += (int64_t)mono[i] * mono[i];
        }
        level_energy += frame_energy;
        s_frame_ms[s_frame_idx] = (float)((double)frame_energy / chunk);
        s_frame_idx = (s_frame_idx + 1) % LEVEL_FRAMES;
        level_samples += chunk;
        if (now_us >= level_next_us) {
            const double rms = level_samples > 0 ? sqrt((double)level_energy / level_samples) : 0.0;
            s_level_dbfs = (float)(20.0 * log10((rms > 1.0 ? rms : 1.0) / 32768.0));
            if (s_level_log) {
                printf("VCMD_NIVEAU %.1f dBFS%s\n", s_level_dbfs, audio_out_busy() ? " (lecture audio)" : "");
            }
            level_energy = 0;
            level_samples = 0;
            level_next_us = now_us + 1000000;
        }

        poll_registration(now_us);

        /* Aucune detection pendant une lecture audio (voix, alerte, bip) : l'appareil ne s'entend pas lui-meme */
        if (audio_out_busy()) {
            muted_until_us = now_us + PLAYBACK_TAIL_US;
            need_clean = true;
            continue;
        }
        if (now_us < muted_until_us) {
            continue;
        }
        if (need_clean) {
            if (wn_fed) {
                wn->clean(wn_data);
            }
            if (mn_fed) {
                mn->clean(mn_data);
            }
            need_clean = false;
            if (s_listening) {
                s_listen_until_us = now_us + MN_WINDOW_MS * 1000LL;
            }
        }
        if (s_pending != VOICE_CMD_NONE && s_await_seq == 0) {
            if (s_confirm_until_us == 0) {
                s_confirm_until_us = now_us + CONFIRM_WINDOW_US; /* la phrase de confirmation vient de se terminer */
                ESP_LOGI(TAG, "%s : fenetre de confirmation ouverte (10 s)", cmd_name(s_pending));
            } else if (now_us > s_confirm_until_us) {
                cancel_pending(s_pending == VOICE_CMD_SENSOR ? "aucun \"Confirmer capteur\" en 10 s"
                                                             : "aucune deuxieme detection en 10 s", true);
                s_listening = false;
                continue;
            }
        }
        const int sim = s_sim_id;
        if (sim != 0) {
            s_sim_id = 0;
            ESP_LOGW(TAG, "TEST : detection simulee de %s", phrase_name(sim));
            on_detection(sim, s_sim_prob, "(simulation)", now_us);
            continue;
        }
        const int add_id = s_add_id;
        if (add_id != 0) {
            char text[48];
            snprintf(text, sizeof(text), "%s X%d", phrase_name(add_id), ++s_extra_count);
            const esp_err_t err = esp_mn_commands_phoneme_add(add_id, text, s_add_phonemes);
            const esp_mn_error_t *errors = err == ESP_OK ? esp_mn_commands_update() : NULL;
            printf("VCMD_AJOUT %s \"%s\" : %s\n", text, s_add_phonemes,
                   err != ESP_OK ? "refusee" : (errors != NULL && errors->num > 0 ? "phonemes non pris en compte" : "active"));
            s_add_id = 0;
            if (mn_fed) {
                mn->clean(mn_data);
            }
            continue;
        }

        const bool need_wake = use_wn && !s_listening && s_pending == VOICE_CMD_NONE;
        if (need_wake) {
            const int64_t t0 = esp_timer_get_time();
            const wakenet_state_t r = wn->detect(wn_data, mono);
            wn_fed = true;
            const int64_t dt = esp_timer_get_time() - t0;
            s_wn_us += dt;
            s_proc_us += dt;
            if (r == WAKENET_DETECTED) {
                s_wake_count++;
                ESP_LOGI(TAG, "Mot d'eveil \"Bonjour ESP\" entendu : commande attendue (6 s)");
                s_listening = true;
                s_listen_until_us = now_us + MN_WINDOW_MS * 1000LL;
                if (mn_fed) {
                    mn->clean(mn_data);
                }
                audio_out_play_tone(1320, 90, volume());
            }
        } else {
            if (s_listening && s_pending == VOICE_CMD_NONE && now_us > s_listen_until_us) {
                s_listening = false;
                ESP_LOGI(TAG, "Aucune commande apres le mot d'eveil");
                if (wn_fed) {
                    wn->clean(wn_data);
                }
            }
            const int64_t t0 = esp_timer_get_time();
            const esp_mn_state_t st = mn->detect(mn_data, mono);
            mn_fed = true;
            const int64_t dt = esp_timer_get_time() - t0;
            s_mn_us += dt;
            s_proc_us += dt;
            if (st == ESP_MN_STATE_DETECTED) {
                const esp_mn_results_t *res = mn->get_results(mn_data);
                if (res != NULL && s_decode_log) {
                    for (int i = 0; i < res->num; i++) {
                        printf("VCMD_DECODE detecte %d/%d commande=%d phrase=%d p=%.2f \"%s\"\n", i + 1, res->num,
                               res->command_id[i], res->phrase_id[i], res->prob[i],
                               esp_mn_commands_get_from_index(res->phrase_id[i]) != NULL
                                   ? esp_mn_commands_get_from_index(res->phrase_id[i])->string : "?");
                    }
                    printf("VCMD_DECODE chaine=\"%s\" brute=\"%s\"\n", res->string, res->raw_string);
                }
                bool ignored = false;
                if (res != NULL && res->num > 0) {
                    /* score exact, phonemes decodes et niveau des micros de chaque detection (suivi des seuils) */
                    ignored = !on_detection(res->command_id[0], res->prob[0], res->string, now_us);
                }
                mn->clean(mn_data);
                if (!ignored && s_pending == VOICE_CMD_NONE && s_await_seq == 0) {
                    s_listening = false;
                }
            } else if (st == ESP_MN_STATE_TIMEOUT) {
                if (s_decode_log) {
                    const esp_mn_results_t *res = mn->get_results(mn_data);
                    if (res != NULL && (res->string[0] != '\0' || res->raw_string[0] != '\0')) {
                        printf("VCMD_DECODE fin de fenetre sans commande : chaine=\"%s\" brute=\"%s\"\n", res->string,
                               res->raw_string);
                    }
                }
                mn->clean(mn_data);
            }
        }

        s_audio_us += (int64_t)chunk * 1000000 / AUDIO_SAMPLE_RATE;
        if (now_us - stats_start_us >= 10 * 1000000LL && s_audio_us > 0) {
            s_cpu_pct = (float)(100.0 * (double)s_proc_us / (double)s_audio_us);
            const double frames = (double)s_audio_us / (chunk * 1000000.0 / AUDIO_SAMPLE_RATE);
            s_wn_ms = (float)(s_wn_us / 1000.0 / frames);
            s_mn_ms = (float)(s_mn_us / 1000.0 / frames);
            s_proc_us = s_audio_us = s_wn_us = s_mn_us = 0;
            stats_start_us = now_us;
        }
    }

    /* Arret (reglage) ou echec : liberation complete */
    if (mic_open) {
        esp_codec_dev_close(mic);
    }
    audio_out_mic_release();
    if (mn_data != NULL) {
        /* destroy() libere aussi la liste des commandes (un appel prealable a esp_mn_commands_free() produisait
         * "MN_COMMAND: The mn commands is not initialized") ; une liste restante serait liberee par
         * esp_mn_commands_alloc() au prochain demarrage */
        mn->destroy(mn_data);
    }
    if (wn_data != NULL) {
        wn->destroy(wn_data);
    }
    free(stereo);
    free(mono);
    s_pending = VOICE_CMD_NONE;
    s_listening = false;
    s_await_seq = 0;
    ESP_LOGI(TAG, "Ecoute arretee : RAM interne %u Ko, PSRAM %u Ko", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    portENTER_CRITICAL(&s_lock);
    s_run = ok ? RUN_STOPPED : RUN_ERROR;
    portEXIT_CRITICAL(&s_lock);
    if (ok) {
        voice_cmd_apply_settings(); /* reactivee pendant l'arret : nouvelle ecoute */
    }
    vTaskDeleteWithCaps(NULL);
}

/* ------------------------------------------------------------------------- */

void voice_cmd_apply_settings(void)
{
    voice_cmd_settings_t cfg;
    settings_vcmd_get(&cfg);
    bool start = false;
    portENTER_CRITICAL(&s_lock);
    if (cfg.enabled && (s_run == RUN_STOPPED || s_run == RUN_ERROR)) {
        s_run = RUN_STARTING;
        s_stop = false;
        s_boot_hold = false;
        start = true;
    } else if (!cfg.enabled && (s_run == RUN_RUNNING || s_run == RUN_STARTING)) {
        s_run = RUN_STOPPING;
        s_stop = true;
    } else if (!cfg.enabled && s_run == RUN_ERROR) {
        s_run = RUN_STOPPED;
        s_boot_hold = false;
    }
    portEXIT_CRITICAL(&s_lock);
    if (start) {
        if (xTaskCreatePinnedToCoreWithCaps(voice_task, "voixcmd", TASK_STACK, NULL, TASK_PRIORITY, NULL,
                                            tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
            s_error = "tache non creee";
            s_run = RUN_ERROR;
            ESP_LOGE(TAG, "Tache des commandes vocales non creee");
        }
    }
}

esp_err_t voice_cmd_init(void)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                                           MODEL_PARTITION);
    voice_cmd_settings_t cfg;
    settings_vcmd_get(&cfg);
    ESP_LOGI(TAG, "Commandes vocales locales %s (partition model : %s, %s)", cfg.enabled ? "activees" : "desactivees",
             part != NULL ? "presente" : "ABSENTE", cfg.wake_word ? "mot d'eveil \"Bonjour ESP\" requis"
             : "commandes directes");
    const esp_reset_reason_t reason = esp_reset_reason();
    if (cfg.enabled && (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
                        reason == ESP_RST_WDT)) {
        s_boot_hold = true;
        s_run = RUN_ERROR;
        s_error = "en pause apres un redemarrage inattendu";
        ESP_LOGW(TAG, "Redemarrage inattendu : ecoute NON demarree automatiquement (reactivation dans RÉGLAGES ou "
                 "vcmd_on)");
        return ESP_OK;
    }
    voice_cmd_apply_settings();
    return ESP_OK;
}

bool voice_cmd_running(void)
{
    return s_run == RUN_RUNNING;
}

const char *voice_cmd_status_text(void)
{
    switch (s_run) {
    case RUN_RUNNING:
        return "à l'écoute";
    case RUN_STARTING:
        return "démarrage...";
    case RUN_STOPPING:
        return "arrêt...";
    case RUN_ERROR:
        return s_boot_hold ? "en pause après un redémarrage inattendu" : "indisponibles";
    default:
        return "désactivées";
    }
}

void voice_cmd_ui_hint(char *buf, size_t size)
{
    buf[0] = '\0';
    if (s_run != RUN_RUNNING) {
        return;
    }
    const voice_cmd_id_t pending = s_pending;
    const int64_t until = s_confirm_until_us;
    if (pending == VOICE_CMD_SENSOR || pending == VOICE_CMD_CATHETER) {
        const char *what = pending == VOICE_CMD_SENSOR ? "Nouveau capteur" : "Nouveau cathéter";
        if (s_await_seq != 0) {
            snprintf(buf, size, "%s : enregistrement…", what);
        } else if (until == 0) {
            snprintf(buf, size, "%s ?", what);
        } else {
            int64_t left = (until - esp_timer_get_time() + 999999) / 1000000;
            left = left < 0 ? 0 : left;
            if (pending == VOICE_CMD_SENSOR) {
                snprintf(buf, size, "Dites « Confirmer capteur » %lld s", (long long)left);
            } else {
                snprintf(buf, size, "Répétez « %s » %lld s", what, (long long)left);
            }
        }
    } else if (s_listening) {
        snprintf(buf, size, "À l'écoute…");
    }
}

void voice_cmd_print_info(void)
{
    voice_cmd_settings_t cfg;
    settings_vcmd_get(&cfg);
    printf("VCMD etat=%s erreur=\"%s\" reglage=%d mot_eveil=%d gain_micro=%u dB modeles=%s+%s (partition %s)\n",
           voice_cmd_status_text(), s_error, cfg.enabled, cfg.wake_word, cfg.mic_gain_db, WN_MODEL, MN_MODEL,
           MODEL_PARTITION);
    printf("VCMD memoire_avant interne=%u psram=%u ; apres interne=%u psram=%u ; ecart interne=%+d Ko psram=%+d Ko ; "
           "maintenant interne=%u (min %u) psram=%u\n", (unsigned)s_int_before, (unsigned)s_psram_before,
           (unsigned)s_int_after, (unsigned)s_psram_after, (int)((int64_t)s_int_after - (int64_t)s_int_before) / 1024,
           (int)((int64_t)s_psram_after - (int64_t)s_psram_before) / 1024,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("VCMD trame=%d echantillons cpu=%.1f%% d'un coeur (wakenet %.2f ms, multinet %.2f ms par trame de %d ms) "
           "niveau_micro=%.1f dBFS erreurs_lecture=%u\n", s_chunk, s_cpu_pct, s_wn_ms, s_mn_ms,
           s_chunk > 0 ? s_chunk * 1000 / AUDIO_SAMPLE_RATE : 0, s_level_dbfs, (unsigned)s_read_errors);
    printf("VCMD detections glycemie=%u capteur=%u catheter=%u mots_eveil=%u attente=%s ecoute=%d ignorees=%u\n",
           (unsigned)s_detections[VOICE_CMD_GLUCOSE], (unsigned)s_detections[VOICE_CMD_SENSOR],
           (unsigned)s_detections[VOICE_CMD_CATHETER], (unsigned)s_wake_count,
           s_pending != VOICE_CMD_NONE ? cmd_name(s_pending) : "aucune", s_listening, (unsigned)s_ignored);
    const int64_t quiet = s_sensor_quiet_until_us - esp_timer_get_time();
    printf("VCMD capteur : seuils enregistrer >= %.3f confirmer >= %.3f ; anti-repetition %lld s restantes ; "
           "glycemie >= %.3f glucose > %.3f (inchanges)\n", SENSOR_REGISTER_MIN_PROB, SENSOR_CONFIRM_MIN_PROB,
           (long long)(quiet > 0 ? quiet / 1000000 : 0), GLYCEMIE_MIN_PROB, GLUCOSE_Z_MIN_PROB);
    const TaskHandle_t task = xTaskGetHandle("voixcmd");
    if (task != NULL) {
        printf("VCMD tache voixcmd : pile libre minimale %u octets (pile en PSRAM)\n",
               (unsigned)uxTaskGetStackHighWaterMark(task));
    }
}

void voice_cmd_simulate(int mn_id, float prob)
{
    if (s_run != RUN_RUNNING || mn_id < 1 || mn_id > MN_ID_MAX) {
        printf("VCMD_SIM refuse : ecoute arretee ou identifiant 1 a %d attendu\n", MN_ID_MAX);
        return;
    }
    s_sim_prob = prob;
    s_sim_id = mn_id;
    printf("VCMD_SIM %s p=%.4f\n", phrase_name(mn_id), prob);
}

bool voice_cmd_inject(const uint8_t *pcm, size_t bytes)
{
    if (s_run != RUN_RUNNING || bytes < 2 || bytes > INJECT_MAX_BYTES) {
        return false;
    }
    if (s_inj_pcm == NULL) {
        s_inj_pcm = heap_caps_malloc(INJECT_MAX_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_inj_pcm == NULL) {
            return false;
        }
    }
    s_inj_len = 0; /* son precedent arrete avant la copie */
    memcpy(s_inj_pcm, pcm, bytes & ~(size_t)1);
    s_inj_pos = 0;
    s_inj_len = bytes / 2;
    s_inj_restart = true;
    ESP_LOGI(TAG, "TEST : son injecte dans l'ecoute (%u ms)", (unsigned)(bytes / 2 * 1000 / AUDIO_SAMPLE_RATE));
    return true;
}

void voice_cmd_set_level_log(bool on)
{
    s_level_log = on;
}

void voice_cmd_set_decode_log(bool on)
{
    s_decode_log = on;
    printf("VCMD_DECODE %s\n", on ? "actif" : "inactif");
}

void voice_cmd_add_phrase(int mn_id, const char *phonemes)
{
    if (s_run != RUN_RUNNING || mn_id < 1 || mn_id > MN_ID_MAX || phonemes[0] == '\0') {
        printf("VCMD_AJOUT refuse (ecoute arretee ou arguments)\n");
        return;
    }
    strlcpy(s_add_phonemes, phonemes, sizeof(s_add_phonemes));
    s_add_id = mn_id;
}
