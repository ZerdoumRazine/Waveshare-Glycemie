# Dexcom G7 family glucose display — Waveshare ESP32‑P4 7″

> ⚠️ **Experimental personal project. This project is not certified for medical use.**
> It must not be used for treatment decisions and does not replace the official Dexcom
> application, its alarms, or any certified monitoring system.

*Documentation en français : **[README_FR.md](README_FR.md)** (version de référence, plus détaillée).*

A permanent wall display showing a Dexcom G7 reading large enough to be read from several
metres away: value, trend arrow, change since the previous reading, data age, 24 h chart,
daily statistics, consumable tracking and spoken alerts in six languages.

Data path: G7 sensor → phone → **Dexcom Share** servers → Internet → display.
The device never talks to the sensor directly.

![Home page](screenshots/01_accueil.png)

**Initiated and directed by Mohamed-Razine Zerdoum.** Hardware: **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**
(7″ touch display, 1024 × 600), microSD card, speaker. Code under the MIT licence; third-party
components keep their own authors and licences.

### Where to start

| I want to… | Go to |
|---|---|
| 🔧 **Set up the device** | [Beginner's tutorial](docs/BEGINNER_TUTORIAL_EN.md) — buy, connect, install, configure |
| ⬇️ **Download the programs** | [Releases](../../releases) — screen firmware, Windows app, sounds |
| 🔊 **Prepare the microSD** | [Voice tutorial](docs/VOICE_TUTORIAL_EN.md) — six languages, with or without a card reader |
| 🧩 **Understand the code** | [Architecture](docs/ARCHITECTURE_FR.md) (French) and the [`main/`](main/) folder |

> **Not yet tried end to end on a brand-new device.** Installation, update, reconnection,
> announcements and the setup wizard were verified on the author's own device; first-time setup of
> a blank device (entering new credentials, saving, restarting) has only been validated in
> simulation. See [docs/LIMITATIONS_FR.md](docs/LIMITATIONS_FR.md).

*Real screenshot taken from the board, in demo mode (fictitious data).*

---

## Features

- **Glucose** — Dexcom Share (European “OUS” account), one reading every 5 minutes, value in
  mg/dL at 320 px, trend arrow, delta, data age (fresh < 6 min, late 6–10 min, stale ≥ 10 min).
- **History** — 288 readings in RAM (24 h), 3 h / 6 h / 12 h / 24 h chart with touch
  inspection, daily CSV on microSD, back‑fill of late readings, gap‑tolerant resume.
- **Today** — time in range, average, min, max, coverage, time low/high, low/high episodes.
- **Consumables** — G7 sensor: due = insertion + exactly 10 days, reminders 24 h and 2 h
  before. Infusion set: 48 h or 72 h, single reminder 2 h before. Dates and reminders are
  stored in NVS, so **a reboot or a rebuild never silently resets them**. Local stock count
  per item, never negative, decremented once per change.
- **Alerts** — urgent‑low / low / high / urgent‑high thresholds (55 / 70 / 250 / 300 mg/dL by
  default, adjustable), “no data” alert, rising volume while a critical alert is unacknowledged,
  urgent‑low audible even when muted, on‑screen panel, repetition guard.
- **Voice** — announcements played from a WAV bank on the microSD, one per language, falling
  back to a beep. Alert sounds always work, with or without a bank. Local voice *commands* are
  **not part of the shipped build** (see Limitations); every action is reachable by touch.
- **Language** — interface **and spoken announcements** in **6 languages** (French, English, Spanish,
  German, Italian, Portuguese), selectable in the settings page, applied immediately and stored in
  NVS. Each language has its own speech bank on the microSD, in the same voice; if one is missing,
  the French bank takes over.
- **Night mode** — simplified night screen on a schedule, reduced brightness, a touch brings
  the normal interface back for 30 s, alerts still take priority.
- **System** — Wi‑Fi through the ESP32‑C6 co‑processor (ESP‑Hosted) with reconnection, SNTP
  time with the French timezone, FAT32 microSD that is never auto‑formatted, persistent
  settings, task watchdog.
- **Setup without rebuilding** — on first start a touchscreen wizard asks for the language, the
  Wi‑Fi network and password, the Dexcom Share account and region, **tries the connection**, and
  stores the credentials only after success and confirmation: working credentials are never
  replaced by invalid ones. Everything can be changed later in SETTINGS → CONNECTION, or from the
  PC with the `GlycemieConnexion.exe` Windows app over USB.
- **Demo mode** — a button in the wizard, a switch in the settings, or
  `demo normal|low|verylow|high|veryhigh|gap|stale` on the serial console: 24 h of **fictitious**
  data with a “MODE DÉMO” banner, so the device can be installed and tried **without a Dexcom
  account**.

Five pages only: home, chart, today, consumables and settings.

---

## Hardware

- Waveshare ESP32‑P4‑WIFI6‑Touch‑LCD‑7B
  ([docs](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B) ·
  [repo](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B))
- ESP32‑P4 **rev 1.3** — the pre‑v3 profile is mandatory (`config/esp32p4_rev1_3.defaults`);
  `main/CMakeLists.txt` refuses to build a configuration resolved as rev 3.x
- 7″ 1024 × 600 MIPI‑DSI (EK79007), GT911 touch, 32 MB flash, 32 MB PSRAM
- ESP32‑C6 Wi‑Fi co‑processor, ES8311 audio, microSD, USB‑C with a CH343 USB‑UART bridge

## Software stack (pinned)

ESP‑IDF 5.5.5 · Waveshare BSP 3.0.1 · LVGL 9.5.0 · esp_wifi_remote 1.2.5 ·
esp_hosted 1.4.x · ESP‑SR 2.5.3. Resolved versions are committed in `dependencies.lock`.

---

## Build

```bash
git clone <repo-url> waveshare-glycemie
cd waveshare-glycemie
copy main\device_secrets.example.h private\device_secrets.h   # then fill it in
idf.py set-target esp32p4
idf.py build
idf.py -p COM7 flash monitor
```

> ⚠️ **The project path must be pure ASCII** (no accented characters). ccache, `ldgen` and
> the linker fail otherwise. Spaces are fine.

Credentials live in `private/device_secrets.h`, which is git‑ignored. They are compiled into
the firmware, so **never publish `build/` or a compiled `.bin`**.

## Tests

```bash
python tests\run_tests.py           # host unit tests (gcc, clang or MSVC Build Tools)
python tests\test_langues_voix.py   # consistency of the six speech banks
python tests\test_depot.py          # repository consistency and no-private-data checks
python tools\verif_avant_flash.py   # 23 checks on the built image, before flashing
```

The host tests compile the real firmware sources (`glucose_math.c`, `app_state.c`,
`sensor_session.c`, `consumables.c`, `stock.c`); only NVS, FreeRTOS, microSD and network are
replaced by stubs.

---

## Documentation

The main beginner and voice guides are available in **English and French**. Most developer documentation is in French:

| Document | Contents |
|---|---|
| [docs/BEGINNER_TUTORIAL_EN.md](docs/BEGINNER_TUTORIAL_EN.md) | **Start here**: buy, connect, install, configure, copy the sounds, test, troubleshoot |
| [docs/VOICE_TUTORIAL_EN.md](docs/VOICE_TUTORIAL_EN.md) | **Spoken announcements**: install a language pack (10 min) or build your own audio |
| [docs/TUTORIEL_DEBUTANT_FR.md](docs/TUTORIEL_DEBUTANT_FR.md) | Same beginner's guide, in French |
| [docs/RETOUR_ARRIERE_FR.md](docs/RETOUR_ARRIERE_FR.md) | Back up the device, roll back, recover a screen that will not start (French) |
| [docs/TUTORIEL_VOIX_FR.md](docs/TUTORIEL_VOIX_FR.md) | Same guide, in French |
| [docs/INSTALLATION_FR.md](docs/INSTALLATION_FR.md) | ESP-IDF on Windows, build, flash, microSD |
| [docs/DEPANNAGE_FR.md](docs/DEPANNAGE_FR.md) | Troubleshooting, serial console, FAQ |

---

## Licence

Project code: **MIT** (see [LICENSE](LICENSE)). Initiated, designed and directed by
**Mohamed-Razine Zerdoum** for real family use; a large part of the code was produced with
automated assistance under his direction — see [CREDITS.md](CREDITS.md) (French). Third‑party
components keep their own authors and licences — see
[docs/LICENCES_TIERCES_FR.md](docs/LICENCES_TIERCES_FR.md).

Supplementary family display, not certified, provided without warranty. Treatment decisions
and safety alarms belong to the official devices and applications, and to medical follow‑up.
