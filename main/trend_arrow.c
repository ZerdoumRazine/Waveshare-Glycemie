/*
 * trend_arrow.c: Maps Dexcom trend states to arrow rendering.
 * Design note: Unknown trend is not the same as a stable trend.
 */

/*
 * Draws a trend icon and halo using LVGL primitives shared with the home view.
 * Draw the trend icon and its bright halo.
 */
#include "trend_arrow.h"

#include <math.h>
#include "app_state.h"
#include "ui_common.h"

/*
 * Icone de tendance 2.5D : disque lumineux flou derriere (enfant, ombre coloree LVGL) et fleche pleine dessinee a la
 * steals after children (event LV_EVENT_DRAW_POST) : tige arrondie et pointe triangulaire. Aucun objet par
 * segment, aucune image. trend_arrow_set_glow() regle l'intensite du disque (respiration lente de l'accueil).
 */

typedef struct {
    int32_t size;
    uint8_t trend;
    lv_color_t color;
    lv_opa_t glow;
    lv_obj_t *halo;
    lv_obj_t *label;
} arrow_ctx_t;

static bool has_arrow(uint8_t trend)
{
    return trend >= TREND_DOUBLE_UP && trend <= TREND_DOUBLE_DOWN;
}

static lv_point_precise_t pt(float x, float y)
{
    return (lv_point_precise_t){(lv_value_precise_t)lroundf(x), (lv_value_precise_t)lroundf(y)};
}

/* Fleche centree en (cx, cy), orientee selon angle_deg (0 = droite, 90 = haut), longueur totale length */
/* Draw the symbol after child objects to preserve legibility. */
static void draw_arrow(lv_layer_t *layer, const arrow_ctx_t *ctx, float cx, float cy, float angle_deg, float length)
{
    const float rad = angle_deg * (float)M_PI / 180.0f;
    const float dx = cosf(rad);
    const float dy = -sinf(rad); /* axe Y de l'ecran vers le bas */
    const float px = -dy;
    const float py = dx;
    const float tip_x = cx + dx * length / 2.0f;
    const float tip_y = cy + dy * length / 2.0f;
    const float tail_x = cx - dx * length / 2.0f;
    const float tail_y = cy - dy * length / 2.0f;
    const float head_len = length * 0.44f;
    const float head_half = length * 0.34f;
    const float base_x = tip_x - dx * head_len;
    const float base_y = tip_y - dy * head_len;
    const lv_color_t core = lv_color_mix(lv_color_white(), ctx->color, 110); /* coeur lumineux */

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.p1 = pt(tail_x, tail_y);
    line.p2 = pt(base_x + dx * head_len * 0.25f, base_y + dy * head_len * 0.25f); /* the stem enters the tip */
    line.width = (int32_t)lroundf((float)ctx->size * 0.11f);
    line.color = core;
    line.round_start = 1;
    lv_draw_line(layer, &line);

    lv_draw_triangle_dsc_t tri;
    lv_draw_triangle_dsc_init(&tri);
    tri.p[0] = pt(tip_x, tip_y);
    tri.p[1] = pt(base_x + px * head_half, base_y + py * head_half);
    tri.p[2] = pt(base_x - px * head_half, base_y - py * head_half);
    tri.color = core;
    tri.opa = LV_OPA_COVER;
    lv_draw_triangle(layer, &tri);
}

static void on_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    arrow_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (ctx == NULL || !has_arrow(ctx->trend)) {
        return;
    }
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const float s = (float)ctx->size;
    const float cx = (float)a.x1 + s / 2.0f;
    const float cy = (float)a.y1 + s / 2.0f;

    switch (ctx->trend) {
    case TREND_DOUBLE_UP:
        draw_arrow(layer, ctx, cx - s * 0.20f, cy, 90.0f, s * 0.64f);
        draw_arrow(layer, ctx, cx + s * 0.20f, cy, 90.0f, s * 0.64f);
        break;
    case TREND_SINGLE_UP:
        draw_arrow(layer, ctx, cx, cy, 90.0f, s * 0.70f);
        break;
    case TREND_FORTY_FIVE_UP:
        draw_arrow(layer, ctx, cx, cy, 45.0f, s * 0.70f);
        break;
    case TREND_FLAT:
        draw_arrow(layer, ctx, cx, cy, 0.0f, s * 0.70f);
        break;
    case TREND_FORTY_FIVE_DOWN:
        draw_arrow(layer, ctx, cx, cy, -45.0f, s * 0.70f);
        break;
    case TREND_SINGLE_DOWN:
        draw_arrow(layer, ctx, cx, cy, -90.0f, s * 0.70f);
        break;
    default: /* TREND_DOUBLE_DOWN */
        draw_arrow(layer, ctx, cx - s * 0.20f, cy, -90.0f, s * 0.64f);
        draw_arrow(layer, ctx, cx + s * 0.20f, cy, -90.0f, s * 0.64f);
        break;
    }
}

lv_obj_t *trend_arrow_create(lv_obj_t *parent, int32_t size, const lv_font_t *fallback_font)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, size, size);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    arrow_ctx_t *ctx = lv_malloc_zeroed(sizeof(arrow_ctx_t));
    LV_ASSERT_MALLOC(ctx);
    ctx->size = size;
    ctx->trend = TREND_NONE;
    ctx->color = lv_color_white();
    ctx->glow = LV_OPA_40;
    lv_obj_set_user_data(cont, ctx);

    /* Light: 16% core and 34% blur of the icon (soft patch without outline, contained in the icon) */
    const int32_t disc = size * 16 / 100;
    ctx->halo = ui_glow(cont, disc, disc, ctx->color, ctx->glow, size * 34 / 100);
    lv_obj_center(ctx->halo);
    lv_obj_add_flag(ctx->halo, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(cont, on_draw, LV_EVENT_DRAW_POST, NULL);

    ctx->label = lv_label_create(cont);
    lv_obj_set_style_text_font(ctx->label, fallback_font, 0);
    lv_obj_center(ctx->label);
    lv_label_set_text(ctx->label, "");
    lv_obj_add_flag(ctx->label, LV_OBJ_FLAG_HIDDEN);
    return cont;
}

/* Choose the icon shape from the received trend value. */
void trend_arrow_set(lv_obj_t *arrow, uint8_t trend, lv_color_t color)
{
    arrow_ctx_t *ctx = lv_obj_get_user_data(arrow);
    if (ctx == NULL) {
        return;
    }
    ctx->trend = trend;
    ctx->color = color;
    if (has_arrow(trend)) {
        ui_glow_set(ctx->halo, color, ctx->glow);
        lv_obj_remove_flag(ctx->halo, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ctx->halo, LV_OBJ_FLAG_HIDDEN);
    }
    if (trend == TREND_NOT_COMPUTABLE || trend == TREND_RATE_OUT_OF_RANGE) {
        lv_label_set_text(ctx->label, trend == TREND_NOT_COMPUTABLE ? "?" : "-");
        lv_obj_set_style_text_color(ctx->label, color, 0);
        lv_obj_remove_flag(ctx->label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ctx->label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_invalidate(arrow);
}

/* Adjust glow as a visual cue only; it does not change alert thresholds. */
void trend_arrow_set_glow(lv_obj_t *arrow, lv_opa_t opa)
{
    arrow_ctx_t *ctx = lv_obj_get_user_data(arrow);
    if (ctx != NULL && ctx->glow != opa) {
        ctx->glow = opa;
        if (has_arrow(ctx->trend)) {
            ui_glow_set(ctx->halo, ctx->color, opa); /* invalide le disque (et la fleche qui le recouvre) */
        }
    }
}
