# WaveShare Glycémie v0.2.0 — pre-release

This experimental pre-release is for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** board with ESP32-P4 revision **1.3 (pre-v3)**. Other board revisions have not been validated.

## What changed

- More cautious NVS initialization and failure handling to avoid silently losing saved settings.
- Safer Wi-Fi and Dexcom credential removal: verify that an empty credential record is actually stored before clearing auxiliary authentication state. Previous credentials remain available if persistence fails, and compiled-in credentials from older private builds must not silently reappear.
- Additional C and Python regression tests, including simulated storage failures, and automated ESP-IDF builds for the target board.
- A USB firmware installer that verifies checksums and the target chip revision and requires explicit user approval.
- Improved developer documentation, source-level comments and third-party licensing records.

## Downloadable release files

| File | Contents |
|---|---|
| `glycemie-installation-ecran-v0.2.0.zip` | Bootloader, partition table, firmware, USB installer and integrity checks |
| `GlycemieConnexion.exe` | Windows USB setup utility for Wi-Fi and Dexcom credentials |
| `glycemie-microSD-v0.2.0.zip` | Optional recordings in all six supported languages |
| `SHA256SUMS.txt` | Checksums for the firmware and Windows application downloads |
| `SHA256SUMS-voix.txt` | SHA-256 checksum for the microSD audio package |
| `SOURCE_COMMIT.txt` | Identification of the source commit used for the compiled release |

GitHub also provides a source-code archive for the tagged version.

## Speech recordings and licences

The separate microSD archive contains **2,376 WAV recordings**, 396 each for French, English, Spanish, German, Italian and Portuguese. If the recordings are absent, the display still provides alert beeps. See the [voice setup guide](VOICE_TUTORIAL_EN.md) for copying and verification instructions.

The speech recordings were generated with **Google Cloud Text-to-Speech**. The MIT source-code licence does **not** cover these audio files, and no ownership of Google voice models or trademarks is claimed. This independent project is not affiliated with or endorsed by Google. Use and redistribution remain subject to Google's applicable terms and the law. Consult the [third-party licence notes](THIRD_PARTY_LICENSES_EN.md) and [Google Cloud documentation](https://docs.cloud.google.com/text-to-speech/docs/basics).

## Validation and limitations

Firmware builds, software regressions and packaging checks passed for the release as described in its original build records. Installation, updates and everyday functionality were exercised on an existing project board, **but the complete first-time configuration of a factory-fresh board has not been validated end to end on physical hardware**. Software simulations do not eliminate this limitation.

**This is an experimental supplementary display, not a certified medical device.** It can show delayed, missing or incorrect glucose readings. Do not use it to make treatment decisions or as a replacement for official Dexcom monitoring or alarms.

Changes to documentation made after v0.2.0 was packaged do **not** retroactively modify the already-uploaded binary release assets.
