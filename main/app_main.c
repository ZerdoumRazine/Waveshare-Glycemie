/*
 * app_main.c: Initializes the ESP32-P4 and starts services in dependency order.
 * FR: Initialise l'ESP32-P4 et démarre les services dans l'ordre requis.
 * Design decision: Persistent settings must be loaded before the interface is started.
 *
 * Experimental supplementary display; official monitoring remains authoritative.
 */

/*
 * glycemie_display - afficheur familial de glycemie Dexcom G7
 * Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (ESP32-P4 rev1.3), ESP-IDF v5.5.5.
 *
 * AVERTISSEMENT : afficheur familial complementaire. Ce n'est pas un dispositif medical ;
 * il ne doit pas servir a prendre des decisions therapeutiques.
 */
/*
 * Brings up the board, background services and USB maintenance console in a defined order.
 * Démarre les services matériels et la console de maintenance USB.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "alerts.h"
#include "app_state.h"
#include "audio_out.h"
#include "consumables.h"
#include "dexcom_guard.h"
#include "glucose_service.h"
#include "net_services.h"
#include "i18n.h"
#include "night_mode.h"
#include "credentials.h"
#include "sd_storage.h"
#include "sensor_session.h"
#include "settings.h"
#include "stock.h"
#include "touch_diag.h"
#include "voice.h"
#include "voice_cmd.h"
#include "voice_rx.h"
#include "ui_main.h"
#include "wifi_manager.h"

/* Garde-fou : ce firmware ne doit jamais etre compile pour les seules puces rev3.x */
#if !CONFIG_ESP32P4_SELECTS_REV_LESS_V3 || CONFIG_ESP32P4_REV_MIN_FULL >= 300
#error "Configuration incompatible avec l'ESP32-P4 rev1.3 : profil pre-v3 (REV_MIN_100) obligatoire"
#endif

static const char *TAG = "app";

static const char *reset_reason_text(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON: return "mise sous tension";
    case ESP_RST_SW: return "redemarrage logiciel";
    case ESP_RST_PANIC: return "erreur fatale (panic)";
    case ESP_RST_INT_WDT: return "watchdog d'interruption";
    case ESP_RST_TASK_WDT: return "watchdog de taches";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "baisse de tension";
    case ESP_RST_EXT: return "reset externe";
    default: return "autre";
    }
}

static void log_system_info(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "=== %s %s - ESP-IDF %s ===", app->project_name, app->version, app->idf_ver);
    ESP_LOGI(TAG, "Demarrage : %s", reset_reason_text(esp_reset_reason()));
    ESP_LOGI(TAG, "Puce : ESP32-P4 revision v%d.%d, %d coeurs (firmware pour v%d.%d a v%d.%d)",
             chip.revision / 100, chip.revision % 100, chip.cores,
             CONFIG_ESP32P4_REV_MIN_FULL / 100, CONFIG_ESP32P4_REV_MIN_FULL % 100,
             CONFIG_ESP32P4_REV_MAX_FULL / 100, CONFIG_ESP32P4_REV_MAX_FULL % 100);
    ESP_LOGI(TAG, "Flash : %" PRIu32 " Mo", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "PSRAM : %u Mo detectee, %u Ko libres ; RAM interne libre : %u Ko",
             (unsigned)(esp_psram_get_size() / (1024 * 1024)),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
}

static void play_test_voice(int volume_pct)
{
    voice_test(120, volume_pct);
}

/* Interface figee : etat des taches utiles (le watchdog de taches produira ensuite le core dump) */
/* Record display stalls so a UI fault can be diagnosed separately from glucose polling. */
static void report_ui_freeze(int64_t age_ms)
{
    static const char *const k_tasks[] = {"lvgl", "swdraw", "alertes", "audio", "reglages", "console", "glycemie",
                                           "microsd", "wifi_mgr", "net_svc", "IDLE0", "IDLE1"};
    static const char *const k_states[] = {"en cours", "prete", "bloquee", "suspendue", "supprimee", "invalide"};
    ESP_LOGE(TAG, "UI_GEL : aucun rafraichissement de l'interface depuis %" PRId64 " ms", age_ms);
    for (size_t i = 0; i < sizeof(k_tasks) / sizeof(k_tasks[0]); i++) {
        TaskHandle_t task = xTaskGetHandle(k_tasks[i]);
        if (task != NULL) {
            const eTaskState state = eTaskGetState(task);
            ESP_LOGE(TAG, "UI_GEL tache %-8s etat=%s pile_libre_min=%u", k_tasks[i],
                     (int)state < 6 ? k_states[state] : "?", (unsigned)uxTaskGetStackHighWaterMark(task));
        }
    }
}

static void print_info(void)
{
    static app_state_t st;
    static glucose_point_t pts[2];
    app_state_get(&st);
    uint32_t seq = 0;
    int count = app_history_get(pts, 2, &seq);
    time_t now = time(NULL);
    printf("INFO wifi=%d ip=%s rssi=%d c6_fw=%s internet=%d heure=%d dexcom=%d demo=%d sd=%d sd_lignes=%u sd_libre_mo=%u "
           "audio=%d", st.wifi, st.ip, st.rssi, st.coproc_fw, st.internet, st.time_sync, st.dexcom, st.demo_mode, st.sd,
           (unsigned)st.sd_rows_written, (unsigned)st.sd_free_mb, st.audio_ok);
    if (count > 0) {
        printf(" derniere=%d tendance=%s age_s=%lld", pts[0].mgdl, trend_name(pts[0].trend),
               net_time_is_valid() ? (long long)(now - pts[0].wt_ms / 1000) : -1LL);
    }
    printf(" heap_int=%u heap_int_min=%u dma_bloc=%u psram=%u vcmd=%d\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), voice_cmd_running());
}

static consumable_t parse_consumable(const char *word)
{
    return strncmp(word, "cath", 4) == 0 ? CONSUMABLE_CATHETER
                                         : (strncmp(word, "capt", 4) == 0 ? CONSUMABLE_SENSOR : CONSUMABLE_COUNT);
}

/* Console de developpement : "snap" (capture d'ecran), "info" (etat, sans secret),
 * "dexcom_status" (verrou d'authentification), "dexcom_auth" (autorise la tentative unique),
 * "page <0-4>", "range <3|6|12|24>", "demo <off|normal|low|verylow|high|veryhigh|gap|stale>",
 * langue de l'interface : "lang" (langue courante), "lang fr|en|es|de|it|pt",
 * tests de fiabilite : "wifi_test_disconnect", "dexcom_test_session", "ui_age_offset <min>",
 * microSD : "sd_check", "sd_repair", "sd_format EFFACER", "sd_test_eject <s>", "sd_test_remount <n>",
 * tactile : "touch_info", "touch_log 0|1", "touch_map <swap> <mirror_x> <mirror_y>",
 * son : "volume <0-100>", "test_sound", "test_voice", "audio_info", "voice_old <type> <valeur>" (ancienne voix),
 * alerte de test : "alert_test", "alert_sim <type> <valeur>", "alert_test off", "alert_ok", "alert_snooze", "alert_info",
 * banque vocale par le cable USB-serie : "vrx_begin", "vrx_put", "vrx_list", "vrx_end" (voir voice_rx.h),
 * mode nuit (tests sans changer l'heure) : "night_test" (apercu), "night_on", "night_wake", "night_off", "night_info",
 * "night_schedule HH:MM HH:MM" et "night_enable 0|1" (reglages enregistres, comme dans RÉGLAGES),
 * stock : "stock_info", "stock_set|stock_add|stock_seuil capteur|catheter <valeur>" (-1 = non renseigne),
 * "stock_consume capteur|catheter <epoch>" (meme chemin qu'un changement reel : une seule fois par epoch),
 * consommables : "conso_info", "conso_dialog capteur|catheter", "dialog_close", "catheter_interval 48|72",
 * "catheter_reminders_reset", "conso_restore capteur|catheter <epoch> <rappels>" (restauration exacte apres un test,
 * sans ligne CSV), "conso_csv_purge <epoch>" (retire de consumables.csv les lignes de test datees de epoch ou apres),
 * identifiants (assistant par USB, aucune valeur affichee) : "ident_info", "ident_wifi <reseau>|<mot de passe>",
 * "ident_dexcom <compte>|<mot de passe>|<ous|us|jp>" (mises a l'essai), "ident_test", "ident_etat",
 * "ident_enregistrer" (apres un essai reussi), "ident_annuler", "ident_effacer", "ident_demo 0|1",
 * commandes vocales : "vcmd_on", "vcmd_off", "vcmd_eveil 0|1", "vcmd_gain <dB>", "vcmd_info",
 * "vcmd_sim <id> [probabilite]" (1 glycemie, 3 catheter, 4 glucose, 5 enregistrer nouveau capteur, 6 confirmer
 * capteur : meme chemin qu'une vraie detection), "vcmd_niveaux 0|1", "voice_glucose", "voice_reply <n>",
 * "vrx_inject" (son de test ajoute aux micros, voir voice_rx.h), journaux microSD : "sd_lire <fichier de logs>", sauvegarde d'un fichier : "sd_dump <chemin sous /glycemie>" */
static void console_task(void *arg)
{
    (void)arg;
    const uart_port_t port = (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM;
    if (!uart_is_driver_installed(port)) {
        if (uart_driver_install(port, 1024, 0, 0, NULL, 0) != ESP_OK) {
            ESP_LOGW(TAG, "Console serie indisponible");
            vTaskDelete(NULL);
            return;
        }
        uart_vfs_dev_use_driver(port);
    }
    char line[192];
    size_t len = 0;
    for (;;) {
        uint8_t ch;
        if (uart_read_bytes(port, &ch, 1, pdMS_TO_TICKS(1000)) != 1) {
            voice_rx_idle();
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            line[len] = '\0';
            if (voice_rx_command(line)) {
                /* copie de la banque vocale sur la microSD (voice_rx.c) */
            } else if (strcmp(line, "snap") == 0) {
                ui_snapshot_dump();
            } else if (strcmp(line, "info") == 0) {
                print_info();
            } else if (strcmp(line, "dexcom_status") == 0) {
                printf("DEXCOM_GUARD etat=%s info=%s\n", dex_guard_state_name(dex_guard_state()), dex_guard_info());
            } else if (strcmp(line, "dexcom_auth") == 0) {
                printf("DEXCOM_AUTH %s\n", glucose_service_request_auth());
            } else if (strcmp(line, "dexcom_test_session") == 0) {
                printf("TEST_SESSION %s\n", glucose_service_test_invalid_session());
            } else if (strcmp(line, "wifi_test_disconnect") == 0) {
                wifi_manager_test_disconnect();
            } else if (strncmp(line, "ui_age_offset ", 14) == 0) {
                int minutes = atoi(line + 14);
                ui_test_set_age_offset(minutes);
                printf("UI_AGE_OFFSET %d min\n", minutes);
            } else if (strncmp(line, "lang", 4) == 0) {
                /* "lang" affiche la langue courante ; "lang <code>" la change (comme dans REGLAGES) */
                const char *arg = line[4] == ' ' ? line + 5 : "";
                if (arg[0] == '\0') {
                    printf("LANG %s (%s)\n", lang_code(i18n_lang()), lang_name(i18n_lang()));
                } else {
                    int found = -1;
                    for (int i = 0; i < LANG_COUNT; i++) {
                        if (strncmp(arg, lang_code((lang_t)i), 2) == 0) {
                            found = i;
                            break;
                        }
                    }
                    if (found < 0) {
                        printf("LANG inconnue : %s (fr, en, es, de, it, pt)\n", arg);
                    } else {
                        ui_request_lang(found);
                        printf("LANG %s demandee\n", lang_code((lang_t)found));
                    }
                }
            } else if (strncmp(line, "page ", 5) == 0) {
                ui_request_page(atoi(line + 5));
                printf("PAGE %d\n", atoi(line + 5));
            } else if (strncmp(line, "range ", 6) == 0) {
                ui_request_chart_hours(atoi(line + 6));
                printf("RANGE %d h\n", atoi(line + 6));
            } else if (strncmp(line, "demo ", 5) == 0) {
                printf("DEMO %s\n", glucose_service_set_demo(line + 5));
            } else if (strcmp(line, "sensor_status") == 0) {
                sensor_status_t s;
                bool done = false;
                sensor_session_get((int64_t)time(NULL), &s);
                const int reminder = sensor_session_current_reminder((int64_t)time(NULL), &done);
                printf("SENSOR connu=%d jour=%d a_changer=%d restant_s=%lld progression=%d%% rappel_en_cours=%d "
                       "deja_signale=%d\n", s.known, s.day, s.expired, (long long)s.remaining_s, s.progress_pct,
                       reminder, done);
            } else if (strncmp(line, "sensor_age_h ", 13) == 0) {
                /* test : capteur pose il y a N heures */
                const int64_t now = (int64_t)time(NULL);
                sensor_session_start(now - (int64_t)atoi(line + 13) * 3600, now);
                printf("SENSOR pose il y a %d h\n", atoi(line + 13));
            } else if (strcmp(line, "sensor_reminders_reset") == 0) {
                sensor_session_reset_reminders();
                printf("SENSOR rappels reinitialises\n");
            } else if (strcmp(line, "sensor_clear") == 0) {
                sensor_session_clear();
                printf("SENSOR efface\n");
            } else if (strncmp(line, "audio_test ", 11) == 0) {
                bool detected = false;
                float gain_db = 0.0f;
                int volume = atoi(line + 11);
                ESP_LOGI(TAG, "Test audio : tonalite 1 kHz, volume %d %%", volume);
                esp_err_t err = audio_out_selftest(volume, &detected, &gain_db);
                printf("AUDIO_TEST err=%s son_detecte=%s gain_db=%.1f\n", esp_err_to_name(err), detected ? "OUI" : "NON",
                       gain_db);
            } else if (strncmp(line, "pattern ", 8) == 0) {
                int pattern = 0;
                int volume = 0;
                if (sscanf(line + 8, "%d %d", &pattern, &volume) == 2) {
                    audio_out_play_pattern((audio_pattern_t)pattern, volume);
                    printf("PATTERN %d volume %d\n", pattern, volume);
                }
            } else if (strncmp(line, "volume ", 7) == 0) {
                settings_t s;
                settings_get(&s);
                const int pct = atoi(line + 7);
                s.volume_pct = (uint8_t)(pct < 0 ? 0 : (pct > 100 ? 100 : pct));
                s.volume_level = VOLUME_CUSTOM; /* recalcule d'apres le pourcentage (MUTE, FAIBLE...) */
                settings_set(&s);
                printf("VOLUME %u %% (codec %.1f dB)\n", s.volume_pct, audio_out_volume_db(s.volume_pct));
            } else if (strcmp(line, "test_sound") == 0) {
                settings_t s;
                settings_get(&s);
                printf("TEST_SOUND volume %u %%\n", s.volume_pct);
                audio_out_play_pattern(AUDIO_PATTERN_TEST, s.volume_pct);
            } else if (strcmp(line, "test_voice") == 0) {
                settings_t s;
                settings_get(&s);
                printf("TEST_VOICE volume %u %%\n", s.volume_pct);
                voice_test(120, s.volume_pct);
            } else if (strcmp(line, "audio_info") == 0) {
                audio_out_print_info();
            } else if (strncmp(line, "audio_measure ", 14) == 0) {
                int volume = 0;
                char kind[8] = "son";
                float gain = 12.0f;
                if (sscanf(line + 14, "%d %7s %f", &volume, kind, &gain) >= 1) {
                    audio_out_measure(volume, strcmp(kind, "voix") == 0 ? play_test_voice : NULL, gain);
                }
            } else if (strncmp(line, "audio_regs ", 11) == 0) {
                audio_out_dump_regs(atoi(line + 11));
            } else if (strcmp(line, "reglages") == 0) {
                settings_print();
            } else if (strcmp(line, "ui_info") == 0) {
                ui_print_stats();
                touch_diag_request_info();
            } else if (strcmp(line, "graph_info") == 0) {
                ui_graph_print_explore();
            } else if (strncmp(line, "ui_stress ", 10) == 0) {
                int seconds = 0;
                int mode = 1;
                if (sscanf(line + 10, "%d %d", &seconds, &mode) >= 1) {
                    ui_request_render_stress(seconds, mode);
                }
            } else if (strncmp(line, "touch_tap ", 10) == 0) {
                int x = 0;
                int y = 0;
                int ms = 120;
                if (sscanf(line + 10, "%d %d %d", &x, &y, &ms) >= 2 && !touch_diag_inject(x, y, x, y, ms)) {
                    printf("TOUCH_TAP file pleine\n");
                }
            } else if (strncmp(line, "touch_swipe ", 12) == 0) {
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
                int ms = 400;
                if (sscanf(line + 12, "%d %d %d %d %d", &x1, &y1, &x2, &y2, &ms) >= 4 &&
                        !touch_diag_inject(x1, y1, x2, y2, ms)) {
                    printf("TOUCH_SWIPE file pleine\n");
                }
            } else if (strcmp(line, "touch_info") == 0) {
                touch_diag_request_info();
            } else if (strncmp(line, "touch_log ", 10) == 0) {
                touch_diag_set_log(atoi(line + 10) != 0);
            } else if (strncmp(line, "touch_map ", 10) == 0) {
                int swap = 0;
                int mirror_x = 0;
                int mirror_y = 0;
                if (sscanf(line + 10, "%d %d %d", &swap, &mirror_x, &mirror_y) == 3) {
                    touch_diag_request_mapping(swap != 0, mirror_x != 0, mirror_y != 0);
                }
            } else if (strcmp(line, "alert_test") == 0) {
                alerts_test_start(ALERT_TEST, 0);
                printf("ALERT_TEST alerte de test demandee\n");
            } else if (strncmp(line, "alert_sim ", 10) == 0) {
                /* alerte de test simulant un vrai type : 0 tres basse, 1 basse, 2 aucune donnee, 3 tres elevee,
                 * 4 elevee, 5 capteur (valeur = rappel 0 : 24 h, 1 : 2 h), 6 catheter (valeur = rappel 0 : 2 h) ;
                 * donnees Dexcom et vraies alertes inchangees */
                int type = 0;
                int value = 0;
                if (sscanf(line + 10, "%d %d", &type, &value) == 2) {
                    alerts_test_start((alert_type_t)type, value);
                    printf("ALERT_SIM %d %d\n", type, value);
                }
            } else if (strncmp(line, "voice_old ", 10) == 0) {
                int type = 0;
                int value = 0;
                if (sscanf(line + 10, "%d %d", &type, &value) == 2) {
                    settings_t s;
                    settings_get(&s);
                    voice_old_announce((alert_type_t)type, value, s.volume_pct);
                    printf("VOICE_OLD %d %d\n", type, value);
                }
            } else if (strcmp(line, "night_on") == 0) {
                night_mode_request(NIGHT_REQ_ON);
                printf("NIGHT nuit simulee (reglages enregistres)\n");
            } else if (strcmp(line, "night_test") == 0) {
                night_mode_request(NIGHT_REQ_TEST);
                printf("NIGHT apercu de l'ecran nuit\n");
            } else if (strncmp(line, "night_schedule ", 15) == 0) {
                int h1 = -1;
                int m1 = -1;
                int h2 = -1;
                int m2 = -1;
                if (sscanf(line + 15, "%d:%d %d:%d", &h1, &m1, &h2, &m2) == 4 && h1 >= 0 && h1 < 24 && m1 >= 0 &&
                        m1 < 60 && h2 >= 0 && h2 < 24 && m2 >= 0 && m2 < 60) {
                    night_settings_t n;
                    settings_night_get(&n);
                    n.start_min = (uint16_t)(h1 * 60 + m1);
                    n.end_min = (uint16_t)(h2 * 60 + m2);
                    settings_night_set(&n);
                    printf("NIGHT plage %02d:%02d-%02d:%02d\n", h1, m1, h2, m2);
                }
            } else if (strncmp(line, "night_enable ", 13) == 0) {
                night_settings_t n;
                settings_night_get(&n);
                n.enabled = atoi(line + 13) != 0;
                settings_night_set(&n);
                printf("NIGHT mode nuit %s\n", n.enabled ? "actif" : "inactif");
            } else if (strcmp(line, "night_wake") == 0) {
                night_mode_request(NIGHT_REQ_WAKE);
                printf("NIGHT reveil comme par un toucher\n");
            } else if (strcmp(line, "night_off") == 0) {
                night_mode_request(NIGHT_REQ_OFF);
                printf("NIGHT fin de simulation (horaire reel)\n");
            } else if (strcmp(line, "night_info") == 0) {
                night_mode_print_info();
            } else if (strcmp(line, "alert_test off") == 0) {
                alerts_test_stop();
                printf("ALERT_TEST arretee\n");
            } else if (strcmp(line, "alert_ok") == 0) {
                ui_request_alert_button(true);
            } else if (strcmp(line, "alert_info") == 0) {
                alerts_print_status();
            } else if (strncmp(line, "alert_test ", 11) == 0) {
                int type = 0;
                int value = 0;
                if (sscanf(line + 11, "%d %d", &type, &value) == 2) {
                    alerts_test_sound((alert_type_t)type, value);
                    printf("ALERT_TEST %d %d\n", type, value);
                }
            } else if (strncmp(line, "voice_test ", 11) == 0) {
                settings_t s;
                settings_get(&s);
                voice_test(atoi(line + 11), s.volume_pct);
                printf("VOICE_TEST %d\n", atoi(line + 11));
            } else if (strcmp(line, "alert_ack") == 0) {
                ui_request_alert_button(true);
            } else if (strcmp(line, "alert_snooze") == 0) {
                ui_request_alert_button(false);
            } else if (strncmp(line, "scroll ", 7) == 0) {
                ui_request_settings_scroll(atoi(line + 7));
            } else if (strcmp(line, "sensor_dialog") == 0) {
                ui_request_consumable_dialog(CONSUMABLE_SENSOR);
            } else if (strncmp(line, "conso_dialog ", 13) == 0) {
                ui_request_consumable_dialog(parse_consumable(line + 13));
            } else if (strcmp(line, "dialog_close") == 0) {
                ui_request_consumable_dialog(-1);
            } else if (strcmp(line, "conso_info") == 0) {
                consumables_print();
            } else if (strcmp(line, "stock_info") == 0) {
                stock_print();
            } else if (strncmp(line, "stock_set ", 10) == 0 || strncmp(line, "stock_add ", 10) == 0 ||
                       strncmp(line, "stock_seuil ", 12) == 0 || strncmp(line, "stock_consume ", 14) == 0) {
                /* tests : stock_set|stock_add|stock_seuil capteur|catheter <valeur>, stock_consume capteur|catheter
                 * <epoch du changement> (meme chemin que NOUVEAU CAPTEUR / NOUVEAU CATHETER) */
                const char *args = strchr(line, ' ') + 1;
                char what[12] = "";
                long long value = 0;
                const stock_item_t item = strncmp(args, "cath", 4) == 0 ? STOCK_CATHETER : STOCK_SENSOR;
                if (sscanf(args, "%11s %lld", what, &value) == 2) {
                    if (strncmp(line, "stock_set ", 10) == 0) {
                        stock_set(item, (int)value);
                    } else if (strncmp(line, "stock_add ", 10) == 0) {
                        stock_add(item, (int)value);
                    } else if (strncmp(line, "stock_seuil ", 12) == 0) {
                        stock_set_low_threshold(item, (int)value);
                    } else {
                        printf("STOCK_CONSUME %s\n", stock_consume(item, (int64_t)value) ? "une unite retiree"
                                                                                        : "inchange");
                    }
                }
                stock_print();
            } else if (strncmp(line, "catheter_interval ", 18) == 0) {
                catheter_set_interval_h(atoi(line + 18));
                printf("CONSO intervalle catheter %d h\n", catheter_interval_h());
            } else if (strcmp(line, "catheter_reminders_reset") == 0) {
                catheter_reset_reminders();
                printf("CONSO rappels du catheter reinitialises\n");
            } else if (strncmp(line, "conso_restore ", 14) == 0) {
                char what[12] = "";
                long long start = -1;
                int mask = 0;
                consumable_t c = CONSUMABLE_COUNT;
                if (sscanf(line + 14, "%11s %lld %i", what, &start, &mask) == 3 &&
                        (c = parse_consumable(what)) != CONSUMABLE_COUNT && start >= 0) {
                    printf("CONSO_RESTORE %s depart=%lld rappels=0x%02x %s\n", what, start, mask & 0xFF,
                           esp_err_to_name(consumable_restore(c, (int64_t)start, mask)));
                } else {
                    printf("CONSO_RESTORE arguments : capteur|catheter <epoch> <rappels>\n");
                }
            } else if (strncmp(line, "conso_csv_purge ", 16) == 0) {
                const long long from = atoll(line + 16);
                if (from > 1700000000LL) {
                    sd_storage_request_consumables_purge((int64_t)from);
                    printf("CONSO_PURGE demandee a partir de %lld\n", from);
                }
            } else if (strcmp(line, "vcmd_on") == 0 || strcmp(line, "vcmd_off") == 0) {
                voice_cmd_settings_t v;
                settings_vcmd_get(&v);
                v.enabled = strcmp(line, "vcmd_on") == 0;
                settings_vcmd_set(&v);
                voice_cmd_apply_settings();
                printf("VCMD %s (reglage enregistre)\n", v.enabled ? "activation" : "arret");
            } else if (strncmp(line, "vcmd_eveil ", 11) == 0) {
                voice_cmd_settings_t v;
                settings_vcmd_get(&v);
                v.wake_word = atoi(line + 11) != 0;
                settings_vcmd_set(&v);
                printf("VCMD mot d'eveil %s\n", v.wake_word ? "requis" : "non requis");
            } else if (strncmp(line, "vcmd_gain ", 10) == 0) {
                voice_cmd_settings_t v;
                settings_vcmd_get(&v);
                const int gain = atoi(line + 10);
                v.mic_gain_db = (uint8_t)(gain < 0 ? 0 : (gain > 42 ? 42 : gain));
                settings_vcmd_set(&v);
                printf("VCMD gain micro %u dB\n", v.mic_gain_db);
            } else if (strcmp(line, "vcmd_info") == 0) {
                voice_cmd_print_info();
            } else if (strncmp(line, "vcmd_sim ", 9) == 0) {
                int id = 0;
                float prob = 1.0f;
                if (sscanf(line + 9, "%d %f", &id, &prob) >= 1) {
                    voice_cmd_simulate(id, prob);
                }
            } else if (strncmp(line, "vcmd_niveaux ", 13) == 0) {
                voice_cmd_set_level_log(atoi(line + 13) != 0);
            } else if (strncmp(line, "vcmd_debug ", 11) == 0) {
                voice_cmd_set_decode_log(atoi(line + 11) != 0);
            } else if (strncmp(line, "vcmd_ajout ", 11) == 0) {
                /* vcmd_ajout <1|2|3> <phonemes MultiNet> : variante temporaire pour regler la reconnaissance */
                const int cmd = atoi(line + 11);
                const char *phonemes = strchr(line + 11, ' ');
                if (phonemes != NULL) {
                    voice_cmd_add_phrase(cmd, phonemes + 1);
                }
            } else if (strcmp(line, "voice_glucose") == 0) {
                glucose_point_t pts[1];
                uint32_t seq = 0;
                const int count = app_history_get(pts, 1, &seq);
                const int64_t age_s = net_time_is_valid() && count > 0 ? (int64_t)time(NULL) - pts[0].wt_ms / 1000 : -1;
                const bool stale = count > 0 && (age_s < 0 || age_s >= (int64_t)GLY_AGE_STALE_MIN * 60);
                settings_t s;
                settings_get(&s);
                voice_say_glucose(count > 0 ? pts[0].mgdl : 0, count > 0 ? pts[0].trend : TREND_NONE, stale, count > 0,
                                  s.volume_pct);
                printf("VOICE_GLUCOSE %d ancienne=%d\n", count > 0 ? pts[0].mgdl : 0, stale);
            } else if (strncmp(line, "voice_reply ", 12) == 0) {
                settings_t s;
                settings_get(&s);
                voice_reply((voice_reply_t)atoi(line + 12), s.volume_pct);
                printf("VOICE_REPLY %d\n", atoi(line + 12));
            } else if (strncmp(line, "sd_format ", 10) == 0) {
                printf("SD_FORMAT %s\n", sd_storage_request_format(line + 10));
            } else if (strcmp(line, "ident_info") == 0) {
                credentials_print();
            } else if (strncmp(line, "ident_wifi ", 11) == 0) {
                /* ident_wifi <nom du reseau>|<mot de passe> : la barre verticale separe les deux.
                 * Les valeurs sont mises A L'ESSAI : les acces precedents restent enregistres. */
                char *sep = strchr(line + 11, '|');
                if (sep == NULL) {
                    printf("IDENT usage : ident_wifi <reseau>|<mot de passe>\n");
                } else {
                    *sep = '\0';
                    const esp_err_t e = credentials_trial_set_wifi(line + 11, sep + 1);
                    memset(line, 0, sizeof(line)); /* le mot de passe ne reste pas en memoire */
                    printf("IDENT wifi %s\n", e == ESP_OK ? "a l'essai" : esp_err_to_name(e));
                }
            } else if (strncmp(line, "ident_dexcom ", 13) == 0) {
                /* ident_dexcom <compte>|<mot de passe>|<region ous|us|jp>, egalement a l'essai */
                char *p1 = strchr(line + 13, '|');
                char *p2 = p1 != NULL ? strchr(p1 + 1, '|') : NULL;
                if (p1 == NULL || p2 == NULL) {
                    printf("IDENT usage : ident_dexcom <compte>|<mot de passe>|<ous|us|jp>\n");
                } else {
                    *p1 = *p2 = '\0';
                    const esp_err_t e = credentials_trial_set_dexcom(line + 13, p1 + 1, p2 + 1);
                    memset(line, 0, sizeof(line));
                    printf("IDENT dexcom %s\n", e == ESP_OK ? "a l'essai" : esp_err_to_name(e));
                }
            } else if (strcmp(line, "ident_enregistrer") == 0) {
                /* Enregistrement uniquement apres un test de connexion reussi : aucun contournement console. */
                char msg[96];
                if (!credentials_trial_active()) {
                    printf("IDENT enregistrer : aucun essai en cours\n");
                } else if (glucose_service_test_state(msg, sizeof(msg)) != GLY_TEST_REUSSI) {
                    printf("IDENT enregistrer refuse : essai non reussi (%s). "
                           "Lancez ident_test.\n", msg);
                } else {
                    const esp_err_t e = credentials_trial_commit();
                    printf("IDENT enregistrer %s\n", e == ESP_OK ? "ok (redemarrer pour appliquer)"
                                                                 : esp_err_to_name(e));
                }
            } else if (strcmp(line, "ident_annuler") == 0) {
                credentials_trial_cancel();
                printf("IDENT annule (acces precedents conserves)\n");
            } else if (strcmp(line, "ident_test") == 0) {
                glucose_service_test_request();
                printf("IDENT essai de connexion lance (suivre les traces)\n");
            } else if (strcmp(line, "ident_etat") == 0) {
                char msg[96];
                printf("IDENT essai=%d detail=%s\n", (int)glucose_service_test_state(msg, sizeof(msg)), msg);
            } else if (strcmp(line, "ident_effacer") == 0) {
                const esp_err_t e = credentials_clear();
                printf("IDENT efface %s\n", e == ESP_OK ? "ok (redemarrer pour appliquer)"
                                                       : esp_err_to_name(e));
            } else if (strncmp(line, "ident_demo ", 11) == 0) {
                credentials_set_demo(atoi(line + 11) != 0);
                credentials_print();
            } else if (strncmp(line, "sd_lire ", 8) == 0) {
                sd_storage_print_log(line + 8);
            } else if (strncmp(line, "sd_dump ", 8) == 0) {
                /* sauvegarde d'un fichier de la carte par le cable, sans la sortir de l'appareil */
                sd_storage_dump_file(line + 8);
            } else if (strcmp(line, "sd_check") == 0) {
                sd_storage_request_check();
            } else if (strcmp(line, "sd_repair") == 0) {
                sd_storage_request_repair();
            } else if (strncmp(line, "sd_test_eject ", 14) == 0) {
                sd_storage_test_eject(atoi(line + 14));
                printf("SD_TEST_EJECT %d s\n", atoi(line + 14));
            } else if (strncmp(line, "sd_test_remount ", 16) == 0) {
                sd_storage_test_remount(atoi(line + 16));
                printf("SD_TEST_REMOUNT %d\n", atoi(line + 16));
            }
            len = 0;
        } else if (ch >= 0x20 && ch < 0x7F && len < sizeof(line) - 1) {
            line[len++] = (char)ch; /* octets parasites du reset ignores */
        }
    }
}

/* Initialise storage and settings before services that depend on them; do not erase NVS to hide an error. */
void app_main(void)
{
    log_system_info();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Ne jamais effacer automatiquement NVS : cette partition contient les
         * acces, les dates de pose et les reglages de l'utilisateur. Une
         * restauration/maintenance doit etre decidee a partir d'une sauvegarde. */
        ESP_LOGE(TAG, "NVS incompatible ou pleine (%s) : donnees conservees ; intervention necessaire",
                 esp_err_to_name(err));
    }
    ESP_ERROR_CHECK(err);

    app_state_init();
    settings_init();
    credentials_init();
    sensor_session_init();
    consumables_init();
    stock_init();
    /* Pas de trace HTTP detaillee : les URL Dexcom contiennent l'identifiant de session */
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);

    /* Ecran, tactile et LVGL : bloc repris de l'exemple officiel Waveshare 09_lvgl_demo_v9
     * (examples/esp-idf/09_lvgl_demo_v9/main/main.c, SPDX-FileCopyrightText: 2025 Espressif
     * Systems (Shanghai) CO LTD, SPDX-License-Identifier: Apache-2.0), sauf le tactile.
     * L'adaptateur tourne l'image de 180 degres, mais le GT911 de cette carte donne deja des coordonnees dans
     * l'orientation affichee : les miroirs X/Y de l'exemple inversaient le toucher (constat physique 2026-09-14). */
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_180,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    cfg.lv_adapter_cfg.task_stack_size = 16 * 1024;

    lv_display_t *display = bsp_display_start_with_config(&cfg);
    if (display == NULL) {
        ESP_LOGE(TAG, "Ecran : echec d'initialisation (ecran ou tactile)");
    } else {
        ESP_LOGI(TAG, "Ecran initialise : EK79007 MIPI-DSI %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
        ESP_LOGI(TAG, "Tactile GT911 : %s", bsp_display_get_input_dev() != NULL ? "detecte et enregistre" : "ABSENT");
        settings_t user_settings;
        settings_get(&user_settings);
        err = bsp_display_brightness_set(user_settings.brightness);
        ESP_LOGI(TAG, "Retroeclairage : %s (%u %%)", err == ESP_OK ? "allume" : esp_err_to_name(err), user_settings.brightness);
        if (bsp_display_lock(-1)) {
            ui_main_create();
            touch_diag_init(bsp_display_get_input_dev(), (int)cfg.rotation);
            bsp_display_unlock();
            ESP_LOGI(TAG, "LVGL demarre");
        }
    }

    /* Audio (ES8311 via le BSP) : aucun son au demarrage */
    if (audio_out_init() != ESP_OK) {
        ESP_LOGW(TAG, "Audio indisponible : alertes visuelles uniquement");
    }

    /* Wi-Fi apres l'ecran (ordre valide sur cette carte), puis heure/Internet et Dexcom */
    err = wifi_manager_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi heberge : demarrage impossible (%s)", esp_err_to_name(err));
    }
    /* microSD apres le Wi-Fi heberge (controleur SDMMC partage) ; jamais bloquante pour le reste */
    if (sd_storage_start() == ESP_OK) {
        glucose_service_set_new_readings_cb(sd_storage_record_readings);
    }
    ESP_ERROR_CHECK(net_services_start());
    ESP_ERROR_CHECK(glucose_service_start());
    if (credentials_demo()) {
        /* Mode demonstration choisi par l'utilisateur : donnees simulees, aucun compte, bandeau MODE DEMO */
        glucose_service_set_demo("normal");
    }
    if (alerts_start() != ESP_OK) {
        ESP_LOGE(TAG, "Gestionnaire d'alertes indisponible");
    }
    /* Commandes vocales locales (micros ES7210, ESP-SR) : ecoute seulement si activee dans RÉGLAGES */
    voice_cmd_init();
    xTaskCreate(console_task, "console", 8192, NULL, 2, NULL);

    int64_t ticks = 0;
    int64_t ui_freeze_reported_ms = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ticks++;
        const int64_t heartbeat_age = glucose_service_heartbeat_age_s();
        if (heartbeat_age > 600) {
            ESP_LOGE(TAG, "Tache glycemie bloquee depuis %lld s : redemarrage", (long long)heartbeat_age);
            esp_restart();
        }
        /* Surveillance de l'interface : diagnostic seulement (le watchdog de taches reste l'unique filet) */
        if (display != NULL) {
            const int64_t ui_age_ms = ui_heartbeat_age_ms();
            if (ui_age_ms > 5000 && (ui_freeze_reported_ms == 0 || ui_age_ms - ui_freeze_reported_ms >= 20000)) {
                report_ui_freeze(ui_age_ms);
                ui_freeze_reported_ms = ui_age_ms;
            } else if (ui_age_ms < 3000 && ui_freeze_reported_ms != 0) {
                ESP_LOGW(TAG, "Interface de nouveau active");
                ui_freeze_reported_ms = 0;
            }
        }
        if (ticks % 6 == 0) {
            ESP_LOGI(TAG, "Memoire : interne %u Ko (min %u Ko, plus grand bloc DMA %u Ko), PSRAM %u Ko",
                     (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                     (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
                     (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024),
                     (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
        }
    }
}
