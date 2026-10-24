/*
 * ui_alert.c: On-screen alert panel and acknowledgement controls.
 * Design note: On-screen alerts are supplementary to official notifications.
 */

/*
 * Renders and updates a single priority alert overlay without changing the alert engine.
 * Displays a priority alert sign without changing the alert rules.
 */
#include "ui_alert.h"

#include "i18n.h"

#include <stdio.h>
#include <string.h>
#include "alerts.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "touch_diag.h"
#include "ui_common.h"
#include "ui_pages.h"

static const char *TAG = "ui_alerte";

#define PANEL_MAX_US (30 * 1000000LL) /* full screen panel: 30 s at most, then back to reception */

static lv_obj_t *s_overlay;
static lv_obj_t *s_panel;
static lv_obj_t *s_title;
static lv_obj_t *s_message;
static lv_obj_t *s_pill;
static lv_obj_t *s_pill_label;
static lv_obj_t *s_ok_button;
static lv_obj_t *s_snooze_button;

static bool s_panel_visible;
static int64_t s_panel_since_us;
static uint32_t s_presented_seq[ALERT_COUNT]; /* last lift already in full screen, by type */

/* Display content: updated only if it changes (no redrawing every second) */
static alert_type_t s_panel_style = ALERT_NONE;
static char s_panel_title[48];
static char s_panel_message[96];
static alert_type_t s_pill_style = ALERT_NONE;
static bool s_pill_pending;
static char s_pill_text[64]; /* titre (47 caracteres au plus) + " (TEST)" */

static void alert_colors(alert_type_t style, lv_color_t *bg, lv_color_t *text)
{
    *bg = UI_COL_RED;
    *text = UI_COL_TEXT;
    switch (style) {
    case ALERT_HIGH:
    case ALERT_NO_DATA:
        *bg = UI_COL_ORANGE;
        *text = UI_COL_BG;
        break;
    case ALERT_SENSOR:
    case ALERT_CATHETER:
    case ALERT_TEST:
        *bg = UI_COL_BLUE;
        break;
    default:
        break;
    }
}

/* Display the highest-priority pending alert above regular pages. */
static void show_panel(void)
{
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
    s_panel_visible = true;
    s_panel_since_us = esp_timer_get_time();
}

static void hide_panel(void)
{
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    s_panel_visible = false;
}

/* Refresh only the visible alert content when data or language changes. */
static void refresh_panel(const alert_view_t *v)
{
    if (v->style != s_panel_style) {
        lv_color_t bg;
        lv_color_t text;
        alert_colors(v->style, &bg, &text);
        lv_obj_set_style_bg_color(s_panel, bg, 0);
        lv_obj_set_style_text_color(s_title, text, 0);
        lv_obj_set_style_text_color(s_message, text, 0);
        s_panel_style = v->style;
    }
    if (strcmp(v->title, s_panel_title) != 0) {
        strlcpy(s_panel_title, v->title, sizeof(s_panel_title));
        lv_label_set_text(s_title, s_panel_title);
    }
    if (strcmp(v->message, s_panel_message) != 0) {
        strlcpy(s_panel_message, v->message, sizeof(s_panel_message));
        lv_label_set_text(s_message, s_panel_message);
    }
}

static void refresh_indicator(const alert_view_t *v)
{
    const bool pending = v->type != ALERT_NONE;
    const alert_type_t style = pending ? v->style : v->ongoing;
    char text[sizeof(s_pill_text)];
    if (pending) {
        const bool simulated = v->type == ALERT_TEST && v->style != ALERT_TEST;
        snprintf(text, sizeof(text), "%s%s", v->title, simulated ? tr(S_ALERT_TEST_SUFFIX) : "");
    } else {
        snprintf(text, sizeof(text), "%s%s", alerts_title(v->ongoing),
                 v->ongoing_test ? tr(S_ALERT_TEST_SUFFIX) : "");
    }
    if (style == s_pill_style && pending == s_pill_pending && strcmp(text, s_pill_text) == 0) {
        return;
    }
    s_pill_style = style;
    s_pill_pending = pending;
    strlcpy(s_pill_text, text, sizeof(s_pill_text));
    if (style == ALERT_NONE) {
        lv_obj_add_flag(s_pill, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_color_t bg;
    lv_color_t fg;
    alert_colors(style, &bg, &fg);
    /* not acquitted: full tablet; condition still present after OK or RAPPEL : contour seul */
    lv_obj_set_style_bg_color(s_pill, pending ? bg : UI_COL_BAR, 0);
    lv_obj_set_style_border_color(s_pill, bg, 0);
    lv_obj_set_style_text_color(s_pill_label, pending ? fg : bg, 0);
    lv_label_set_text(s_pill_label, s_pill_text);
    lv_obj_remove_flag(s_pill, LV_OBJ_FLAG_HIDDEN);
}

/* Acknowledge the displayed event in the alert engine, not in glucose history. */
static void on_ok(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Bouton OK touche");
    alerts_acknowledge();
    hide_panel();
}

/* Apply the alert's defined snooze policy instead of dismissing it permanently. */
static void on_snooze(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Bouton RAPPEL 30 MIN touche");
    alerts_snooze(30);
    hide_panel();
}

static void on_pill(lv_event_t *e)
{
    (void)e;
    alert_view_t v;
    alerts_get_view(&v);
    if (v.type == ALERT_NONE) {
        return; /* alerte deja acquittee ou reportee : indicateur informatif */
    }
    ESP_LOGI(TAG, "Indicateur touche : panneau d'alerte rouvert (%s)", v.title);
    refresh_panel(&v);
    show_panel();
}

void ui_alert_create(lv_obj_t *screen)
{
    s_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, UI_SCR_W, UI_SCR_H);
    lv_obj_set_style_bg_color(s_overlay, UI_COL_BG, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_60, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    s_panel = ui_box(s_overlay, 72, 110, 880, 380, UI_COL_RED);
    lv_obj_set_style_radius(s_panel, 24, 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);

    s_title = ui_label(s_panel, g_fonts.xl, UI_COL_TEXT, "");
    lv_obj_set_width(s_title, 880);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_title, 0, 40);

    s_message = ui_label(s_panel, g_fonts.l, UI_COL_TEXT, "");
    lv_obj_set_width(s_message, 820);
    lv_obj_set_style_text_align(s_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_message, 30, 130);

    lv_obj_t *ok = ui_button(s_panel, tr(S_ALERT_OK), g_fonts.title, 360, 96, on_ok, NULL);
    s_ok_button = ok;
    lv_obj_set_pos(ok, 40, 250);
    lv_obj_set_style_bg_color(ok, lv_color_hex(0x111111), 0);
    lv_obj_t *snooze = ui_button(s_panel, tr(S_ALERT_SNOOZE), g_fonts.nav, 400, 96, on_snooze, NULL);
    s_snooze_button = snooze;
    lv_obj_set_pos(snooze, 440, 250);
    lv_obj_set_style_bg_color(snooze, lv_color_hex(0x333333), 0);

    touch_diag_name(s_overlay, "fond alerte");
    touch_diag_name(s_panel, "panneau alerte");
    touch_diag_name(ok, "OK");
    touch_diag_name(snooze, "RAPPEL DANS 30 MIN");

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* Language change: the alert sign and its indicator live on the screen, not in the
 * Pages rebuilt. Fixed titles are rewritten here, and empty text caches so that
 * the next refreshment rests title and message in the new language. */
void ui_alert_relang(void)
{
    if (s_ok_button != NULL) {
        lv_label_set_text(lv_obj_get_child(s_ok_button, 0), tr(S_ALERT_OK));
    }
    if (s_snooze_button != NULL) {
        lv_label_set_text(lv_obj_get_child(s_snooze_button, 0), tr(S_ALERT_SNOOZE));
    }
    s_panel_title[0] = '\0';
    s_panel_message[0] = '\0';
    s_pill_text[0] = '\0';
    s_pill_style = ALERT_NONE;
}

void ui_alert_create_indicator(lv_obj_t *parent)
{
    s_pill = lv_obj_create(parent);
    lv_obj_remove_style_all(s_pill);
    lv_obj_set_size(s_pill, LV_SIZE_CONTENT, 38);
    lv_obj_set_flex_flow(s_pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(s_pill, 16, 0);
    lv_obj_set_style_radius(s_pill, 19, 0);
    lv_obj_set_style_bg_opa(s_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pill, 3, 0);
    lv_obj_add_flag(s_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_ext_click_area(s_pill, 5);
    lv_obj_add_event_cb(s_pill, on_pill, LV_EVENT_CLICKED, NULL);
    s_pill_label = ui_label(s_pill, g_fonts.nav, UI_COL_TEXT, "");
    touch_diag_name(s_pill, "indicateur alerte");
    lv_obj_add_flag(s_pill, LV_OBJ_FLAG_HIDDEN);
}

/* Keep banner and overlay consistent with the engine's current event. */
void ui_alert_update(void)
{
    alert_view_t v;
    alerts_get_view(&v);
    refresh_indicator(&v);
    if (v.type == ALERT_NONE) {
        if (s_panel_visible) {
            hide_panel();
        }
        return;
    }
    refresh_panel(&v);
    if (v.seq != s_presented_seq[v.type]) {
        /* new levee: full screen panel */
        s_presented_seq[v.type] = v.seq;
        show_panel();
        ESP_LOGI(TAG, "Panneau d'alerte affiche : %s (30 s au plus)", v.title);
    } else if (s_panel_visible && esp_timer_get_time() - s_panel_since_us >= PANEL_MAX_US) {
        hide_panel();
        ui_show_page(UI_PAGE_HOME);
        ESP_LOGI(TAG, "Panneau d'alerte ferme apres 30 s : accueil, indicateur conserve, alerte toujours active (%s)",
                 v.title);
    }
}

bool ui_alert_panel_visible(void)
{
    return s_panel_visible;
}

alert_type_t ui_alert_panel_style(void)
{
    return s_panel_style;
}

void ui_alert_acknowledge(void)
{
    on_ok(NULL);
}

void ui_alert_snooze(void)
{
    on_snooze(NULL);
}
