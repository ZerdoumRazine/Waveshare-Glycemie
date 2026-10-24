# Software architecture

Project ESP-IDF classic: all firmware is in `main/`, only one local component in
`components/`, the rest comes from the Espressive Component Manager.

---

## Data flow

```
capteur G7
   │  BLE
   ▼
téléphone (application Dexcom)
   │  Internet
   ▼
serveurs Dexcom Share  ──HTTPS──►  dexcom_share.c  ──►  glucose_service.c
                                        (lecture)         │  cache 24 h (288 mesures, PSRAM)
                                                          │  fusion sans doublon, rattrapage
                                                          ▼
                                                     app_state.c
                                         (état partagé + historique, sous mutex)
                                        ┌─────────────┼─────────────┐
                                        ▼             ▼             ▼
                                  ui_*.c (LVGL)   alerts.c     sd_storage.c
                                                      │             │
                                                      ▼             ▼
                                              audio_out.c /     CSV quotidien
                                                 voice.c        + journal
```

Pages do **never** network query: they read the shared cache.

---

## Modules

| File | Role |
|---|---|
| `app_main.c` | start (NVS → status → settings → sensor → ecran/tactile → interface → audio → Wi-Fi → microSD → network → Dexcom → alerts), supervision, development console |
| `app_state.c` | shared and historical status (288 measurements) under mutex; only source of truth for display |
| `app_config.h` | all the constants of the application (cadence, freshness, beaches, spindle) |
| `i18n.c` | screen language: text table in 6 languages, long dates, `tr()` et `tr_fr()` |
| `wifi_manager.c` | Wi-Fi station via theESP32-C6, scan, progressive reconnection |
| `net_services.c` | SNTP (Fuseau France), test DNS + HTTPS neutral, RSSI |
| `dexcom_share.c` | Dexcom Client Share native (according to *pydexcom*) |
| `dexcom_guard.c` | authentication lock NVS (account protection) |
| `glucose_service.c` | read rate, 24 h cache, merge, catch up, session, demo mode |
| `glucose_math.c` | **pure functions**: age, freshness, variation, real rate, next measurement |
| `sensor_session.c` | G7 sensor tracking: installation, delay at +10 days, reminders 24 / 2 hours, NVS |
| `consumables.c` | catheter (range, reminder 2 h), "now = departure time", `consumables.csv` |
| `stock.c` | storage of sensors and catheters: NVS, single decrementation by change, thresholds, local event STOCK_LOW |
| `sd_storage.c` | microSD: mounting without formatting, folders, CSV, deduplication, journal |
| `audio_out.c` | sounds generated, ouverture/fermeture Codec measurements, micro |
| `voice.c` | reading sentences: bank of the chosen language (`/glycemie/audio/voice/<code>/`), back on the French bank, then on a beep. The old Windows voice embedded is only played by the console command `voice_old`and only if `main/voice/voice_fr.pcm` was compiled (optional) |
| `voice_rx.c` | reception of a voice bank by the serial console (`vrx_dir` Select the language folder) |
| `voice_cmd.c` | local voice commands ESP-SR (MultiNet7 / WakeNet9L, confirmations, thresholds) |
| `alerts.c` | Alert manager: conditions, priorities, repetitions, hysteresis |
| `settings.c` | persistent settings (NVS) : volume, voice, alerts, thresholds, brightness, night, voice |
| `night_mode.c` | minimalist night screen, backlight, touch, alerts, preview |
| `ui_main.c` | banner, navigation, cooling cycle |
| `ui_home.c` `ui_graph.c` `ui_today.c` `ui_consumables.c` `ui_settings.c` | 5 pages |
| `ui_alert.c` | full screen alert sign |
| `ui_chart.c` | curve and tactile exploration |
| `ui_common.c` | fonts, style chips, "glass" cards, lights, rings, colors |
| `trend_arrow.c` | trend arrow drawn (`LV_EVENT_DRAW_POST`) |
| `touch_diag.c` | tactile diagnosis (journal, synthetic test supports) |
| `components/esp_driver_ppa` | copy of pilot PPA d'ESP-IDF **with the gel patch** (see `CORRECTIF.txt`) |

---

## Tasks

| Task | Cell | Role |
|---|---|---|
| `main` | 10 Kb internal | start and then monitor and console |
| LVGL (BSP) | Internal | **all** interface; no other wire touches LVGL |
| blood glucose | PSRAM | Dexcom rate, queries HTTPS, cache fusion |
| alerts | 6 Ko | assessment of conditions, repetitions, triggering son/voix |
| audio | Internal | codec opening, reading PCM ; Never called from the interface |
| microSD | PSRAM | writing CSV and newspaper, winding up |
| settings | — | Writing NVS Deferred (2 s after last change) |
| `voixcmd` | 16 KB PSRAM | voice commands (if enabled), priority 3 |

Rules followed everywhere:

- **Any call LVGL is done in the task LVGL** ;
- flash playback cannot be done from a stack in PSRAM (hence the task
  temporary internal battery charging `srmodels.bin`) ;
- Writing NVS is never done since an interface callback.

---

## Interface: rendering principles

Dark glass style 2.5D, **forms LVGL native only** — no images, no bitmap:

- wallpaper in gradient blue night → black, painted once (pages are transparent);
- "glass" cards: vertical gradient **opaque** (real translucidity won nothing
  visual and expensive), semi-transparent fine edge, short shadow, second contour
  discreet interior;
- Halos: LVGL draws the shadow only**around** of a non opaque object — a hollow halo
  I'd give you an outline. A correct halo is therefore a blurred colored shadow around a small
  rounded core, or an object **without background** whose only shade is tinted;
- Pill buttons; `lv_buttonmatrix_set_button_width` accept only **1 to 15**.

**Cost of rendering**: Each shadow costs according to its width (- 15 ms per map at 24 px). The
As a result, shadows were reduced to 10 px (maps) and 16 px (glycaemia zone). Refreshment
complete page measured (command `ui_stress`historical measures of September 2026):
ACCUEIL 112 ms, GRAPHIQUE 116–122 ms, AUJOURD'HUI 137–144 ms, CONSOMMABLES 130 ms,
MARKS 77–79 ms. At rest on the welcome: 5 refreshments of 5 ms per second.

**Polices**: Montserrat Medium and Bold on board (TTF) and returned by TinyTTF, with a
cache LRU 64 glyphs per size. This cache explains a decrease of PSRAM during
first hour, then a stabilization: **this is not a leak** (verified on a
4 h endurance: 185 consecutive minutes without any variation).

---

## Tactile and interface gel — mandatory patch

Adapter `esp_lvgl_adapter` turns the image 180° itself (LVGL is not turned) and
le GT911 of this map already returns coordinates in the displayed orientation. The
configuration validated at finger is `swap_xy = 0`, `mirror_x = 0`, `mirror_y = 0`
(in `app_main.c`). Mirrors X/Y of the example Waveshare reversed the touch.

**Reproducible gel**: quick supports permanently blocked the task LVGL in
`ppa_do_operation()` during 180° rotation of partial zones (mode `TRIPLE_PARTIAL`),
Until watchdog. Diagnosed by core dump. Correction is the official correction
Espressive ‘ PPA hang", reported in a **local copy of the component `esp_driver_ppa`**
(`components/esp_driver_ppa`, see `CORRECTIF.txt`) ; ESP-IDF Stay intact.
After corrective: 2,322 supports in 10 min without freezing (before: freeze on 3rd support).

> **Never remove `components/esp_driver_ppa`** or modify touch mapping without
> Repeat those tests.

---

## Persistence (NVS)

| Space | Keys | Content |
|---|---|---|
| `settings` | `v1`, `nuit`, `vcmd` | general settings, night mode, voice controls (separate blocks) |
| `sensor` | `start`, `remind2` | sensor setting time, callbacks already ringed (bit 0 = 24 h, bit 1 = 2 h) |
| `catheter` | `start`, `hours`, `remind2` | change, interval, reminder 2 hours already ringed |
| `conso` | `log_g7`, `log_cath` | last time already written in `consumables.csv` (anti-doubling) |
| `stock` | `q_g7`, `q_cath`, `s_g7`, `s_cath`, `t_g7`, `t_cath` | quantities, thresholds, decrementation markers |
| `dexcom` | — | authentication lock (ID fingerprint, never IDs) |

The old keys `remind` are **read** for conversion from versions
but never rewritten.

The partition `nvs` is never written by a flash: see [HARDWARE_EN.md](HARDWARE_EN.md).

---

## Identifier

`main/app_secrets.h` includes `device_secrets.h` **if there is** (via `__has_include`) and
provides otherwise empty values. `main/CMakeLists.txt` adds `<racine>/private` to
include private only if the file is present. The project therefore compiles without
IDs — the device starts and remains usable in demonstration mode.
