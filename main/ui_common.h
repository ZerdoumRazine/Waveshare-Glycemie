/* ui_common.h: Shared LVGL widgets, fonts and colors.
 * Shared definitions for the firmware implementation.
 */
/*
 * Common interface elements: dimensions, colors, fonts, calculated data
 * once a second and shared between pages.
 */
/* Public interface / Interface publique
 * Shared LVGL colours, fonts and formatting helpers for all pages.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "lvgl.h"
#include "app_state.h"
#include "glucose_math.h"

#define UI_SCR_W        1024
#define UI_SCR_H        600
#define UI_TOP_H        48
#define UI_NAV_H        56
#define UI_CONTENT_H    (UI_SCR_H - UI_TOP_H - UI_NAV_H)

#define UI_COL_BG       lv_color_hex(0x000000)
#define UI_COL_BAR      lv_color_hex(0x0B1220)
#define UI_COL_PANEL    lv_color_hex(0x141414)
#define UI_COL_BORDER   lv_color_hex(0x2A2A2A)
#define UI_COL_TEXT     lv_color_hex(0xFFFFFF)
#define UI_COL_DIM      lv_color_hex(0x7A7A7A)
#define UI_COL_GREEN    lv_color_hex(0x3DDC84)
#define UI_COL_ORANGE   lv_color_hex(0xFFA000)
#define UI_COL_RED      lv_color_hex(0xFF4040)
#define UI_COL_BLUE     lv_color_hex(0x2F6FD6)
#define UI_COL_CHART_BG lv_color_hex(0x0C0C0C)

/* Theme bleu nuit (ACCUEIL, CONSOMMABLES) : fond en degrade, cartes discretes */
#define UI_COL_NAVY_TOP  lv_color_hex(0x0C1628)
#define UI_COL_NAVY_BOT  lv_color_hex(0x03060C)
#define UI_COL_CARD_TOP  lv_color_hex(0x13203A)
#define UI_COL_CARD_BOT  lv_color_hex(0x0B1426)
#define UI_COL_CARD_LINE lv_color_hex(0x22324F)
#define UI_COL_CARD_DOWN lv_color_hex(0x1A2A48) /* carte appuyee */
#define UI_COL_SUBTLE    lv_color_hex(0x8792A8) /* secondary text */
#define UI_COL_FAINT     lv_color_hex(0x5F6B82) /* tertiary text */
#define UI_COL_SOFT      lv_color_hex(0xDDE3EE) /* clear text attached */
#define UI_COL_DATE      lv_color_hex(0xAFBACD)
#define UI_COL_TRACK     lv_color_hex(0x1C2940) /* bottom of the progression bars */

/* Style 2.5D "dark glass" (visual recast): night to black blue background, translucent cards with thin edge and soft shadow.
 * Colours d'accent de la glycemie plus douces ; le panneau d'alert keeps its colors (UI_COL_REDORANGE...). */
#define UI_G_BG_TOP      lv_color_hex(0x0C1834)
#define UI_G_BG_BOT      lv_color_hex(0x000000)
#define UI_G_GLASS_TOP   lv_color_hex(0x192844) /* verre 84 % deja melange au fond */
#define UI_G_GLASS_BOT   lv_color_hex(0x0B1424) /* verre 73 % deja melange au fond */
#define UI_G_GLASS_DOWN  lv_color_hex(0x2A3F66) /* carte appuyee */
#define UI_G_EDGE        lv_color_hex(0x46609A) /* bord fin (opacite partielle) */
#define UI_G_LINE        lv_color_hex(0x1C2A44) /* web and navigation nets */
#define UI_G_TEXT        lv_color_hex(0xF4F7FC)
#define UI_G_TEXT2       lv_color_hex(0x9AA8C2)
#define UI_G_TEXT3       lv_color_hex(0x63718C)
#define UI_G_ACCENT      lv_color_hex(0x4C8DFF) /* bleu d'interface (boutons, anneau) */
#define UI_G_ACCENT_DEEP lv_color_hex(0x2F5FD6)
#define UI_G_OK          lv_color_hex(0x42E2B8) /* glycemie normale : vert-bleu doux */
#define UI_G_LOW         lv_color_hex(0xFFC04D) /* basse : ambre */
#define UI_G_HIGH        lv_color_hex(0xFF9A3D) /* elevee : orange */
#define UI_G_CRIT        lv_color_hex(0xFF5C6C) /* tres basse / tres elevee : rouge doux */
#define UI_G_STALE       lv_color_hex(0x6B7A94)

typedef struct {
    lv_font_t *value;  /* Giant value */
    lv_font_t *delta;  /* delta */
    lv_font_t *xl;     /* large numbers (statistics, sensor) */
    lv_font_t *title;  /* titres */
    lv_font_t *l;
    lv_font_t *m;
    lv_font_t *s;
    lv_font_t *nav;
    lv_font_t *clock;  /* heure du bandeau */
    lv_font_t *card;   /* card values */
    lv_font_t *xs;     /* petites mentions */
    lv_font_t *label;  /* card titles (gras) */
    lv_font_t *navbar; /* barre de navigation (6 pages) */
} ui_fonts_t;

typedef enum {
    UI_ICON_TARGET = 0, /* cible (TIR) */
    UI_ICON_BARS,       /* mini histogramme (moyenne) */
    UI_ICON_SENSOR,     /* sensor disc */
    UI_ICON_DROP,       /* drop (catheter) */
    UI_ICON_DOWN,       /* bas */
    UI_ICON_UP,         /* haut */
} ui_icon_t;

typedef struct {
    app_state_t st;
    glucose_point_t *hist;  /* GLY_HISTORY_MAX mesures, en PSRAM */
    int count;
    uint32_t seq;
    bool history_changed;
    bool minute_changed;
    bool time_ok;
    int64_t now_ms;
    struct tm now_tm;
    int64_t age_s;        /* -1 si inconnu */
    gm_freshness_t fresh;
    bool delta_ok;
    int delta;
    int32_t cadence_s;
    int64_t next_ms;      /* next estimated measure (0 if unknown) */
} ui_data_t;

extern ui_fonts_t g_fonts;
extern ui_data_t g_ui;

void ui_fonts_init(void);
lv_font_t *ui_font_create(bool bold, int32_t px); /* police Montserrat supplementaire (TinyTTF, glyphes en PSRAM) */
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
lv_obj_t *ui_box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t bg);
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, const lv_font_t *font, int32_t w, int32_t h,
                    lv_event_cb_t cb, void *user_data);
lv_color_t ui_glucose_color(int mgdl);
lv_color_t ui_freshness_color(gm_freshness_t fresh);
const char *ui_freshness_text(gm_freshness_t fresh);
const char *ui_trend_text(uint8_t trend);
void ui_format_hm(int64_t epoch_ms, char *buf, size_t size);
void ui_format_age(int64_t age_s, char *buf, size_t size);

/* Theme bleu nuit */
void ui_navy_background(lv_obj_t *obj);
lv_obj_t *ui_card(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius);

/* 2.5D style: degrade background of the screen, glass (translucent, thin edge, soft shadow), halo, pill buttons,
 * fine ring, small icons in shapes LVGL native (no image) */
void ui_screen_background(lv_obj_t *obj);
void ui_glass_style(lv_obj_t *obj, int32_t radius);
lv_obj_t *ui_glow(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t color, lv_opa_t opa, int32_t blur);
void ui_glow_set(lv_obj_t *glow, lv_color_t color, lv_opa_t opa); /* only redraws if the value changes */
void ui_pill_buttons_style(lv_obj_t *btnm, const lv_font_t *font, bool glass_container);
lv_obj_t *ui_ring(lv_obj_t *parent, int32_t size, int32_t width);
void ui_ring_set(lv_obj_t *ring, int value, lv_color_t color);
lv_obj_t *ui_icon(lv_obj_t *parent, ui_icon_t icon, lv_color_t color);
lv_obj_t *ui_progress(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h); /* barre 0..100 */

/* Update only if value changes (do not redraw every second) */
void ui_set_text(lv_obj_t *label, const char *text);
void ui_set_text_color(lv_obj_t *obj, lv_color_t color);

/* "Tuesday 15 September", "15/09 at 16:42", duration "2 days 8 hours" / "17:05" (compact: "17 hours") */
void ui_format_date_long(const struct tm *tm_local, char *buf, size_t size);
void ui_format_day_hm(int64_t epoch_s, char *buf, size_t size);
void ui_format_duration(int64_t seconds, bool compact, char *buf, size_t size);
