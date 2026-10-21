/*
 * ui_common.c: Shared visual widgets, fonts and layout helpers.
 * FR: Composants visuels, polices et mise en page communs.
 * Design note: Consistent styles make the display legible from a distance.
 */

/*
 * Shares typography, colors, layout primitives and formatting across the five pages.
 * Centralise polices, couleurs, composants et formats de l'interface.
 */
#include "ui_common.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "i18n.h"

static const char *TAG = "ui";

/* Polices Montserrat (SIL Open Font License 1.1, voir main/fonts/OFL.txt), rendues par TinyTTF */
extern const uint8_t font_medium_start[] asm("_binary_Montserrat_Medium_ttf_start");
extern const uint8_t font_medium_end[] asm("_binary_Montserrat_Medium_ttf_end");
extern const uint8_t font_bold_start[] asm("_binary_Montserrat_Bold_ttf_start");
extern const uint8_t font_bold_end[] asm("_binary_Montserrat_Bold_ttf_end");

ui_fonts_t g_fonts;
ui_data_t g_ui;

static lv_font_t *make_font(bool bold, int32_t px)
{
    const uint8_t *start = bold ? font_bold_start : font_medium_start;
    const uint8_t *end = bold ? font_bold_end : font_medium_end;
    lv_font_t *font = lv_tiny_ttf_create_data_ex(start, (size_t)(end - start), px, LV_FONT_KERNING_NONE, 64);
    if (font == NULL) {
        ESP_LOGE(TAG, "Police TinyTTF %" PRId32 " px indisponible", px);
        return (lv_font_t *)&lv_font_montserrat_28;
    }
    return font;
}

lv_font_t *ui_font_create(bool bold, int32_t px)
{
    return make_font(bold, px);
}

/* Load the fonts once before any page creates text objects. */
void ui_fonts_init(void)
{
    g_fonts.value = make_font(true, 320);
    g_fonts.delta = make_font(true, 76);
    g_fonts.xl = make_font(true, 52);
    g_fonts.title = make_font(true, 34);
    g_fonts.l = make_font(false, 36);
    g_fonts.m = make_font(false, 28);
    g_fonts.s = make_font(false, 22);
    g_fonts.nav = make_font(true, 24);
    g_fonts.clock = make_font(true, 44);
    g_fonts.card = make_font(true, 44);
    g_fonts.xs = make_font(false, 20);
    g_fonts.label = make_font(true, 20);
    g_fonts.navbar = make_font(true, 21);
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t *ui_box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t bg)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_color(box, bg, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, const lv_font_t *font, int32_t w, int32_t h,
                    lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_color(btn, UI_COL_BLUE, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COL_BLUE, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t *label = ui_label(btn, font, UI_COL_TEXT, text);
    lv_obj_center(label);
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

/* Accent d'affichage : tres basse < 54, basse < 70, normale 70-180, elevee <= 250, tres elevee au-dela */
lv_color_t ui_glucose_color(int mgdl)
{
    if (mgdl < GLY_RANGE_VERY_LOW) {
        return UI_G_CRIT;
    }
    if (mgdl < GLY_RANGE_LOW) {
        return UI_G_LOW;
    }
    if (mgdl <= GLY_RANGE_HIGH) {
        return UI_G_OK;
    }
    if (mgdl <= GLY_RANGE_VERY_HIGH) {
        return UI_G_HIGH;
    }
    return UI_G_CRIT;
}

/* Distinguish a delayed reading from a freshly received one. */
lv_color_t ui_freshness_color(gm_freshness_t fresh)
{
    switch (fresh) {
    case GM_FRESH_OK:
        return UI_G_OK;
    case GM_FRESH_LATE:
        return UI_G_HIGH;
    case GM_FRESH_STALE:
        return UI_G_CRIT;
    default:
        return UI_G_STALE;
    }
}

const char *ui_freshness_text(gm_freshness_t fresh)
{
    switch (fresh) {
    case GM_FRESH_OK:
        return tr(S_FRESH_OK);
    case GM_FRESH_LATE:
        return tr(S_FRESH_LATE);
    case GM_FRESH_STALE:
        return tr(S_FRESH_STALE);
    default:
        return tr(S_FRESH_WAITING);
    }
}

const char *ui_trend_text(uint8_t trend)
{
    /* Meme ordre que trend_t : l'indice 0 (TREND_NONE) n'affiche rien. */
    static const int16_t k_trend_ids[TREND_COUNT] = {
        -1, S_TREND_UP_FAST, S_TREND_UP, S_TREND_UP_SLOW, S_TREND_FLAT,
        S_TREND_DOWN_SLOW, S_TREND_DOWN, S_TREND_DOWN_FAST, S_TREND_UNKNOWN, S_TREND_OUT_OF_RANGE,
    };
    if (trend >= TREND_COUNT || k_trend_ids[trend] < 0) {
        return "";
    }
    return tr((str_id_t)k_trend_ids[trend]);
}

void ui_format_hm(int64_t epoch_ms, char *buf, size_t size)
{
    time_t t = (time_t)(epoch_ms / 1000);
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    snprintf(buf, size, "%02d:%02d", tm_local.tm_hour, tm_local.tm_min);
}

/* Les pages sont transparentes : le degrade bleu nuit vers noir est peint une seule fois par l'ecran */
void ui_navy_background(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
}

void ui_screen_background(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, UI_G_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(obj, UI_G_BG_BOT, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
}

/* Verre : degrade vertical (reflet en haut), bord fin semi-transparent, ombre douce decalee vers le bas.
 * Fond opaque aux couleurs du verre translucide deja melange au fond bleu nuit (identique sur ce fond uniforme, sans
 * melange pixel par pixel). Ombre fine : son cout de rendu croit avec sa largeur (mesure : ~15 ms par carte a 24 px
 * sur un rafraichissement complet de l'ecran) */
void ui_glass_style(lv_obj_t *obj, int32_t radius)
{
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, UI_G_GLASS_TOP, 0);
    lv_obj_set_style_bg_grad_color(obj, UI_G_GLASS_BOT, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(obj, UI_G_EDGE, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_50, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(obj, 10, 0);
    lv_obj_set_style_shadow_offset_y(obj, 5, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_50, 0);
}

lv_obj_t *ui_card(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius)
{
    lv_obj_t *card = ui_box(parent, x, y, w, h, UI_G_GLASS_TOP);
    ui_glass_style(card, radius);
    return card;
}

/* Halo : tache coloree floue. LVGL ne dessine l'ombre qu'autour de l'objet : le centre est donc rempli a la moitie
 * de l'opacite de l'ombre (raccord sans contour visible) */
lv_obj_t *ui_glow(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t color, lv_opa_t opa, int32_t blur)
{
    lv_obj_t *glow = lv_obj_create(parent);
    lv_obj_remove_style_all(glow);
    lv_obj_set_size(glow, w, h);
    lv_obj_remove_flag(glow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(glow, blur, 0);
    lv_obj_set_style_shadow_color(glow, color, 0);
    lv_obj_set_style_bg_color(glow, color, 0);
    lv_obj_set_style_shadow_opa(glow, opa, 0);
    lv_obj_set_style_bg_opa(glow, (lv_opa_t)(opa / 2), 0);
    return glow;
}

void ui_glow_set(lv_obj_t *glow, lv_color_t color, lv_opa_t opa)
{
    if (!lv_color_eq(lv_obj_get_style_shadow_color(glow, 0), color)) {
        lv_obj_set_style_shadow_color(glow, color, 0);
        lv_obj_set_style_bg_color(glow, color, 0);
    }
    if (lv_obj_get_style_shadow_opa(glow, 0) != opa) {
        lv_obj_set_style_shadow_opa(glow, opa, 0);
        lv_obj_set_style_bg_opa(glow, (lv_opa_t)(opa / 2), 0);
    }
}

/* Boutons en pilules (navigation, durees du graphique) : bouton actif en degrade bleu avec lueur */
void ui_pill_buttons_style(lv_obj_t *btnm, const lv_font_t *font, bool glass_container)
{
    if (glass_container) {
        ui_glass_style(btnm, 22);
    } else {
        lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btnm, 0, 0);
        lv_obj_set_style_shadow_width(btnm, 0, 0);
        lv_obj_set_style_radius(btnm, 0, 0);
    }
    lv_obj_set_style_pad_all(btnm, 5, 0);
    lv_obj_set_style_pad_gap(btnm, 6, 0);
    lv_obj_set_style_text_font(btnm, font, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, UI_G_TEXT2, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 16, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_50, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btnm, UI_G_GLASS_DOWN, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btnm, UI_G_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_color(btnm, UI_G_ACCENT_DEEP, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(btnm, LV_GRAD_DIR_VER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btnm, UI_G_TEXT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_color(btnm, UI_G_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(btnm, 10, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_opa(btnm, LV_OPA_40, LV_PART_ITEMS | LV_STATE_CHECKED);
}

/* Anneau fin 0..100 (depart en haut, sens horaire), sans bouton ni appui */
lv_obj_t *ui_ring(lv_obj_t *parent, int32_t size, int32_t width)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(arc, 0, 0);
    lv_obj_set_style_arc_color(arc, UI_COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, UI_G_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    return arc;
}

void ui_ring_set(lv_obj_t *ring, int value, lv_color_t color)
{
    value = value < 0 ? 0 : (value > 100 ? 100 : value);
    if (lv_arc_get_value(ring) != value) {
        lv_arc_set_value(ring, value);
    }
    if (!lv_color_eq(lv_obj_get_style_arc_color(ring, LV_PART_INDICATOR), color)) {
        lv_obj_set_style_arc_color(ring, color, LV_PART_INDICATOR);
    }
}

static lv_obj_t *icon_shape(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    return obj;
}

/* Petites icones 20 x 20 dessinees avec des formes natives (quelques objets, aucune image) */
lv_obj_t *ui_icon(lv_obj_t *parent, ui_icon_t icon, lv_color_t color)
{
    lv_obj_t *box = icon_shape(parent, 0, 0, 20, 20, 0);
    switch (icon) {
    case UI_ICON_TARGET: {
        lv_obj_t *ring = icon_shape(box, 0, 0, 20, 20, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_color(ring, color, 0);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_t *dot = icon_shape(box, 7, 7, 6, 6, LV_RADIUS_CIRCLE);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, color, 0);
        break;
    }
    case UI_ICON_BARS: {
        static const int32_t k_heights[3] = {8, 16, 11};
        for (int i = 0; i < 3; i++) {
            lv_obj_t *bar = icon_shape(box, 1 + i * 7, 18 - k_heights[i], 4, k_heights[i], 1);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(bar, color, 0);
        }
        break;
    }
    case UI_ICON_SENSOR: {
        lv_obj_t *disc = icon_shape(box, 0, 0, 20, 20, LV_RADIUS_CIRCLE);
        lv_obj_set_style_bg_opa(disc, LV_OPA_30, 0);
        lv_obj_set_style_bg_color(disc, color, 0);
        lv_obj_set_style_border_color(disc, color, 0);
        lv_obj_set_style_border_width(disc, 2, 0);
        lv_obj_t *dot = icon_shape(box, 7, 7, 6, 6, LV_RADIUS_CIRCLE);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, color, 0);
        break;
    }
    case UI_ICON_DROP:
    case UI_ICON_DOWN:
    case UI_ICON_UP: {
        const char *symbol = icon == UI_ICON_DROP ? LV_SYMBOL_TINT : (icon == UI_ICON_DOWN ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
        lv_obj_t *label = ui_label(box, &lv_font_montserrat_20, color, symbol);
        lv_obj_center(label);
        break;
    }
    default:
        break;
    }
    return box;
}

lv_obj_t *ui_progress(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, UI_COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, UI_COL_GREEN, LV_PART_INDICATOR);
    return bar;
}

void ui_set_text(lv_obj_t *label, const char *text)
{
    const char *shown = lv_label_get_text(label);
    if (shown == NULL || strcmp(shown, text) != 0) {
        lv_label_set_text(label, text);
    }
}

void ui_set_text_color(lv_obj_t *obj, lv_color_t color)
{
    if (!lv_color_eq(lv_obj_get_style_text_color(obj, 0), color)) {
        lv_obj_set_style_text_color(obj, color, 0);
    }
}

void ui_format_date_long(const struct tm *tm_local, char *buf, size_t size)
{
    i18n_format_date_long(tm_local, buf, size);
}

void ui_format_day_hm(int64_t epoch_s, char *buf, size_t size)
{
    const time_t t = (time_t)epoch_s;
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    snprintf(buf, size, tr(S_DATE_DAY_HM), tm_local.tm_mday, tm_local.tm_mon + 1, tm_local.tm_hour,
             tm_local.tm_min);
}

void ui_format_duration(int64_t seconds, bool compact, char *buf, size_t size)
{
    if (seconds < 0) {
        seconds = -seconds;
    }
    const int days = (int)(seconds / 86400);
    const int hours = (int)((seconds % 86400) / 3600);
    const int minutes = (int)((seconds % 3600) / 60);
    if (days > 0) {
        snprintf(buf, size, tr(S_DUR_DH), days, hours);
    } else if (hours > 0) {
        if (compact) {
            snprintf(buf, size, tr(S_DUR_H), hours);
        } else {
            snprintf(buf, size, tr(S_DUR_HM), hours, minutes);
        }
    } else {
        snprintf(buf, size, tr(S_DUR_MIN), minutes);
    }
}

/* Express reading age clearly so a stale number is not mistaken for a live value. */
void ui_format_age(int64_t age_s, char *buf, size_t size)
{
    const int64_t min = age_s / 60;
    if (age_s < 0) {
        snprintf(buf, size, "%s", tr(S_AGE_UNKNOWN));
    } else if (min < 1) {
        snprintf(buf, size, "%s", tr(S_AGE_LESS_1MIN));
    } else if (min < 60) {
        snprintf(buf, size, tr(S_AGE_MIN), (int)min);
    } else {
        snprintf(buf, size, tr(S_AGE_HM), (int)(min / 60), (int)(min % 60));
    }
}
