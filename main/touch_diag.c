/*
 * touch_diag.c: Touch-screen diagnostics and calibration support.
 * Design note: Diagnostic labels aid testing but do not change sensor data.
 */

/*
 * Provides controlled touch diagnostics and synthetic tap injection for development.
 * Provides tactile diagnostics and injection of test supports.
 */
#include "touch_diag.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lv_adapter_input.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "tactile";

#define MAX_NAMES          64
#define GT911_CONFIG_REG   0x8047 /* version, puis X max (2 octets), Y max (2 octets) */
#define GESTURE_QUEUE_LEN  64
#define GESTURE_GAP_US     80000  /* minimal release between two synthetic supports */

typedef struct {
    lv_obj_t *obj;
    const char *name;
} name_entry_t;

typedef struct {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    int32_t ms;
} gesture_t;

static QueueHandle_t s_gestures;
static gesture_t s_gesture;
static int s_gesture_state;  /* 0 none, 1 support, 2 release */
static int64_t s_gesture_t0;
static uint32_t s_count_synthetic;
static int64_t s_last_read_us;
static uint32_t s_read_gap_max_ms;

static name_entry_t s_names[MAX_NAMES];
static int s_name_count;
static lv_indev_t *s_indev;
static int s_rotation;
static volatile bool s_log;
static volatile bool s_req_info;
static volatile int s_req_map = -1; /* bit 2 : swap_xy, bit 1 : mirror_x, bit 0 : mirror_y */

/* Tache LVGL uniquement */
static uint16_t s_raw_x;
static uint16_t s_raw_y;
static uint16_t s_raw_min_x = UINT16_MAX;
static uint16_t s_raw_max_x;
static uint16_t s_raw_min_y = UINT16_MAX;
static uint16_t s_raw_max_y;
static bool s_pressed;
static int64_t s_press_us;
static int32_t s_map_x;
static int32_t s_map_y;
static int32_t s_min_x;
static int32_t s_max_x;
static int32_t s_min_y;
static int32_t s_max_y;
static bool s_release_check;
static bool s_key_frames;
static uint32_t s_click_at_press;
static uint32_t s_count_press;
static uint32_t s_count_click;
static uint32_t s_count_no_click;
static uint32_t s_count_scroll;
static uint32_t s_count_read_err;
static uint32_t s_ui_last_ms;
static uint32_t s_ui_max_ms;

static const char *class_name(const lv_obj_t *obj)
{
    if (lv_obj_get_parent(obj) == NULL) {
        return "ecran";
    }
    if (lv_obj_check_type(obj, &lv_button_class)) {
        return "button";
    }
    if (lv_obj_check_type(obj, &lv_buttonmatrix_class)) {
        return "buttonmatrix";
    }
    if (lv_obj_check_type(obj, &lv_slider_class)) {
        return "slider";
    }
    if (lv_obj_check_type(obj, &lv_switch_class)) {
        return "switch";
    }
    if (lv_obj_check_type(obj, &lv_label_class)) {
        return "label";
    }
    return "obj";
}

static const char *lookup(const lv_obj_t *obj)
{
    for (int i = 0; i < s_name_count; i++) {
        if (s_names[i].obj == obj) {
            return s_names[i].name;
        }
    }
    return NULL;
}

/* "nom (classe)[bouton] < parent nomme" : the object touches then its first antre named */
static void describe(lv_obj_t *obj, bool with_button_text, char *buf, size_t size)
{
    size_t used = 0;
    buf[0] = '\0';
    for (int depth = 0; obj != NULL && depth < 6 && used < size; depth++) {
        const char *name = lookup(obj);
        if (depth == 0 || name != NULL) {
            used += (size_t)snprintf(buf + used, size - used, "%s%s (%s)", depth ? " < " : "", name ? name : "-",
                                     class_name(obj));
            if (used < size && with_button_text && depth == 0 && lv_obj_check_type(obj, &lv_buttonmatrix_class)) {
                const uint32_t id = lv_buttonmatrix_get_selected_button(obj);
                const char *text = id != LV_BUTTONMATRIX_BUTTON_NONE ? lv_buttonmatrix_get_button_text(obj, id) : NULL;
                used += (size_t)snprintf(buf + used, size - used, "[%s]", text ? text : "-");
            }
            if (depth > 0) {
                break;
            }
        }
        obj = lv_obj_get_parent(obj);
    }
}

/* Coordonnees brutes du GT911before miroir/inversion logiciels d'esp_lcd_touch */
static void raw_hook(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num,
                     uint8_t max_point_num)
{
    (void)tp;
    (void)strength;
    (void)max_point_num;
    if (*point_num == 0) {
        return;
    }
    s_raw_x = x[0];
    s_raw_y = y[0];
    s_raw_min_x = s_raw_x < s_raw_min_x ? s_raw_x : s_raw_min_x;
    s_raw_max_x = s_raw_x > s_raw_max_x ? s_raw_x : s_raw_max_x;
    s_raw_min_y = s_raw_y < s_raw_min_y ? s_raw_y : s_raw_min_y;
    s_raw_max_y = s_raw_y > s_raw_max_y ? s_raw_y : s_raw_max_y;
}

static void print_info(esp_lcd_touch_handle_t tp)
{
    uint8_t cfg[5] = {0};
    const esp_err_t err = esp_lcd_panel_io_rx_param(tp->io, GT911_CONFIG_REG, cfg, sizeof(cfg));
    bool swap = false;
    bool mirror_x = false;
    bool mirror_y = false;
    esp_lcd_touch_get_swap_xy(tp, &swap);
    esp_lcd_touch_get_mirror_x(tp, &mirror_x);
    esp_lcd_touch_get_mirror_y(tp, &mirror_y);
    lv_display_t *disp = lv_indev_get_display(s_indev);
    printf("TOUCH_INFO ecran=%" PRId32 "x%" PRId32 " rotation_adaptateur=%d rotation_lvgl=%d\n",
           lv_display_get_horizontal_resolution(disp), lv_display_get_vertical_resolution(disp), s_rotation,
           (int)lv_display_get_rotation(disp) * 90);
    printf("TOUCH_INFO gt911 config_version=%u resolution=%ux%u (lecture %s)\n", cfg[0],
           (unsigned)(cfg[1] | (cfg[2] << 8)), (unsigned)(cfg[3] | (cfg[4] << 8)), esp_err_to_name(err));
    printf("TOUCH_INFO esp_lcd_touch x_max=%u y_max=%u swap_xy=%d mirror_x=%d mirror_y=%d int_gpio=%d rst_gpio=%d\n",
           tp->config.x_max, tp->config.y_max, swap, mirror_x, mirror_y, tp->config.int_gpio_num,
           tp->config.rst_gpio_num);
    printf("TOUCH_INFO appuis=%" PRIu32 " clics=%" PRIu32 " sans_clic=%" PRIu32 " defilements=%" PRIu32
           " erreurs_lecture=%" PRIu32 " trames_touche=%s\n", s_count_press, s_count_click, s_count_no_click,
           s_count_scroll, s_count_read_err, s_key_frames ? "oui" : "non");
    printf("TOUCH_INFO brut_x=[%u..%u] brut_y=[%u..%u] dernier RAW x=%u y=%u MAPPED x=%" PRId32 " y=%" PRId32 "\n",
           s_raw_min_x == UINT16_MAX ? 0 : s_raw_min_x, s_raw_max_x, s_raw_min_y == UINT16_MAX ? 0 : s_raw_min_y,
           s_raw_max_y, s_raw_x, s_raw_y, s_map_x, s_map_y);
    printf("TOUCH_INFO minuterie_ui derniere=%" PRIu32 " ms max=%" PRIu32 " ms ; ecart max entre deux lectures "
           "tactiles (disponibilite LVGL)=%" PRIu32 " ms ; appuis synthetiques=%" PRIu32 " ; journal=%s\n",
           s_ui_last_ms, s_ui_max_ms, s_read_gap_max_ms, s_count_synthetic, s_log ? "actif" : "inactif");
}

/* Appui synthetique en cours : coordonnees interpolees, puis relachement d'au moins GESTURE_GAP_US */
static bool synthetic_point(int64_t now_us, bool *touched, int32_t *x, int32_t *y)
{
    if (s_gesture_state == 0) {
        if (s_gestures == NULL || xQueueReceive(s_gestures, &s_gesture, 0) != pdTRUE) {
            return false;
        }
        s_gesture_state = 1;
        s_gesture_t0 = now_us;
        s_count_synthetic++;
    }
    const int64_t elapsed_us = now_us - s_gesture_t0;
    const int64_t duration_us = (int64_t)s_gesture.ms * 1000;
    if (s_gesture_state == 1 && elapsed_us < duration_us) {
        const float t = duration_us > 0 ? (float)elapsed_us / (float)duration_us : 1.0f;
        *touched = true;
        *x = s_gesture.x1 + (int32_t)((s_gesture.x2 - s_gesture.x1) * t);
        *y = s_gesture.y1 + (int32_t)((s_gesture.y2 - s_gesture.y1) * t);
        return true;
    }
    if (s_gesture_state == 1) {
        s_gesture_state = 2;
        *touched = true; /* dernier point exactement a l'arrivee */
        *x = s_gesture.x2;
        *y = s_gesture.y2;
        return true;
    }
    *touched = false;
    if (elapsed_us >= duration_us + GESTURE_GAP_US) {
        s_gesture_state = 0;
    }
    return true;
}

/* Reading the same as the adapter, plus tracking the supports (tache LVGL) */
static esp_err_t diag_read(esp_lcd_touch_handle_t tp, esp_lcd_touch_point_data_t *points, uint8_t *count,
                           uint8_t max_count, void *user_ctx)
{
    (void)user_ctx;
    if (tp->config.process_coordinates != raw_hook) {
        tp->config.process_coordinates = raw_hook;
    }
    if (s_req_map >= 0) {
        const int map = s_req_map;
        s_req_map = -1;
        esp_lcd_touch_set_swap_xy(tp, (map & 4) != 0);
        esp_lcd_touch_set_mirror_x(tp, (map & 2) != 0);
        esp_lcd_touch_set_mirror_y(tp, (map & 1) != 0);
        printf("TOUCH_MAP swap_xy=%d mirror_x=%d mirror_y=%d\n", (map & 4) != 0, (map & 2) != 0, (map & 1) != 0);
    }
    if (s_req_info) {
        s_req_info = false;
        print_info(tp);
    }
    if (s_release_check) {
        s_release_check = false;
        if (s_count_click == s_click_at_press) {
            s_count_no_click++;
            if (s_log) {
                printf("TOUCH RESULTAT aucun clic (defilement ou appui perdu)\n");
            }
        }
    }

    const int64_t now_us = esp_timer_get_time();
    if (s_last_read_us != 0) {
        const uint32_t gap_ms = (uint32_t)((now_us - s_last_read_us) / 1000);
        s_read_gap_max_ms = gap_ms > s_read_gap_max_ms ? gap_ms : s_read_gap_max_ms;
    }
    s_last_read_us = now_us;

    if (esp_lcd_touch_read_data(tp) != ESP_OK) {
        s_count_read_err++;
    }
    *count = 0;
    esp_err_t err = esp_lcd_touch_get_data(tp, points, count, max_count);
    if (tp->data.buttons > 0) {
        s_key_frames = true;
    }

    bool synthetic_touched = false;
    int32_t sx = 0;
    int32_t sy = 0;
    if (synthetic_point(now_us, &synthetic_touched, &sx, &sy)) {
        /* synthetic support: replaces the real frame (the GT911 was read to empty his data) */
        err = ESP_OK;
        *count = synthetic_touched ? 1 : 0;
        if (synthetic_touched) {
            memset(&points[0], 0, sizeof(points[0]));
            points[0].x = (uint16_t)sx;
            points[0].y = (uint16_t)sy;
            s_raw_x = (uint16_t)sx;
            s_raw_y = (uint16_t)sy;
        }
    }

    if (err == ESP_OK && *count > 0) {
        s_map_x = points[0].x;
        s_map_y = points[0].y;
        if (!s_pressed) {
            s_pressed = true;
            s_press_us = now_us;
            s_click_at_press = s_count_click;
            s_min_x = s_max_x = s_map_x;
            s_min_y = s_max_y = s_map_y;
            s_count_press++;
            if (s_log) {
                printf("TOUCH PRESSED  RAW x=%u y=%u  MAPPED x=%" PRId32 " y=%" PRId32 "\n", s_raw_x, s_raw_y, s_map_x,
                       s_map_y);
            }
        } else {
            s_min_x = s_map_x < s_min_x ? s_map_x : s_min_x;
            s_max_x = s_map_x > s_max_x ? s_map_x : s_max_x;
            s_min_y = s_map_y < s_min_y ? s_map_y : s_min_y;
            s_max_y = s_map_y > s_max_y ? s_map_y : s_max_y;
        }
    } else if (s_pressed) {
        s_pressed = false;
        s_release_check = true;
        const int32_t moved = (s_max_x - s_min_x) > (s_max_y - s_min_y) ? s_max_x - s_min_x : s_max_y - s_min_y;
        if (s_log) {
            printf("TOUCH RELEASED RAW x=%u y=%u  MAPPED x=%" PRId32 " y=%" PRId32 "  duree=%" PRId64
                   " ms  deplacement=%" PRId32 " px\n", s_raw_x, s_raw_y, s_map_x, s_map_y,
                   (now_us - s_press_us) / 1000, moved);
        }
    }
    return err;
}

/* Record event timing to diagnose missed touches or UI stalls. */
static void on_indev_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED &&
            code != LV_EVENT_LONG_PRESSED) {
        return;
    }
    lv_obj_t *obj = lv_event_get_param(e);
    char desc[128] = "aucun";
    if (code == LV_EVENT_CLICKED) {
        s_count_click++;
    }
    if (code == LV_EVENT_RELEASED) {
        lv_indev_t *indev = lv_indev_active();
        lv_obj_t *scroll = indev != NULL ? lv_indev_get_scroll_obj(indev) : NULL;
        if (scroll == NULL) {
            return;
        }
        s_count_scroll++;
        obj = scroll;
    }
    if (!s_log) {
        return;
    }
    if (obj != NULL) {
        describe(obj, code == LV_EVENT_CLICKED, desc, sizeof(desc));
    }
    switch (code) {
    case LV_EVENT_PRESSED:
        printf("TOUCH OBJ %s\n", desc);
        break;
    case LV_EVENT_CLICKED:
        printf("TOUCH CLICKED %s\n", desc);
        break;
    case LV_EVENT_LONG_PRESSED:
        printf("TOUCH LONG_PRESSED %s\n", desc);
        break;
    default:
        printf("TOUCH SCROLL %s (le clic est annule)\n", desc);
        break;
    }
}

/* Attach diagnostics to the input device without changing its normal mapping. */
void touch_diag_init(lv_indev_t *indev, int rotation_deg)
{
    if (indev == NULL) {
        return;
    }
    s_indev = indev;
    s_rotation = rotation_deg;
    s_gestures = xQueueCreate(GESTURE_QUEUE_LEN, sizeof(gesture_t));
    const esp_lv_adapter_touch_callbacks_t cbs = {
        .custom_touch_read = diag_read,
    };
    if (esp_lv_adapter_set_touch_callbacks(indev, &cbs) != ESP_OK) {
        ESP_LOGW(TAG, "Diagnostic tactile indisponible (callbacks de l'adaptateur refuses)");
        return;
    }
    lv_indev_add_event_cb(indev, on_indev_event, LV_EVENT_ALL, NULL);
    ESP_LOGI(TAG, "Diagnostic tactile pret (console : touch_info, touch_log 0/1, touch_map <swap> <mx> <my>)");
}

void touch_diag_name(lv_obj_t *obj, const char *name)
{
    if (obj != NULL && s_name_count < MAX_NAMES) {
        s_names[s_name_count++] = (name_entry_t){.obj = obj, .name = name};
    }
}

void touch_diag_set_log(bool on)
{
    s_log = on;
    printf("TOUCH_LOG %s\n", on ? "actif" : "inactif");
}

bool touch_diag_log_enabled(void)
{
    return s_log;
}

void touch_diag_request_info(void)
{
    s_req_info = true;
}

void touch_diag_request_mapping(bool swap_xy, bool mirror_x, bool mirror_y)
{
    s_req_map = (swap_xy ? 4 : 0) | (mirror_x ? 2 : 0) | (mirror_y ? 1 : 0);
}

void touch_diag_ui_timer_ms(uint32_t ms)
{
    s_ui_last_ms = ms;
    s_ui_max_ms = ms > s_ui_max_ms ? ms : s_ui_max_ms;
}

bool touch_diag_inject(int x1, int y1, int x2, int y2, int ms)
{
    if (s_gestures == NULL) {
        return false;
    }
    const gesture_t g = {
        .x1 = (int16_t)x1, .y1 = (int16_t)y1, .x2 = (int16_t)x2, .y2 = (int16_t)y2, .ms = ms < 30 ? 30 : ms,
    };
    return xQueueSend(s_gestures, &g, 0) == pdTRUE;
}
