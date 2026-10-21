/*
 * ui_home.c: Home page showing the latest glucose value and its age.
 * FR: Accueil affichant la glycémie et son ancienneté.
 * Design note: A large number must not hide that a reading is stale.
 */

/*
 * Keeps the glucose value visually dominant and indicates trend and data freshness.
 * Met en avant la valeur, sa tendance et l'ancienneté de la donnée.
 */
#include "i18n.h"
#include "ui_pages.h"

#include <stdio.h>
#include <string.h>
#include "night_mode.h"
#include "touch_diag.h"
#include "trend_arrow.h"
#include "ui_alert.h"

/*
 * ACCUEIL (1024 x 496) : UNE seule grande carte GLYCEMIE occupant toute la zone de contenu.
 * L'heure et la date sont remontees dans le bandeau (ui_main.c) : la page entiere sert la glycemie.
 * La carte porte la pastille de fraicheur, la valeur geante (320 px) avec halo local et fin calque d'ombre,
 * l'anneau de fraicheur, l'icone de tendance dessinee, la variation et le texte de tendance.
 * Profondeur "2.5D" par superposition : fond degrade de l'ecran -> cartes en degrade a bord fin, ombre courte et
 * second contour interieur tres discret -> halo local -> chiffres et leur ombre portee. Aucune grande ombre floue,
 * aucune transparence sur toute la surface.
 * L'ambiance de la grande carte suit l'etat DEJA calcule de la glycemie (aucun nouveau seuil) : teinte tres sombre
 * derivee de la couleur d'accent, plus neutre quand la donnee est ancienne.
 * Aucune statistique, aucun consommable, aucun stock, aucun mini-graphique : ces informations ont leur propre page.
 */

#define MAIN_X        16
#define MAIN_Y        8
#define MAIN_W        992
#define MAIN_H        480
#define PILL_X        28
#define PILL_Y        16
#define VALUE_X       16    /* colonne de la glycemie dans la carte */
#define VALUE_W       680
#define VALUE_Y       58
#define UNIT_Y        392
#define HALO_W        360
#define HALO_H        110
#define HALO_Y        200
#define SHADOW_DX     4
#define SHADOW_DY     5
#define RIGHT_CX      832   /* centre de la colonne tendance dans la carte */
#define RIGHT_W       316
#define RING_SIZE     200
#define RING_W        9
#define RING_CY       158
#define ARROW_SIZE    128
#define DELTA_Y       276
#define TREND_Y       392
#define BREATH_MS     250   /* respiration du halo : 4 pas par seconde */
#define BREATH_STEPS  16    /* cycle de 4 s */

static lv_obj_t *s_page;
static lv_obj_t *s_main;
static lv_obj_t *s_fresh_dot;
static lv_obj_t *s_fresh_text;
static lv_obj_t *s_last;
static lv_obj_t *s_halo;
static lv_obj_t *s_value_shadow;
static lv_obj_t *s_value;
static lv_obj_t *s_unit;
static lv_obj_t *s_ring;
static lv_obj_t *s_arrow;
static lv_obj_t *s_delta;
static lv_obj_t *s_trend;
static lv_obj_t *s_note;   /* cause d'une donnee ancienne ou message d'attente (sinon vide) */
static int s_arrow_trend = -1;
static lv_color_t s_arrow_color;
static lv_color_t s_halo_color;
static lv_color_t s_mood_color;
static lv_opa_t s_halo_opa = LV_OPA_20;
static lv_opa_t s_value_opa = LV_OPA_COVER;
static bool s_fast_trend;  /* tendance rapide Dexcom : mise en avant visuelle (aucune alerte) */
static lv_timer_t *s_breath;
static int s_breath_step;
static bool s_breath_on;
static char s_value_text[8];

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void on_go_page(lv_event_t *e)
{
    ui_show_page((ui_page_t)(intptr_t)lv_event_get_user_data(e));
}

static void set_breathing(bool on);

/* Accueil reellement visible : page affichee, ni ecran nuit ni panneau d'alerte par-dessus */
static bool home_visible(void)
{
    return !lv_obj_has_flag(s_page, LV_OBJ_FLAG_HIDDEN) && !night_mode_screen_visible() && !ui_alert_panel_visible();
}

/* Respiration lente du halo de l'icone ; plus marquee en tendance rapide (jamais de clignotement) */
static void breath_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!home_visible()) {
        set_breathing(false);
        return;
    }
    s_breath_step = (s_breath_step + 1) % BREATH_STEPS;
    const int phase = s_breath_step < BREATH_STEPS / 2 ? s_breath_step : BREATH_STEPS - s_breath_step;
    trend_arrow_set_glow(s_arrow, (lv_opa_t)(s_fast_trend ? 90 + phase * 14 : 60 + phase * 10));
}

static void set_breathing(bool on)
{
    if (on == s_breath_on) {
        return;
    }
    s_breath_on = on;
    if (on) {
        lv_timer_resume(s_breath);
    } else {
        lv_timer_pause(s_breath);
        trend_arrow_set_glow(s_arrow, LV_OPA_40);
    }
}

/* Lueur locale derriere les chiffres : uniquement l'ombre floue, sans noyau visible (jamais une forme) */
static lv_obj_t *soft_glow(lv_obj_t *parent, int32_t w, int32_t h, int32_t blur)
{
    lv_obj_t *glow = plain(parent);
    lv_obj_set_size(glow, w, h);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(glow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(glow, blur, 0);
    lv_obj_set_style_shadow_color(glow, UI_G_STALE, 0);
    lv_obj_set_style_shadow_opa(glow, LV_OPA_20, 0);
    return glow;
}

/* Second contour interieur : un filet clair tres discret, 3 px en retrait (profondeur, cout negligeable) */
static void inner_edge(lv_obj_t *card, int32_t w, int32_t h, int32_t radius)
{
    lv_obj_t *edge = plain(card);
    lv_obj_set_pos(edge, 3, 3);
    lv_obj_set_size(edge, w - 6, h - 6);
    lv_obj_set_style_radius(edge, radius, 0);
    lv_obj_set_style_border_color(edge, UI_G_TEXT, 0);
    lv_obj_set_style_border_opa(edge, LV_OPA_10, 0);
    lv_obj_set_style_border_width(edge, 1, 0);
}

static lv_obj_t *value_label(lv_obj_t *parent, lv_color_t color, int32_t dx, int32_t dy)
{
    lv_obj_t *label = ui_label(parent, g_fonts.value, color, "---");
    lv_obj_set_width(label, VALUE_W);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, VALUE_X + dx, VALUE_Y + dy);
    return label;
}

void ui_home_create(lv_obj_t *page)
{
    /* Reconstruction (changement de langue) : le minuteur de respiration de la version precedente
     * pointerait sur des objets detruits. Il est supprime avant de tout recreer. */
    if (s_breath != NULL) {
        lv_timer_delete(s_breath);
        s_breath = NULL;
        s_breath_on = false;
    }
    s_arrow_trend = -1;
    s_value_text[0] = '\0';
    s_page = page;
    ui_navy_background(page);

    /* Grande carte GLYCEMIE, seule zone de la page (appui -> GRAPHIQUE) */
    s_main = ui_card(page, MAIN_X, MAIN_Y, MAIN_W, MAIN_H, 30);
    inner_edge(s_main, MAIN_W, MAIN_H, 27);
    lv_obj_add_flag(s_main, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_main, on_go_page, LV_EVENT_CLICKED, (void *)(intptr_t)UI_PAGE_GRAPH);
    touch_diag_name(s_main, "carte glycemie");
    s_mood_color = UI_G_STALE;

    /* Pastille de fraicheur, dans la grande carte (petite et secondaire) */
    lv_obj_t *pill = plain(s_main);
    ui_glass_style(pill, 19);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, 38);
    lv_obj_set_style_pad_hor(pill, 14, 0);
    lv_obj_set_style_shadow_width(pill, 0, 0); /* deja posee sur une carte : aucune ombre supplementaire */
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pill, 9, 0);
    lv_obj_set_pos(pill, PILL_X, PILL_Y);
    s_fresh_dot = plain(pill);
    lv_obj_set_size(s_fresh_dot, 12, 12);
    lv_obj_set_style_radius(s_fresh_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_fresh_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_fresh_dot, UI_G_STALE, 0);
    s_fresh_text = ui_label(pill, g_fonts.label, UI_G_STALE, tr(S_FRESH_WAITING));
    s_last = ui_label(pill, g_fonts.xs, UI_G_TEXT2, "");

    /* Cause d'une donnee ancienne ou message d'attente : a droite de la pastille, sinon vide */
    s_note = ui_label(s_main, g_fonts.xs, UI_G_TEXT2, "");
    lv_obj_set_width(s_note, 600);
    lv_label_set_long_mode(s_note, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(s_note, PILL_X + 4, PILL_Y + 44); /* sous la pastille, a l'ecart de l'anneau */

    /* Glycemie : halo local, calque d'ombre tres fin, puis les chiffres */
    s_halo = soft_glow(s_main, HALO_W, HALO_H, 80);
    lv_obj_set_pos(s_halo, VALUE_X + (VALUE_W - HALO_W) / 2, HALO_Y);
    s_halo_color = UI_G_STALE;

    s_value_shadow = value_label(s_main, UI_G_BG_BOT, SHADOW_DX, SHADOW_DY);
    lv_obj_set_style_text_opa(s_value_shadow, LV_OPA_40, 0);
    s_value = value_label(s_main, UI_G_STALE, 0, 0);

    s_unit = ui_label(s_main, g_fonts.m, UI_G_TEXT3, tr(S_UNIT_MGDL));
    lv_obj_set_width(s_unit, VALUE_W);
    lv_obj_set_style_text_align(s_unit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_unit, VALUE_X, UNIT_Y);

    /* Tendance : anneau fin de fraicheur, icone dessinee, variation, texte */
    s_ring = ui_ring(s_main, RING_SIZE, RING_W);
    lv_obj_set_pos(s_ring, RIGHT_CX - RING_SIZE / 2, RING_CY - RING_SIZE / 2);
    s_arrow = trend_arrow_create(s_main, ARROW_SIZE, g_fonts.xl);
    lv_obj_set_pos(s_arrow, RIGHT_CX - ARROW_SIZE / 2, RING_CY - ARROW_SIZE / 2);

    s_delta = ui_label(s_main, g_fonts.delta, UI_G_TEXT, "");
    lv_obj_set_width(s_delta, RIGHT_W);
    lv_label_set_long_mode(s_delta, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(s_delta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_delta, RIGHT_CX - RIGHT_W / 2, DELTA_Y);

    s_trend = ui_label(s_main, g_fonts.m, UI_COL_SOFT, "");
    lv_obj_set_width(s_trend, RIGHT_W);
    lv_label_set_long_mode(s_trend, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(s_trend, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_trend, RIGHT_CX - RIGHT_W / 2, TREND_Y);

    s_breath = lv_timer_create(breath_cb, BREATH_MS, NULL);
    lv_timer_pause(s_breath);
}

/* ------------------------------------------------------------------------- */

static const char *stale_reason(const ui_data_t *d)
{
    if (!d->st.wifi_configured) {
        return tr(S_WIFI_NOT_CONFIGURED);
    }
    if (d->st.wifi != LINK_OK) {
        return tr(S_WIFI_DOWN);
    }
    switch (d->st.dexcom) {
    case DEX_STATE_AUTH_REFUSED:
        return tr(S_DEXCOM_REFUSED);
    case DEX_STATE_LOCKED:
        return tr(S_DEXCOM_LOCKED);
    case DEX_STATE_AUTH_BLOCKED:
        return tr(S_DEXCOM_CHECK);
    case DEX_STATE_RETRYING:
        return tr(S_DEXCOM_UNAVAILABLE);
    default:
        break;
    }
    if (d->st.internet == LINK_ERROR) {
        return tr(S_INTERNET_DOWN);
    }
    return tr(S_NO_NEW_READING);
}

static const char *waiting_message(const ui_data_t *d)
{
    if (!d->st.wifi_configured) {
        return tr(S_WIFI_SETUP_NEEDED);
    }
    if (d->st.wifi != LINK_OK) {
        return tr(S_WIFI_CONNECTING);
    }
    switch (d->st.dexcom) {
    case DEX_STATE_NO_CONFIG:
        return tr(S_DEXCOM_NO_CREDENTIALS);
    case DEX_STATE_AUTH_REFUSED:
        return tr(S_DEXCOM_REFUSED);
    case DEX_STATE_LOCKED:
        return tr(S_DEXCOM_LOCKED);
    case DEX_STATE_AWAITING_ARM:
        return tr(S_DEXCOM_WAITING_AUTH);
    case DEX_STATE_AUTH_BLOCKED:
        return tr(S_DEXCOM_CHECK);
    case DEX_STATE_RETRYING:
        return tr(S_DEXCOM_RETRYING);
    case DEX_STATE_NO_DATA:
        return tr(S_NO_READING_24H);
    default:
        return tr(S_DEXCOM_CONNECTING);
    }
}

/* Show the trend reported by Dexcom without extrapolating a new medical prediction. */
static void set_arrow(uint8_t trend, lv_color_t color)
{
    if (trend != s_arrow_trend || !lv_color_eq(color, s_arrow_color)) {
        s_arrow_trend = trend;
        s_arrow_color = color;
        trend_arrow_set(s_arrow, trend, color);
    }
}

static void set_halo(lv_color_t color, lv_opa_t opa)
{
    if (!lv_color_eq(color, s_halo_color)) {
        s_halo_color = color;
        lv_obj_set_style_shadow_color(s_halo, color, 0);
    }
    if (opa != s_halo_opa) {
        s_halo_opa = opa;
        lv_obj_set_style_shadow_opa(s_halo, opa, 0);
    }
}

/* Ambiance de la grande carte : degrade tres sombre teinte par la couleur d'accent deja calculee */
static void set_mood(lv_color_t accent)
{
    if (lv_color_eq(accent, s_mood_color)) {
        return;
    }
    s_mood_color = accent;
    lv_obj_set_style_bg_color(s_main, lv_color_mix(accent, UI_G_GLASS_TOP, 26), 0);
    lv_obj_set_style_bg_grad_color(s_main, lv_color_mix(accent, UI_G_GLASS_BOT, 13), 0);
}

/* Chiffres et leur ombre portee : meme texte, jamais redessines si la valeur ne change pas */
static void set_value_text(const char *text)
{
    if (strcmp(text, s_value_text) != 0) {
        strlcpy(s_value_text, text, sizeof(s_value_text));
        lv_label_set_text(s_value, s_value_text);
        lv_label_set_text(s_value_shadow, s_value_text);
    }
}

/* Valeur moins mise en avant quand la donnee n'est plus fraiche (jamais effacee) */
static void set_value_opa(lv_opa_t opa)
{
    if (opa != s_value_opa) {
        s_value_opa = opa;
        lv_obj_set_style_text_opa(s_value, opa, 0);
        lv_obj_set_style_text_opa(s_unit, opa, 0);
        lv_obj_set_style_text_opa(s_value_shadow, opa == LV_OPA_COVER ? LV_OPA_40 : LV_OPA_20, 0);
    }
}

/* Heure et age de la derniere mesure : plus lisibles des que la donnee est en retard */
static void set_last_font(const lv_font_t *font)
{
    if (lv_obj_get_style_text_font(s_last, 0) != font) {
        lv_obj_set_style_text_font(s_last, font, 0);
    }
}

/* Keep date, time and status legible without adding redundant connectivity indicators. */
static void update_header(const ui_data_t *d)
{
    char buf[64];
    const lv_color_t fresh_color = d->count > 0 ? ui_freshness_color(d->fresh) : UI_G_STALE;
    if (!lv_color_eq(lv_obj_get_style_bg_color(s_fresh_dot, 0), fresh_color)) {
        lv_obj_set_style_bg_color(s_fresh_dot, fresh_color, 0);
    }
    ui_set_text(s_fresh_text, d->count > 0 ? ui_freshness_text(d->fresh) : tr(S_FRESH_WAITING));
    ui_set_text_color(s_fresh_text, fresh_color);

    if (d->count == 0) {
        ui_set_text(s_last, tr(S_NO_READING));
        ui_set_text_color(s_last, UI_G_TEXT2);
        set_last_font(g_fonts.xs);
        return;
    }
    char hm[8] = "--:--";
    if (d->time_ok) {
        ui_format_hm(d->hist[0].wt_ms, hm, sizeof(hm));
    }
    char age[40];
    ui_format_age(d->age_s, age, sizeof(age));
    if (i18n_lang() == LANG_FR && age[0] == 'I') {
        age[0] = 'i'; /* francais seulement : "Il y a 3 min" -> "il y a 3 min" */
    }
    snprintf(buf, sizeof(buf), "%s · %s", hm, age);
    ui_set_text(s_last, buf);
    const bool stale = d->fresh == GM_FRESH_STALE || d->fresh == GM_FRESH_NONE;
    ui_set_text_color(s_last, stale ? UI_G_CRIT : (d->fresh == GM_FRESH_LATE ? UI_G_HIGH : UI_G_TEXT2));
    /* RETARD ou DONNEES ANCIENNES : heure et age de la derniere mesure plus visibles */
    set_last_font(d->fresh == GM_FRESH_OK ? g_fonts.xs : g_fonts.s);
}

/* Anneau : progression vers la prochaine mesure (a jour), orange en RETARD, rouge plein si DONNEES ANCIENNES */
/* Use colors and halo intensity as presentation only, never as a treatment recommendation. */
static void update_ring(const ui_data_t *d)
{
    if (d->count == 0 || d->age_s < 0) {
        ui_ring_set(s_ring, 0, UI_G_ACCENT);
        return;
    }
    switch (d->fresh) {
    case GM_FRESH_OK: {
        const int32_t cadence = d->cadence_s > 0 ? d->cadence_s : GLY_CADENCE_DEFAULT_S;
        ui_ring_set(s_ring, (int)(d->age_s * 100 / cadence), UI_G_ACCENT);
        break;
    }
    case GM_FRESH_LATE:
        ui_ring_set(s_ring, 100, UI_G_HIGH);
        break;
    default:
        ui_ring_set(s_ring, 100, UI_G_CRIT);
        break;
    }
}

/* Update the central glucose value together with age and trend context. */
static void update_hero(const ui_data_t *d)
{
    char buf[48];
    update_ring(d);
    if (d->count == 0) {
        set_value_text("---");
        ui_set_text_color(s_value, UI_G_STALE);
        ui_set_text_color(s_unit, UI_G_STALE);
        set_halo(UI_G_STALE, LV_OPA_20);
        set_mood(UI_G_STALE);
        set_value_opa(LV_OPA_60);
        s_fast_trend = false;
        set_arrow(TREND_NONE, UI_G_STALE);
        set_breathing(false);
        ui_set_text(s_delta, "");
        ui_set_text(s_trend, "");
        ui_set_text(s_note, waiting_message(d));
        ui_set_text_color(s_note, UI_G_TEXT2);
        return;
    }
    const glucose_point_t *p = &d->hist[0];
    const bool stale = d->fresh == GM_FRESH_STALE || d->fresh == GM_FRESH_NONE;
    const lv_color_t value_color = stale ? UI_G_STALE : ui_glucose_color(p->mgdl);
    snprintf(buf, sizeof(buf), "%d", p->mgdl);
    set_value_text(buf);
    ui_set_text_color(s_value, value_color);
    ui_set_text_color(s_unit, stale ? UI_G_STALE : UI_G_TEXT3);

    /* Interface adaptative : calme quand tout va bien, valeur moins mise en avant des que la donnee vieillit */
    const bool fast = !stale && (p->trend == TREND_DOUBLE_UP || p->trend == TREND_DOUBLE_DOWN);
    s_fast_trend = fast;
    set_halo(value_color, stale ? LV_OPA_20 : (fast ? LV_OPA_40 : LV_OPA_20));
    set_mood(value_color);
    set_value_opa(stale ? LV_OPA_60 : (d->fresh == GM_FRESH_LATE ? LV_OPA_80 : LV_OPA_COVER));

    /* Tendance et variation jamais presentees comme actuelles sur une donnee ancienne */
    if (stale) {
        set_arrow(TREND_NONE, UI_G_STALE);
        set_breathing(false);
        ui_set_text(s_delta, "--");
        ui_set_text_color(s_delta, UI_G_STALE);
        ui_set_text(s_trend, tr(S_TREND_DASH));
        ui_set_text_color(s_trend, UI_G_STALE);
        ui_set_text(s_note, stale_reason(d));
        ui_set_text_color(s_note, UI_G_CRIT);
        return;
    }
    set_arrow(p->trend, value_color);
    set_breathing(p->trend >= TREND_DOUBLE_UP && p->trend <= TREND_DOUBLE_DOWN && home_visible());
    if (d->delta_ok) {
        if (d->delta == 0) {
            ui_set_text(s_delta, "0");
        } else {
            snprintf(buf, sizeof(buf), "%+d", d->delta);
            ui_set_text(s_delta, buf);
        }
        ui_set_text_color(s_delta, fast ? value_color : UI_G_TEXT);
    } else {
        ui_set_text(s_delta, "--");
        ui_set_text_color(s_delta, UI_G_STALE);
    }
    ui_set_text(s_trend, ui_trend_text(p->trend));
    ui_set_text_color(s_trend, fast ? value_color : UI_COL_SOFT);
    ui_set_text(s_note, ""); /* accueil epure : rien d'autre tant que la donnee est fraiche */
}

void ui_home_update(const ui_data_t *d, bool force)
{
    (void)force;
    if (lv_obj_has_flag(s_page, LV_OBJ_FLAG_HIDDEN)) {
        set_breathing(false); /* page masquee : aucune animation */
        return;
    }
    update_header(d);
    update_hero(d);
}
