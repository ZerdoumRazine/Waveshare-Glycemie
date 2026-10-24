# Licences — project code and third party components

**Original contributions** of the project are licensed **MIT**
(see [../LICENSE](../LICENSE)). In particular, they are `main/`,
`tests/`, `tools/` et `docs/` ; Some of these files also contain
or adapt third party code, subject to **its own license**.

This document lists these components and mandatory entries.
**No third-party code is reallocated to the project. **

---

## 1. Third-party code *compiled with* project but **not versioned here**

These components are downloaded by the Espressive Component Manager at the first
compilation, to versions frozen in `dependencies.lock`. They don't exist in the depot.

| Component | Version | Licence | Origin |
|---|---|---|---|
| ESP-IDF | 5.5.5 | Apache-2.0 | Espressive |
| `waveshare/esp32_p4_wifi6_touch_lcd_7b` (BSP) | 3.0.1 | Apache-2.0 | Waveshare |
| `espressif/esp_lvgl_adapter` | 0.6.x | Apache-2.0 | Espressive |
| `lvgl/lvgl` | 9.5.0 | MIT | LVGL LLC |
| `espressif/esp_lcd_ek79007` | 2.0.2~1 | Apache-2.0 | Espressive |
| `espressif/esp_lcd_touch_gt911` | 1.2.1 | Apache-2.0 | Espressive |
| `espressif/esp_codec_dev` | 1.5.11 | Apache-2.0 | Espressive |
| `espressif/esp_hosted` | 1.4.x | Apache-2.0 | Espressive |
| `espressif/esp_wifi_remote` | 1.2.5 | Apache-2.0 | Espressive |
| `espressif/esp-sr` | 2.5.3 | Apache-2.0 (libraries and models: Espressive conditions) | Espressive |
| `espressif/esp-dl`, `esp-dsp`, `dl_fft`, `cjson` | see `dependencies.lock` | Apache-2.0 / MIT | Espressive |

> **voice recognition models** (WakeNet, MultiNet) are provided by Espressif
> with esp-sr and remain subject to its conditions. They're not redistributed here:
> are downloaded and packaged at the compilation.

## 2. Third-party code **in the filing**

### `components/esp_driver_ppa/` — Apache-2.0 (Espressive)

Copying the component `esp_driver_ppa` of**ESP-IDF v5.5.5** as amended by the application of
official Espressive patch « PPA freeze fix » (only one function,
`ppa_srm_transaction_on_picked()`). Original license headers are retained;
the amendment is described in `components/esp_driver_ppa/CORRECTIF.txt`.

**This code belongs to Espressif, not to the project.** It is included because the project does not
compile not properly without it and do not modify the installationESP-IDF.

## 3. Resources

### Montserrat police — SIL Open Font License 1.1

`main/fonts/Montserrat-Medium.ttf` et `Montserrat-Bold.ttf`, embedded in firmware and
by TinyTTF. The full text of the license is in `main/fonts/OFL.txt`, as
requires itOFL. Authors: Julieta Ulanovsky and contributors.

### Former Windows voice — `main/voice/voice_fr.pcm` : **optional and not distributed**

40 excerpts (1.25 MB, PCM 16 bits mono 16 kHz) generated **locally on a PC
Windows** with the French voice of the system (`System.Speech`, Microsoft). They're not played.
only by the historical console command `voice_old` : normal announcements come from the
microSD bank, and withdrawal without bank is a **bip**.

**Decision (16 October 2026):** these samples are an output of the synthesis engine of
Windows; their redistribution would fall under Microsoft's terms of use, that this
project has not established. The file is therefore **withdrawn from the published copy**, and the firmware
makes it **optional**:

- `main/CMakeLists.txt` does not board `voice/voice_fr.pcm` only if it exists and defines
  `GLY_VOICE_OLD_PCM` accordingly;
- without the file, the compilation succeeds (verified); `voice_old` simply replied that
  the old voice is absent from this compilation;
- to find him on his own Windows PC:
  `powershell -ExecutionPolicy Bypass -File tools\voice\gen_voice.ps1`
  then `python tools\voice\pack_voice.py`, and place it back in `main/voice/`.

The project does not claim any rights in this audio.

### MicroSD Voice Banks — Google Cloud Audio Files Text-to-Speech

The **2 376 announcements WAV** of the project (396 in each of the six languages)
produced with **Google Cloud Text-to-Speech**, voice `Chirp3-HD-Aoede`from
WaveShare Glycemia phrases. The Google engine is **not** included:
the device only reads the audio files on its microSD.

**Planned use and conditions:** Google officially indicates that
Audio files created with Cloud Text-to-Speech can be used in
** applications or media**, in accordance with Google Cloud conditions
and applicable laws:

- [Google Cloud — Text-to-Speech, Basic Concepts](https://docs.cloud.google.com/text-to-speech/docs/basics)
- [Google Cloud — limitations and usage of audio files](https://docs.cloud.google.com/text-to-speech/quotas)
- [General Google Cloud Terms and Conditions](https://cloud.google.com/terms)

The vocal package is distributed **as an ad resource for this display**,
and not as an independent library of voice synthesis. Distribute
these WAV with the project does not transfer any rights to the models, services,
Google brands or voices; the project does not propose any voice cloning,
no training or competing system.

**The licence MIT of code NE covers PAS files WAV products by Google
Cloud Text-to-Speech.** Users remain responsible for compliance
the conditions applicable to the service and the laws of their jurisdiction.
The project is independent of Google, without affiliation or approval.

The version `v0.2.0` includes separate Release resource code
`glycemie-microSD-v0.2.0.zip`, containing only WAV necessary,
their tree and a record of provenance FR/EN.
It does not include blood glucose logs, identifiers, or gross blood sugar caches.
generation. Each user can also recreate their own ads
with the generator and its Google account.

What is provided in the repository to rebuild them:

- `assets/banque_vocale_manifeste.csv` — the list of sentences (filename, text,
  duration, size, CRC32) ; this text is that of the draft;
- `tools/voice/gen_voice_google.py` — the generator (project code, license MIT), which
  rebuilds the bank with a Google Cloud account;
- `tools/voice/sd_voice_upload.py` — copying on microSD via cable USB-Series.

Anyone who rebuilds the bank does it with **his own** Google Cloud account and under the
Google conditions. Indicative cost noted at the time: 11,181 characters, i.e., $0.34
at the price Chirp 3 HD, in the monthly free.

Without a voice bank, the device works: ads are replaced by a beep.

## 4. Third-party code included in this project

Two passages of official examples and a set of protocol constants come from projects
third. Their origin and license are written **in the relevant files**; this table
Summary.

| Origin | Licence | What's happening | Where it's quoted |
|---|---|---|---|
| Example Waveshare `09_lvgl_demo_v9` (© Espressive) | Apache-2.0 | the screen initialization block (`bsp_display_cfg_t`, `bsp_display_start_with_config`) | `main/app_main.c` |
| Example Waveshare / Espressive `05_wifistation` | Apache-2.0 | the Wi-Fi start-up skeleton (event managers, event group waiting) | `main/wifi_manager.c` |
| [pydexcom]](https://github.com/gagebenne/pydexcom) — © Gage Benne | MIT | Dexcom protocol constants Share: service addresses, application identifiers, pathsAPI | `main/dexcom_share.c` |
| `components/esp_driver_ppa/` (© Espressive, ESP-IDF 5.5.5) | Apache-2.0 | copied component, a modified function | headers SPDX files + `CORRECTIF.txt` |

Trend names returned by theAPI Dexcom (`Flat`, `DoubleUp`...) are **values of
protocol**: every client must use the same channels.

The rest of the code — interface, alerts, consumables tracking, night mode, voice, assistants
connection, microSD storage, tests — has been written for this project and is published under license MIT
(see `LICENSE`). Other clients Free Dexcom were consulted during design; a
automatic code comparison found **no files or functions included**, and they
therefore are not cited here. The graphical interface is original.

### If you redistribute

Apache-2.0 requests that copyright and license statements be retained, and that the
changes: it is done in the files themselves and in
`components/esp_driver_ppa/CORRECTIF.txt`. The full text of the Apache-2.0 license
is in [`components/esp_driver_ppa/LICENSE`](../components/esp_driver_ppa/LICENSE).
MIT request to retain copyright:
She's in `main/dexcom_share.c` for pydexcom, and in `LICENSE` for the project code.

## 5. What is not published

By choice, and to protect personal health data:

- Wi-Fi and Dexcom identifiers (`private/device_secrets.h`) ;
- backups of the flash and microSD of the personal device;
- firmwares compiled **from the personal folder** — identifiers are included;
  the published programme is compiled from these sources without any identifier;
- development logs and actual glycemic history;
- Voice banks: too large for a deposit, they are provided **except**
  (see section 3).

See [PUBLICATION_GITHUB_EN.md](PUBLICATION_GITHUB_EN.md).

## 6. If you redistribute this project

1. Keep `LICENSE` et `main/fonts/OFL.txt`.
2. Keep Espressive headers and file `components/esp_driver_ppa/LICENSE`.
3. Do not redistribute compiled firmware containing identifiers.
4. Check the conditions for any generated audio you add.
