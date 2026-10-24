/*
 * night_mode.c: Scheduled dim display and temporary wake-up.
 * Design note: Urgent alerts still take priority in night mode.
 */

/*
 * Controls the night display, brightness transitions and temporary touch previews.
 * Pilot night mode, brightness and touch previews.
 */
#include "night_mode.h"

#include "i18n.h"

#include <stdio.h>
#include <string.h>
#include "bsp/display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "settings.h"
#include "touch_diag.h"
#include "trend_arrow.h"
#include "ui_alert.h"

static const char *TAG = "nuit";

#define FADE_PERIOD_MS 40
#define FADE_MS        1500                  /* transition douce du retroeclairage */
#define TOUCH_AWAKE_US (30 * 1000000LL)      /* normal interface 30 s after the last interaction */
#define ALERT_AWAKE_US (45 * 1000000LL)      /* return to the screen night 45 s after the disappearance of the warning sign */
#define PREVIEW_US     (60 * 1000000LL)      /* apercu TEST MODE NUIT : 60 s au plus */

#define NIGHT_COL_TIME lv_color_hex(0xAAB7CE)
#define NIGHT_COL_TEXT lv_color_hex(0x8C8C8C)
#define NIGHT_COL_HINT lv_color_hex(0xB07A00)

typedef enum {
    SIM_AUTO = 0, /* horaire reel */
    SIM_NIGHT,    /* test : nuit simulee */
} sim_t;

static const char *const k_sim_names[] = {"horaire reel", "nuit simulee"};

/* Night screen: child of the screen in the foreground, opaque and clickable (the first touch remains there) */
static lv_obj_t *s_screen;
static lv_obj_t *s_time;
static lv_obj_t *s_halo;  /* halo very discreet behind the glycemia (color shade, no image) */
static lv_obj_t *s_value;
static lv_obj_t *s_arrow;
static lv_obj_t *s_dot;
static lv_obj_t *s_status;
static lv_obj_t *s_last;
static lv_obj_t *s_hint;
static char s_time_text[8];
static char s_value_text[8];
static char s_status_text[64];
static char s_last_text[96];
static lv_color_t s_value_color;
static lv_color_t s_dot_color;
static lv_color_t s_status_color;
static lv_color_t s_arrow_color;
static int s_arrow_trend = -1;

/* Retroeclairage et etat */
static lv_timer_t *s_fade;
static int s_level = -1;              /* luminosite appliquee (%) */
static int s_target = -1;
static int s_fade_from;
static int64_t s_fade_start_us;
static bool s_night;
static bool s_preview;
static int64_t s_preview_until_us;
static int64_t s_touch_until_us;
static int64_t s_alert_until_us;
static volatile bool s_screen_visible;
static volatile night_request_t s_request;
static volatile sim_t s_sim;
static const char *volatile s_state = "demarrage";

static void set_level(int level)
{
    static bool s_bsp_quiet;
    if (level != s_level) {
        if (s_level >= 0 && !s_bsp_quiet) {
            /* bsp_display_brightness_set log each call: traces of information of the BSP masks of the first
             * change after the start (transitions make several; night mode logs its states) */
            esp_log_level_set("ESP32_P4_EV", ESP_LOG_WARN);
            s_bsp_quiet = true;
        }
        bsp_display_brightness_set(level);
        s_level = level;
    }
}

static void fade_cb(lv_timer_t *timer)
{
    (void)timer;
    const int64_t elapsed = esp_timer_get_time() - s_fade_start_us;
    if (elapsed >= FADE_MS * 1000LL) {
        set_level(s_target);
        lv_timer_pause(s_fade);
        return;
    }
    set_level(s_fade_from + (int)((s_target - s_fade_from) * elapsed / (FADE_MS * 1000LL)));
}

static void go_to(int target, bool immediate)
{
    if (s_level < 0 || (immediate && s_level != target)) {
        s_target = target;
        lv_timer_pause(s_fade);
        set_level(target);
        return;
    }
    if (target != s_target) {
        s_target = target;
        s_fade_from = s_level;
        s_fade_start_us = esp_timer_get_time();
        lv_timer_resume(s_fade);
    }
}

static bool wakes_screen(alert_type_t style)
{
    return style == ALERT_VERY_LOW || style == ALERT_LOW || style == ALERT_NO_DATA || style == ALERT_VERY_HIGH ||
           style == ALERT_HIGH;
}

/* Interpret schedules correctly when the end time is on the next day. */
static bool in_night_period(const night_settings_t *n, const ui_data_t *d)
{
    if (!n->enabled || !d->time_ok || n->start_min == n->end_min) {
        return false;
    }
    const int minute = d->now_tm.tm_hour * 60 + d->now_tm.tm_min;
    return n->start_min < n->end_min ? minute >= n->start_min && minute < n->end_min
                                     : minute >= n->start_min || minute < n->end_min;
}

/* ------------------------------------------------------------------------- */
/* Minimum night screen */

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void set_text(lv_obj_t *label, char *shown, size_t size, const char *text)
{
    if (strcmp(shown, text) != 0) {
        strlcpy(shown, text, size);
        lv_label_set_text(label, shown);
    }
}

static void set_text_color(lv_obj_t *label, lv_color_t *shown, lv_color_t color)
{
    if (!lv_color_eq(*shown, color)) {
        *shown = color;
        lv_obj_set_style_text_color(label, color, 0);
    }
}

/* Content updated only if it changes (no redrawing every second) */
static void refresh_screen(const ui_data_t *d)
{
    char buf[96];
    if (d->time_ok) {
        snprintf(buf, sizeof(buf), "%02d:%02d", d->now_tm.tm_hour, d->now_tm.tm_min);
    } else {
        strlcpy(buf, "--:--", sizeof(buf));
    }
    set_text(s_time, s_time_text, sizeof(s_time_text), buf);

    lv_color_t value_color = UI_COL_DIM;
    lv_color_t fresh_color = UI_COL_DIM;
    int trend = TREND_NONE;
    if (d->count == 0) {
        set_text(s_value, s_value_text, sizeof(s_value_text), "---");
        set_text(s_status, s_status_text, sizeof(s_status_text), tr(S_FRESH_WAITING));
        set_text(s_last, s_last_text, sizeof(s_last_text), tr(S_NO_READING));
    } else {
        const glucose_point_t *p = &d->hist[0];
        const bool stale = d->fresh == GM_FRESH_STALE || d->fresh == GM_FRESH_NONE;
        snprintf(buf, sizeof(buf), "%d", p->mgdl);
        set_text(s_value, s_value_text, sizeof(s_value_text), buf);
        fresh_color = ui_freshness_color(d->fresh);
        if (!stale) {
            value_color = ui_glucose_color(p->mgdl);
            trend = p->trend;
        }
        const char *trend_text = stale ? "" : ui_trend_text(p->trend);
        snprintf(buf, sizeof(buf), "%s%s%s", ui_freshness_text(d->fresh), trend_text[0] ? "  ·  " : "", trend_text);
        set_text(s_status, s_status_text, sizeof(s_status_text), buf);

        char hm[16] = "--:--";
        if (d->time_ok) {
            ui_format_hm(p->wt_ms, hm, sizeof(hm));
        }
        char age[48];
        ui_format_age(d->age_s, age, sizeof(age));
        if (i18n_lang() == LANG_FR && age[0] == 'I') {
            age[0] = 'i'; /* English only: "Il y a 3 min" -> "il y a 3 min" */
        }
        snprintf(buf, sizeof(buf), "%s : %s  (%s)", tr(S_LAST_READING), hm, age);
        set_text(s_last, s_last_text, sizeof(s_last_text), buf);
    }
    set_text_color(s_value, &s_value_color, value_color);
    set_text_color(s_status, &s_status_color, d->fresh == GM_FRESH_OK ? NIGHT_COL_TEXT : fresh_color);
    if (!lv_color_eq(s_dot_color, fresh_color)) {
        s_dot_color = fresh_color;
        lv_obj_set_style_bg_color(s_dot, fresh_color, 0);
        lv_obj_set_style_shadow_color(s_dot, fresh_color, 0);
    }
    if (trend != s_arrow_trend || !lv_color_eq(s_arrow_color, value_color)) {
        s_arrow_trend = trend;
        s_arrow_color = value_color;
        trend_arrow_set(s_arrow, (uint8_t)trend, value_color);
        ui_glow_set(s_halo, value_color, LV_OPA_20);
    }
}

static void show_screen(bool visible, const ui_data_t *d)
{
    const bool hidden = lv_obj_has_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    if (visible) {
        refresh_screen(d);
        if (s_preview && lv_obj_has_flag(s_hint, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_remove_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        } else if (!s_preview && !lv_obj_has_flag(s_hint, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        }
        if (hidden) {
            lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_screen);
        }
    } else if (!hidden) {
        lv_obj_add_flag(s_screen, LV_OBJ_FLAG_HIDDEN);
    }
    s_screen_visible = visible;
}

/* ------------------------------------------------------------------------- */

/* Let an important alert take precedence over the quiet night view. */
static void evaluate(const ui_data_t *d, bool immediate)
{
    const int64_t now = esp_timer_get_time();
    settings_t cfg;
    settings_get(&cfg);
    night_settings_t night_cfg;
    settings_night_get(&night_cfg);

    bool woke = immediate;
    if (s_preview && now >= s_preview_until_us) {
        s_preview = false;
        woke = true;
        ESP_LOGI(TAG, "Apercu du mode nuit termine (60 s)");
    }
    const bool night = s_sim != SIM_AUTO || in_night_period(&night_cfg, d);
    if (night != s_night) {
        s_night = night;
        s_touch_until_us = 0;
        if (night) {
            ESP_LOGI(TAG, "Periode nuit : debut (%s) : ecran nuit, luminosite %u %%", k_sim_names[s_sim],
                     night_cfg.brightness);
        } else {
            ESP_LOGI(TAG, "Periode nuit : fin : interface normale, luminosite jour %u %%", cfg.brightness);
        }
    }

    /* High priority alert sign: luminosite day, back to the screen night 45 s after its disappearance */
    if (ui_alert_panel_visible() && wakes_screen(ui_alert_panel_style())) {
        if (s_preview) {
            s_preview = false;
            woke = true;
            ESP_LOGI(TAG, "Apercu du mode nuit interrompu par une alerte");
        }
        if (night) {
            if (now >= s_alert_until_us) {
                ESP_LOGI(TAG, "Alerte pendant la nuit : panneau prioritaire, luminosite jour %u %%", cfg.brightness);
            }
            s_alert_until_us = now + ALERT_AWAKE_US;
            woke = true;
        }
    }
    /* Normal interface visible at night (after touch or alert): prolongs 30 s after each interaction */
    if (night && (s_touch_until_us > now || s_alert_until_us > now)) {
        const int64_t last_touch_us = now - (int64_t)lv_display_get_inactive_time(NULL) * 1000;
        if (last_touch_us + TOUCH_AWAKE_US > s_touch_until_us) {
            s_touch_until_us = last_touch_us + TOUCH_AWAKE_US;
        }
    }

    int target = night_cfg.brightness;
    bool screen = false;
    const char *state;
    if (s_preview) {
        state = "apercu de l'ecran nuit";
        screen = true;
    } else if (!night) {
        target = cfg.brightness;
        state = "jour";
    } else if (now < s_alert_until_us) {
        target = cfg.brightness;
        state = "nuit, alerte";
    } else if (now < s_touch_until_us) {
        state = "nuit, interface normale apres toucher";
    } else {
        state = "nuit, ecran nuit";
        screen = true;
    }
    if (state != s_state) {
        ESP_LOGI(TAG, "Etat : %s (retroeclairage %d %%)", state, target);
        s_state = state;
    }
    show_screen(screen, d);
    go_to(target, woke && target > s_level);
}

static void start_preview(void)
{
    s_preview = true;
    s_preview_until_us = esp_timer_get_time() + PREVIEW_US;
    s_touch_until_us = 0;
    ESP_LOGI(TAG, "Apercu du mode nuit : 60 s au plus, un toucher le termine (horaires inchanges)");
}

/* Touching the night screen: this touch does not trigger anything under the finger */
static void on_screen_pressed(lv_event_t *e)
{
    (void)e;
    lv_indev_wait_release(lv_indev_active());
    const bool preview = s_preview;
    s_preview = false;
    if (s_night) {
        s_touch_until_us = esp_timer_get_time() + TOUCH_AWAKE_US;
        ESP_LOGI(TAG, "Toucher sur l'ecran nuit : interface normale 30 s apres la derniere interaction");
    } else if (preview) {
        ESP_LOGI(TAG, "Apercu du mode nuit quitte par un toucher");
    }
    evaluate(&g_ui, true);
}

static void create_screen(void)
{
    s_screen = plain_obj(lv_screen_active());
    lv_obj_set_size(s_screen, UI_SCR_W, UI_SCR_H);
    lv_obj_set_style_bg_color(s_screen, UI_COL_BG, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_screen, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_screen, on_screen_pressed, LV_EVENT_PRESSED, NULL);
    touch_diag_name(s_screen, "ecran nuit");

    /* Order of importance: glycemia, fleche, hour, status, last measure */
    s_time = ui_label(s_screen, ui_font_create(true, 96), NIGHT_COL_TIME, "--:--");
    lv_obj_align(s_time, LV_ALIGN_TOP_MID, 0, 2);

    s_halo = ui_glow(s_screen, 300, 120, UI_COL_DIM, LV_OPA_20, 80); /* noyau arrondi, flou moyen : lueur diffuse */
    lv_obj_align(s_halo, LV_ALIGN_CENTER, -115, -6);

    lv_obj_t *row = plain_obj(s_screen);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 40, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -6);
    s_value = ui_label(row, ui_font_create(true, 260), UI_COL_DIM, "---");
    s_arrow = trend_arrow_create(row, 190, g_fonts.xl);
    trend_arrow_set_glow(s_arrow, LV_OPA_40); /* halo fixed, no animation at night */

    lv_obj_t *status_row = plain_obj(s_screen);
    lv_obj_set_size(status_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_row, 14, 0);
    lv_obj_align(status_row, LV_ALIGN_BOTTOM_MID, 0, -92);
    s_dot = plain_obj(status_row);
    lv_obj_set_size(s_dot, 22, 22);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_dot, UI_COL_DIM, 0);
    lv_obj_set_style_shadow_width(s_dot, 14, 0);
    lv_obj_set_style_shadow_opa(s_dot, LV_OPA_40, 0);
    lv_obj_set_style_shadow_color(s_dot, UI_COL_DIM, 0);
    s_status = ui_label(status_row, g_fonts.title, NIGHT_COL_TEXT, "");

    s_last = ui_label(s_screen, g_fonts.l, NIGHT_COL_TEXT, "");
    lv_obj_align(s_last, LV_ALIGN_BOTTOM_MID, 0, -26);

    s_hint = ui_label(s_screen, g_fonts.s, NIGHT_COL_HINT, tr(S_NIGHT_PREVIEW));
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_hint, LV_ALIGN_TOP_RIGHT, -18, 14);
    lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);

    s_value_color = UI_COL_DIM;
    s_status_color = NIGHT_COL_TEXT;
    s_dot_color = UI_COL_DIM;
}

/* Language change: the texts of the night screen are posed by set_text(), which compares with
 * A copy in cache. Clearing these caches is enough: the next refreshment rewrites them in the
 * New language. The night screen itself is never destroyed. */
void night_mode_relang(void)
{
    s_time_text[0] = '\0';
    s_value_text[0] = '\0';
    s_status_text[0] = '\0';
    s_last_text[0] = '\0';
    if (s_hint != NULL) {
        lv_label_set_text(s_hint, tr(S_NIGHT_PREVIEW));
    }
}

void night_mode_init(void)
{
    create_screen();
    s_fade = lv_timer_create(fade_cb, FADE_PERIOD_MS, NULL);
    lv_timer_pause(s_fade);

    night_settings_t n;
    settings_night_get(&n);
    ESP_LOGI(TAG, "Mode nuit %s : %02u:%02u-%02u:%02u, ecran nuit minimaliste, luminosite nuit %u %%",
             n.enabled ? "actif" : "inactif", n.start_min / 60, n.start_min % 60, n.end_min / 60, n.end_min % 60,
             n.brightness);
}

void night_mode_update(const ui_data_t *d)
{
    const night_request_t req = s_request;
    s_request = NIGHT_REQ_NONE;
    bool immediate = false;
    switch (req) {
    case NIGHT_REQ_ON:
    case NIGHT_REQ_OFF:
        s_sim = req == NIGHT_REQ_ON ? SIM_NIGHT : SIM_AUTO;
        ESP_LOGI(TAG, "TEST : %s", k_sim_names[s_sim]);
        break;
    case NIGHT_REQ_WAKE:
        if (s_night) {
            s_touch_until_us = esp_timer_get_time() + TOUCH_AWAKE_US;
            ESP_LOGI(TAG, "TEST : reveil comme par un toucher");
            immediate = true;
        }
        break;
    case NIGHT_REQ_TEST:
        start_preview();
        break;
    default:
        break;
    }
    evaluate(d, immediate);
}

void night_mode_brightness_changed(void)
{
    evaluate(&g_ui, true);
}

void night_mode_preview(void)
{
    start_preview();
    evaluate(&g_ui, false);
}

bool night_mode_screen_visible(void)
{
    return s_screen_visible;
}

void night_mode_request(night_request_t req)
{
    s_request = req;
}

void night_mode_print_info(void)
{
    night_settings_t n;
    settings_night_get(&n);
    settings_t s;
    settings_get(&s);
    const int64_t now = esp_timer_get_time();
    const int64_t touch_s = s_touch_until_us > now ? (s_touch_until_us - now) / 1000000 : 0;
    const int64_t alert_s = s_alert_until_us > now ? (s_alert_until_us - now) / 1000000 : 0;
    const int64_t preview_s = s_preview && s_preview_until_us > now ? (s_preview_until_us - now) / 1000000 : 0;
    printf("NIGHT_INFO etat=\"%s\" test=\"%s\" ecran_nuit=%s apercu_s=%lld retroeclairage=%d%% cible=%d%% "
           "interface_toucher_s=%lld alerte_s=%lld\n", s_state, k_sim_names[s_sim], s_screen_visible ? "affiche" : "masque",
           (long long)preview_s, s_level, s_target, (long long)touch_s, (long long)alert_s);
    printf("NIGHT_INFO reglages mode_nuit=%d debut=%02u:%02u fin=%02u:%02u jour=%u%% nuit=%u%%\n", n.enabled,
           n.start_min / 60, n.start_min % 60, n.end_min / 60, n.end_min % 60, s.brightness, n.brightness);
}
