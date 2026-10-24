/*
 * ui_graph.c: Layout and touch interactions of the glucose graph.
 * Design note: Keep display calculations separate from data acquisition.
 */

/*
 * Owns the graph page controls and coordinates range selection with the chart widget.
 * Pilot the graphic page and selected durations.
 */
#include "i18n.h"
#include "ui_pages.h"

#include <stdio.h>
#include "esp_log.h"
#include "touch_diag.h"
#include "ui_chart.h"
#include "ui_main.h"

static const char *TAG = "ui_graph";

static const char *const k_range_map[] = {"3H", "6H", "12H", "24H", ""};
static const int k_range_hours[] = {3, 6, 12, 24};

static lv_obj_t *s_ranges;
static lv_obj_t *s_last;
static ui_chart_t s_chart;
static int s_drawn_hours;

static int range_index(int hours)
{
    for (int i = 0; i < 4; i++) {
        if (k_range_hours[i] == hours) {
            return i;
        }
    }
    return 1;
}

/* Change the plotted time window without editing glucose history. */
static void on_range_changed(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target_obj(e);
    uint32_t idx = lv_buttonmatrix_get_selected_button(btnm);
    if (idx < 4) {
        ESP_LOGI(TAG, "Bouton %s touche", k_range_map[idx]);
        ui_set_chart_hours(k_range_hours[idx]);
    }
}

void ui_graph_create(lv_obj_t *page)
{
    /* Purees: pills in a glass band */
    s_ranges = lv_buttonmatrix_create(page);
    touch_diag_name(s_ranges, "durees graphique");
    lv_buttonmatrix_set_map(s_ranges, k_range_map);
    lv_buttonmatrix_set_button_ctrl_all(s_ranges, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(s_ranges, true);
    lv_obj_set_size(s_ranges, 460, 62);
    lv_obj_set_pos(s_ranges, 20, 10);
    ui_pill_buttons_style(s_ranges, g_fonts.nav, true);
    lv_buttonmatrix_set_button_ctrl(s_ranges, (uint32_t)range_index(ui_chart_hours()), LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_add_event_cb(s_ranges, on_range_changed, LV_EVENT_VALUE_CHANGED, NULL);

    s_last = ui_label(page, g_fonts.m, UI_G_TEXT, "");
    lv_obj_set_size(s_last, 300, 72);
    lv_obj_set_style_text_align(s_last, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_last, UI_SCR_W - 300 - 24, 6);

    ui_chart_create(&s_chart, page, 84, 104, 904, 316, true);
    ui_chart_enable_explore(&s_chart); /* appui ou glissement : mesure reelle la plus proche */

    lv_obj_t *legend = ui_label(page, g_fonts.xs, UI_G_TEXT3,
                                tr(S_GRAPH_HINT));
    lv_obj_set_pos(legend, 28, 468);
}

void ui_graph_print_explore(void)
{
    ui_chart_explore_print(&s_chart);
}

/* Pass current data to the chart and keep the exploration state coherent. */
void ui_graph_update(const ui_data_t *d, bool force)
{
    const int hours = ui_chart_hours();
    const int idx = range_index(hours);
    if (!lv_buttonmatrix_has_button_ctrl(s_ranges, (uint32_t)idx, LV_BUTTONMATRIX_CTRL_CHECKED)) {
        lv_buttonmatrix_set_button_ctrl(s_ranges, (uint32_t)idx, LV_BUTTONMATRIX_CTRL_CHECKED);
    }

    if (d->count > 0) {
        char hm[8];
        ui_format_hm(d->hist[0].wt_ms, hm, sizeof(hm));
        const bool stale = d->fresh == GM_FRESH_STALE || d->fresh == GM_FRESH_NONE;
        lv_label_set_text_fmt(s_last, "%d mg/dL\n%s", d->hist[0].mgdl, d->time_ok ? hm : "--:--");
        lv_obj_set_style_text_color(s_last, stale ? UI_G_STALE : ui_glucose_color(d->hist[0].mgdl), 0);
    } else {
        lv_label_set_text(s_last, tr(S_NO_MEASURE));
        lv_obj_set_style_text_color(s_last, UI_G_STALE, 0);
    }

    if (force || d->history_changed || d->minute_changed || hours != s_drawn_hours) {
        if (force || hours != s_drawn_hours) {
            ui_chart_explore_reset(&s_chart); /* arrivee sur la page ou autre duree : curseur efface */
        }
        ui_chart_update(&s_chart, d, hours);
        s_drawn_hours = hours;
    }
    ui_chart_explore_tick(&s_chart); /* cursor clears a few seconds after release */
}
