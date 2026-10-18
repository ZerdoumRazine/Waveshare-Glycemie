#include "ui_main.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "mbedtls/base64.h"
#include "app_config.h"
#include "app_state.h"
#include "consumables.h"
#include "net_services.h"
#include "credentials.h"
#include "night_mode.h"
#include "touch_diag.h"
#include "ui_common.h"
#include "ui_alert.h"
#include "i18n.h"
#include "settings.h"
#include "voice.h"
#include "ui_pages.h"
#include "voice_cmd.h"

static const char *TAG = "ui";

static lv_obj_t *s_pages[UI_PAGE_COUNT];
static lv_obj_t *s_nav;
static ui_page_t s_page = UI_PAGE_HOME;
static lv_obj_t *s_clock;
static lv_obj_t *s_date;
static lv_obj_t *s_demo;
static lv_obj_t *s_voice_pill;
static char s_voice_text[64];

static int s_chart_hours = 6;
static bool s_wdt_subscribed;

/* Diagnostic de la tache LVGL : battement du rafraichissement periodique, duree de rendu, images par seconde */
static volatile int64_t s_heartbeat_us;
static int64_t s_refr_start_us;
static uint32_t s_refr_count;
static uint32_t s_refr_last_ms;
static uint32_t s_refr_max_ms;
static volatile int64_t s_stress_until_us;
static volatile int s_stress_mode; /* 1 : plein ecran ; 2 : petites zones dispersees (nombreux transferts) */
static int s_last_minute = -1;
static volatile int32_t s_test_age_offset_min;
static volatile int s_requested_page = -1;
static volatile int s_requested_hours;
static volatile int s_requested_dialog; /* 1 = NOUVEAU CAPTEUR, 2 = fermer, 3 = NOUVEAU CATHETER */
static volatile int s_requested_alert;  /* 1 = OK, 2 = rappel 30 min */
static volatile int s_requested_scroll = -1;
static volatile int s_requested_lang = -1; /* langue demandee depuis REGLAGES, appliquee hors du callback */
/* Commande vocale confirmee : enregistrement dans la tache LVGL (pile en RAM interne, ecriture NVS permise) */
static volatile int s_requested_new = -1;
static volatile uint32_t s_new_seq;
static volatile uint32_t s_new_done_seq;
static volatile int s_new_result;

/* Intitules de la barre de navigation : remplis par fill_nav_map() selon la langue choisie.
 * LVGL conserve le pointeur de la carte : le tableau doit rester valide (statique). */
static const char *k_nav_map[UI_PAGE_COUNT + 1];
static const str_id_t k_nav_ids[UI_PAGE_COUNT] = {S_NAV_HOME, S_NAV_GRAPH, S_NAV_TODAY, S_NAV_CONSUM,
                                                  S_NAV_SETTINGS};

static void fill_nav_map(void)
{
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        k_nav_map[i] = tr(k_nav_ids[i]);
    }
    k_nav_map[UI_PAGE_COUNT] = "";
}
/* Largeurs relatives (1 a 15) des boutons de navigation : intitules de longueurs tres differentes */
static const uint8_t k_nav_widths[UI_PAGE_COUNT] = {7, 9, 10, 12, 8};

/* ------------------------------------------------------------------------- */

static void update_page(ui_page_t page, bool force)
{
    switch (page) {
    case UI_PAGE_HOME:
        ui_home_update(&g_ui, force);
        break;
    case UI_PAGE_GRAPH:
        ui_graph_update(&g_ui, force);
        break;
    case UI_PAGE_TODAY:
        ui_today_update(&g_ui, force);
        break;
    case UI_PAGE_CONSUMABLES:
        ui_consumables_update(&g_ui, force);
        break;
    case UI_PAGE_SETTINGS:
        ui_settings_update(&g_ui, force);
        break;
    default:
        break;
    }
}

void ui_show_page(ui_page_t page)
{
    if (page >= UI_PAGE_COUNT) {
        return;
    }
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        if (i == (int)page) {
            lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (!lv_buttonmatrix_has_button_ctrl(s_nav, (uint32_t)page, LV_BUTTONMATRIX_CTRL_CHECKED)) {
        lv_buttonmatrix_set_button_ctrl(s_nav, (uint32_t)page, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    if (s_page != page) {
        ESP_LOGI(TAG, "Page %s", k_nav_map[page]);
    }
    s_page = page;
    update_page(page, true);
}

static void on_nav_changed(lv_event_t *e)
{
    uint32_t idx = lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(e));
    if (idx < UI_PAGE_COUNT) {
        ui_show_page((ui_page_t)idx);
    }
}

int ui_chart_hours(void)
{
    return s_chart_hours;
}

void ui_set_chart_hours(int hours)
{
    if (hours != 3 && hours != 6 && hours != 12 && hours != 24) {
        return;
    }
    if (hours != s_chart_hours) {
        ESP_LOGI(TAG, "Graphique : %d h", hours);
    }
    s_chart_hours = hours;
    update_page(s_page, true);
}

/* ------------------------------------------------------------------------- */

static int64_t now_epoch_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void collect_data(void)
{
    ui_data_t *d = &g_ui;
    app_state_get(&d->st);

    uint32_t seq = app_history_seq();
    d->history_changed = false;
    if (seq != d->seq) {
        d->count = app_history_get(d->hist, GLY_HISTORY_MAX, &d->seq);
        d->history_changed = true;
    }

    d->time_ok = net_time_is_valid();
    d->now_ms = now_epoch_ms();
    time_t now_s = (time_t)(d->now_ms / 1000);
    localtime_r(&now_s, &d->now_tm);
    d->minute_changed = d->now_tm.tm_min != s_last_minute;
    s_last_minute = d->now_tm.tm_min;

    d->age_s = gm_age_s(d->hist, d->count, d->now_ms, d->time_ok);
    if (d->age_s >= 0) {
        d->age_s += (int64_t)s_test_age_offset_min * 60;
    }
    d->fresh = gm_freshness(d->age_s);
    d->delta_ok = gm_delta_current(d->hist, d->count, d->age_s, &d->delta);
    d->cadence_s = gm_cadence_s(d->hist, d->count);
    d->next_ms = gm_next_expected_ms(d->hist, d->count);
}

static void update_top_bar(void)
{
    const ui_data_t *d = &g_ui;
    /* Le bandeau porte le temps pour toutes les pages : date a gauche, heure a droite */
    char buf[64];
    if (d->time_ok) {
        snprintf(buf, sizeof(buf), "%02d:%02d", d->now_tm.tm_hour, d->now_tm.tm_min);
        ui_set_text(s_clock, buf);
        ui_format_date_long(&d->now_tm, buf, sizeof(buf));
        ui_set_text(s_date, buf);
    } else {
        ui_set_text(s_clock, "--:--");
        ui_set_text(s_date, tr(S_TIME_NOT_SYNCED));
    }

    /* Commande vocale en attente de confirmation ou a l'ecoute */
    char voice[sizeof(s_voice_text)];
    voice_cmd_ui_hint(voice, sizeof(voice));
    if (strcmp(voice, s_voice_text) != 0) {
        strlcpy(s_voice_text, voice, sizeof(s_voice_text));
        if (voice[0] != '\0') {
            lv_label_set_text(lv_obj_get_child(s_voice_pill, 0), s_voice_text);
            lv_obj_remove_flag(s_voice_pill, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_voice_pill, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (d->st.demo_mode) {
        lv_obj_remove_flag(s_demo, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_demo, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Changement de langue : appele UNIQUEMENT depuis la tache LVGL (ui_timer_cb), jamais depuis
 * le callback d'un bouton, puisque les objets qui portent ce callback sont detruits ici.
 * Seules les pages sont reconstruites : le bandeau, la barre de navigation, l'ecran nuit et la
 * fenetre d'alerte sont conserves et simplement remis a jour. */
static void apply_language(lang_t lang)
{
    settings_lang_set(lang); /* applique la langue puis demande l'enregistrement NVS differe */

    ui_consumables_close_dialog(); /* une boite de dialogue ouverte resterait dans l'ancienne langue */

    fill_nav_map();
    lv_buttonmatrix_set_map(s_nav, k_nav_map);
    lv_buttonmatrix_set_button_ctrl_all(s_nav, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_nav, true);
    for (uint32_t i = 0; i < UI_PAGE_COUNT; i++) {
        lv_buttonmatrix_set_button_width(s_nav, i, k_nav_widths[i]);
    }
    ui_set_text(s_demo, tr(S_DEMO_MODE));

    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        lv_obj_clean(s_pages[i]); /* les pages elles-memes restent, seul leur contenu est refait */
    }
    ui_home_create(s_pages[UI_PAGE_HOME]);
    ui_graph_create(s_pages[UI_PAGE_GRAPH]);
    ui_today_create(s_pages[UI_PAGE_TODAY]);
    ui_consumables_create(s_pages[UI_PAGE_CONSUMABLES]);
    ui_settings_create(s_pages[UI_PAGE_SETTINGS]);
    ui_alert_relang();
    night_mode_relang();
    voice_lang_changed(); /* la banque vocale de la nouvelle langue sera cherchee a nouveau */

    ui_show_page(s_page); /* reaffiche la page courante et force un rafraichissement complet */
    ESP_LOGI(TAG, "Langue de l'interface appliquee : %s (pages reconstruites)", lang_code(lang));
}

void ui_request_lang(int lang)
{
    if (lang >= 0 && lang < (int)LANG_COUNT) {
        s_requested_lang = lang;
    }
}

static void on_display_refresh(lv_event_t *e)
{
    const int64_t now_us = esp_timer_get_time();
    if (lv_event_get_code(e) == LV_EVENT_REFR_START) {
        s_refr_start_us = now_us;
        return;
    }
    if (s_refr_start_us != 0) {
        s_refr_last_ms = (uint32_t)((now_us - s_refr_start_us) / 1000);
        s_refr_max_ms = s_refr_last_ms > s_refr_max_ms ? s_refr_last_ms : s_refr_max_ms;
        s_refr_count++;
    }
}

/* Test d'endurance du rendu : invalide tout l'ecran a chaque cycle pendant la duree demandee */
static void stress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_stress_until_us != 0) {
        if (esp_timer_get_time() < s_stress_until_us) {
            lv_obj_t *scr = lv_screen_active();
            if (s_stress_mode == 2) {
                static uint32_t phase;
                phase++;
                for (int i = 0; i < 24; i++) {
                    const int32_t x = (int32_t)((i * 211u + phase * 37u) % (UI_SCR_W - 48));
                    const int32_t y = (int32_t)((i * 97u + phase * 53u) % (UI_SCR_H - 48));
                    const lv_area_t area = {.x1 = x, .y1 = y, .x2 = x + 40, .y2 = y + 40};
                    lv_obj_invalidate_area(scr, &area);
                }
            } else {
                lv_obj_invalidate(scr);
            }
        } else {
            s_stress_until_us = 0;
            ESP_LOGI(TAG, "Test de rendu termine");
        }
    }
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const int64_t start_us = esp_timer_get_time();
    s_heartbeat_us = start_us;
    ui_setup_tick(); /* assistant de configuration : recherche de reseaux, essai de connexion */
    if (!s_wdt_subscribed) {
        /* La tache LVGL est surveillee par le watchdog de taches : redemarrage si elle se bloque */
        s_wdt_subscribed = esp_task_wdt_add(NULL) == ESP_OK;
        if (s_wdt_subscribed) {
            ESP_LOGI(TAG, "Tache LVGL surveillee par le watchdog");
        }
    }
    if (s_wdt_subscribed) {
        esp_task_wdt_reset();
    }

    collect_data();
    update_top_bar();

    if (s_requested_hours != 0) {
        ui_set_chart_hours(s_requested_hours);
        s_requested_hours = 0;
    }
    if (s_requested_dialog == 1 || s_requested_dialog == 3) {
        ui_show_page(UI_PAGE_CONSUMABLES);
        ui_consumables_open_dialog(s_requested_dialog == 1 ? CONSUMABLE_SENSOR : CONSUMABLE_CATHETER);
    } else if (s_requested_dialog == 2) {
        ui_consumables_close_dialog();
    }
    s_requested_dialog = 0;
    if (s_requested_new >= 0) {
        const consumable_t c = (consumable_t)s_requested_new;
        s_requested_new = -1;
        s_new_result = consumable_start_now(c, "voix") == ESP_OK ? 1 : -1;
        s_new_done_seq = s_new_seq;
        if (s_new_result > 0) {
            ui_consumables_close_dialog();
            s_requested_page = UI_PAGE_CONSUMABLES; /* retour visuel du nouveau compteur */
        }
    }
    if (s_requested_alert == 1) {
        ui_alert_acknowledge();
    } else if (s_requested_alert == 2) {
        ui_alert_snooze();
    }
    s_requested_alert = 0;
    if (s_requested_scroll >= 0) {
        lv_obj_scroll_to_y(s_pages[UI_PAGE_SETTINGS], s_requested_scroll, LV_ANIM_OFF);
        s_requested_scroll = -1;
    }
    if (s_requested_lang >= 0) {
        const lang_t lang = (lang_t)s_requested_lang;
        s_requested_lang = -1;
        apply_language(lang);
    }
    if (s_requested_page >= 0) {
        ui_show_page((ui_page_t)s_requested_page);
        s_requested_page = -1;
    } else {
        update_page(s_page, false);
    }
    ui_alert_update();
    night_mode_update(&g_ui);
    touch_diag_ui_timer_ms((uint32_t)((esp_timer_get_time() - start_us) / 1000));
}

/* ------------------------------------------------------------------------- */

bool ui_main_create(void)
{
    g_ui.hist = heap_caps_calloc(GLY_HISTORY_MAX, sizeof(glucose_point_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (g_ui.hist == NULL) {
        return false;
    }
    ui_fonts_init();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    ui_screen_background(scr); /* degrade bleu nuit vers noir, peint une seule fois pour toutes les pages */
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    touch_diag_name(scr, "ecran");

    /* Bandeau superieur (couleur pleine UI_COL_BAR, comme la pastille d'alerte, et filet fin) */
    lv_obj_t *bar = ui_box(scr, 0, 0, UI_SCR_W, UI_TOP_H, UI_COL_BAR);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, UI_G_LINE, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    touch_diag_name(bar, "bandeau");
    /* Date a gauche, heure a droite (K17 : plus aucun voyant de liaison, l'etat detaille reste dans REGLAGES) */
    s_date = ui_label(bar, g_fonts.m, UI_COL_DATE, "");
    lv_obj_set_width(s_date, 420);
    lv_label_set_long_mode(s_date, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_align(s_date, LV_ALIGN_LEFT_MID, 20, 0);
    s_clock = ui_label(bar, g_fonts.clock, UI_COL_TEXT, "--:--");
    lv_obj_set_width(s_clock, 200);
    lv_obj_set_style_text_align(s_clock, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_clock, LV_ALIGN_RIGHT_MID, -20, 0);

    /* Ligne d'etat entre la date et l'heure : MODE DEMO puis indicateur d'alerte persistant */
    lv_obj_t *status = lv_obj_create(bar);
    lv_obj_remove_style_all(status);
    lv_obj_set_size(status, 400, UI_TOP_H); /* s'arrete avant l'heure, qui est plus large depuis K18 */
    lv_obj_align(status, LV_ALIGN_LEFT_MID, 456, 0);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status, 12, 0);
    lv_obj_remove_flag(status, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    s_demo = ui_label(status, g_fonts.nav, UI_COL_TEXT, tr(S_DEMO_MODE));
    lv_obj_set_style_bg_color(s_demo, UI_COL_RED, 0);
    lv_obj_set_style_bg_opa(s_demo, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(s_demo, 12, 0);
    lv_obj_set_style_pad_ver(s_demo, 4, 0);
    lv_obj_set_style_radius(s_demo, 8, 0);
    lv_obj_add_flag(s_demo, LV_OBJ_FLAG_HIDDEN);
    ui_alert_create_indicator(status);

    /* Commande vocale : "Repetez pour confirmer" (fenetre de 10 s) ou ecoute apres le mot d'eveil */
    s_voice_pill = lv_obj_create(status);
    lv_obj_remove_style_all(s_voice_pill);
    lv_obj_set_size(s_voice_pill, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_pad_hor(s_voice_pill, 14, 0);
    lv_obj_set_style_radius(s_voice_pill, 18, 0);
    lv_obj_set_style_bg_opa(s_voice_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_voice_pill, UI_COL_BLUE, 0);
    lv_obj_remove_flag(s_voice_pill, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *voice_label = ui_label(s_voice_pill, g_fonts.label, UI_COL_TEXT, "");
    lv_obj_center(voice_label);
    lv_obj_add_flag(s_voice_pill, LV_OBJ_FLAG_HIDDEN);

    /* Pages */
    static const char *const k_page_names[UI_PAGE_COUNT] = {"page ACCUEIL", "page GRAPHIQUE", "page AUJOURD'HUI",
                                                            "page CONSOMMABLES", "page RÉGLAGES"};
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        s_pages[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(s_pages[i]);
        lv_obj_set_pos(s_pages[i], 0, UI_TOP_H);
        lv_obj_set_size(s_pages[i], UI_SCR_W, UI_CONTENT_H);
        lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        touch_diag_name(s_pages[i], k_page_names[i]);
    }
    ui_home_create(s_pages[UI_PAGE_HOME]);
    ui_graph_create(s_pages[UI_PAGE_GRAPH]);
    ui_today_create(s_pages[UI_PAGE_TODAY]);
    ui_consumables_create(s_pages[UI_PAGE_CONSUMABLES]);
    ui_settings_create(s_pages[UI_PAGE_SETTINGS]);

    /* Barre de navigation */
    s_nav = lv_buttonmatrix_create(scr);
    touch_diag_name(s_nav, "navigation");
    fill_nav_map();
    lv_buttonmatrix_set_map(s_nav, k_nav_map);
    lv_buttonmatrix_set_button_ctrl_all(s_nav, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_nav, true);
    for (uint32_t i = 0; i < UI_PAGE_COUNT; i++) {
        lv_buttonmatrix_set_button_width(s_nav, i, k_nav_widths[i]);
    }
    lv_obj_set_pos(s_nav, 0, UI_SCR_H - UI_NAV_H);
    lv_obj_set_size(s_nav, UI_SCR_W, UI_NAV_H);
    ui_pill_buttons_style(s_nav, g_fonts.navbar, false);
    /* Barre pleine (couleur du bandeau) avec filet fin en haut ; page active en pilule bleue lumineuse */
    lv_obj_set_style_bg_opa(s_nav, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_nav, UI_COL_BAR, 0);
    lv_obj_set_style_border_side(s_nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(s_nav, UI_G_LINE, 0);
    lv_obj_set_style_border_width(s_nav, 1, 0);
    lv_obj_add_event_cb(s_nav, on_nav_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* Fenetre d'alerte au-dessus de tout */
    ui_alert_create(scr);
    /* Assistant de configuration : au-dessus des pages, ouvert tant que rien n'est renseigne */
    ui_setup_create();
    /* Mode nuit : retroeclairage seulement (reveil par toucher sur lv_layer_top) */
    night_mode_init();

    collect_data();
    update_top_bar();
    ui_show_page(UI_PAGE_HOME);
    if (!credentials_configured() && !credentials_demo()) {
        ui_setup_open(true);
    }
    lv_display_t *disp = lv_display_get_default();
    lv_display_add_event_cb(disp, on_display_refresh, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(disp, on_display_refresh, LV_EVENT_REFR_READY, NULL);
    lv_timer_create(stress_timer_cb, 20, NULL);
    s_heartbeat_us = esp_timer_get_time();
    lv_timer_create(ui_timer_cb, 1000, NULL);
    ESP_LOGI(TAG, "Interface creee (LVGL %d.%d.%d) : %d pages, style 2.5D", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
             LVGL_VERSION_PATCH, UI_PAGE_COUNT);
    return true;
}

void ui_test_set_age_offset(int minutes)
{
    s_test_age_offset_min = minutes;
}

void ui_request_page(int page)
{
    if (page >= 0 && page < UI_PAGE_COUNT) {
        s_requested_page = page;
    }
}

void ui_request_chart_hours(int hours)
{
    s_requested_hours = hours;
}

void ui_request_consumable_dialog(int kind)
{
    s_requested_dialog = kind == CONSUMABLE_SENSOR ? 1 : (kind == CONSUMABLE_CATHETER ? 3 : 2);
}

uint32_t ui_request_consumable_new(int kind)
{
    if (kind != CONSUMABLE_SENSOR && kind != CONSUMABLE_CATHETER) {
        return 0;
    }
    s_new_seq++;
    s_requested_new = kind;
    return s_new_seq;
}

int ui_consumable_new_result(uint32_t seq)
{
    return s_new_done_seq == seq ? s_new_result : 0;
}

void ui_request_alert_button(bool acknowledge)
{
    s_requested_alert = acknowledge ? 1 : 2;
}

void ui_request_settings_scroll(int y)
{
    s_requested_scroll = y;
}

int64_t ui_heartbeat_age_ms(void)
{
    return (esp_timer_get_time() - s_heartbeat_us) / 1000;
}

void ui_request_render_stress(int seconds, int mode)
{
    s_stress_mode = mode == 2 ? 2 : 1;
    s_stress_until_us = seconds > 0 ? esp_timer_get_time() + (int64_t)seconds * 1000000 : 0;
    ESP_LOGI(TAG, "Test de rendu : %d s, %s", seconds, s_stress_mode == 2 ? "24 petites zones par cycle" : "plein ecran");
}

void ui_print_stats(void)
{
    static uint32_t s_prev_count;
    static int64_t s_prev_us;
    const int64_t now_us = esp_timer_get_time();
    const uint32_t count = s_refr_count;
    const float fps = s_prev_us != 0 && now_us > s_prev_us ? (count - s_prev_count) * 1e6f / (float)(now_us - s_prev_us) : 0.0f;
    s_prev_count = count;
    s_prev_us = now_us;
    printf("UI_INFO cycles_rafraichissement=%" PRIu32 " (%.1f/s depuis la derniere lecture) rendu_dernier=%" PRIu32
           " ms rendu_max=%" PRIu32 " ms battement_ui=%" PRId64 " ms test_rendu=%s\n", count, fps, s_refr_last_ms,
           s_refr_max_ms, ui_heartbeat_age_ms(), s_stress_until_us != 0 ? "actif" : "inactif");
}

/* ------------------------------------------------------------------------- */
/* Capture d'ecran RLE RGB565 encodee en base64 sur la console (outil de developpement) */

#define SNAP_RAW_CHUNK 3000
static uint8_t *s_snap_raw;      /* PSRAM, alloue a la premiere capture */
static unsigned char *s_snap_b64;

static void snapshot_flush(uint8_t *raw, size_t *raw_len, bool final)
{
    size_t usable = final ? *raw_len : (*raw_len / 3) * 3;
    if (usable == 0) {
        return;
    }
    size_t out_len = 0;
    if (mbedtls_base64_encode(s_snap_b64, 4200 - 1, &out_len, raw, usable) == 0) {
        s_snap_b64[out_len] = '\0';
        printf("S:%s\n", (char *)s_snap_b64);
    }
    memmove(raw, raw + usable, *raw_len - usable);
    *raw_len -= usable;
}

void ui_snapshot_dump(void)
{
    if (s_snap_raw == NULL) {
        s_snap_raw = heap_caps_malloc(SNAP_RAW_CHUNK + 8, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        s_snap_b64 = heap_caps_malloc(4200, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_snap_raw == NULL || s_snap_b64 == NULL) {
            printf("SNAP_ERR memoire\n");
            return;
        }
    }
    if (!bsp_display_lock(5000)) {
        printf("SNAP_ERR lock\n");
        return;
    }
    /* L'assistant de configuration occupe toute la couche superieure : sans ce choix, la capture
     * montrerait la page qui se trouve DERRIERE lui, ce qui est trompeur. */
    lv_obj_t *source = ui_setup_visible() ? lv_layer_top() : lv_screen_active();
    lv_draw_buf_t *snap = lv_snapshot_take(source, LV_COLOR_FORMAT_RGB565);
    bsp_display_unlock();
    if (snap == NULL) {
        printf("SNAP_ERR memoire\n");
        return;
    }

    const uint32_t w = snap->header.w;
    const uint32_t h = snap->header.h;
    const uint32_t stride = snap->header.stride;
    printf("SNAP_BEGIN %" PRIu32 " %" PRIu32 " RLE565\n", w, h);

    uint8_t *raw = s_snap_raw;
    size_t raw_len = 0;
    uint16_t run_color = 0;
    uint32_t run_len = 0;
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *row = snap->data + y * stride;
        for (uint32_t x = 0; x < w; x++) {
            const uint16_t px = (uint16_t)(row[2 * x] | (row[2 * x + 1] << 8));
            if (run_len > 0 && (px != run_color || run_len == 0xFFFF)) {
                raw[raw_len++] = (uint8_t)(run_len & 0xFF);
                raw[raw_len++] = (uint8_t)(run_len >> 8);
                raw[raw_len++] = (uint8_t)(run_color & 0xFF);
                raw[raw_len++] = (uint8_t)(run_color >> 8);
                run_len = 0;
                if (raw_len >= SNAP_RAW_CHUNK) {
                    snapshot_flush(raw, &raw_len, false);
                }
            }
            run_color = px;
            run_len++;
        }
    }
    if (run_len > 0) {
        raw[raw_len++] = (uint8_t)(run_len & 0xFF);
        raw[raw_len++] = (uint8_t)(run_len >> 8);
        raw[raw_len++] = (uint8_t)(run_color & 0xFF);
        raw[raw_len++] = (uint8_t)(run_color >> 8);
    }
    snapshot_flush(raw, &raw_len, true);
    printf("SNAP_END\n");
    fflush(stdout);

    if (bsp_display_lock(5000)) {
        lv_draw_buf_destroy(snap);
        bsp_display_unlock();
    }
}
