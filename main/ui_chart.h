/*
 * Graphique de glycemie reutilisable (accueil et page GRAPHIQUE) :
 * zone cible, seuils, courbe par creneaux de 5 min, axe horaire, derniere valeur.
 *
 * Exploration tactile (option) : un appui ou un glissement sur la zone de trace selectionne la mesure REELLE la plus
 * proche horizontalement (jamais de valeur interpolee) et affiche un curseur, le point et une bulle
 * (heure, glycemie, tendance et variation seulement si l'historique les contient). Le curseur disparait quelques
 * secondes apres le relachement. Aucune lecture microSD ni allocation pendant le glissement.
 */
/* Public interface / Interface publique
 * EN: Graph widget and point exploration from cached glucose history.
 * FR: Courbe et exploration des mesures historiques.
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
    lv_obj_t *card;          /* verre englobant la courbe et ses axes */
    lv_obj_t *box;
    lv_obj_t *band;
    lv_obj_t *line_low;
    lv_obj_t *line_high;
    lv_obj_t *glow;          /* meme courbe, trait large et translucide, sous la courbe fine */
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

    /* Exploration tactile : positions des points reellement traces (pixels dans la zone) et index d'historique */
    bool explore;
    int16_t *pt_x;
    int16_t *pt_y;
    int16_t *pt_index;
    int pt_count;
    int selected;            /* index dans pt_* ; -1 = aucun */
    int64_t sel_wt_ms;       /* horodatage du point selectionne (retrouve apres un redessin) */
    bool cursor_visible;
    uint32_t last_move_ms;   /* limitation du rythme pendant le glissement */
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

/* Exploration tactile : a appeler une fois apres ui_chart_create (aucun effet si la memoire manque) */
void ui_chart_enable_explore(ui_chart_t *c);
/* A appeler a chaque seconde : masque le curseur quelques secondes apres le relachement */
void ui_chart_explore_tick(ui_chart_t *c);
/* Masque immediatement le curseur (changement de duree ou de page) */
void ui_chart_explore_reset(ui_chart_t *c);
/* Console : etat de l'exploration (aucun appel LVGL) */
void ui_chart_explore_print(const ui_chart_t *c);
