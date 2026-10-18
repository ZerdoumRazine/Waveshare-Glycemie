#include "ui_pages.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "credentials.h"
#include "glucose_service.h"
#include "settings.h"
#include "touch_diag.h"
#include "wifi_manager.h"

/*
 * Assistant de premiere mise en route (ecran tactile) : langue, reseau Wi-Fi, compte Dexcom, verification.
 * Personne n'a besoin de recompiler le firmware ni de brancher un PC pour mettre ses propres identifiants.
 *
 * - L'assistant occupe tout l'ecran (couche superieure de LVGL) et s'ouvre tout seul tant que rien n'est
 *   renseigne ; il se rouvre depuis RÉGLAGES > CONNEXION > Modifier.
 * - Les deux operations longues (recherche des reseaux, essai de connexion) tournent dans leurs propres
 *   taches : la tache LVGL n'est jamais bloquee. L'avancement est relu chaque seconde par ui_setup_tick().
 * - Les mots de passe sont masques a l'ecran, jamais journalises, et effaces de la memoire de l'interface
 *   a la fermeture de l'assistant comme apres l'enregistrement (credentials.c, espace NVS separe).
 * - LES ACCES PRECEDENTS NE SONT JAMAIS PERDUS : la saisie est un essai (credentials_trial_*). Le test de
 *   connexion porte sur les nouvelles valeurs, mais rien n'est enregistre avant l'appui sur "Enregistrer".
 *   Un echec, un abandon, une coupure de courant : l'appareil garde ses acces precedents.
 * - Apres l'enregistrement, l'appareil redemarre : les services (Wi-Fi, Dexcom) repartent proprement avec
 *   les nouveaux identifiants, sans code de reprise a chaud difficile a verifier.
 */

#define RESEAUX_MAX 14

typedef enum {
    ETAPE_LANGUE = 0,
    ETAPE_WIFI,
    ETAPE_NOM_RESEAU,   /* reseau masque : saisie du nom */
    ETAPE_MDP_WIFI,
    ETAPE_DEXCOM,
    ETAPE_VERIF,
} etape_t;

static lv_obj_t *s_root;        /* fond plein ecran */
static lv_obj_t *s_titre;
static lv_obj_t *s_etape_txt;
static lv_obj_t *s_contenu;     /* zone reconstruite a chaque etape */
static lv_obj_t *s_btn_retour;
static lv_obj_t *s_btn_suivant;
static lv_obj_t *s_kb;          /* clavier partage */
static lv_obj_t *s_ta;          /* champ en cours de saisie */

static etape_t s_etape;
static bool s_premier;          /* premiere mise en route (pas d'acces aux pages) */
static char s_ssid[CRED_SSID_MAX];
static char s_mdp_wifi[CRED_PASS_MAX];
static char s_user[CRED_USER_MAX];
static char s_mdp_dex[CRED_PASS_MAX];
static int s_region;
static lv_obj_t *s_ta_user;
static lv_obj_t *s_ta_mdp;
static lv_obj_t *s_verif_txt;
static lv_obj_t *s_verif_detail;
static bool s_confirmation;     /* essai reussi : l'enregistrement attend l'appui de l'utilisateur */

/* Recherche des reseaux : tache dediee, resultats relus par la tache LVGL */
static wifi_reseau_t s_reseaux[RESEAUX_MAX];
static volatile int s_nb_reseaux;
static volatile bool s_scan_en_cours;
static volatile bool s_scan_fini;

static void afficher_etape(etape_t etape);

/* ------------------------------------------------------------------------- */

static void scan_task(void *arg)
{
    (void)arg;
    const int n = wifi_manager_scan((wifi_reseau_t *)s_reseaux, RESEAUX_MAX);
    s_nb_reseaux = n;
    s_scan_en_cours = false;
    s_scan_fini = true;
    vTaskDelete(NULL);
}

static void lancer_scan(void)
{
    if (s_scan_en_cours) {
        return;
    }
    s_nb_reseaux = 0;
    s_scan_fini = false;
    s_scan_en_cours = true;
    if (xTaskCreate(scan_task, "scan_wifi", 4096, NULL, 4, NULL) != pdPASS) {
        s_scan_en_cours = false;
        s_scan_fini = true;
    }
}

/* ------------------------------------------------------------------------- */
/* Fabrique d'elements */

static void vider_contenu(void)
{
    if (s_kb != NULL) {
        lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(s_kb, NULL);
    }
    s_ta = s_ta_user = s_ta_mdp = NULL;
    s_verif_txt = s_verif_detail = NULL;
    lv_obj_clean(s_contenu);
}

static lv_obj_t *texte(lv_obj_t *parent, const lv_font_t *font, lv_color_t couleur, const char *txt,
                       int32_t x, int32_t y, int32_t w)
{
    lv_obj_t *l = ui_label(parent, font, couleur, txt);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void on_clavier(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(s_kb, NULL);
    }
}

static void on_champ(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target_obj(e);
    s_ta = ta;
    lv_keyboard_set_textarea(s_kb, ta);
    lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_kb);
}

/* Champ de saisie : etiquette + zone de texte (le clavier s'ouvre au toucher) */
static lv_obj_t *champ(lv_obj_t *parent, const char *etiquette, const char *valeur, bool mdp,
                       int32_t x, int32_t y, int32_t w)
{
    ui_label(parent, g_fonts.label, UI_G_TEXT2, etiquette);
    lv_obj_set_pos(lv_obj_get_child(parent, -1), x, y);
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_pos(ta, x, y + 26);
    lv_obj_set_size(ta, w, 58);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, mdp);
    lv_textarea_set_text(ta, valeur);
    lv_obj_set_style_text_font(ta, g_fonts.m, 0);
    lv_obj_set_style_bg_color(ta, UI_G_GLASS_TOP, 0);
    lv_obj_set_style_text_color(ta, UI_G_TEXT, 0);
    lv_obj_set_style_border_color(ta, UI_COL_BLUE, 0);
    lv_obj_add_event_cb(ta, on_champ, LV_EVENT_CLICKED, NULL);
    return ta;
}

static void on_afficher_mdp(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_user_data(e);
    const bool masque = lv_textarea_get_password_mode(ta);
    lv_textarea_set_password_mode(ta, !masque);
}

/* ------------------------------------------------------------------------- */
/* Actions */

/* Les acces saisis sont ESSAYES : les precedents restent enregistres jusqu'a la confirmation. */
static void essayer(void)
{
    credentials_trial_set_wifi(s_ssid, s_mdp_wifi);
    credentials_trial_set_dexcom(s_user, s_mdp_dex, credentials_region_code(s_region));
    s_confirmation = false;
    glucose_service_test_request();
}

static void on_langue(lv_event_t *e)
{
    const lang_t lang = (lang_t)(intptr_t)lv_event_get_user_data(e);
    settings_lang_set(lang); /* applique et enregistre ; l'assistant se redessine dans la nouvelle langue */
    afficher_etape(ETAPE_LANGUE);
}

static void on_demo(lv_event_t *e)
{
    (void)e;
    credentials_set_demo(true);
    glucose_service_set_demo("normal");
    ui_setup_close();
}

static void on_reseau(lv_event_t *e)
{
    const int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (index < 0) {
        s_ssid[0] = '\0';
        afficher_etape(ETAPE_NOM_RESEAU);
        return;
    }
    strlcpy(s_ssid, s_reseaux[index].ssid, sizeof(s_ssid));
    afficher_etape(ETAPE_MDP_WIFI);
}

static void on_rescan(lv_event_t *e)
{
    (void)e;
    lancer_scan();
    afficher_etape(ETAPE_WIFI);
}

static void on_region(lv_event_t *e)
{
    s_region = (int)(intptr_t)lv_event_get_user_data(e);
    afficher_etape(ETAPE_DEXCOM);
}

static void on_retour(lv_event_t *e)
{
    (void)e;
    switch (s_etape) {
    case ETAPE_LANGUE:
        if (!s_premier) {
            ui_setup_close();
        }
        break;
    case ETAPE_WIFI:
        afficher_etape(ETAPE_LANGUE);
        break;
    case ETAPE_NOM_RESEAU:
    case ETAPE_MDP_WIFI:
        afficher_etape(ETAPE_WIFI);
        break;
    case ETAPE_DEXCOM:
        afficher_etape(ETAPE_MDP_WIFI);
        break;
    case ETAPE_VERIF:
        if (s_confirmation) {
            /* "Garder les anciens accès" : l'essai est abandonne, rien n'a ete enregistre. */
            ui_setup_close();
        } else {
            afficher_etape(ETAPE_DEXCOM);
        }
        break;
    default:
        break;
    }
}

static void on_suivant(lv_event_t *e)
{
    (void)e;
    switch (s_etape) {
    case ETAPE_LANGUE:
        lancer_scan();
        afficher_etape(ETAPE_WIFI);
        break;
    case ETAPE_NOM_RESEAU:
        if (s_ta_user != NULL) {
            strlcpy(s_ssid, lv_textarea_get_text(s_ta_user), sizeof(s_ssid));
        }
        if (s_ssid[0] != '\0') {
            afficher_etape(ETAPE_MDP_WIFI);
        }
        break;
    case ETAPE_MDP_WIFI:
        if (s_ta_mdp != NULL) {
            strlcpy(s_mdp_wifi, lv_textarea_get_text(s_ta_mdp), sizeof(s_mdp_wifi));
        }
        afficher_etape(ETAPE_DEXCOM);
        break;
    case ETAPE_DEXCOM:
        if (s_ta_user != NULL) {
            strlcpy(s_user, lv_textarea_get_text(s_ta_user), sizeof(s_user));
        }
        if (s_ta_mdp != NULL) {
            strlcpy(s_mdp_dex, lv_textarea_get_text(s_ta_mdp), sizeof(s_mdp_dex));
        }
        if (s_user[0] != '\0' && s_mdp_dex[0] != '\0') {
            afficher_etape(ETAPE_VERIF);
            essayer();
        }
        break;
    case ETAPE_VERIF:
        if (s_confirmation) {
            /* Confirmation : c'est seulement maintenant que les acces sont enregistres. */
            if (credentials_trial_commit() == ESP_OK) {
                ui_set_text(s_verif_detail, tr(S_SETUP_RESTART));
                memset(s_mdp_wifi, 0, sizeof(s_mdp_wifi));
                memset(s_mdp_dex, 0, sizeof(s_mdp_dex));
                ESP_LOGW("assistant", "Acces enregistres apres confirmation : redemarrage");
                esp_restart();
            } else {
                ui_set_text(s_verif_txt, tr(S_SETUP_FAIL_OTHER));
                ui_set_text_color(s_verif_txt, UI_G_CRIT);
            }
        } else {
            essayer(); /* bouton "Réessayer" */
            afficher_etape(ETAPE_VERIF);
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Etapes */

static void bouton_bas(lv_obj_t *btn, const char *texte_btn, bool visible)
{
    lv_label_set_text(lv_obj_get_child(btn, 0), texte_btn);
    if (visible) {
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void etape_langue(void)
{
    texte(s_contenu, g_fonts.m, UI_G_TEXT2, tr(S_SETUP_INTRO), 0, 0, 940);
    texte(s_contenu, g_fonts.label, UI_G_TEXT2, tr(S_SETUP_LANG), 0, 92, 940);
    for (int i = 0; i < LANG_COUNT; i++) {
        lv_obj_t *b = ui_button(s_contenu, lang_name((lang_t)i), g_fonts.m, 290, 64, on_langue,
                                (void *)(intptr_t)i);
        lv_obj_set_pos(b, (i % 3) * 310, 124 + (i / 3) * 76);
        if ((lang_t)i == i18n_lang()) {
            lv_obj_set_style_border_color(b, UI_COL_TEXT, 0);
            lv_obj_set_style_border_width(b, 3, 0);
        }
    }
    lv_obj_t *demo = ui_button(s_contenu, tr(S_SETUP_DEMO_TRY), g_fonts.s, 340, 56, on_demo, NULL);
    lv_obj_set_pos(demo, 0, 292);
    lv_obj_set_style_bg_color(demo, UI_G_GLASS_TOP, 0);
    texte(s_contenu, g_fonts.xs, UI_G_TEXT3, tr(S_SETUP_DEMO_NOTE), 356, 300, 580);
}

static void etape_wifi(void)
{
    texte(s_contenu, g_fonts.xs, UI_G_TEXT2, tr(S_SETUP_WIFI_HELP), 0, 0, 940);
    if (s_scan_en_cours) {
        texte(s_contenu, g_fonts.m, UI_G_TEXT, tr(S_SETUP_SCANNING), 0, 60, 940);
        return;
    }
    const int n = s_nb_reseaux;
    if (n == 0) {
        texte(s_contenu, g_fonts.m, UI_G_HIGH, tr(S_SETUP_NO_NETWORK), 0, 60, 940);
    }
    for (int i = 0; i < n && i < 8; i++) {
        char ligne[96];
        snprintf(ligne, sizeof(ligne), "%.32s   %d dBm%s", s_reseaux[i].ssid, (int)s_reseaux[i].rssi,
                 s_reseaux[i].protege ? "" : "   (ouvert)");
        lv_obj_t *b = ui_button(s_contenu, ligne, g_fonts.s, 460, 54, on_reseau, (void *)(intptr_t)i);
        lv_obj_set_pos(b, (i % 2) * 480, 48 + (i / 2) * 62);
        lv_obj_set_style_bg_color(b, UI_G_GLASS_TOP, 0);
    }
    lv_obj_t *masque = ui_button(s_contenu, tr(S_SETUP_HIDDEN), g_fonts.s, 460, 54, on_reseau,
                                 (void *)(intptr_t)-1);
    lv_obj_set_pos(masque, 0, 300);
    lv_obj_t *rescan = ui_button(s_contenu, tr(S_SETUP_RESCAN), g_fonts.s, 460, 54, on_rescan, NULL);
    lv_obj_set_pos(rescan, 480, 300);
}

static void etape_nom_reseau(void)
{
    texte(s_contenu, g_fonts.xs, UI_G_TEXT2, tr(S_SETUP_WIFI_HELP), 0, 0, 940);
    s_ta_user = champ(s_contenu, tr(S_SETUP_NETWORK_NAME), s_ssid, false, 0, 50, 600);
}

static void etape_mdp_wifi(void)
{
    char ligne[192]; /* un texte traduit peut etre long : jamais de troncature */
    snprintf(ligne, sizeof(ligne), "%s : %s", tr(S_SETUP_WIFI_TITLE), s_ssid);
    texte(s_contenu, g_fonts.s, UI_G_TEXT2, ligne, 0, 0, 940);
    s_ta_mdp = champ(s_contenu, tr(S_SETUP_WIFI_PASS), s_mdp_wifi, true, 0, 44, 600);
    lv_obj_t *voir = ui_button(s_contenu, tr(S_SETUP_SHOW), g_fonts.s, 180, 58, on_afficher_mdp, s_ta_mdp);
    lv_obj_set_pos(voir, 620, 70);
    lv_obj_set_style_bg_color(voir, UI_G_GLASS_TOP, 0);
}

static void etape_dexcom(void)
{
    texte(s_contenu, g_fonts.xs, UI_G_TEXT2, tr(S_SETUP_DEXCOM_HELP), 0, 0, 940);
    texte(s_contenu, g_fonts.label, UI_G_TEXT2, tr(S_SETUP_REGION), 0, 48, 300);
    for (int i = 0; i < credentials_region_count(); i++) {
        lv_obj_t *b = ui_button(s_contenu, credentials_region_label(i), g_fonts.s, 300, 52, on_region,
                                (void *)(intptr_t)i);
        lv_obj_set_pos(b, i * 316, 74);
        if (i != s_region) {
            lv_obj_set_style_bg_color(b, UI_G_GLASS_TOP, 0);
        }
    }
    s_ta_user = champ(s_contenu, tr(S_SETUP_USER), s_user, false, 0, 140, 600);
    s_ta_mdp = champ(s_contenu, tr(S_SETUP_PASSWORD), s_mdp_dex, true, 0, 230, 600);
    lv_obj_t *voir = ui_button(s_contenu, tr(S_SETUP_SHOW), g_fonts.s, 180, 58, on_afficher_mdp, s_ta_mdp);
    lv_obj_set_pos(voir, 620, 256);
    lv_obj_set_style_bg_color(voir, UI_G_GLASS_TOP, 0);
}

static void etape_verif(void)
{
    s_verif_txt = texte(s_contenu, g_fonts.l, UI_G_TEXT, tr(S_SETUP_CHECK_WIFI), 0, 40, 940);
    s_verif_detail = texte(s_contenu, g_fonts.m, UI_G_TEXT2, "", 0, 120, 940);
}

static void afficher_etape(etape_t etape)
{
    s_etape = etape;
    vider_contenu();
    static const str_id_t k_titres[] = {S_SETUP_WELCOME, S_SETUP_WIFI_TITLE, S_SETUP_WIFI_TITLE,
                                        S_SETUP_WIFI_TITLE, S_SETUP_DEXCOM_TITLE, S_SETUP_CHECK_TITLE};
    ui_set_text(s_titre, tr(k_titres[etape]));
    char pos[16];
    static const int k_num[] = {1, 2, 2, 2, 3, 4};
    snprintf(pos, sizeof(pos), "%d/4", k_num[etape]);
    ui_set_text(s_etape_txt, pos);

    switch (etape) {
    case ETAPE_LANGUE:
        etape_langue();
        bouton_bas(s_btn_retour, tr(S_CANCEL), !s_premier);
        bouton_bas(s_btn_suivant, tr(S_SETUP_START), true);
        break;
    case ETAPE_WIFI:
        etape_wifi();
        bouton_bas(s_btn_retour, tr(S_SETUP_BACK), true);
        bouton_bas(s_btn_suivant, tr(S_SETUP_NEXT), false);
        break;
    case ETAPE_NOM_RESEAU:
        etape_nom_reseau();
        bouton_bas(s_btn_retour, tr(S_SETUP_BACK), true);
        bouton_bas(s_btn_suivant, tr(S_SETUP_NEXT), true);
        break;
    case ETAPE_MDP_WIFI:
        etape_mdp_wifi();
        bouton_bas(s_btn_retour, tr(S_SETUP_BACK), true);
        bouton_bas(s_btn_suivant, tr(S_SETUP_NEXT), true);
        break;
    case ETAPE_DEXCOM:
        etape_dexcom();
        bouton_bas(s_btn_retour, tr(S_SETUP_BACK), true);
        bouton_bas(s_btn_suivant, tr(S_SETUP_FINISH), true);
        break;
    case ETAPE_VERIF:
        etape_verif();
        bouton_bas(s_btn_retour, tr(S_SETUP_BACK), true);
        bouton_bas(s_btn_suivant, tr(S_SETUP_RETRY), false);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */

void ui_setup_create(void)
{
    s_root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, UI_SCR_W, UI_SCR_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_root, UI_G_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(s_root, UI_G_BG_BOT, 0);
    lv_obj_set_style_bg_grad_dir(s_root, LV_GRAD_DIR_VER, 0);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    touch_diag_name(s_root, "assistant");

    s_titre = ui_label(s_root, g_fonts.title, UI_G_TEXT, "");
    lv_obj_set_pos(s_titre, 40, 26);
    s_etape_txt = ui_label(s_root, g_fonts.m, UI_G_TEXT3, "");
    lv_obj_set_pos(s_etape_txt, 930, 32);

    s_contenu = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_contenu);
    lv_obj_set_pos(s_contenu, 40, 86);
    lv_obj_set_size(s_contenu, 944, 380);
    lv_obj_remove_flag(s_contenu, LV_OBJ_FLAG_SCROLLABLE);

    s_btn_retour = ui_button(s_root, "", g_fonts.m, 260, 70, on_retour, NULL);
    lv_obj_set_pos(s_btn_retour, 40, 500);
    lv_obj_set_style_bg_color(s_btn_retour, UI_G_GLASS_TOP, 0);
    s_btn_suivant = ui_button(s_root, "", g_fonts.m, 320, 70, on_suivant, NULL);
    lv_obj_set_pos(s_btn_suivant, 664, 500);

    s_kb = lv_keyboard_create(s_root);
    lv_obj_set_size(s_kb, UI_SCR_W, 280);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(s_kb, &lv_font_montserrat_20, 0); /* police integree : symboles du clavier */
    lv_obj_add_event_cb(s_kb, on_clavier, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

void ui_setup_open(bool premier_demarrage)
{
    s_premier = premier_demarrage;
    s_confirmation = false;
    credentials_trial_begin(); /* les acces actuels sont mis de cote : ils ne seront pas perdus */
    s_region = credentials_region_index("ous");
    s_ssid[0] = s_mdp_wifi[0] = s_user[0] = s_mdp_dex[0] = '\0';
    credentials_t cred;
    credentials_get(&cred);
    strlcpy(s_ssid, cred.wifi_ssid, sizeof(s_ssid)); /* le nom du reseau n'est pas un secret */
    strlcpy(s_user, cred.dexcom_user, sizeof(s_user));
    const int r = credentials_region_index(cred.dexcom_region);
    s_region = r >= 0 ? r : 0;
    memset(&cred, 0, sizeof(cred));

    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_root);
    afficher_etape(ETAPE_LANGUE);
    ESP_LOGI("assistant", "Assistant de configuration ouvert (%s)",
             premier_demarrage ? "premiere mise en route" : "modification");
}

void ui_setup_close(void)
{
    memset(s_mdp_wifi, 0, sizeof(s_mdp_wifi));
    memset(s_mdp_dex, 0, sizeof(s_mdp_dex));
    /* Sortie sans confirmation : retour aux acces precedents (rien n'a ete enregistre) et
     * reconnexion avec eux si besoin. */
    if (credentials_trial_active()) {
        credentials_trial_cancel();
        if (credentials_has_wifi()) {
            wifi_manager_apply_credentials(0); /* sans attendre : la tache LVGL ne doit pas bloquer */
        }
    }
    s_confirmation = false;
    vider_contenu();
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

bool ui_setup_visible(void)
{
    return s_root != NULL && !lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

/* Appelee chaque seconde par la tache LVGL : avancement de la recherche et de l'essai de connexion */
void ui_setup_tick(void)
{
    if (!ui_setup_visible()) {
        return;
    }
    if (s_etape == ETAPE_WIFI && s_scan_fini) {
        s_scan_fini = false;
        afficher_etape(ETAPE_WIFI);
        return;
    }
    if (s_etape != ETAPE_VERIF || s_verif_txt == NULL) {
        return;
    }
    char detail[96];
    const gly_test_t etat = glucose_service_test_state(detail, sizeof(detail));
    static const struct {
        const char *cle;
        str_id_t texte;
    } k_msg[] = {
        {"wifi", S_SETUP_CHECK_WIFI}, {"internet", S_SETUP_CHECK_NET}, {"dexcom", S_SETUP_CHECK_DEX},
        {"demarrage", S_SETUP_CHECK_WIFI},
    };
    static const struct {
        const char *cle;
        str_id_t texte;
    } k_echec[] = {
        {"wifi", S_SETUP_FAIL_WIFI}, {"internet", S_SETUP_FAIL_NET},
        {"dexcom_refuse", S_SETUP_FAIL_DEX_AUTH}, {"dexcom_bloque", S_SETUP_FAIL_DEX_LOCK},
        {"dexcom_reseau", S_SETUP_FAIL_DEX_NET},
    };

    if (etat == GLY_TEST_EN_COURS) {
        for (size_t i = 0; i < sizeof(k_msg) / sizeof(k_msg[0]); i++) {
            if (strcmp(detail, k_msg[i].cle) == 0) {
                ui_set_text(s_verif_txt, tr(k_msg[i].texte));
                break;
            }
        }
        ui_set_text(s_verif_detail, "");
        bouton_bas(s_btn_suivant, tr(S_SETUP_RETRY), false);
    } else if (etat == GLY_TEST_REUSSI) {
        /* L'essai a reussi : rien n'est enregistre avant que l'utilisateur appuie sur "Enregistrer". */
        ui_set_text(s_verif_txt, tr(S_SETUP_OK));
        ui_set_text_color(s_verif_txt, UI_G_OK);
        ui_set_text(s_verif_detail, tr(S_SETUP_CONFIRM));
        s_confirmation = true;
        bouton_bas(s_btn_suivant, tr(S_CONN_SAVE), true);
        bouton_bas(s_btn_retour, tr(S_SETUP_KEEP_OLD), !s_premier);
    } else if (etat == GLY_TEST_ECHEC) {
        ui_set_text(s_verif_txt, tr(S_SETUP_FAIL_OTHER));
        ui_set_text_color(s_verif_txt, UI_G_CRIT);
        for (size_t i = 0; i < sizeof(k_echec) / sizeof(k_echec[0]); i++) {
            if (strcmp(detail, k_echec[i].cle) == 0) {
                ui_set_text(s_verif_txt, tr(k_echec[i].texte));
                break;
            }
        }
        /* Echec : les acces precedents sont toujours ceux qui sont enregistres. */
        ui_set_text(s_verif_detail, s_premier ? "" : tr(S_SETUP_KEPT_OLD));
        s_confirmation = false;
        bouton_bas(s_btn_suivant, tr(S_SETUP_RETRY), true);
        bouton_bas(s_btn_retour, tr(S_SETUP_BACK), true);
    }
}
