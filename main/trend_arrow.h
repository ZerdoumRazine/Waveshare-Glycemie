/*
 * Icone de tendance 2.5D dessinee avec les primitives LVGL (lisible a distance, aucune image ni police requise).
 */
/* Public interface / Interface publique
 * EN: Trend symbol drawing; an icon is not a medical prediction.
 * FR: Affichage de tendance sans prédiction médicale.
 */
#pragma once

#include <stdint.h>
#include "lvgl.h"

lv_obj_t *trend_arrow_create(lv_obj_t *parent, int32_t size, const lv_font_t *fallback_font);
void trend_arrow_set(lv_obj_t *arrow, uint8_t trend, lv_color_t color);
/* Intensite du halo (respiration lente) ; ne redessine que l'icone */
void trend_arrow_set_glow(lv_obj_t *arrow, lv_opa_t opa);
