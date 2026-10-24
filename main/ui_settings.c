/*
 * ui_settings.c: Settings controls and connection confirmation.
 * Design note: Never announce successful credential removal after an NVS failure.
 */

/* Touchscreen settings, language selection and credential management.
 * Never print passwords or report a failed erase as successful. */
#include "i18n.h"
#include "ui_main.h"
#include "ui_pages.h"

#include <inttypes.h>
#include <stdio.h>
#include "audio_out.h"
#include "bsp/display.h"
#include "consumables.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "night_mode.h"
#include "sd_storage.h"
#ifndef GLY_VOICE_CMD
#define GLY_VOICE_CMD 0
#endif

#include "credentials.h"
#include "glucose_service.h"
#include "esp_system.h"
#include "settings.h"
#include "touch_diag.h"
#include "voice.h"
#include "voice_cmd.h"

static const char *TAG = "ui_reglages";

typedef enum {
    THR_VERY_LOW = 0,
    THR_LOW,
    THR_HIGH,
    THR_VERY_HIGH,
    THR_COUNT,
} thr_id_t;

/* Keep translated volume labels in static storage; LVGL retains pointers. */
static const char *k_volume_map[6];
static const str_id_t k_volume_ids[5] = {S_MUTE, S_LOW, S_MEDIUM, S_HIGH, S_MAX};

static void fill_volume_map(void)
{
    for (int i = 0; i < 5; i++) {
        k_volume_map[i] = tr(k_volume_ids[i]);
    }
    k_volume_map[5] = "";
}

static lv_obj_t *s_volume_btns;
static lv_obj_t *s_volume_value;
static lv_obj_t *s_volume_slider;
static lv_obj_t *s_voice_sw;
static lv_obj_t *s_alerts_sw;
static lv_obj_t *s_ramp_sw;
static lv_obj_t *s_vlow_sw;
static lv_obj_t *s_thr_value[THR_COUNT];
static lv_obj_t *s_brightness;
static lv_obj_t *s_brightness_value;
static lv_obj_t *s_night_sw;
static lv_obj_t *s_night_start;
static lv_obj_t *s_night_end;
static lv_obj_t *s_night_brightness;
static lv_obj_t *s_night_brightness_value;
static lv_obj_t *s_info;
static lv_obj_t *s_interval_btns;
static lv_obj_t *s_lang_btns;
static lv_obj_t *s_vcmd_sw;
static lv_obj_t *s_wake_sw;
static lv_obj_t *s_vcmd_status;
static lv_obj_t *s_conn_wifi;
static lv_obj_t *s_conn_dexcom;
static lv_obj_t *s_conn_aide;
static lv_obj_t *s_demo_sw;
static lv_obj_t *s_effacer_modal;
static const char *s_vcmd_status_shown;
static uint32_t s_revision_drawn = UINT32_MAX;
static uint32_t s_conso_revision_drawn = UINT32_MAX;
static bool s_updating;

static const char *const k_interval_map[] = {"48 H", "72 H", ""};

/* Entries follow lang_t; the newline separates two button rows. */
static const char *const k_lang_map[] = {"Français", "English", "Español", "\n",
                                         "Deutsch", "Italiano", "Português", ""};

/* Queue language changes for the LVGL task to avoid destroying a widget
 * from within its own event callback. */
/* Reuse the first-run configuration wizard from Settings. */
static void on_connexion_modifier(lv_event_t *e)
{
    (void)e;
    ui_setup_open(false);
}

/* Switch between demo and live modes explicitly so synthetic data stays labelled. */
static void on_demo_switch(lv_event_t *e)
{
    const bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    credentials_set_demo(on);
    glucose_service_set_demo(on ? "normal" : "off");
}

static void fermer_modal(void)
{
    if (s_effacer_modal != NULL) {
        lv_obj_delete(s_effacer_modal);
        s_effacer_modal = NULL;
    }
}

static void on_effacer_annuler(lv_event_t *e)
{
    (void)e;
    fermer_modal();
}

/* Confirm that NVS accepted an empty account before restarting the device. */
/* Only restart the device when credential removal has actually succeeded.
 */
static void on_effacer_confirmer(lv_event_t *e)
{
    (void)e;
    const esp_err_t err = credentials_clear();
    fermer_modal();
    if (err != ESP_OK) {
        /* A failed NVS write must not cause a restart or a success message. */
        ESP_LOGE(TAG, "Effacement des acces impossible : %s", esp_err_to_name(err));
        lv_label_set_text(s_conn_aide, tr(S_SETUP_FAIL_OTHER));
        return;
    }
    esp_restart(); /* the empty configuration is saved: open the wizard */
}

/* Require a second explicit confirmation before erasing credentials. */
static void on_effacer(lv_event_t *e)
{
    (void)e;
    if (s_effacer_modal != NULL) {
        return;
    }
    s_effacer_modal = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_effacer_modal);
    lv_obj_set_size(s_effacer_modal, UI_SCR_W, UI_SCR_H);
    lv_obj_set_pos(s_effacer_modal, 0, 0);
    lv_obj_set_style_bg_opa(s_effacer_modal, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(s_effacer_modal, lv_color_black(), 0);
    lv_obj_add_flag(s_effacer_modal, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *carte = ui_card(s_effacer_modal, 212, 150, 600, 300, 28);
    lv_obj_t *titre = ui_label(carte, g_fonts.title, UI_COL_TEXT, tr(S_CONN_ERASE_ASK));
    lv_obj_set_pos(titre, 40, 36);
    lv_obj_t *aide = ui_label(carte, g_fonts.s, UI_G_TEXT2, tr(S_CONN_ERASE_HELP));
    lv_obj_set_width(aide, 520);
    lv_label_set_long_mode(aide, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(aide, 40, 96);
    lv_obj_t *non = ui_button(carte, tr(S_CANCEL), g_fonts.m, 240, 72, on_effacer_annuler, NULL);
    lv_obj_set_pos(non, 40, 200);
    lv_obj_set_style_bg_color(non, UI_G_GLASS_TOP, 0);
    lv_obj_t *oui = ui_button(carte, tr(S_CONN_ERASE), g_fonts.m, 240, 72, on_effacer_confirmer, NULL);
    lv_obj_set_pos(oui, 320, 200);
    lv_obj_set_style_bg_color(oui, UI_G_CRIT, 0);
}

/* Schedule translation changes without destroying the pressed LVGL button in its own callback. */
static void on_language(lv_event_t *e)
{
    if (s_updating) {
        return;
    }
    const uint32_t index = lv_buttonmatrix_get_selected_button(lv_event_get_target(e));
    if (index < (uint32_t)LANG_COUNT) {
        ui_request_lang((int)index);
    }
}

static lv_obj_t *section_title(lv_obj_t *page, int32_t y, const char *text)
{
    lv_obj_t *label = ui_label(page, g_fonts.title, UI_COL_BLUE, text);
    lv_obj_set_pos(label, 24, y);
    return label;
}

static lv_obj_t *switch_row(lv_obj_t *page, int32_t y, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *label = ui_label(page, g_fonts.m, UI_COL_TEXT, text);
    lv_obj_set_pos(label, 24, y + 8);
    lv_obj_t *sw = lv_switch_create(page);
    lv_obj_set_size(sw, 96, 48);
    lv_obj_set_pos(sw, UI_SCR_W - 24 - 96 - 10, y);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return sw;
}

static void apply(settings_t *s)
{
    if (!s_updating) {
        settings_set(s);
    }
}

static void show_volume(int pct)
{
    if (pct == 0) {
        lv_label_set_text(s_volume_value, tr(S_VOLUME_MUTED));
    } else {
        lv_label_set_text_fmt(s_volume_value, tr(S_VOLUME_PCT), pct);
    }
}

static void on_volume_level(lv_event_t *e)
{
    const uint32_t idx = lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(e));
    if (idx > VOLUME_MAX) {
        return;
    }
    settings_t s;
    settings_get(&s);
    s.volume_pct = (uint8_t)settings_level_volume((volume_level_t)idx);
    ESP_LOGI(TAG, "Volume %s (%u %%, codec %.1f dB)", k_volume_map[idx], s.volume_pct,
             audio_out_volume_db(s.volume_pct));
    apply(&s);
}

/* Preview the volume while dragging; apply and persist on release. */
static void on_volume_slider(lv_event_t *e)
{
    const int pct = lv_slider_get_value(lv_event_get_target_obj(e));
    show_volume(pct);
    if (lv_event_get_code(e) != LV_EVENT_RELEASED) {
        return;
    }
    settings_t s;
    settings_get(&s);
    s.volume_pct = (uint8_t)pct;
    ESP_LOGI(TAG, "Curseur volume : %d %% (codec %.1f dB)", pct, audio_out_volume_db(pct));
    apply(&s);
}

static void on_test_sound(lv_event_t *e)
{
    (void)e;
    settings_t s;
    settings_get(&s);
    ESP_LOGI(TAG, "Bouton TEST SON touche (volume %u %%)", s.volume_pct);
    audio_out_play_pattern(AUDIO_PATTERN_TEST, s.volume_pct);
}

static void on_test_voice(lv_event_t *e)
{
    (void)e;
    settings_t s;
    settings_get(&s);
    ESP_LOGI(TAG, "Bouton TEST VOIX touche (volume %u %%)", s.volume_pct);
    voice_test(120, s.volume_pct); /* "Glycemie cent vingt." (microSD), test beep if bank is absent */
}

static void on_switch(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    const bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    settings_t s;
    settings_get(&s);
    if (sw == s_voice_sw) {
        s.voice = on;
    } else if (sw == s_alerts_sw) {
        s.alerts = on;
    } else if (sw == s_ramp_sw) {
        s.ramp = on;
    } else if (sw == s_vlow_sw) {
        s.very_low_always = on;
    }
    apply(&s);
}

static void show_thresholds(const settings_t *s)
{
    const uint16_t values[THR_COUNT] = {s->thr_very_low, s->thr_low, s->thr_high, s->thr_very_high};
    for (int i = 0; i < THR_COUNT; i++) {
        lv_label_set_text_fmt(s_thr_value[i], "%u", values[i]);
    }
}

/* Update the displayed threshold immediately and defer NVS writes to
 * the settings worker rather than blocking the LVGL callback. */
static void on_threshold(lv_event_t *e)
{
    const intptr_t code = (intptr_t)lv_event_get_user_data(e);
    const int id = (int)(code / 10);
    const bool plus = (code % 10) == 1;
    settings_t s;
    settings_get(&s);
    uint16_t *const values[THR_COUNT] = {&s.thr_very_low, &s.thr_low, &s.thr_high, &s.thr_very_high};
    *values[id] = (uint16_t)(*values[id] + (plus ? 5 : -5));
    apply(&s); /* memory + notification of the backup spot, without waiting */
    settings_get(&s); /* Reload bounds-checked values from shared settings. */
    show_thresholds(&s);
}

/* Preview daytime brightness while dragging and persist it on release;
 * night mode continues to apply its own configured brightness. */
static void on_brightness(lv_event_t *e)
{
    const int pct = lv_slider_get_value(lv_event_get_target_obj(e));
    lv_label_set_text_fmt(s_brightness_value, "%d %%", pct);
    if (lv_event_get_code(e) != LV_EVENT_RELEASED) {
        return;
    }
    settings_t s;
    settings_get(&s);
    s.brightness = (uint8_t)pct;
    apply(&s);
    night_mode_brightness_changed();
}

static void apply_night(const night_settings_t *n)
{
    if (!s_updating) {
        settings_night_set(n);
    }
}

static void show_night_times(const night_settings_t *n)
{
    lv_label_set_text_fmt(s_night_start, "%02u:%02u", n->start_min / 60, n->start_min % 60);
    lv_label_set_text_fmt(s_night_end, "%02u:%02u", n->end_min / 60, n->end_min % 60);
}

static void on_night_switch(lv_event_t *e)
{
    night_settings_t n;
    settings_night_get(&n);
    n.enabled = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    apply_night(&n);
}

/* Preview night mode briefly without changing the saved schedule. */
static void on_night_test(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Bouton TEST MODE NUIT touche");
    night_mode_preview();
}

/* Adjust scheduled start and end times in 15-minute increments. */
static void on_night_time(lv_event_t *e)
{
    const intptr_t code = (intptr_t)lv_event_get_user_data(e);
    night_settings_t n;
    settings_night_get(&n);
    uint16_t *const value = code < 2 ? &n.start_min : &n.end_min;
    *value = (uint16_t)((*value + ((code & 1) ? 15 : 24 * 60 - 15)) % (24 * 60));
    apply_night(&n);
    show_night_times(&n);
}

static void on_night_brightness(lv_event_t *e)
{
    const int pct = lv_slider_get_value(lv_event_get_target_obj(e));
    lv_label_set_text_fmt(s_night_brightness_value, "%d %%", pct);
    if (lv_event_get_code(e) != LV_EVENT_RELEASED) {
        return;
    }
    night_settings_t n;
    settings_night_get(&n);
    n.brightness = (uint8_t)pct;
    apply_night(&n);
}

/* Configurable infusion-set reminders are not clinical recommendations. */
static void on_interval(lv_event_t *e)
{
    if (s_updating) {
        return;
    }
    const uint32_t idx = lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(e));
    if (idx <= 1) {
        ESP_LOGI(TAG, "Intervalle du catheter : %s", k_interval_map[idx]);
        catheter_set_interval_h(idx == 0 ? 48 : 72);
    }
}

static void on_vcmd_switch(lv_event_t *e)
{
    if (s_updating) {
        return;
    }
    lv_obj_t *sw = lv_event_get_target_obj(e);
    voice_cmd_settings_t v;
    settings_vcmd_get(&v);
    if (sw == s_vcmd_sw) {
        v.enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        ESP_LOGI(TAG, "Commandes vocales : %s", v.enabled ? "activees" : "desactivees");
    } else {
        v.wake_word = lv_obj_has_state(sw, LV_STATE_CHECKED);
        ESP_LOGI(TAG, "Mot d'eveil \"Bonjour ESP\" : %s", v.wake_word ? "requis" : "non requis");
    }
    settings_vcmd_set(&v);
    voice_cmd_apply_settings();
}

static lv_obj_t *brightness_row(lv_obj_t *page, int32_t y, const char *text, int32_t min, lv_event_cb_t cb,
                                lv_obj_t **value_label)
{
    lv_obj_t *label = ui_label(page, g_fonts.m, UI_COL_TEXT, text);
    lv_obj_set_pos(label, 24, y);
    lv_obj_t *slider = lv_slider_create(page);
    lv_slider_set_range(slider, min, 100);
    lv_obj_set_pos(slider, 320, y + 6);
    lv_obj_set_size(slider, 560, 28);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_RELEASED, NULL);
    *value_label = ui_label(page, g_fonts.m, UI_COL_TEXT, "");
    lv_obj_set_width(*value_label, 100);
    lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(*value_label, UI_SCR_W - 24 - 100, y);
    return slider;
}

static lv_obj_t *time_row(lv_obj_t *page, int col, int32_t y, const char *text, int code, const char *name)
{
    static char minus_names[2][24];
    static char plus_names[2][24];
    const int32_t x = 24 + col * 500;
    lv_obj_t *label = ui_label(page, g_fonts.m, UI_COL_TEXT, text);
    lv_obj_set_pos(label, x, y + 16);
    lv_obj_t *minus = ui_button(page, "-", g_fonts.title, 70, 64, on_night_time, (void *)(intptr_t)code);
    lv_obj_set_pos(minus, x + 200, y);
    lv_obj_t *value = ui_label(page, g_fonts.title, UI_COL_TEXT, "--:--");
    lv_obj_set_width(value, 120);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(value, x + 274, y + 12);
    lv_obj_t *plus = ui_button(page, "+", g_fonts.title, 70, 64, on_night_time, (void *)(intptr_t)(code + 1));
    lv_obj_set_pos(plus, x + 398, y);
    snprintf(minus_names[col], sizeof(minus_names[col]), "- %s", name);
    snprintf(plus_names[col], sizeof(plus_names[col]), "+ %s", name);
    touch_diag_name(minus, minus_names[col]);
    touch_diag_name(plus, plus_names[col]);
    return value;
}

static void threshold_row(lv_obj_t *page, int col, int row, thr_id_t id, const char *text)
{
    static const char *const k_minus_names[THR_COUNT] = {"- TRES BASSE", "- BASSE", "- ELEVEE", "- TRES ELEVEE"};
    static const char *const k_plus_names[THR_COUNT] = {"+ TRES BASSE", "+ BASSE", "+ ELEVEE", "+ TRES ELEVEE"};
    const int32_t x = 24 + col * 500;
    const int32_t y = 574 + row * 78;
    lv_obj_t *label = ui_label(page, g_fonts.m, UI_COL_TEXT, text);
    lv_obj_set_pos(label, x, y + 16);
    lv_obj_t *minus = ui_button(page, "-", g_fonts.title, 70, 64, on_threshold, (void *)(intptr_t)(id * 10 + 0));
    lv_obj_set_pos(minus, x + 240, y);
    s_thr_value[id] = ui_label(page, g_fonts.title, UI_COL_TEXT, "--");
    lv_obj_set_width(s_thr_value[id], 90);
    lv_obj_set_style_text_align(s_thr_value[id], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_thr_value[id], x + 314, y + 12);
    lv_obj_t *plus = ui_button(page, "+", g_fonts.title, 70, 64, on_threshold, (void *)(intptr_t)(id * 10 + 1));
    lv_obj_set_pos(plus, x + 408, y);
    touch_diag_name(minus, k_minus_names[id]);
    touch_diag_name(plus, k_plus_names[id]);
}

void ui_settings_create(lv_obj_t *page)
{
    /* Make the entire settings page scrollable while keeping sliders,
 * buttons and switches directly interactive. The navigation bar is separate. */
    lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(page, 40, 0);

    section_title(page, 12, tr(S_VOLUME_TITLE));
    s_volume_value = ui_label(page, g_fonts.title, UI_COL_TEXT, "");
    lv_obj_set_pos(s_volume_value, 440, 12);

    /* Enlarge the slider touch target without changing its range. */
    s_volume_slider = lv_slider_create(page);
    lv_slider_set_range(s_volume_slider, 0, 100);
    lv_obj_set_pos(s_volume_slider, 48, 80);
    lv_obj_set_size(s_volume_slider, 928, 32);
    lv_obj_set_ext_click_area(s_volume_slider, 24);
    lv_obj_set_style_bg_color(s_volume_slider, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_volume_slider, UI_COL_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_volume_slider, UI_COL_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_volume_slider, 12, LV_PART_KNOB);
    lv_obj_add_event_cb(s_volume_slider, on_volume_slider, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_volume_slider, on_volume_slider, LV_EVENT_RELEASED, NULL);

    s_volume_btns = lv_buttonmatrix_create(page);
    fill_volume_map();
    lv_buttonmatrix_set_map(s_volume_btns, k_volume_map);
    lv_buttonmatrix_set_button_ctrl_all(s_volume_btns, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_volume_btns, true);
    lv_obj_set_pos(s_volume_btns, 16, 136);
    lv_obj_set_size(s_volume_btns, 668, 76);
    ui_pill_buttons_style(s_volume_btns, g_fonts.nav, true);
    lv_obj_set_style_pad_gap(s_volume_btns, 10, 0);
    lv_obj_add_event_cb(s_volume_btns, on_volume_level, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *test_sound = ui_button(page, tr(S_TEST_SOUND), g_fonts.nav, 152, 64, on_test_sound, NULL);
    lv_obj_set_pos(test_sound, 698, 142);
    lv_obj_t *test_voice = ui_button(page, tr(S_TEST_VOICE), g_fonts.nav, 152, 64, on_test_voice, NULL);
    lv_obj_set_pos(test_voice, 860, 142);

    touch_diag_name(s_volume_slider, "curseur volume");
    touch_diag_name(s_volume_btns, "niveaux volume");
    touch_diag_name(test_sound, "TEST SON");
    touch_diag_name(test_voice, "TEST VOIX");

    s_voice_sw = switch_row(page, 230, tr(S_VOICE_ANNOUNCE), on_switch);

    section_title(page, 300, tr(S_ALERTS_TITLE));
    s_alerts_sw = switch_row(page, 350, tr(S_ALERTS_SOUND), on_switch);
    s_ramp_sw = switch_row(page, 412, tr(S_RAMP), on_switch);
    s_vlow_sw = switch_row(page, 474, tr(S_VERYLOW_IN_MUTE), on_switch);

    lv_obj_t *thr_title = ui_label(page, g_fonts.m, UI_COL_DIM, tr(S_THRESHOLDS));
    lv_obj_set_pos(thr_title, 24, 540);
    threshold_row(page, 0, 0, THR_VERY_LOW, tr(S_THR_VERY_LOW));
    threshold_row(page, 0, 1, THR_LOW, tr(S_THR_LOW));
    threshold_row(page, 1, 0, THR_HIGH, tr(S_THR_HIGH));
    threshold_row(page, 1, 1, THR_VERY_HIGH, tr(S_THR_VERY_HIGH));

    section_title(page, 752, tr(S_SCREEN_TITLE));
    s_brightness = brightness_row(page, 806, tr(S_DAY_BRIGHTNESS), 10, on_brightness, &s_brightness_value);
    touch_diag_name(s_brightness, "curseur luminosite");
    touch_diag_name(s_voice_sw, "interrupteur annonces vocales");
    touch_diag_name(s_alerts_sw, "interrupteur alertes sonores");
    touch_diag_name(s_ramp_sw, "interrupteur volume croissant");
    touch_diag_name(s_vlow_sw, "interrupteur tres basse en MUTE");

    section_title(page, 870, tr(S_NIGHT_TITLE));
    s_night_sw = switch_row(page, 920, tr(S_NIGHT_ENABLE), on_night_switch);
    s_night_start = time_row(page, 0, 990, tr(S_NIGHT_START), 0, "DEBUT NUIT");
    s_night_end = time_row(page, 1, 990, tr(S_NIGHT_END), 2, "FIN NUIT");
    s_night_brightness = brightness_row(page, 1080, tr(S_NIGHT_BRIGHTNESS), 1, on_night_brightness,
                                        &s_night_brightness_value);
    lv_obj_t *night_test = ui_button(page, tr(S_NIGHT_TEST), g_fonts.nav, 300, 64, on_night_test, NULL);
    lv_obj_set_pos(night_test, 24, 1136);
    lv_obj_t *night_hint = ui_label(page, g_fonts.s, UI_COL_DIM, tr(S_NIGHT_HELP));
    lv_obj_set_width(night_hint, 976);
    lv_obj_set_pos(night_hint, 24, 1216);
    touch_diag_name(s_night_sw, "interrupteur mode nuit");
    touch_diag_name(night_test, "TEST MODE NUIT");
    touch_diag_name(s_night_brightness, "curseur luminosite nuit");

    section_title(page, 1300, tr(S_CONSUM_TITLE));
    lv_obj_t *interval_label = ui_label(page, g_fonts.m, UI_COL_TEXT, tr(S_CATHETER_REMINDER));
    lv_obj_set_pos(interval_label, 24, 1366);
    s_interval_btns = lv_buttonmatrix_create(page);
    lv_buttonmatrix_set_map(s_interval_btns, k_interval_map);
    lv_buttonmatrix_set_button_ctrl_all(s_interval_btns, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_interval_btns, true);
    lv_obj_set_pos(s_interval_btns, UI_SCR_W - 24 - 300, 1350);
    lv_obj_set_size(s_interval_btns, 300, 72);
    ui_pill_buttons_style(s_interval_btns, g_fonts.nav, true);
    lv_obj_set_style_pad_gap(s_interval_btns, 10, 0);
    lv_obj_add_event_cb(s_interval_btns, on_interval, LV_EVENT_VALUE_CHANGED, NULL);
    touch_diag_name(s_interval_btns, "intervalle catheter");
    lv_obj_t *interval_hint = ui_label(page, g_fonts.s, UI_COL_DIM, tr(S_CATHETER_HELP));
    lv_obj_set_width(interval_hint, 976);
    lv_obj_set_pos(interval_hint, 24, 1432);

    /* Connection settings: available even before Wi-Fi/Dexcom is configured. */
    section_title(page, 1510, tr(S_CONN_TITLE));
    s_conn_wifi = ui_label(page, g_fonts.m, UI_COL_TEXT, "");
    lv_obj_set_width(s_conn_wifi, 620);
    lv_label_set_long_mode(s_conn_wifi, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(s_conn_wifi, 24, 1566);
    s_conn_dexcom = ui_label(page, g_fonts.m, UI_COL_TEXT, "");
    lv_obj_set_width(s_conn_dexcom, 620);
    lv_label_set_long_mode(s_conn_dexcom, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(s_conn_dexcom, 24, 1610);
    lv_obj_t *btn_modif = ui_button(page, tr(S_CONN_MODIFY), g_fonts.m, 220, 72, on_connexion_modifier, NULL);
    lv_obj_set_pos(btn_modif, 664, 1566);
    touch_diag_name(btn_modif, "modifier les identifiants");
    lv_obj_t *btn_eff = ui_button(page, tr(S_CONN_ERASE), g_fonts.s, 92, 72, on_effacer, NULL);
    lv_obj_set_pos(btn_eff, 900, 1566);
    lv_obj_set_style_bg_color(btn_eff, UI_G_GLASS_TOP, 0);
    touch_diag_name(btn_eff, "effacer les identifiants");
    s_demo_sw = switch_row(page, 1664, tr(S_CONN_DEMO), on_demo_switch);
    touch_diag_name(s_demo_sw, "interrupteur mode demonstration");
    s_conn_aide = ui_label(page, g_fonts.s, UI_COL_DIM, tr(S_CONN_HELP));
    lv_obj_set_width(s_conn_aide, 976);
    lv_label_set_long_mode(s_conn_aide, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_conn_aide, 24, 1724);

    /* Six user-interface languages in two rows of three. */
    section_title(page, 1810, tr(S_LANG_TITLE));
    s_lang_btns = lv_buttonmatrix_create(page);
    lv_buttonmatrix_set_map(s_lang_btns, k_lang_map);
    lv_buttonmatrix_set_button_ctrl_all(s_lang_btns, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_lang_btns, true);
    lv_obj_set_pos(s_lang_btns, 16, 1866);
    lv_obj_set_size(s_lang_btns, 992, 156);
    ui_pill_buttons_style(s_lang_btns, g_fonts.nav, true);
    lv_obj_set_style_pad_gap(s_lang_btns, 10, 0);
    lv_obj_add_event_cb(s_lang_btns, on_language, LV_EVENT_VALUE_CHANGED, NULL);
    touch_diag_name(s_lang_btns, "choix de la langue");
    lv_obj_t *lang_hint = ui_label(page, g_fonts.s, UI_COL_DIM, tr(S_LANG_HELP));
    lv_obj_set_width(lang_hint, 976);
    lv_obj_set_pos(lang_hint, 24, 2034);

#if GLY_VOICE_CMD
    /* Optional voice commands are not included in the public firmware.
 * See docs/LIMITATIONS_EN.md for this build's capabilities. */
    section_title(page, 2400, tr(S_VOICECMD_TITLE));
    s_vcmd_sw = switch_row(page, 2456, tr(S_VOICECMD_ENABLE), on_vcmd_switch);
    s_wake_sw = switch_row(page, 2518, tr(S_WAKEWORD), on_vcmd_switch);
    touch_diag_name(s_vcmd_sw, "interrupteur commandes vocales");
    touch_diag_name(s_wake_sw, "interrupteur mot d'eveil");
    s_vcmd_status = ui_label(page, g_fonts.s, UI_COL_DIM, "");
    lv_obj_set_width(s_vcmd_status, 976);
    lv_obj_set_pos(s_vcmd_status, 24, 2586);
#endif

    section_title(page, 2110, tr(S_INFO_TITLE));
    s_info = ui_label(page, g_fonts.s, UI_COL_TEXT, "");
    lv_obj_set_width(s_info, 976);
    lv_obj_set_pos(s_info, 24, 2160);
}

/* Read settings and refresh controls without rewriting identical values to flash. */
void ui_settings_update(const ui_data_t *d, bool force)
{
    const uint32_t revision = settings_revision();
    const bool settings_changed = force || revision != s_revision_drawn;
    if (settings_changed) {
        s_revision_drawn = revision;
        settings_t s;
        settings_get(&s);
        s_updating = true;
        lv_buttonmatrix_set_button_ctrl(s_lang_btns, (uint32_t)i18n_lang(), LV_BUTTONMATRIX_CTRL_CHECKED);
        if (s.volume_level <= VOLUME_MAX) {
            lv_buttonmatrix_set_button_ctrl(s_volume_btns, s.volume_level, LV_BUTTONMATRIX_CTRL_CHECKED);
        } else {
            lv_buttonmatrix_clear_button_ctrl_all(s_volume_btns, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
        show_volume(s.volume_pct);
        lv_slider_set_value(s_volume_slider, s.volume_pct, LV_ANIM_OFF);
        lv_obj_set_state(s_voice_sw, LV_STATE_CHECKED, s.voice);
        lv_obj_set_state(s_alerts_sw, LV_STATE_CHECKED, s.alerts);
        lv_obj_set_state(s_ramp_sw, LV_STATE_CHECKED, s.ramp);
        lv_obj_set_state(s_vlow_sw, LV_STATE_CHECKED, s.very_low_always);
        show_thresholds(&s);
        lv_slider_set_value(s_brightness, s.brightness, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_brightness_value, "%u %%", s.brightness);
        night_settings_t n;
        settings_night_get(&n);
        lv_obj_set_state(s_night_sw, LV_STATE_CHECKED, n.enabled);
        show_night_times(&n);
        lv_slider_set_value(s_night_brightness, n.brightness, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_night_brightness_value, "%u %%", n.brightness);
        if (s_vcmd_sw != NULL) {
            voice_cmd_settings_t v;
            settings_vcmd_get(&v);
            lv_obj_set_state(s_vcmd_sw, LV_STATE_CHECKED, v.enabled);
            lv_obj_set_state(s_wake_sw, LV_STATE_CHECKED, v.wake_word);
        }
        lv_obj_set_state(s_demo_sw, LV_STATE_CHECKED, credentials_demo());
        s_updating = false;
    }
    const uint32_t conso_revision = consumables_revision();
    if (force || conso_revision != s_conso_revision_drawn) {
        s_conso_revision_drawn = conso_revision;
        s_updating = true;
        lv_buttonmatrix_set_button_ctrl(s_interval_btns, catheter_interval_h() == 48 ? 0 : 1,
                                        LV_BUTTONMATRIX_CTRL_CHECKED);
        s_updating = false;
    }
    /* Account names are masked, never passwords. Keep this section accessible
 * even when the network or Dexcom authentication fails. */
    static bool essai_affiche;
    const bool essai = credentials_trial_active();
    if (force || settings_changed || essai != essai_affiche) {
        char wifi[CRED_SSID_MAX];
        char dex[CRED_USER_MAX + 8];
        credentials_masked_wifi(wifi, sizeof(wifi));
        credentials_masked_dexcom(dex, sizeof(dex));
        lv_label_set_text_fmt(s_conn_wifi, "%s : %s", tr(S_CONN_WIFI),
                              credentials_has_wifi() ? wifi : tr(S_CONN_NOT_SET));
        lv_label_set_text_fmt(s_conn_dexcom, "%s : %s", tr(S_CONN_DEXCOM),
                              credentials_has_dexcom() ? dex : tr(S_CONN_NOT_SET));
        lv_label_set_text(s_conn_aide, essai ? tr(S_CONN_TRIAL) : tr(S_CONN_HELP));
        essai_affiche = essai;
    }

    const char *vcmd_status = voice_cmd_status_text();
    if (s_vcmd_status != NULL && (settings_changed || vcmd_status != s_vcmd_status_shown)) {
        s_vcmd_status_shown = vcmd_status;
        voice_cmd_settings_t v;
        settings_vcmd_get(&v);
        lv_label_set_text_fmt(s_vcmd_status, "%s : %s. %s%s%s", tr(S_STATE), vcmd_status, tr(S_VOICECMD_HELP),
                              v.wake_word ? " " : "", v.wake_word ? tr(S_WAKEWORD_NOTE) : "");
    }

    if (!force && !d->minute_changed) {
        return;
    }
    const esp_app_desc_t *app = esp_app_get_description();
    char sd[96];
    switch (d->st.sd) {
    case SD_STATE_OK:
        snprintf(sd, sizeof(sd), tr(S_SD_OK_FMT), d->st.sd_type, (unsigned)d->st.sd_capacity_mb,
                 (unsigned)d->st.sd_free_mb, (unsigned)d->st.sd_rows_written);
        break;
    case SD_STATE_ABSENT:
        snprintf(sd, sizeof(sd), "%s", tr(S_SD_ABSENT));
        break;
    case SD_STATE_UNSUPPORTED:
        if (d->st.sd_capacity_mb > 0) {
            snprintf(sd, sizeof(sd), tr(S_SD_UNSUPPORTED_FMT), d->st.sd_type, (unsigned)d->st.sd_capacity_mb);
        } else {
            snprintf(sd, sizeof(sd), "%s", tr(S_SD_UNSUPPORTED));
        }
        break;
    case SD_STATE_ERROR:
        snprintf(sd, sizeof(sd), "%s", tr(S_SD_ERROR));
        break;
    default:
        snprintf(sd, sizeof(sd), "%s", tr(S_SD_INIT));
        break;
    }
    char voice[48];
    const int voice_files = sd_storage_voice_files();
    if (voice_files > 0) {
        snprintf(voice, sizeof(voice), tr(S_VOICE_FILES_FMT), voice_files);
    } else {
        snprintf(voice, sizeof(voice), "%s", tr(S_VOICE_ABSENT));
    }
    char info[512];
    int n = snprintf(info, sizeof(info), tr(S_INFO_VERSION), app->version, app->idf_ver);
    n += snprintf(info + n, sizeof(info) - n, tr(S_INFO_WIFI),
                  tr(d->st.wifi == LINK_OK ? S_CONNECTED : S_DISCONNECTED), d->st.ip[0] ? d->st.ip : "-",
                  d->st.rssi, d->st.coproc_fw);
    n += snprintf(info + n, sizeof(info) - n, tr(S_INFO_DEXCOM),
                  tr(d->st.dexcom == DEX_STATE_OK ? S_CONNECTED_EUROPE : S_NOT_CONNECTED));
    n += snprintf(info + n, sizeof(info) - n, tr(S_INFO_SD), sd);
    snprintf(info + n, sizeof(info) - n, tr(S_INFO_AUDIO), tr(d->st.audio_ok ? S_READY : S_UNAVAILABLE), voice);
    lv_label_set_text(s_info, info);
}
