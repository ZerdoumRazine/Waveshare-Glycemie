# Developer walkthrough: WaveShare Glycémie

This guide follows the actual firmware architecture from startup to rendering, maintenance, and release packaging. It is written for someone learning the project for the first time, not as a line-by-line restatement of the C source.

> **Safety scope:** WaveShare Glycémie is an experimental supplementary glucose display, not a certified medical device. Official Dexcom monitoring and alarms remain the source of truth. Its values and warnings must not be used to make treatment decisions.

## 1. Start with the hardware and repository

The target board is the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**, including the ESP32-P4 pre-v3 hardware profile, the Wi-Fi coprocessor, the touchscreen, a speaker, and optional FAT32 microSD storage. The project uses **ESP-IDF 5.5.5**, LVGL, and the Waveshare BSP. The dependency lockfile records the resolved components.

Read `README.md` for a product overview, `docs/BEGINNER_TUTORIAL_EN.md` for first-time installation, and `docs/CODE_GUIDE_EN.md` for module navigation. The C firmware lives in `main/`; reusable tools are in `tools/`; hardware-independent regression tests are in `tests/`. The copied Espressif PPA driver under `components/` retains its upstream copyright and Apache-2.0 licence.

## 2. Boot sequence and shared state

**Entry point:** `main/app_main.c`.

The firmware initializes persistent state and hardware before starting its background services and touchscreen. This ordering matters: saved insertion dates, preferences, and credentials must be available before any UI or network code relies on them.

Key modules:

- `main/settings.c` owns persistent preferences such as volume, thresholds, language, brightness, and night mode.
- `main/app_state.c` holds the current connection and glucose snapshots. Consumers read a coherent state instead of each page independently contacting Dexcom.
- `main/net_services.c` handles network-related services, including time synchronization.
- `main/wifi_manager.c` performs Wi-Fi scans, connection attempts, and recovery.

**Invariant:** a routine reboot must not be interpreted as a new sensor insertion or a request to reset user preferences. If NVS initialization fails, the application must report an error rather than silently erasing the user's dates and configuration.

## 3. Where glucose data comes from

```text
Dexcom G7 sensor
    -> phone running the official Dexcom application
    -> Dexcom Share servers
    -> Wi-Fi and HTTPS on the Waveshare
    -> dexcom_share.c
    -> glucose_service.c
    -> app_state.c
    -> on-screen pages, alert evaluation, and microSD history
```

`main/dexcom_share.c` implements the read-only Share client and parses server responses. `main/glucose_service.c` coordinates refreshes, updates, stale-data handling, and demo mode. `main/glucose_math.c` contains arithmetic suitable for isolated tests.

**Invariant:** a network failure is not a glucose measurement. An old reading must remain marked as old; missing intervals must not be presented as newly measured values.

## 4. Authentication and first-run configuration

**Relevant code:** `main/credentials.c`, `main/dexcom_guard.c`, `main/ui_setup.c`, and `tools/assistant_config.py`.

1. The user enters Wi-Fi and Dexcom details through the touchscreen wizard or the Windows USB configuration application.
2. The software starts a trial without replacing the last known working configuration. `credentials_trial_begin()`, `credentials_trial_set_wifi()`, and `credentials_trial_set_dexcom()` represent this provisional stage.
3. `glucose_service_test_request()` initiates a connectivity check. The new details are accepted only if validation succeeds and the user confirms the action.
4. `credentials_trial_commit()` writes the accepted settings to persistent storage. `credentials_trial_cancel()` restores the previous state if the trial is abandoned.
5. The Dexcom authentication guard records attempted login state to prevent repeated failed logins from locking an account after restarts.

The public firmware must not contain personal Wi-Fi or Dexcom credentials. NVS obfuscation is **not cryptographic encryption**; physical access to device flash or its serial interface can expose information. The system should not be described as a secure medical-data vault.

**Invariant:** clearing credentials reports success only after an explicitly empty configuration is saved. Failing to save must not make the interface claim that the data was removed.

## 5. How a reading reaches the touchscreen

The navigation layer is `main/ui_main.c`. The five primary pages have separate implementations:

| Page | Source | What to inspect |
|---|---|---|
| Home | `ui_home.c` | Large value, trend arrow, connection state, reading age |
| History graph | `ui_graph.c` and `ui_chart.c` | Time windows, plotted points, touch exploration |
| Today | `ui_today.c` | Daily statistics and incomplete coverage |
| Consumables | `ui_consumables.c` | Sensor/catheter changes and stock controls |
| Settings | `ui_settings.c` | Language, audio, alerts, connections, and confirmation dialogs |

`ui_common.c` contains reusable LVGL elements; `ui_alert.c` manages warning overlays; `trend_arrow.c` renders trend symbols. The pages consume shared state rather than running separate Share requests.

**Invariant:** preserve the semantics of unknown trends, missing data, and stale values when changing colors, text, or charts.

## 6. Alerts and audio

`main/alerts.c` selects alert priorities, repetition rules, acknowledgment, and re-raising. `main/audio_out.c` generates tones and drives audio output. `main/voice.c` plays prerecorded WAV announcements stored on the microSD.

The standard public firmware **does not run voice-command recognition**. The historical experimental recognition code remains isolated in `voice_cmd.c`, with a disabled-build implementation in `voice_cmd_stub.c`.

**Invariant:** the unit's tones and speech are supplementary. Do not suppress the official Dexcom alert channel, and do not claim that a successful test beep certifies a complete medical warning system.

## 7. Six language support

`main/i18n.c` stores interface strings and selection logic; `tools/voice/langues_voix.py` defines spoken phrases and numeric announcements. The available languages are French, English, Spanish, German, Italian, and Brazilian Portuguese.

An English-only developer repository **does not mean an English-only screen**. Do not translate the French UI string table, change audio filenames, or remove a language bank while cleaning source comments. The device still uses the French speech bank as an optional fallback.

Audio WAV files were generated with Google Cloud Text-to-Speech and have licensing and provenance notes separate from the MIT licence for the project's own code. See the voice tutorial and third-party notices.

## 8. MicroSD and consumable persistence

`main/sd_storage.c` manages the card, CSV history, logs, and file operations. `main/sensor_session.c`, `main/consumables.c`, and `main/stock.c` manage insertion dates, reminders, and local counts.

**Invariants:**

- Do not automatically format a card containing history.
- Keep archived glucose readings and diagnostic logs out of the public repository.
- A repeated button event must not decrement stock twice.
- Changes to dates and thresholds must survive ordinary restarts.

## 9. How to make a safe code change

1. Create a separate Git branch and make the smallest focused change.
2. Explain **why** a non-obvious decision exists in a short English comment near the relevant function. Avoid narrating every assignment.
3. Keep executable code and comment-only cleanups in different pull requests so they are easy to audit.
4. Run the C host tests and Python regression suites. They deliberately use fake data and simulated ESP-IDF services.
5. Let GitHub Actions compile the actual ESP32-P4 firmware with the pinned ESP-IDF environment. A successful host test does not replace an ESP-IDF build.
6. Review the diff: confirm that user-facing language strings, credentials, resource manifests, and third-party notices have not been changed unintentionally.
7. Merge after the checks pass. When behavior changes, prepare a new versioned firmware download; older Release archives do not update themselves.

Useful commands from the repository root:

```bash
python tests/run_tests.py
python tests/test_depot.py
python tests/test_assistant_config.py
python tests/test_pc_app.py
python tests/test_sd_voice_upload.py
python tests/test_recuperation_microsd.py
python tests/test_pack_release.py
```

Some tests require Windows or an appropriate simulator; GitHub Actions provides the platform-specific jobs. `tests/hote/` supplies mocked C dependencies for PC-side test execution.

## 10. Comment style and maintenance notes

Prefer comments such as:

```c
/* Save an explicitly empty credential record before clearing
 * auxiliary authentication state. A restart must not restore
 * credentials embedded by an older private firmware build. */
```

Do not write comments that merely repeat the statement beneath them. Keep comments factual, use English for technical documentation, and preserve the original licences of copied components.

## 11. Limitations to communicate publicly

First-time setup on an unused, factory-fresh board has not been exercised end to end, even though the code has passed compilation and simulation checks and the family display was operated on the target hardware. The software depends on Dexcom Share and Internet availability. Only documented hardware configurations have been verified. The project does not claim clinical validation or medical-device certification.

These limitations are part of the engineering documentation, not a reason to imply that the project has features or verification results it does not actually have.
