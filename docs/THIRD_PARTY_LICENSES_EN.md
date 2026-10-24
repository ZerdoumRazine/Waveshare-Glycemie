# Licences, attributions and third-party resources

The original contributions of WaveShare Glycémie are published under the [MIT licence](../LICENSE). **This does not relicense any third-party code, font, voice model or audio recording.** Preserve the corresponding authors, copyright notices and licence texts when redistributing the project.

## Components downloaded during a build

The ESP-IDF component manager resolves versions recorded in `dependencies.lock`. The following components are dependencies, not code claimed as the author's own:

| Component | Reference version | Licence family |
|---|---|---|
| Espressif ESP-IDF | 5.5.5 | Apache-2.0 |
| Waveshare ESP32-P4 7B BSP | 3.0.1 | Apache-2.0 |
| Espressif LVGL adapter | 0.6.x | Apache-2.0 |
| LVGL | 9.5.0 | MIT |
| EK79007 display and GT911 touch drivers | See lockfile | Apache-2.0 |
| Espressif codec, ESP-Hosted and Wi-Fi remote | See lockfile | Apache-2.0 |
| ESP-SR | 2.5.3 | Espressif terms apply to libraries and models |
| Espressif DSP/DL utilities and other libraries | See lockfile | Their respective licences |

ESP-SR recognition models (WakeNet/MultiNet) are obtained from Espressif during optional builds; they are not presented as original project content. Voice-command recognition is disabled in the default public firmware.

## Third-party source included in the repository

The local `components/esp_driver_ppa/` directory is derived from the Espressif component in ESP-IDF 5.5.5. It contains an adaptation of an Espressif PPA freeze fix. Keep its original source headers, the complete [Apache-2.0 licence](../components/esp_driver_ppa/LICENSE), and the change description in `CORRECTIF.txt`. **This driver belongs to Espressif.**

Other attributed adaptations include the display-initialization pattern from the Waveshare/Espressif LVGL example in `main/app_main.c`, a Wi-Fi startup pattern in `main/wifi_manager.c`, and documented Dexcom Share protocol constants derived from [pydexcom](https://github.com/gagebenne/pydexcom) by Gage Benne (MIT). Each source file retains the relevant attribution. Protocol-required trend strings and API field names are not arbitrary text that can be translated.

## Fonts

The embedded Montserrat Medium and Bold font files are licensed under the **SIL Open Font License 1.1**. See [main/fonts/OFL.txt](../main/fonts/OFL.txt). Credit Julieta Ulanovsky and contributors. Do not distribute font files without their applicable licence.

## Speech audio generated with Google Cloud

The optional microSD speech banks contain **2,376 WAV files**, 396 for each of six languages, generated with Google Cloud Text-to-Speech (Chirp 3 HD Aoede voices). The device plays stored recordings; it does not ship a Google speech synthesis model.

Google documents usage of generated speech audio in applications and media, subject to Google Cloud terms and applicable law. Consult the [Text-to-Speech documentation](https://docs.cloud.google.com/text-to-speech/docs/basics) and [Google Cloud terms](https://cloud.google.com/terms).

**The MIT source-code licence does not apply to these WAV files.** The files are offered as the display's application audio resources; the project does not claim rights in Google's voice models, services or branding, and it is not affiliated with or endorsed by Google. Each language-bank archive includes a provenance notice. Users can alternatively generate files with their own Google Cloud account using `tools/voice/gen_voice_google.py`.

The optional historical Windows PCM file `main/voice/voice_fr.pcm` was produced through Microsoft System.Speech. Redistribution rights were not established, so **this file is excluded** from the public sources and builds. A missing file disables only the old console voice test; ordinary WAV announcements and fallback beeps do not depend on it.

## What is not distributed

Personal Wi-Fi and Dexcom credentials, secret-bearing private builds, NVS dumps, full flash backups, patient glucose CSVs, serial debug logs, and private microSD contents must not be uploaded. A sanitization script and GitHub checks reduce risk, but a human review is still required when preparing a release.

## Redistribution checklist

1. Retain the project's MIT `LICENSE` and all source-level third-party attribution.
2. Retain the PPA driver Apache-2.0 licence, original headers and modification notice.
3. Retain `main/fonts/OFL.txt` alongside redistributed Montserrat fonts.
4. Keep Google-generated WAV provenance separate from the licence for original source code.
5. Never distribute a firmware binary containing user credentials or medical histories.

This page documents provenance and known licence conditions; it is not a legal opinion.
