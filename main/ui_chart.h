/* ui_chart.h: Chart drawing and selected time-window contracts.
 * Shared definitions for the firmware implementation.
 */
/*
 * Graphique de glycemie reutilisable (accueil et page GRAPHIQUE) :
 * target area, thresholds, 5 min crennel curve, hourly axis, last value.
 *
 * Tactile Exploration (Option): A support or slip on the trace area selects the measurement REELLE Most
 * close horizontally (never of interpole value) and displays a cursor, point and bubble
 * (hour, glycemia, trend and variation only if history contains them). The cursor disappears a few
 * seconds after release. No microSD reading or allocation during slippage.
 */
/* Public interface / Interface publique
 * Graph widget and point exploration from cached glucose history.
 */
#pragma once

#include "ui_common.h"

#define UI_CHART_MAX_XLABELS 8
#define UI_CHART_CURSOR_HIDE_MS 4000

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    bool time_labels;
    lv_obj_t *card;          /* glass covering the curve and its axes */
    lv_obj_t *box;
    lv_obj_t *band;
    lv_obj_t *line_low;
    lv_obj_t *line_high;
    lv_obj_t *glow;          /* same curve, broad and translucent line, under the fine curve */
    lv_chart_series_t *glow_series;
    lv_obj_t *chart;
    lv_chart_series_t *series;
    lv_obj_t *y_top;
    lv_obj_t *y_high;
    lv_obj_t *y_low;
    lv_obj_t *x_labels[UI_CHART_MAX_XLABELS];
    lv_obj_t *last_value;
    lv_obj_t *range_label;
    lv_point_precise_t low_pts[2];
    lv_point_precise_t high_pts[2];
    int y_max;

    /* Touch exploration: positions of real trace points (pixels in the area) and historical index */
    bool explore;
    int16_t *pt_x;
    int16_t *pt_y;
    int16_t *pt_index;
    int pt_count;
    int selected;            /* index in pt * ; -1 = none */
    int64_t sel_wt_ms;       /* time stamping of the selected point (refined after a redrawing) */
    bool cursor_visible;
    uint32_t last_move_ms;   /* limiting the speed during the slide */
    uint32_t release_ms;     /* 0 = doigt pose ou curseur deja masque */
    lv_obj_t *cursor;        /* trait vertical */
    lv_obj_t *dot;           /* point selectionne */
    lv_obj_t *bubble;
    lv_obj_t *bubble_time;
    lv_obj_t *bubble_value;
    lv_obj_t *bubble_trend;
} ui_chart_t;

void ui_chart_create(ui_chart_t *c, lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, bool time_labels);
void ui_chart_update(ui_chart_t *c, const ui_data_t *d, int hours);

/* Touch exploration: to call once after ui_chart_create (no effect if memory is missing) */
void ui_chart_enable_explore(ui_chart_t *c);
/* To call every second: hide the cursor a few seconds after release */
void ui_chart_explore_tick(ui_chart_t *c);
/* Immediately mask the cursor (duration or page change) */
void ui_chart_explore_reset(ui_chart_t *c);
/* Console: state of exploration (no call LVGL) */
void ui_chart_explore_print(const ui_chart_t *c);
