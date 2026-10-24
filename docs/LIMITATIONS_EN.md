# Known limitations, safety and validation scope

This page distinguishes **software tests**, **historical observations on the family's board**, and **features not validated on unused hardware**. It is part of the project's public technical documentation.

## Medical and regulatory limitations

WaveShare Glycémie is an **experimental supplementary display**. It has no clinical validation or medical-device certification. Its regulatory classification has not been established; a specialist assessment would be necessary before claiming conformity for regulated medical use.

It cannot replace the official Dexcom receiver, application, or alarms. **Do not make treatment decisions from this display.** Thresholds are user-configured choices, not medical advice. Infusion-set reminders are convenience features, not prescriptions. The project is provided without warranty under its applicable licences.

## Dexcom Share dependency

Dexcom Share is not a formally documented public API. Changes on Dexcom's servers may interrupt this client without warning. A supported phone, an active Share configuration and Internet access are required for new readings. Only the European `ous` account region was exercised with real credentials during development; additional regions are implemented but were not similarly validated.

The authentication guard reduces repeated rejected logins, but a new connection should not be retried blindly after an account error. A network outage or a stale reading must remain obvious on the screen.

## Deliberately unsupported features

- Glucose values are displayed in **mg/dL**, not mmol/L.
- The public build **does not listen for voice commands**. Multilingual prerecorded announcements and alert beeps are separate features.
- There is no Nightscout integration, Dexcom Clarity/v3 API, automatic sensor-insertion detection, AI meal analysis or extra statistics page.
- The former dedicated STATS page was removed; the Today page provides daily summaries.
- Firmware updates are performed over USB, not over-the-air.
- Existing CSV fields and historical debug output can remain in French for format compatibility. This is independent of English-only developer comments and guides.

## Privacy and local storage

Stored account details use reversible NVS obfuscation, **not cryptographic encryption**. Physical flash access or local serial diagnostics may expose information. Removing credentials records an empty configuration to prevent an older personalized build from restoring them, but does not guarantee forensic erasure of every obsolete flash page.

USB microSD export is not protected by an additional local password. Keep the screen, card, flash backups and serial logs in a trusted environment. None of those files belongs in a public Git repository.

If NVS is full or incompatible, the firmware deliberately avoids automatically erasing user dates, stock and settings. Manual restoration from a verified backup may be required, and the display may be temporarily unavailable.

## Hardware and performance boundaries

The build targets an **ESP32-P4 v1.3/pre-v3 profile** and a specific Waveshare board; it is not a universal ESP32 image. Paths used to compile with ESP-IDF should contain ASCII characters only. An incompatible chip revision must not be flashed.

At 100% volume the tested analogue audio stage distorted; 85% was the clean setting used during measurements. A louder alarm would require evaluating a different speaker/enclosure or amplifier. The project does not guarantee audibility in all environments.

The locally patched `components/esp_driver_ppa` addresses an actual rotated-partial-rendering hang. Removing it can restore the fault. One memory crash was observed historically only under an extreme combined rendering and continuous-touch stress test; that test does not represent normal use.

## Verification actually performed

GitHub Actions has executed host-side C/Python tests, Windows application checks, and an ESP-IDF build of the public firmware. The build verification examines chip revision and partition constraints. Earlier work on the family's device included reconnection, audio, touch and microSD experiments.

**The complete installation on an unused, factory-fresh board has not been exercised end to end.** The account-setup wizard and its failure paths are covered by simulations; this is different from claiming real-world validation on every new board.

Host tests do not fully exercise the live Dexcom HTTPS service, FreeRTOS timing, all physical microSD fault conditions, or clinical alarm reliability. Reported rendering times, internal RAM availability and endurance results from September 2026 are historical observations, **not a benchmark rerun for each release**.

## Safe distribution policy

Do not include personal compiled credentials, NVS backups, real glucose logs, proprietary third-party assets without their terms, or uncensored screenshots. Preserve all Espressif, LVGL and font licences, keep WAV provenance separate from the MIT code licence, and describe the display as an experimental prototype.

See [Hardware](HARDWARE_EN.md), [Third-party licences](THIRD_PARTY_LICENSES_EN.md), and [the developer walkthrough](DEVELOPER_WALKTHROUGH.md).
