/*
 * Draws historical glucose curves with touch exploration and variable time windows.
 * Trace les courbes de glycémie et gère l'exploration tactile.
 */
#include "ui_chart.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "glucose_math.h"
#include "touch_diag.h"

#define CHART_Y_MIN 40
#define SLOT_MS     (5 * 60 * 1000)
#define BUBBLE_W    186
#define BUBBLE_H    104
#define EXPLORE_MIN_MS 40 /* limitation du rythme pendant le glissement */

static int32_t value_to_y(const ui_chart_t *c, int value)
{
    if (value < CHART_Y_MIN) {
        value = CHART_Y_MIN;
    }
    if (value > c->y_max) {
        value = c->y_max;
    }
    return (int32_t)((int64_t)(c->y_max - value) * (c->h - 1) / (c->y_max - CHART_Y_MIN));
}

/* Position target bands and glucose scale consistently across time ranges. */
static void layout_references(ui_chart_t *c)
{
    const int32_t y_low = value_to_y(c, GLY_RANGE_LOW);
    const int32_t y_high = value_to_y(c, GLY_RANGE_HIGH);

    lv_obj_set_pos(c->band, 0, y_high);
    lv_obj_set_size(c->band, c->w, y_low - y_high);

    c->low_pts[0] = (lv_point_precise_t){0, y_low};
    c->low_pts[1] = (lv_point_precise_t){c->w - 1, y_low};
    c->high_pts[0] = (lv_point_precise_t){0, y_high};
    c->high_pts[1] = (lv_point_precise_t){c->w - 1, y_high};
    lv_line_set_points(c->line_low, c->low_pts, 2);
    lv_line_set_points(c->line_high, c->high_pts, 2);

    const int32_t box_y = c->y;
    const int32_t box_x = c->x;
    lv_obj_set_pos(c->y_low, box_x - 62, box_y + y_low - 13);
    lv_obj_set_pos(c->y_high, box_x - 62, box_y + y_high - 13);
    lv_obj_set_pos(c->y_top, box_x - 62, box_y - 4);
    lv_label_set_text_fmt(c->y_top, "%d", c->y_max);
    lv_chart_set_axis_range(c->chart, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN, c->y_max);
    lv_chart_set_axis_range(c->glow, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN, c->y_max);
}

static lv_obj_t *chart_layer(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *chart = lv_chart_create(parent);
    lv_obj_set_pos(chart, 0, 0);
    lv_obj_set_size(chart, w, h);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_radius(chart, 0, 0);
    lv_obj_set_style_pad_all(chart, 0, 0);
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    return chart;
}

static lv_obj_t *reference_line(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_opa(line, LV_OPA_60, 0);
    lv_obj_set_style_line_dash_width(line, 6, 0);
    lv_obj_set_style_line_dash_gap(line, 6, 0);
    return line;
}

void ui_chart_create(ui_chart_t *c, lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, bool time_labels)
{
    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    c->time_labels = time_labels;
    c->y_max = 300;

    /* Verre englobant les valeurs de l'axe (a gauche) et les heures (en dessous) */
    c->card = ui_card(parent, x - 72, y - 16, w + 88, h + (time_labels ? 60 : 32), 22);

    c->box = ui_box(parent, x, y, w, h, UI_COL_CHART_BG);
    lv_obj_set_style_bg_opa(c->box, LV_OPA_TRANSP, 0);

    c->band = lv_obj_create(c->box);
    lv_obj_remove_style_all(c->band);
    lv_obj_set_style_bg_color(c->band, UI_G_OK, 0);
    lv_obj_set_style_bg_opa(c->band, LV_OPA_10, 0);
    lv_obj_remove_flag(c->band, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    c->line_low = reference_line(c->box, UI_G_LOW);
    c->line_high = reference_line(c->box, UI_G_HIGH);

    /* Lueur : trait large et translucide ; courbe fine claire par-dessus (memes valeurs) */
    c->glow = chart_layer(c->box, w, h);
    c->glow_series = lv_chart_add_series(c->glow, UI_G_OK, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_opa(c->glow, LV_OPA_20, LV_PART_ITEMS);
    lv_obj_set_style_width(c->glow, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(c->glow, 0, LV_PART_INDICATOR);
    c->chart = chart_layer(c->box, w, h);
    c->series = lv_chart_add_series(c->chart, lv_color_hex(0xE8F1FF), LV_CHART_AXIS_PRIMARY_Y);

    c->y_top = ui_label(parent, g_fonts.s, UI_G_TEXT3, "300");
    c->y_high = ui_label(parent, g_fonts.s, UI_G_HIGH, "180");
    c->y_low = ui_label(parent, g_fonts.s, UI_G_LOW, "70");
    lv_obj_t *const y_labels[3] = {c->y_top, c->y_high, c->y_low};
    for (int i = 0; i < 3; i++) {
        lv_obj_set_width(y_labels[i], 54);
        lv_obj_set_style_text_align(y_labels[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_opa(y_labels[i], LV_OPA_80, 0);
    }

    for (int i = 0; i < UI_CHART_MAX_XLABELS; i++) {
        c->x_labels[i] = ui_label(parent, g_fonts.s, UI_G_TEXT3, "");
        lv_obj_set_width(c->x_labels[i], 120);
        lv_obj_set_style_text_align(c->x_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_flag(c->x_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    c->last_value = ui_label(c->box, g_fonts.m, UI_COL_TEXT, "");
    lv_obj_set_width(c->last_value, 90);
    lv_obj_set_style_text_align(c->last_value, LV_TEXT_ALIGN_RIGHT, 0);

    /* Duree affichee a droite de la mini-courbe, hors de la zone de trace */
    c->range_label = ui_label(parent, g_fonts.s, UI_COL_DIM, "");
    if (time_labels) {
        lv_obj_add_flag(c->range_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_width(c->range_label, UI_SCR_W - 24 - (x + w + 8));
        lv_obj_set_style_text_align(c->range_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(c->range_label, x + w + 8, y - 4);
    }

/* Position target bands and glucose scale consistently across time ranges. */
    layout_references(c);
}

/* ------------------------------------------------------------------------- */
/* Exploration tactile : curseur, point et bulle deplaces (jamais recrees) sur la mesure reelle la plus proche */

static const char *trend_arrow_text(uint8_t trend)
{
    switch (trend) {
    case TREND_DOUBLE_UP:
        return "↑↑";
    case TREND_SINGLE_UP:
        return "↑";
    case TREND_FORTY_FIVE_UP:
        return "↗";
    case TREND_FLAT:
        return "→";
    case TREND_FORTY_FIVE_DOWN:
        return "↘";
    case TREND_SINGLE_DOWN:
        return "↓";
    case TREND_DOUBLE_DOWN:
        return "↓↓";
    default:
        return ""; /* tendance absente de l'historique : rien d'invente */
    }
}

static void explore_show(ui_chart_t *c, bool visible)
{
    lv_obj_t *const objs[3] = {c->cursor, c->dot, c->bubble};
    for (int i = 0; i < 3; i++) {
        if (objs[i] == NULL) {
            continue;
        }
        if (visible) {
            lv_obj_remove_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    c->cursor_visible = visible;
    if (!visible) {
        c->selected = -1;
        c->release_ms = 0;
    }
}

/* Selection d'un point deja trace (index dans pt_*) : aucune valeur interpolee, aucune allocation */
static void explore_select(ui_chart_t *c, int n)
{
    if (n < 0 || n >= c->pt_count || g_ui.hist == NULL) {
        return;
    }
    const int hist_index = c->pt_index[n];
    if (hist_index < 0 || hist_index >= g_ui.count) {
        explore_show(c, false);
        return;
    }
    const glucose_point_t *p = &g_ui.hist[hist_index];
    const int32_t px = c->pt_x[n];
    const int32_t py = c->pt_y[n];
    c->selected = n;
    c->sel_wt_ms = p->wt_ms;

    lv_obj_set_pos(c->cursor, px - 1, 0);
    lv_obj_set_pos(c->dot, px - 8, py - 8);
    lv_obj_set_style_bg_color(c->dot, ui_glucose_color(p->mgdl), 0);

    char buf[24];
    ui_format_hm(p->wt_ms, buf, sizeof(buf));
    ui_set_text(c->bubble_time, buf);
    snprintf(buf, sizeof(buf), "%d mg/dL", p->mgdl);
    ui_set_text(c->bubble_value, buf);
    ui_set_text_color(c->bubble_value, ui_glucose_color(p->mgdl));

    /* Tendance et variation seulement si l'historique les contient reellement pour ce point */
    const char *arrow = trend_arrow_text(p->trend);
    int delta = 0;
    const bool has_delta = hist_index + 1 < g_ui.count &&
                           gm_delta_between(p, &g_ui.hist[hist_index + 1], &delta);
    if (arrow[0] != '\0' && has_delta) {
        snprintf(buf, sizeof(buf), "%s  %+d", arrow, delta);
    } else if (arrow[0] != '\0') {
        snprintf(buf, sizeof(buf), "%s", arrow);
    } else if (has_delta) {
        snprintf(buf, sizeof(buf), "%+d", delta);
    } else {
        buf[0] = '\0';
    }
    ui_set_text(c->bubble_trend, buf);

    /* Bulle du cote ou il reste de la place, jamais hors de la zone de trace */
    int32_t bx = px + 20;
    if (bx + BUBBLE_W > c->w) {
        bx = px - 20 - BUBBLE_W;
    }
    bx = bx < 0 ? 0 : (bx + BUBBLE_W > c->w ? c->w - BUBBLE_W : bx);
    int32_t by = py - BUBBLE_H / 2;
    by = by < 0 ? 0 : (by + BUBBLE_H > c->h ? c->h - BUBBLE_H : by);
    lv_obj_set_pos(c->bubble, bx, by);
    explore_show(c, true);
}

/* Mesure reelle la plus proche horizontalement du doigt */
static void explore_select_at(ui_chart_t *c, int32_t x)
{
    int best = -1;
    int32_t best_d = INT32_MAX;
    for (int i = 0; i < c->pt_count; i++) {
        const int32_t d = c->pt_x[i] > x ? c->pt_x[i] - x : x - c->pt_x[i];
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    if (best >= 0 && best != c->selected) {
        explore_select(c, best);
    } else if (best >= 0) {
        explore_show(c, true); /* meme point : simple reaffichage */
    }
}

/* Handle touch events without changing the underlying measurements. */
static void explore_event(lv_event_t *e)
{
    ui_chart_t *c = (ui_chart_t *)lv_event_get_user_data(e);
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        c->release_ms = lv_tick_get();
        if (c->release_ms == 0) {
            c->release_ms = 1;
        }
        return;
    }
    if ((code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) || c->pt_count == 0) {
        return;
    }
    if (code == LV_EVENT_PRESSING && lv_tick_elaps(c->last_move_ms) < EXPLORE_MIN_MS) {
        return; /* rythme limite : pas de redessin a chaque pixel */
    }
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_area_t area;
    lv_obj_get_coords(c->box, &area);
    c->last_move_ms = lv_tick_get();
    c->release_ms = 0;
/* Find the closest historical point to a touch rather than inventing an interpolated reading. */
    explore_select_at(c, point.x - area.x1);
    if (code == LV_EVENT_PRESSED && c->selected >= 0) {
        const int hist_index = c->pt_index[c->selected];
        char hm[8] = "--:--";
        ui_format_hm(c->sel_wt_ms, hm, sizeof(hm));
        ESP_LOGI("ui_graph", "Point selectionne : %s, %d mg/dL (mesure %d de l'historique)", hm,
                 hist_index < g_ui.count ? g_ui.hist[hist_index].mgdl : 0, hist_index);
    }
}

void ui_chart_explore_print(const ui_chart_t *c)
{
    char hm[8] = "--:--";
    if (c->selected >= 0) {
        ui_format_hm(c->sel_wt_ms, hm, sizeof(hm));
    }
    printf("GRAPH_EXPLORE actif=%d points_traces=%d selection=%d heure=%s curseur=%s\n", c->explore, c->pt_count,
           c->selected, hm, c->cursor_visible ? "visible" : "masque");
}

void ui_chart_enable_explore(ui_chart_t *c)
{
    const size_t count = GLY_HISTORY_MAX;
    c->pt_x = heap_caps_malloc(count * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    c->pt_y = heap_caps_malloc(count * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    c->pt_index = heap_caps_malloc(count * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (c->pt_x == NULL || c->pt_y == NULL || c->pt_index == NULL) {
        free(c->pt_x);
        free(c->pt_y);
        free(c->pt_index);
        c->pt_x = c->pt_y = c->pt_index = NULL;
        return; /* sans memoire : graphique normal, sans exploration */
    }
    c->selected = -1;

    c->cursor = ui_box(c->box, 0, 0, 2, c->h, UI_G_TEXT2);
    lv_obj_set_style_bg_opa(c->cursor, LV_OPA_60, 0);
    lv_obj_add_flag(c->cursor, LV_OBJ_FLAG_HIDDEN);

    c->dot = ui_box(c->box, 0, 0, 16, 16, UI_G_OK);
    lv_obj_set_style_radius(c->dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(c->dot, UI_G_TEXT, 0);
    lv_obj_set_style_border_width(c->dot, 2, 0);
    lv_obj_set_style_border_opa(c->dot, LV_OPA_80, 0);
    lv_obj_add_flag(c->dot, LV_OBJ_FLAG_HIDDEN);

    c->bubble = ui_box(c->box, 0, 0, BUBBLE_W, BUBBLE_H, UI_G_GLASS_TOP);
    lv_obj_set_style_radius(c->bubble, 16, 0);
    lv_obj_set_style_border_color(c->bubble, UI_G_EDGE, 0);
    lv_obj_set_style_border_width(c->bubble, 1, 0);
    lv_obj_set_style_border_opa(c->bubble, LV_OPA_50, 0);
    lv_obj_add_flag(c->bubble, LV_OBJ_FLAG_HIDDEN);

    c->bubble_time = ui_label(c->bubble, g_fonts.s, UI_G_TEXT2, "");
    c->bubble_value = ui_label(c->bubble, g_fonts.m, UI_G_TEXT, "");
    c->bubble_trend = ui_label(c->bubble, g_fonts.s, UI_G_TEXT2, "");
    lv_obj_t *const lines[3] = {c->bubble_time, c->bubble_value, c->bubble_trend};
    const int32_t line_y[3] = {10, 36, 72};
    for (int i = 0; i < 3; i++) {
        lv_obj_set_width(lines[i], BUBBLE_W);
        lv_obj_set_style_text_align(lines[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lines[i], 0, line_y[i]);
        lv_label_set_long_mode(lines[i], LV_LABEL_LONG_MODE_CLIP);
    }

    lv_obj_add_flag(c->box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c->box, explore_event, LV_EVENT_ALL, c);
    touch_diag_name(c->box, "graphique (exploration)");
    c->explore = true;
}

void ui_chart_explore_tick(ui_chart_t *c)
{
    if (!c->explore || c->selected < 0 || c->release_ms == 0) {
        return;
    }
    if (lv_tick_elaps(c->release_ms) >= UI_CHART_CURSOR_HIDE_MS) {
        explore_show(c, false);
    }
}

void ui_chart_explore_reset(ui_chart_t *c)
{
    if (c->explore && c->selected >= 0) {
        explore_show(c, false);
    }
}

/* Redraw cached samples when the time range or history revision changes. */
void ui_chart_update(ui_chart_t *c, const ui_data_t *d, int hours)
{
    const int slots = hours * 12;
    const int64_t range_ms = (int64_t)hours * 3600 * 1000;
    const int64_t end_ms = d->time_ok ? d->now_ms : (d->count > 0 ? d->hist[0].wt_ms : 0);
    const int64_t start_ms = end_ms - range_ms;

    int y_max = 300;
    for (int i = 0; i < d->count && d->hist[i].wt_ms > start_ms; i++) {
        if (d->hist[i].mgdl > 290) {
            y_max = 400;
            break;
        }
    }
    c->y_max = y_max;
/* Position target bands and glucose scale consistently across time ranges. */
    layout_references(c);

    lv_chart_set_point_count(c->chart, (uint32_t)slots);
    lv_chart_set_all_values(c->chart, c->series, LV_CHART_POINT_NONE);
    lv_chart_set_point_count(c->glow, (uint32_t)slots);
    lv_chart_set_all_values(c->glow, c->glow_series, LV_CHART_POINT_NONE);
    /* Courbe fine, petits points seulement sur les durees courtes ; lueur a la couleur de la derniere valeur */
    const int32_t dot = hours <= 3 ? 6 : (hours <= 6 ? 4 : 0);
    lv_obj_set_style_width(c->chart, dot, LV_PART_INDICATOR);
    lv_obj_set_style_height(c->chart, dot, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(c->chart, hours <= 6 ? 3 : 2, LV_PART_ITEMS);
    lv_obj_set_style_line_width(c->glow, hours <= 6 ? 10 : 8, LV_PART_ITEMS);
    if (d->count > 0) {
        lv_chart_set_series_color(c->glow, c->glow_series,
                                  d->fresh == GM_FRESH_STALE ? UI_G_STALE : ui_glucose_color(d->hist[0].mgdl));
    }

    int newest_slot = -1;
    int newest_value = 0;
    int previous_value = -1;
    int slot_from_end = 0;
    int64_t anchor_ms = 0;
    bool placed = false;
    c->pt_count = 0;
    for (int i = 0; i < d->count; i++) {
        const int64_t wt_ms = d->hist[i].wt_ms;
        if (!placed) {
            int64_t ago_ms = end_ms - wt_ms;
            if (ago_ms < 0) {
                ago_ms = 0;
            }
            slot_from_end = (int)((ago_ms + SLOT_MS / 2) / SLOT_MS);
        } else {
            /* Placement d'une mesure a l'autre : quelques secondes de decalage entre mesures
             * ne creent ni faux trou ni chevauchement ; un vrai trou reste visible */
            const int step = (int)((anchor_ms - wt_ms + SLOT_MS / 2) / SLOT_MS);
            if (step <= 0) {
                continue;
            }
            slot_from_end += step;
        }
        if (slot_from_end >= slots) {
            break;
        }
        placed = true;
        anchor_ms = wt_ms;
        const int idx = slots - 1 - slot_from_end;
        int v = d->hist[i].mgdl;
        v = v < CHART_Y_MIN ? CHART_Y_MIN : (v > y_max ? y_max : v);
        lv_chart_set_series_value_by_id(c->chart, c->series, (uint32_t)idx, v);
        lv_chart_set_series_value_by_id(c->glow, c->glow_series, (uint32_t)idx, v);
        if (c->explore && c->pt_count < GLY_HISTORY_MAX) {
            /* position reelle du point trace : sert a l'exploration tactile (aucune valeur interpolee) */
            c->pt_x[c->pt_count] = (int16_t)((int64_t)idx * (c->w - 1) / (slots > 1 ? slots - 1 : 1));
            c->pt_y[c->pt_count] = (int16_t)value_to_y(c, v);
            c->pt_index[c->pt_count] = (int16_t)i;
            c->pt_count++;
        }
        if (newest_slot < 0) {
            newest_slot = idx;
            newest_value = d->hist[i].mgdl;
        } else if (previous_value < 0) {
            previous_value = d->hist[i].mgdl;
        }
    }
    lv_chart_refresh(c->glow);
    lv_chart_refresh(c->chart);

    /* Exploration en cours : le point selectionne est retrouve a sa nouvelle position (l'echelle a pu bouger) */
    if (c->explore && c->selected >= 0) {
        int found = -1;
        for (int i = 0; i < c->pt_count && found < 0; i++) {
            const int hist_index = c->pt_index[i];
            if (hist_index < d->count && d->hist[hist_index].wt_ms == c->sel_wt_ms) {
                found = i;
            }
        }
        if (found >= 0) {
            explore_select(c, found);
        } else {
            explore_show(c, false);
        }
    }

    /* Derniere valeur au bout de la courbe (graphique complet ; l'accueil l'affiche deja en grand) */
    if (newest_slot >= 0 && c->time_labels) {
        lv_label_set_text_fmt(c->last_value, "%d", newest_value);
        lv_obj_set_style_text_color(c->last_value,
                                    d->fresh == GM_FRESH_STALE ? UI_G_STALE : ui_glucose_color(newest_value), 0);
        int32_t px = (int32_t)((int64_t)newest_slot * (c->w - 1) / (slots > 1 ? slots - 1 : 1));
        int32_t py = value_to_y(c, newest_value);
        int32_t lx = px - 96;
        /* Du cote oppose a la courbe : dessous si elle descend, dessus si elle monte */
        int32_t ly = previous_value > newest_value ? py + 10 : py - 38;
        if (ly < 0) {
            ly = py + 10;
        } else if (ly + 34 > c->h) {
            ly = py - 38;
        }
        lx = lx < 0 ? 0 : lx;
        lv_obj_set_pos(c->last_value, lx, ly);
        lv_obj_remove_flag(c->last_value, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(c->last_value, LV_OBJ_FLAG_HIDDEN);
    }

    /* Axe horaire */
    const int32_t base_x = c->x;
    const int32_t label_y = c->y + c->h + 2;
    for (int i = 0; i < UI_CHART_MAX_XLABELS; i++) {
        lv_obj_add_flag(c->x_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (c->time_labels && d->time_ok) {
        const int step_min = hours <= 3 ? 30 : (hours <= 6 ? 60 : (hours <= 12 ? 120 : 240));
        time_t start_s = (time_t)(start_ms / 1000);
        struct tm tm_start;
        localtime_r(&start_s, &tm_start);
        const int minute_of_day = tm_start.tm_hour * 60 + tm_start.tm_min;
        const int mod = minute_of_day % step_min;
        time_t t = start_s - tm_start.tm_sec + (time_t)((mod == 0 && tm_start.tm_sec == 0) ? 0 : (step_min - mod)) * 60;
        int n = 0;
        for (; t * 1000LL <= end_ms && n < UI_CHART_MAX_XLABELS; t += step_min * 60) {
            struct tm tm_tick;
            localtime_r(&t, &tm_tick);
            const int32_t x = (int32_t)(((int64_t)t * 1000 - start_ms) * c->w / range_ms);
            if (step_min >= 60) {
                lv_label_set_text_fmt(c->x_labels[n], "%dh", tm_tick.tm_hour);
            } else {
                lv_label_set_text_fmt(c->x_labels[n], "%02d:%02d", tm_tick.tm_hour, tm_tick.tm_min);
            }
            int32_t lx = base_x + x - 60;
            lv_obj_set_pos(c->x_labels[n], lx, label_y);
            lv_obj_remove_flag(c->x_labels[n], LV_OBJ_FLAG_HIDDEN);
            n++;
        }
    } else if (!c->time_labels) {
        lv_label_set_text_fmt(c->range_label, "%d h", hours);
    }
    if (c->time_labels) {
        lv_label_set_text(c->range_label, "");
    }
}
