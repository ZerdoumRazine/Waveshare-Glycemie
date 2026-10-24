# Firmware architecture

WaveShare Glycémie is an ESP-IDF application for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B. Application code lives in `main/`. Espressif and Waveshare dependencies are resolved by the ESP-IDF component manager; one locally patched Espressif PPA component lives in `components/esp_driver_ppa/`.

## Data flow

```text
Dexcom G7 sensor
  -> smartphone with the official Dexcom app
  -> Dexcom Share servers (HTTPS)
  -> dexcom_share.c (read-only client)
  -> glucose_service.c (polling, deduplication, freshness)
  -> app_state.c (shared snapshot and recent history)
       |-> ui_*.c (LVGL pages)
       |-> alerts.c -> audio_out.c / voice.c
       `-> sd_storage.c (daily CSV and event logs)
```

The touchscreen pages **never call Dexcom directly**; they read shared application state. A lost connection is recorded as a connection or freshness problem, not treated as a new glucose value.

## Module map

| Module | Responsibility |
|---|---|
| `app_main.c` | Boot sequence, supervision, USB diagnostic console |
| `app_config.h` | Refresh periods, limits and configuration constants |
| `app_state.c` | Shared snapshots and recent history, protected by synchronization |
| `wifi_manager.c` | Scanning, association and reconnect logic through the ESP32-C6 |
| `net_services.c` | Time synchronization, DNS/HTTPS diagnostics and network information |
| `credentials.c` | Persistent access details and reversible configuration trials |
| `dexcom_guard.c` | Authentication-attempt state persisted across restarts |
| `dexcom_share.c` | Read-only communication with Dexcom Share |
| `glucose_service.c` | Fetch schedule, history integration, demo mode and freshness |
| `glucose_math.c` | Hardware-independent glucose calculations |
| `sensor_session.c` | G7 insertion date and reminders |
| `consumables.c` | Infusion-set schedule and replacement events |
| `stock.c` | Inventory counts and one-time decrements |
| `sd_storage.c` | FAT32 card mounting, CSV records, logs and file transfer |
| `settings.c` | Volume, thresholds, display preferences and NVS persistence |
| `night_mode.c` | Night schedule and temporary return to normal brightness |
| `i18n.c` | Translated screen strings and selected language |
| `voice.c` | Playback of prepared microSD speech banks |
| `voice_rx.c` | USB-serial WAV transfer and checksum verification |
| `audio_out.c` | Audio codec output and fallback alert tones |
| `voice_cmd.c` / `voice_cmd_stub.c` | Optional experimental voice recognition / disabled public implementation |
| `alerts.c` | Alert state, priorities, repetition and acknowledgement |
| `ui_main.c` / `ui_*.c` | Page navigation, data views, charts and settings |
| `ui_common.c` | Common LVGL widgets, fonts and styling |
| `touch_diag.c` | Diagnostic touch input and hit-area tests |
| `components/esp_driver_ppa/` | Locally patched Espressif PPA driver; preserve its notices |

## Task ownership and concurrency

The main task starts the system and provides supervision. The BSP's LVGL task owns UI creation and rendering. Separate work handles network polling, alerts, audio playback and microSD operations. **Only the LVGL task should access LVGL widgets.** UI callbacks do not perform blocking network operations or direct NVS writes.

The original project also used a temporary internal-RAM stack when loading the optional ESP-SR model from flash. That recognition path is **not enabled in the standard public firmware**.

## Rendering and touch input

The UI uses native LVGL shapes and embedded Montserrat fonts. The dark-glass visual style relies on opaque gradients, borders and carefully limited shadow sizes; expensive shadows previously increased rendering time. Historical measurements for the five pages are documented in the version history and should not be treated as new benchmark results.

The display adapter handles 180-degree rotation. The touch-controller settings validated on the family's board are `swap_xy = 0`, `mirror_x = 0`, `mirror_y = 0`. The upstream PPA code had a reproducible freeze during rotated partial updates; the repository includes an Espressif-based fix in `components/esp_driver_ppa/`, documented by `CORRECTIF.txt`. **Do not remove that component or change touch orientation without regression testing.**

## Persistence and credentials

NVS namespaces separate responsibilities:

| Namespace | Typical contents |
|---|---|
| `settings` | General preferences, night mode and optional voice-command settings |
| `sensor` | Insertion time and reminder markers |
| `catheter` | Change time, interval and reminder marker |
| `conso` | Deduplication markers for consumable event records |
| `stock` | Quantities, warning thresholds and decrement markers |
| `ident` | Wi-Fi / Dexcom settings and demo-mode state |
| `dexcom` | Authentication-attempt guard state (not the password itself) |

An earlier private-build mode could embed credentials using `private/device_secrets.h`. Public builds intentionally omit this file and use the touchscreen or USB setup assistant instead. Credentials kept in NVS use reversible obfuscation, **not cryptographic encryption**. The application must not automatically erase NVS on initialization failure.

Configuration follows the pattern **begin trial -> validate Wi-Fi and Dexcom -> user confirms -> persist**. A failed test or cancellation restores the previous settings. Removing credentials explicitly stores an empty configuration first, so an older private firmware cannot silently restore embedded defaults.

## microSD and sound resources

The software mounts the card without automatic formatting. CSV records and event logs may contain health information and must remain private. The six optional language banks reside under `glycemie/audio/voice/`; French clips sit at that root and the other languages use their own subfolders. Audio playback falls back to a beep when prerecorded speech is unavailable.

## Developing safely

Start from [the developer walkthrough](DEVELOPER_WALKTHROUGH.md), then run the host regression suites and the full ESP-IDF build. The host tests use simulated hardware: they establish useful invariants but do not certify clinical accuracy or reproduce an untouched factory-board installation.

Never modify third-party copyright notices or redistribute personal NVS, glucose-history dumps, serial logs, or credentials with a Release. Read [third-party notices](THIRD_PARTY_LICENSES_EN.md) and [known limitations](LIMITATIONS_EN.md) before distributing a build.
