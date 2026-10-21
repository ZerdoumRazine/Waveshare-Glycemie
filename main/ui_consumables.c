/*
 * Implements touchscreen confirmation for sensor, infusion set and stock changes.
 * Gère les confirmations tactiles des changements et des stocks.
 */
#include "i18n.h"
#include "ui_pages.h"

#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "sensor_session.h"
#include "stock.h"
#include "touch_diag.h"

static const char *TAG = "ui_conso";

/*
 * CONSOMMABLES : deux grandes sections, CAPTEUR G7 et SITE DE PERFUSION (catheter), et sous chacune une carte STOCK.
 * NOUVEAU CAPTEUR / NOUVEAU CATHETER : une seule confirmation, puis l'heure actuelle devient l'heure de depart
 * (aucune saisie de date ni d'heure) ; le stock renseigne perd alors une unite (une seule fois par changement).
 * STOCK : [-] quantite [+], appui sur la quantite = saisie directe ; "NON RENSEIGNE" tant que l'utilisateur n'a rien
 * saisi (aucune decrementation ne la transforme en 0) ; mention discrete "STOCK FAIBLE" au seuil.
 */

#define SEC_Y    12
#define SEC_W    482
#define SEC_H    372
#define SEC_PAD  24
#define STK_Y    392
#define STK_H    96
#define STK_PAD  20
#define STK_BTN  60
#define DLG_W    680
#define DLG_H    350
#define DLG_X    ((UI_SCR_W - DLG_W) / 2)
#define DLG_Y    120

typedef struct {
    lv_obj_t *subtitle;
    lv_obj_t *started;
    lv_obj_t *status;
    lv_obj_t *remaining;
    lv_obj_t *detail;
    lv_obj_t *bar;
} section_t;

typedef struct {
    lv_obj_t *title;
    lv_obj_t *low;      /* "STOCK FAIBLE" */
    lv_obj_t *value;    /* quantite (appui : saisie directe) */
    lv_obj_t *minus;
    lv_obj_t *plus;
    lv_obj_t *set;      /* RENSEIGNER (quantite inconnue) */
} stock_ui_t;

static section_t s_sections[CONSUMABLE_COUNT];
static stock_ui_t s_stock[STOCK_COUNT];
static lv_obj_t *s_backdrop;
static lv_obj_t *s_dialog_when;
static consumable_t s_dialog_kind;
static lv_obj_t *s_qty_backdrop;
static lv_obj_t *s_qty_value;
static stock_item_t s_qty_item;
static int s_qty_edit;
static uint32_t s_revision_drawn = UINT32_MAX;

/* Apply a consumable change only after an explicit confirmation. */
static void on_confirm(lv_event_t *e)
{
    (void)e;
    const consumable_t c = s_dialog_kind;
    ui_consumables_close_dialog();
    if (consumable_start_now(c, "bouton") != ESP_OK) {
        ESP_LOGW(TAG, "%s : enregistrement impossible (heure non synchronisee)", consumable_name(c));
    }
    ui_consumables_update(&g_ui, true);
}

static void on_cancel(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "%s annule", consumable_name(s_dialog_kind));
    ui_consumables_close_dialog();
}

void ui_consumables_close_dialog(void)
{
    if (s_backdrop != NULL) {
        lv_obj_delete_async(s_backdrop);
        s_backdrop = NULL;
        s_dialog_when = NULL;
    }
}

/* Fond plein ecran qui bloque les appuis ; la boite reste sur l'ecran actif (capture incluse) */
static lv_obj_t *dialog_backdrop(const char *name)
{
    lv_obj_t *backdrop = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, UI_SCR_W, UI_SCR_H);
    lv_obj_set_style_bg_color(backdrop, UI_COL_BG, 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_70, 0);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    touch_diag_name(backdrop, name);
    return backdrop;
}

static lv_obj_t *dialog_button(lv_obj_t *parent, const char *text, int32_t x, int32_t y, int32_t w, int32_t h,
                               lv_color_t bg, lv_event_cb_t cb, void *user_data, const char *name)
{
    lv_obj_t *btn = ui_button(parent, text, g_fonts.nav, w, h, cb, user_data);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    touch_diag_name(btn, name);
    return btn;
}

/* Open the right replacement dialog while leaving saved dates untouched until confirmation. */
void ui_consumables_open_dialog(consumable_t c)
{
    if (s_backdrop != NULL || c >= CONSUMABLE_COUNT) {
        return;
    }
    s_dialog_kind = c;
    s_backdrop = dialog_backdrop("fond confirmation");

    lv_obj_t *box = ui_card(s_backdrop, (UI_SCR_W - DLG_W) / 2, 150, DLG_W, 300, 24);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = ui_label(box, g_fonts.title, UI_COL_TEXT,
                               c == CONSUMABLE_SENSOR ? tr(S_NEW_SENSOR_NOW) : tr(S_NEW_CATHETER_NOW));
    lv_obj_set_width(title, DLG_W);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 34);

    s_dialog_when = ui_label(box, g_fonts.m, UI_COL_DATE, "");
    lv_obj_set_width(s_dialog_when, DLG_W);
    lv_obj_set_style_text_align(s_dialog_when, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_dialog_when, 0, 92);

    lv_obj_t *note = ui_label(box, g_fonts.xs, UI_COL_SUBTLE,
                              c == CONSUMABLE_SENSOR ? tr(S_RESTART_DAY1_SENSOR) : tr(S_RESTART_DAY1_CATHETER));
    lv_obj_set_width(note, DLG_W);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(note, 0, 132);

    dialog_button(box, tr(S_CANCEL), 32, 300 - 32 - 76, 290, 76, lv_color_hex(0x2A3550), on_cancel, NULL,
                  "ANNULER consommable");
    dialog_button(box, tr(S_YES_NOW), DLG_W - 32 - 290, 300 - 32 - 76, 290, 76, UI_COL_BLUE, on_confirm, NULL,
                  "OUI MAINTENANT consommable");
    ESP_LOGI(TAG, "Confirmation %s affichee", consumable_name(c));
    ui_consumables_update(&g_ui, true);
}

static void on_new(lv_event_t *e)
{
    const consumable_t c = (consumable_t)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Bouton %s touche", consumable_name(c));
    ui_consumables_open_dialog(c);
}

/* ------------------------------------------------------------------------- */
/* Stock : saisie directe de la quantite */

static void qty_refresh(void)
{
    if (s_qty_value == NULL) {
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", s_qty_edit);
    ui_set_text(s_qty_value, buf);
}

static void qty_close(void)
{
    if (s_qty_backdrop != NULL) {
        lv_obj_delete_async(s_qty_backdrop);
        s_qty_backdrop = NULL;
        s_qty_value = NULL;
    }
}

static void on_qty_step(lv_event_t *e)
{
    const int delta = (int)(intptr_t)lv_event_get_user_data(e);
    s_qty_edit += delta;
    if (s_qty_edit < 0) {
        s_qty_edit = 0;
    } else if (s_qty_edit > STOCK_MAX) {
        s_qty_edit = STOCK_MAX;
    }
    qty_refresh();
}

static void on_qty_cancel(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Saisie du stock annulee");
    qty_close();
}

/* Validate a requested quantity before saving it to local stock. */
static void on_qty_ok(lv_event_t *e)
{
    (void)e;
    const stock_item_t item = s_qty_item;
    const int value = s_qty_edit;
    qty_close();
    stock_set(item, value);
    ui_consumables_update(&g_ui, true);
}

static void open_qty_dialog(stock_item_t item)
{
    if (s_qty_backdrop != NULL || item >= STOCK_COUNT) {
        return;
    }
    s_qty_item = item;
    const int current = stock_get(item);
    s_qty_edit = current == STOCK_UNKNOWN ? 0 : current;
    s_qty_backdrop = dialog_backdrop("fond saisie stock");

    lv_obj_t *box = ui_card(s_qty_backdrop, DLG_X, DLG_Y, DLG_W, DLG_H, 24);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);

    char title_text[40];
    snprintf(title_text, sizeof(title_text), "%s",
             tr(item == STOCK_SENSOR ? S_STOCK_SENSORS : S_STOCK_CATHETERS));
    lv_obj_t *title = ui_label(box, g_fonts.title, UI_COL_TEXT, title_text);
    lv_obj_set_width(title, DLG_W);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 18);

    s_qty_value = ui_label(box, g_fonts.delta, UI_COL_TEXT, "0");
    lv_obj_set_width(s_qty_value, DLG_W);
    lv_obj_set_style_text_align(s_qty_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_qty_value, 0, 64);
    qty_refresh();

    static const char *const k_steps[4] = {"−10", "−1", "+1", "+10"};
    static const int k_deltas[4] = {-10, -1, 1, 10};
    for (int i = 0; i < 4; i++) {
        dialog_button(box, k_steps[i], 36 + i * 156, 168, 140, 64, lv_color_hex(0x2A3550), on_qty_step,
                      (void *)(intptr_t)k_deltas[i], k_steps[i]);
    }
    dialog_button(box, tr(S_CANCEL), 24, 254, 290, 72, lv_color_hex(0x2A3550), on_qty_cancel, NULL,
                  "ANNULER stock");
    dialog_button(box, tr(S_VALIDATE), DLG_W - 24 - 290, 254, 290, 72, UI_COL_BLUE, on_qty_ok, NULL, "VALIDER stock");
    ESP_LOGI(TAG, "Saisie du stock %s ouverte (valeur actuelle %d)", stock_name(item), current);
}

static void on_stock_open(lv_event_t *e)
{
    open_qty_dialog((stock_item_t)(intptr_t)lv_event_get_user_data(e));
}

/* user_data : (item << 1) | (1 = +1, 0 = -1) */
static void on_stock_step(lv_event_t *e)
{
    const intptr_t code = (intptr_t)lv_event_get_user_data(e);
    const stock_item_t item = (stock_item_t)(code >> 1);
    stock_add(item, (code & 1) ? 1 : -1);
    ui_consumables_update(&g_ui, true);
}

static void stock_create(lv_obj_t *page, stock_item_t item, int32_t x, const char *title)
{
    stock_ui_t *s = &s_stock[item];
    lv_obj_t *card = ui_card(page, x, STK_Y, SEC_W, STK_H, 20);

    s->title = ui_label(card, g_fonts.label, UI_G_TEXT2, title);
    lv_obj_set_pos(s->title, STK_PAD, 8);

    s->low = ui_label(card, g_fonts.label, UI_G_HIGH, tr(S_STOCK_LOW));
    lv_obj_set_width(s->low, 180);
    lv_obj_set_style_text_align(s->low, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s->low, SEC_W - STK_PAD - 180, 8);
    lv_obj_add_flag(s->low, LV_OBJ_FLAG_HIDDEN);

    s->minus = ui_button(card, "−", g_fonts.xl, STK_BTN, STK_BTN, on_stock_step, (void *)(intptr_t)(item << 1));
    lv_obj_set_pos(s->minus, STK_PAD, 30);
    lv_obj_set_style_radius(s->minus, 16, 0);
    lv_obj_set_style_bg_color(s->minus, lv_color_hex(0x2A3550), 0);
    touch_diag_name(s->minus, item == STOCK_SENSOR ? "stock capteur -" : "stock catheter -");

    s->plus = ui_button(card, "+", g_fonts.xl, STK_BTN, STK_BTN, on_stock_step, (void *)(intptr_t)((item << 1) | 1));
    lv_obj_set_pos(s->plus, SEC_W - STK_PAD - STK_BTN, 30);
    lv_obj_set_style_radius(s->plus, 16, 0);
    lv_obj_set_style_bg_color(s->plus, lv_color_hex(0x2A3550), 0);
    touch_diag_name(s->plus, item == STOCK_SENSOR ? "stock capteur +" : "stock catheter +");

    /* Quantite : appui = saisie directe (zone de clic elargie) */
    s->value = ui_label(card, g_fonts.card, UI_G_TEXT, "--");
    lv_obj_set_width(s->value, SEC_W - 2 * (STK_PAD + STK_BTN) - 20);
    lv_label_set_long_mode(s->value, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(s->value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s->value, STK_PAD + STK_BTN + 10, 36);
    lv_obj_add_flag(s->value, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s->value, 8);
    lv_obj_add_event_cb(s->value, on_stock_open, LV_EVENT_CLICKED, (void *)(intptr_t)item);
    touch_diag_name(s->value, item == STOCK_SENSOR ? "stock capteur valeur" : "stock catheter valeur");

    s->set = ui_button(card, tr(S_SET_IT), g_fonts.nav, 180, STK_BTN, on_stock_open, (void *)(intptr_t)item);
    lv_obj_set_pos(s->set, SEC_W - STK_PAD - 180, 30);
    lv_obj_set_style_radius(s->set, 16, 0);
    lv_obj_set_style_bg_color(s->set, UI_G_ACCENT, 0);
    lv_obj_set_style_bg_grad_color(s->set, UI_G_ACCENT_DEEP, 0);
    lv_obj_set_style_bg_grad_dir(s->set, LV_GRAD_DIR_VER, 0);
    touch_diag_name(s->set, item == STOCK_SENSOR ? "RENSEIGNER capteur" : "RENSEIGNER catheter");
    /* Etat initial coherent avec [-] et [+] visibles : RENSEIGNER masque (il recouvre le bouton [+]) */
    lv_obj_add_flag(s->set, LV_OBJ_FLAG_HIDDEN);
}

/* ------------------------------------------------------------------------- */

static lv_obj_t *section_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int32_t y)
{
    lv_obj_t *label = ui_label(parent, font, color, "");
    lv_obj_set_width(label, SEC_W - 2 * SEC_PAD);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(label, SEC_PAD, y);
    return label;
}

static void section_create(lv_obj_t *page, consumable_t c, int32_t x, const char *title, const char *subtitle,
                           const char *button)
{
    section_t *s = &s_sections[c];
    lv_obj_t *card = ui_card(page, x, SEC_Y, SEC_W, SEC_H, 22);

    lv_obj_t *t = section_label(card, g_fonts.title, UI_COL_TEXT, 8);
    lv_label_set_text(t, title);
    s->subtitle = section_label(card, g_fonts.xs, UI_COL_FAINT, 52);
    lv_label_set_text(s->subtitle, subtitle);

    s->started = section_label(card, g_fonts.m, UI_COL_SOFT, 82);
    s->status = section_label(card, g_fonts.xl, UI_COL_TEXT, 118);
    s->remaining = section_label(card, g_fonts.m, UI_COL_SUBTLE, 188);
    s->detail = section_label(card, g_fonts.m, UI_COL_SOFT, 224);
    s->bar = ui_progress(card, SEC_PAD, 268, SEC_W - 2 * SEC_PAD, 14);

    lv_obj_t *btn = ui_button(card, button, g_fonts.nav, SEC_W - 2 * SEC_PAD, 64, on_new, (void *)(intptr_t)c);
    lv_obj_set_pos(btn, SEC_PAD, SEC_H - 14 - 64);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_bg_color(btn, UI_G_ACCENT, 0);
    lv_obj_set_style_bg_grad_color(btn, UI_G_ACCENT_DEEP, 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_shadow_color(btn, UI_G_ACCENT, 0);
    lv_obj_set_style_shadow_width(btn, 10, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_40, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x6AA2FF), LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(btn, UI_G_ACCENT, LV_STATE_PRESSED);
    touch_diag_name(btn, button);
}

void ui_consumables_create(lv_obj_t *page)
{
    /* Reconstruction (changement de langue) : les boites de dialogue sont filles de l'ecran, pas de
     * la page. ui_main.c les ferme avant de reconstruire ; ces pointeurs sont remis a zero par surete. */
    s_backdrop = NULL;
    s_dialog_when = NULL;
    s_qty_backdrop = NULL;
    s_qty_value = NULL;
    s_revision_drawn = UINT32_MAX;

    ui_navy_background(page);
    section_create(page, CONSUMABLE_SENSOR, 20, tr(S_SENSOR_G7), tr(S_SENSOR_SUB), tr(S_NEW_SENSOR));
    section_create(page, CONSUMABLE_CATHETER, 522, tr(S_INFUSION_SITE), "", tr(S_NEW_CATHETER));
    stock_create(page, STOCK_SENSOR, 20, tr(S_STOCK_SENSORS));
    stock_create(page, STOCK_CATHETER, 522, tr(S_STOCK_CATHETERS));
}

static void set_bar(lv_obj_t *bar, int value, lv_color_t color)
{
    lv_bar_set_value(bar, value < 0 ? 0 : (value > 100 ? 100 : value), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

static void show_unknown(section_t *s, const char *what)
{
    ui_set_text(s->started, what);
    ui_set_text(s->status, tr(S_NOT_RECORDED));
    ui_set_text_color(s->status, UI_G_STALE);
    ui_set_text(s->remaining, tr(S_BUTTON_BELOW));
    ui_set_text(s->detail, "");
    set_bar(s->bar, 0, UI_G_STALE);
}

/* Present the sensor countdown from the last recorded installation time. */
static void update_sensor(int64_t now_s)
{
    section_t *s = &s_sections[CONSUMABLE_SENSOR];
    sensor_status_t st;
    sensor_session_get(now_s, &st);
    if (!st.known) {
        show_unknown(s, tr(S_NO_SENSOR_RECORDED));
        return;
    }
    char buf[96];
    char when[32];
    char dur[24];
    ui_format_day_hm(st.start_s, when, sizeof(when));
    snprintf(buf, sizeof(buf), "%s : %s", tr(S_PLACED), when);
    ui_set_text(s->started, buf);

    /* echeance = pose + 10 jours exactement : la grace Dexcom de 12 h ne prolonge pas le compteur */
    lv_color_t color;
    ui_format_duration(st.remaining_s, false, dur, sizeof(dur));
    if (st.expired) {
        color = UI_G_CRIT;
        ui_set_text(s->status, tr(S_TO_CHANGE));
        snprintf(buf, sizeof(buf), "%s %s", tr(S_OVERDUE_SINCE), dur);
        set_bar(s->bar, 0, color);
    } else {
        color = st.remaining_s <= 2 * 3600 ? UI_G_CRIT : (st.remaining_s <= 24 * 3600 ? UI_G_HIGH : UI_G_OK);
        snprintf(buf, sizeof(buf), tr(S_DAY_N_OF), st.day, 10);
        ui_set_text(s->status, buf);
        snprintf(buf, sizeof(buf), "%s : %s", tr(S_TIME_LEFT), dur);
        set_bar(s->bar, (int)(st.remaining_s * 100 / SENSOR_SESSION_S), color);
    }
    ui_set_text_color(s->status, color);
    ui_set_text(s->remaining, buf);
    char due[32];
    ui_format_day_hm(st.due_s, due, sizeof(due));
    snprintf(buf, sizeof(buf), "%s : %s", tr(S_DUE), due);
    ui_set_text(s->detail, buf);
}

/* Present the catheter deadline according to the selected replacement interval. */
static void update_catheter(int64_t now_s)
{
    section_t *s = &s_sections[CONSUMABLE_CATHETER];
    catheter_status_t st;
    catheter_get(now_s, &st);
    if (!st.known) {
        show_unknown(s, tr(S_NO_CATHETER_RECORDED));
        return;
    }
    char buf[96];
    char when[32];
    char dur[24];
    ui_format_day_hm(st.start_s, when, sizeof(when));
    snprintf(buf, sizeof(buf), "%s : %s", tr(S_CHANGED), when);
    ui_set_text(s->started, buf);
    ui_format_duration(st.remaining_s, false, dur, sizeof(dur));
    lv_color_t color;
    if (st.overdue) {
        color = UI_G_CRIT;
        ui_set_text(s->status, tr(S_TO_CHANGE));
        snprintf(buf, sizeof(buf), "%s %s", tr(S_OVERDUE_SINCE), dur);
        set_bar(s->bar, 0, color);
    } else {
        color = st.remaining_s <= 2 * 3600 ? UI_G_CRIT : (st.remaining_s <= 24 * 3600 ? UI_G_HIGH : UI_G_OK);
        snprintf(buf, sizeof(buf), tr(S_DAY_N_OF), st.day, st.days_total);
        ui_set_text(s->status, buf);
        snprintf(buf, sizeof(buf), "%s : %s", tr(S_TIME_LEFT), dur);
        set_bar(s->bar, 100 - st.progress_pct, color);
    }
    ui_set_text_color(s->status, color);
    ui_set_text(s->remaining, buf);
    char due[32];
    ui_format_day_hm(st.due_s, due, sizeof(due));
    snprintf(buf, sizeof(buf), "%s : %s", tr(S_DUE), due);
    ui_set_text(s->detail, buf);
}

static void update_stock(stock_item_t item)
{
    stock_ui_t *s = &s_stock[item];
    const int value = stock_get(item);
    const bool known = value != STOCK_UNKNOWN;
    const bool low = stock_is_low(item);
    char buf[16];
    const lv_font_t *font = known ? g_fonts.card : g_fonts.m;
    if (lv_obj_get_style_text_font(s->value, 0) != font) {
        lv_obj_set_style_text_font(s->value, font, 0);
    }
    if (known) {
        snprintf(buf, sizeof(buf), "%d", value);
        ui_set_text(s->value, buf);
    } else {
        ui_set_text(s->value, tr(S_NOT_SET));
    }
    ui_set_text_color(s->value, known ? (low ? UI_G_HIGH : UI_G_TEXT) : UI_G_STALE);
    if (lv_obj_has_flag(s->minus, LV_OBJ_FLAG_HIDDEN) == known) {
        /* bascule entre [-] quantite [+] et NON RENSEIGNE + RENSEIGNER */
        if (known) {
            lv_obj_remove_flag(s->minus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(s->plus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s->set, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_width(s->value, SEC_W - 2 * (STK_PAD + STK_BTN) - 20);
            lv_obj_set_style_text_align(s->value, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(s->value, STK_PAD + STK_BTN + 10, 36);
        } else {
            lv_obj_add_flag(s->minus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s->plus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(s->set, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_width(s->value, SEC_W - 2 * STK_PAD - 200);
            lv_obj_set_style_text_align(s->value, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_pos(s->value, STK_PAD, 44);
        }
    }
    if (low == lv_obj_has_flag(s->low, LV_OBJ_FLAG_HIDDEN)) {
        if (low) {
            lv_obj_remove_flag(s->low, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s->low, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_consumables_update(const ui_data_t *d, bool force)
{
    if (s_dialog_when != NULL && d->time_ok) {
        /* l'heure enregistree sera celle de la confirmation : affichee a la minute */
        char when[48];
        char buf[96];
        snprintf(when, sizeof(when), tr(S_DATE_DAY_HM), d->now_tm.tm_mday, d->now_tm.tm_mon + 1,
                 d->now_tm.tm_hour, d->now_tm.tm_min);
        snprintf(buf, sizeof(buf), "%s : %s %s", tr(S_START_TIME_LABEL), i18n_day_name(d->now_tm.tm_wday), when);
        ui_set_text(s_dialog_when, buf);
    } else if (s_dialog_when != NULL) {
        ui_set_text(s_dialog_when, tr(S_TIME_NOT_SYNCED_NO_RECORD));
    }

    const uint32_t revision = consumables_revision() + stock_revision();
    if (!force && !d->minute_changed && revision == s_revision_drawn) {
        return;
    }
    s_revision_drawn = revision;
    char sub[48];
    snprintf(sub, sizeof(sub), tr(S_CATHETER_SUB), catheter_interval_h());
    ui_set_text(s_sections[CONSUMABLE_CATHETER].subtitle, sub);
    for (int i = 0; i < STOCK_COUNT; i++) {
        update_stock((stock_item_t)i);
    }

    if (!d->time_ok) {
        for (int c = 0; c < CONSUMABLE_COUNT; c++) {
            show_unknown(&s_sections[c], "Heure non synchronisée");
        }
        return;
    }
    const int64_t now_s = d->now_ms / 1000;
    update_sensor(now_s);
    update_catheter(now_s);
}
