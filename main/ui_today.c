#include "i18n.h"
#include "ui_pages.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * AUJOURD'HUI (depuis minuit, heure locale) : TIR 70-180, moyenne, minimum, maximum, couverture, temps bas (< 70) et
 * haut (> 180), episodes bas / hauts (au moins 3 mesures consecutives, soit 15 min). Calcule sur l'historique 24 h
 * deja en memoire (aucune lecture supplementaire), seulement quand l'historique ou la minute change.
 * Frise de la journee : 96 creneaux de 15 min dessines en plages de meme etat (evenement de dessin, aucun objet par
 * creneau, aucune image).
 */

#define PAD        20
#define LINE_Y     58
#define LINE_H     104
#define ROW1_Y     174
#define ROW2_Y     328
#define TILE_H     142
#define TILE_GAP   12
#define SLOTS      96
#define SLOT_MS    (15 * 60 * 1000LL)
#define GAP_MS     (10 * 60 * 1000LL)  /* au-dela : mesures non consecutives */
#define EVENT_MIN  3                   /* 3 mesures = 15 min */

typedef enum {
    SLOT_FUTURE = 0,
    SLOT_NO_DATA,
    SLOT_OK,
    SLOT_LOW,
    SLOT_HIGH,
} slot_state_t;

typedef enum {
    T_TIR = 0,
    T_MEAN,
    T_MIN,
    T_MAX,
    T_COVERAGE,
    T_TIME_LOW,
    T_TIME_HIGH,
    T_EVENTS_LOW,
    T_EVENTS_HIGH,
    T_COUNT,
} tile_id_t;

typedef struct {
    lv_obj_t *box;
    lv_obj_t *value;
    lv_obj_t *sub;
} tile_t;

typedef struct {
    int n;
    int expected;
    int coverage;
    int tir;
    int mean;
    int min;
    int max;
    int low_n;
    int high_n;
    int low_events;
    int high_events;
    bool reliable;
} today_t;

static tile_t s_tiles[T_COUNT];
static lv_obj_t *s_subtitle;
static lv_obj_t *s_timeline;
static uint8_t s_slots[SLOTS];
static int32_t s_now_pos = -1; /* 0..1000 : position de l'heure actuelle dans la journee */

static void tile_create(tile_t *t, lv_obj_t *page, int32_t x, int32_t y, int32_t w, const char *title, bool icon_low,
                        bool icon_high, const char *sub)
{
    t->box = ui_card(page, x, y, w, TILE_H, 20);
    int32_t title_x = 18;
    if (icon_low || icon_high) {
        lv_obj_t *ico = ui_icon(t->box, icon_low ? UI_ICON_DOWN : UI_ICON_UP, icon_low ? UI_G_LOW : UI_G_HIGH);
        lv_obj_set_pos(ico, 14, 13);
        title_x = 38;
    }
    lv_obj_t *label = ui_label(t->box, g_fonts.label, UI_G_TEXT2, title);
    lv_obj_set_width(label, w - title_x - 6);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(label, title_x, 12);

    t->value = ui_label(t->box, g_fonts.card, UI_G_STALE, "--");
    lv_obj_set_width(t->value, w - 32);
    lv_label_set_long_mode(t->value, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(t->value, 16, 44);

    t->sub = ui_label(t->box, g_fonts.xs, UI_G_TEXT3, sub);
    lv_obj_set_width(t->sub, w - 32);
    lv_label_set_long_mode(t->sub, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(t->sub, 16, TILE_H - 36);
}

static lv_color_t slot_color(uint8_t state)
{
    switch (state) {
    case SLOT_OK:
        return UI_G_OK;
    case SLOT_LOW:
        return UI_G_CRIT;
    case SLOT_HIGH:
        return UI_G_HIGH;
    default:
        return UI_G_STALE;
    }
}

/* Frise : piste, plages de meme etat, repere de l'heure actuelle */
static void on_timeline_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t w = lv_area_get_width(&a);

    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = UI_COL_TRACK;
    rect.bg_opa = LV_OPA_COVER;
    rect.radius = 6;
    lv_draw_rect(layer, &rect, &a);

    for (int i = 0; i < SLOTS;) {
        const uint8_t state = s_slots[i];
        int j = i;
        while (j < SLOTS && s_slots[j] == state) {
            j++;
        }
        if (state != SLOT_FUTURE) {
            lv_area_t run = {
                .x1 = a.x1 + (int32_t)((int64_t)i * w / SLOTS),
                .y1 = a.y1,
                .x2 = a.x1 + (int32_t)((int64_t)j * w / SLOTS) - 1,
                .y2 = a.y2,
            };
            lv_draw_rect_dsc_init(&rect);
            rect.bg_color = slot_color(state);
            rect.bg_opa = state == SLOT_NO_DATA ? LV_OPA_30 : LV_OPA_90;
            rect.radius = 4;
            lv_draw_rect(layer, &rect, &run);
        }
        i = j;
    }

    if (s_now_pos >= 0) {
        const int32_t x = a.x1 + (int32_t)((int64_t)s_now_pos * (w - 1) / 1000);
        lv_area_t mark = {.x1 = x - 1, .y1 = a.y1 - 6, .x2 = x + 1, .y2 = a.y2 + 6};
        lv_draw_rect_dsc_init(&rect);
        rect.bg_color = UI_G_TEXT;
        rect.bg_opa = LV_OPA_90;
        rect.radius = 1;
        lv_draw_rect(layer, &rect, &mark);
    }
}

static void on_timeline_ext_size(lv_event_t *e)
{
    lv_event_set_ext_draw_size(e, 7); /* repere de l'heure au-dessus et au-dessous de la frise */
}

static lv_obj_t *legend_item(lv_obj_t *parent, lv_color_t color, lv_opa_t opa, const char *text)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(item, 7, 0);
    lv_obj_t *dot = lv_obj_create(item);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_bg_opa(dot, opa, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    ui_label(item, g_fonts.xs, UI_G_TEXT2, text);
    return item;
}

void ui_today_create(lv_obj_t *page)
{
    ui_navy_background(page);
    lv_obj_t *title = ui_label(page, g_fonts.title, UI_G_TEXT, tr(S_TODAY_TITLE));
    lv_obj_set_pos(title, 24, 8);

    s_subtitle = ui_label(page, g_fonts.xs, UI_G_TEXT2, tr(S_SINCE_MIDNIGHT));
    lv_obj_set_width(s_subtitle, 620);
    lv_label_set_long_mode(s_subtitle, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(s_subtitle, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_subtitle, UI_SCR_W - 24 - 620, 20);

    /* Frise de la journee */
    lv_obj_t *line = ui_card(page, PAD, LINE_Y, UI_SCR_W - 2 * PAD, LINE_H, 20);
    lv_obj_t *head = ui_label(line, g_fonts.label, UI_G_TEXT2, tr(S_DAY_BAND));
    lv_obj_set_pos(head, 18, 10);
    lv_obj_t *legend = lv_obj_create(line);
    lv_obj_remove_style_all(legend);
    lv_obj_remove_flag(legend, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(legend, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(legend, 18, 0);
    lv_obj_align(legend, LV_ALIGN_TOP_RIGHT, -18, 10);
    legend_item(legend, UI_G_OK, LV_OPA_90, tr(S_LEG_TARGET));
    legend_item(legend, UI_G_CRIT, LV_OPA_90, tr(S_LEG_LOW));
    legend_item(legend, UI_G_HIGH, LV_OPA_90, tr(S_LEG_HIGH));
    legend_item(legend, UI_G_STALE, LV_OPA_30, tr(S_LEG_NODATA));

    const int32_t bar_w = UI_SCR_W - 2 * PAD - 36;
    s_timeline = lv_obj_create(line);
    lv_obj_remove_style_all(s_timeline);
    lv_obj_remove_flag(s_timeline, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_timeline, 18, 44);
    lv_obj_set_size(s_timeline, bar_w, 22);
    lv_obj_add_event_cb(s_timeline, on_timeline_draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(s_timeline, on_timeline_ext_size, LV_EVENT_REFR_EXT_DRAW_SIZE, NULL);
    lv_obj_refresh_ext_draw_size(s_timeline);
    for (int h = 0; h <= 24; h += 6) {
        char text[8];
        snprintf(text, sizeof(text), tr(S_DUR_H), h);
        lv_obj_t *tick = ui_label(line, g_fonts.xs, UI_G_TEXT3, text);
        lv_obj_set_width(tick, 60);
        lv_obj_set_style_text_align(tick, h == 0 ? LV_TEXT_ALIGN_LEFT : (h == 24 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER),
                                    0);
        const int32_t x = 18 + bar_w * h / 24;
        lv_obj_set_pos(tick, h == 0 ? x : (h == 24 ? x - 60 : x - 30), 72);
    }

    /* Tuiles */
    const int32_t w1 = (UI_SCR_W - 2 * PAD - 4 * TILE_GAP) / 5;
    const int32_t w2 = (UI_SCR_W - 2 * PAD - 3 * TILE_GAP) / 4;
    tile_create(&s_tiles[T_TIR], page, PAD, ROW1_Y, w1, tr(S_TIR), false, false, tr(S_RANGE_70_180));
    tile_create(&s_tiles[T_MEAN], page, PAD + (w1 + TILE_GAP), ROW1_Y, w1, tr(S_AVERAGE), false, false,
                tr(S_UNIT_MGDL));
    tile_create(&s_tiles[T_MIN], page, PAD + 2 * (w1 + TILE_GAP), ROW1_Y, w1, tr(S_MINIMUM), false, false,
                tr(S_UNIT_MGDL));
    tile_create(&s_tiles[T_MAX], page, PAD + 3 * (w1 + TILE_GAP), ROW1_Y, w1, tr(S_MAXIMUM), false, false,
                tr(S_UNIT_MGDL));
    tile_create(&s_tiles[T_COVERAGE], page, PAD + 4 * (w1 + TILE_GAP), ROW1_Y, w1, tr(S_COVERAGE), false, false, "");
    tile_create(&s_tiles[T_TIME_LOW], page, PAD, ROW2_Y, w2, tr(S_TIME_LOW), true, false, tr(S_UNDER_70));
    tile_create(&s_tiles[T_TIME_HIGH], page, PAD + (w2 + TILE_GAP), ROW2_Y, w2, tr(S_TIME_HIGH), false, true,
                tr(S_OVER_180));
    tile_create(&s_tiles[T_EVENTS_LOW], page, PAD + 2 * (w2 + TILE_GAP), ROW2_Y, w2, tr(S_EPISODES_LOW), true,
                false, tr(S_AT_LEAST_15MIN));
    tile_create(&s_tiles[T_EVENTS_HIGH], page, PAD + 3 * (w2 + TILE_GAP), ROW2_Y, w2, tr(S_EPISODES_HIGH), false,
                true, tr(S_AT_LEAST_15MIN));
}

/* ------------------------------------------------------------------------- */

static int64_t local_midnight_ms(const struct tm *now_tm)
{
    struct tm m = *now_tm;
    m.tm_hour = 0;
    m.tm_min = 0;
    m.tm_sec = 0;
    m.tm_isdst = -1;
    return (int64_t)mktime(&m) * 1000;
}

/* Statistiques depuis minuit et etat de chaque creneau de 15 min */
static void compute_today(const ui_data_t *d, int64_t midnight_ms, today_t *t, uint8_t *slots)
{
    memset(t, 0, sizeof(*t));
    const int64_t elapsed_ms = d->now_ms - midnight_ms;
    t->expected = elapsed_ms > 0 ? (int)(elapsed_ms / (5 * 60 * 1000LL)) : 0;
    t->expected = t->expected < 1 ? 1 : t->expected;
    t->min = 10000;
    for (int i = 0; i < SLOTS; i++) {
        slots[i] = (int64_t)i * SLOT_MS < elapsed_ms ? SLOT_NO_DATA : SLOT_FUTURE;
    }

    int64_t sum = 0;
    int in = 0;
    int low_run = 0;
    int high_run = 0;
    int64_t prev_ms = 0;
    for (int i = d->count - 1; i >= 0; i--) { /* du plus ancien au plus recent */
        const glucose_point_t *p = &d->hist[i];
        if (p->wt_ms < midnight_ms || p->wt_ms > d->now_ms + 5 * 60 * 1000LL) {
            continue;
        }
        if (prev_ms != 0 && p->wt_ms - prev_ms > GAP_MS) {
            t->low_events += low_run >= EVENT_MIN;
            t->high_events += high_run >= EVENT_MIN;
            low_run = 0;
            high_run = 0;
        }
        prev_ms = p->wt_ms;
        const int v = p->mgdl;
        t->n++;
        sum += v;
        t->min = v < t->min ? v : t->min;
        t->max = v > t->max ? v : t->max;

        uint8_t state = SLOT_OK;
        if (v < GLY_RANGE_LOW) {
            t->low_n++;
            low_run++;
            state = SLOT_LOW;
        } else {
            t->low_events += low_run >= EVENT_MIN;
            low_run = 0;
        }
        if (v > GLY_RANGE_HIGH) {
            t->high_n++;
            high_run++;
            state = SLOT_HIGH;
        } else {
            t->high_events += high_run >= EVENT_MIN;
            high_run = 0;
        }
        in += state == SLOT_OK;

        const int slot = (int)((p->wt_ms - midnight_ms) / SLOT_MS);
        if (slot >= 0 && slot < SLOTS) {
            const uint8_t cur = slots[slot];
            /* priorite : bas > haut > cible > sans donnee */
            if (state == SLOT_LOW || (state == SLOT_HIGH && cur != SLOT_LOW) || (state == SLOT_OK && cur < SLOT_OK)) {
                slots[slot] = state;
            }
        }
    }
    t->low_events += low_run >= EVENT_MIN;
    t->high_events += high_run >= EVENT_MIN;
    if (t->n > 0) {
        t->mean = (int)((sum + t->n / 2) / t->n);
        t->tir = (in * 100 + t->n / 2) / t->n;
        t->coverage = t->n * 100 / t->expected;
        t->coverage = t->coverage > 100 ? 100 : t->coverage;
        t->reliable = t->coverage >= GLY_STATS_MIN_COVERAGE_PCT;
    }
}

static void set_tile(tile_id_t id, const char *value, lv_color_t color)
{
    ui_set_text(s_tiles[id].value, value);
    ui_set_text_color(s_tiles[id].value, color);
}

void ui_today_update(const ui_data_t *d, bool force)
{
    if (!force && !d->history_changed && !d->minute_changed) {
        return;
    }
    char buf[96];
    if (!d->time_ok) {
        ui_set_text(s_subtitle, tr(S_TIME_NOT_SYNCED));
        for (int i = 0; i < T_COUNT; i++) {
            set_tile((tile_id_t)i, "--", UI_G_STALE);
        }
        return;
    }

    today_t t;
    uint8_t slots[SLOTS];
    compute_today(d, local_midnight_ms(&d->now_tm), &t, slots);
    const int32_t now_pos = (int32_t)((d->now_tm.tm_hour * 60 + d->now_tm.tm_min) * 1000 / (24 * 60));
    if (memcmp(slots, s_slots, sizeof(slots)) != 0 || now_pos != s_now_pos) {
        memcpy(s_slots, slots, sizeof(slots));
        s_now_pos = now_pos;
        lv_obj_invalidate(s_timeline);
    }

    ui_set_text(s_subtitle, tr(S_SINCE_MIDNIGHT)); /* la date complete est dans le bandeau : aucun doublon */

    if (t.n == 0) {
        for (int i = 0; i < T_COUNT; i++) {
            set_tile((tile_id_t)i, "--", UI_G_STALE);
        }
        snprintf(buf, sizeof(buf), tr(S_MEASURES), 0);
        ui_set_text(s_tiles[T_COVERAGE].sub, buf);
        return;
    }
    const lv_color_t neutral = t.reliable ? UI_G_TEXT : UI_G_STALE;
    snprintf(buf, sizeof(buf), "%d %%", t.tir);
    set_tile(T_TIR, buf, t.reliable ? UI_G_OK : UI_G_STALE);
    snprintf(buf, sizeof(buf), "%d", t.mean);
    set_tile(T_MEAN, buf, neutral);
    snprintf(buf, sizeof(buf), "%d", t.min);
    set_tile(T_MIN, buf, ui_glucose_color(t.min));
    snprintf(buf, sizeof(buf), "%d", t.max);
    set_tile(T_MAX, buf, ui_glucose_color(t.max));
    snprintf(buf, sizeof(buf), "%d %%", t.coverage);
    set_tile(T_COVERAGE, buf, t.reliable ? UI_G_TEXT : UI_G_HIGH);
    snprintf(buf, sizeof(buf), tr(S_MEASURES), t.n);
    ui_set_text(s_tiles[T_COVERAGE].sub, buf);

    ui_format_duration((int64_t)t.low_n * 300, false, buf, sizeof(buf));
    set_tile(T_TIME_LOW, buf, t.low_n > 0 ? UI_G_CRIT : neutral);
    ui_format_duration((int64_t)t.high_n * 300, false, buf, sizeof(buf));
    set_tile(T_TIME_HIGH, buf, t.high_n > 0 ? UI_G_HIGH : neutral);
    snprintf(buf, sizeof(buf), "%d", t.low_events);
    set_tile(T_EVENTS_LOW, buf, t.low_events > 0 ? UI_G_CRIT : neutral);
    snprintf(buf, sizeof(buf), "%d", t.high_events);
    set_tile(T_EVENTS_HIGH, buf, t.high_events > 0 ? UI_G_HIGH : neutral);
}
