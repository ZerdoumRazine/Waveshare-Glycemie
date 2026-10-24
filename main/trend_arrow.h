/* trend_arrow.h: Trend arrow rendering and mapping interface.
 * The declarations below are shared with other firmware modules.
 */
/*
 * Trend Icon 2.5D drawing with primitives LVGL (readable at distance, no images or font required).
 */
/* Public interface / Interface publique
 * Trend symbol drawing; an icon is not a medical prediction.
 */
#pragma once

#include <stdint.h>
#include "lvgl.h"

lv_obj_t *trend_arrow_create(lv_obj_t *parent, int32_t size, const lv_font_t *fallback_font);
void trend_arrow_set(lv_obj_t *arrow, uint8_t trend, lv_color_t color);
/* Intensite du halo (respiration lente) ; ne redessine que l'icone */
void trend_arrow_set_glow(lv_obj_t *arrow, lv_opa_t opa);
