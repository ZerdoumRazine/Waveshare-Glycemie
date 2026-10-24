# Limitations, precautions and what is not tested

Voluntary candid document. It distinguishes what is **verified**, which is
** reported** (measured in September 2026 but not since) and
**Never** was tested.

---

## 1. Medical warning

This project is a ** complementary family display**, personal and experimental.

- **This experimental device is neither certified for medical use nor clinically validated. **
  Its regulatory ranking has not been determined; a specialized analysis would be
  necessary before any claim of conformity or large-scale distribution.
- It must not be used for **no therapeutic decision**.
- It does not replace **n** the official Dexcom app, **n** its alarms, **n** any
  certified monitoring system. Safety alarms remain the sensor and
  The phone.
- Alert thresholds are user choices, not medical recommendations. The
  recall of the catheter (48 h / 72 h) is a practical reminder, not a prescription.
- No guarantee of any kind (see [LICENSE](../LICENSE)).

## 2. Dependence on Dexcom Share

- **Share is not a API ** Client relies on behaviour
  observed and on the work of *pydexcom*. A Dexcom-side change can interrupt the
  service without notice.
- Only the region **`ous` (Europe)** was actually used. The region `us` exists in
  the code but ** was never tried**.
- The channel depends on the **telephone**: if the Dexcom application is closed, offline, or if
  sharing is disabled, screen displays "Data ANCIENNES — what is the
  behaviour, but this is not a device failure.
- The authentication lock prevents any loop from attempting, at the price of a
  manual authorization at first start (`dexcom_auth`).

## 3. Functions absent (voluntary)

- **No mmol/L** : the display is in mg/dL Only.
- **Speech recognition and microphone listening are excluded from the public program.** Spoken ads and alerts remain available in six languages from banks WAV MicroSD. The old voice recognition variant is experimental and is not provided in the public program.
- The microSD log, the files CSV and the console remain in French, so that the
  old and new records remain comparable.
- Portuguese voice is **Brazilian**: Google does not propose a voice Chirp 3 HD in Portuguese
  European.
- Voice banks are **not** in the deposit (-) 22 MB per language, audio produced by a
  third party service): `tools/voice/gen_voice_google.py --lang <code>` Regenerates them.
- **New screen configuration not fully validated**: touch assistant and configuration USB are present, but the full path of input, backup and restart has been checked in simulation, not yet end to end on a blank card.
- **No page STATS**: it existed until K14 and has been removed** from K15. It does not
  must not be recreated. AUJOURD'HUI is the only page of statistics.
- **No Nightscout**, noAPI Dexcom v3 / Clarity, no automatic detection of
  sensor installation (theAPI Share does not provide session information).
- **No artificial intelligence, no food analysis, no dashboard
  These ideas may have been mentioned, but they have never been implemented.
- No network update (OTA) : flash is made by USB.

## 4. Confidentiality and internal memory

- Identifiers are stored in space NVS of the apparatus with a
  ** Reversible scrubbing, not cryptographic encryption**. Access
  physical flash or console USB may expose data.
- "Clear IDs" removes accesses from the active configuration
  and keeps an empty marker to prevent their reappearance from a
  old custom program. It does not guarantee erasure
  medico-legal of the old flash pages.
- If NVS is full or incompatible, the program must not
  automatically erase dates, stocks and settings: recovery
  manual from backup may be necessary. In this situation,
  the additional display may become unavailable.
- Exporting history on microSD via USB do not ask
  local authentication. Keep the device and its card in a
  trust environment.

## 5. Compilation constraints

- **The path of the project must be purely ASCIIWith an accent, the compilation fails
  in `ccache`, in `ldgen.py` and publishing links. Spaces are accepted.
- `IDF_PATH` et `IDF_TOOLS_PATH` must also be in a way ASCII.
- The project targets a **ESP32-P4 rev1.3**. A binary compiled in profile rev3.x **starts
  Not** on this map. Three guards forbid him, but one card from another
  revision would require a thoughtful adaptation of the profile.
- The compilation is in `-Werror` : a warning stops the construction.

## 6. Equipment

- **Audio**: the analog limit is reached at 100% volume (measured saturation,
  3rd harmonic to −11 dB for useful +1.3 dB). **85 % is the clean maximum.** For a true
  alarm level, a loudspeaker should be more sensitive in closed enclosures, or even a
  external amplifier — assembly not performed or validated.
- **Known stability reserve**: under extreme artificial load (24 zones)
  Redesigned every 20 ms **and** continuous supports), a memory crash was observed
  **once** in the partial rotation path of the adapter LVGL. Restarting
  automatic in seconds, settings kept. Never observed in normal use, nor
  under each of these charges taken separately.
- The component `components/esp_driver_ppa` contains the fix to the interface gel.
  **Reintroduces a reproducible gel. **
- The firmware of theESP32-C6 is never modified; its version remains "unknown" because it does not
  does not answer the version request.

## 7. Voice commands — experimental variant not distributed

The **public version does not offer voice command recognition** and does not open the microphone for this service. The historical variant, based on ESP-SR, was dismissed because of its insufficient reliability and material needs. Advertisements and sound alerts remain independent and available with microSD banks in all six languages.


## 8. What is covered by automatic tests

Run and passed the **2026-10-15** on this machine:

| Test | Scope | Result |
|---|---|---|
| Compilation ESP-IDF 5.5.5 | complete draft | successful, 6,129,952 |
| `tools/verif_avant_flash.py` | partitions, profile, image, no ID | 23 / 23 |
| `tests/hote/test_glucose_math.c` | age, freshness, cadence, variation, next measurement, trends, history | 38 / 38 |
| `tests/hote/test_consommables.c` | sensor and catheter deadlines, recalls, anti-repetition, persistence NVS after restart, stock | 65 / 65 |
| `tests/test_depot.py` | structure, paths, secrets, partitions, dependencies, captures | see output of script |

## 9. What is **not** covered by automatic tests

- **Dexcom Share**: no automatic test, neither offline nor online. The client HTTPS
  is not insolable without ESP-IDF, and there is no Share test server.
- **Network connection**: historically validated on the map
  (`wifi_test_disconnect`, `dexcom_test_session`, reconnexion in 6 s), **not** by a test
  automatic.
- **microSD**: deduplication, removal and winding historically validated on the map
  (`sd_check`, `sd_repair`, `sd_test_eject`), **not** by automatic test — code
  depends on the pilot SDMMC.
- **Graphic components**: LVGL It's not possible on PC here. Checked by catch
  real screen and control `ui_stress` on the map.
- **Alerts and repetitions**: the repetition logic depends on FreeRTOS and the
  audio engine; it has been validated on the map with `alert_sim`Not in the host test. Alone
  the part "recalls of consumables and anti-repetition" is covered in the host test.
- **Audio and voice**: validated in the ear and by measuring the microphone of the map.

## 10. Measures reported, not repeated

The performance and memory figures cited in the documentation (112 ms of
Welcome refreshment, 94–104 Ko de RAM Internal, 22 MB of PSRAM, stamina of
31 minutes at 4 hours without error) are the **reduced September 2026** on the actual map. They
are reported as such, as a historical reference: **they have not been redone**
when the deposit is made.

## 11. Voluntary transactions not carried out

When this deposit was made, no transaction was made to change the card:
no flash, no erasure, no writing NVS, no microSD formatting, no
modification of firmware of theESP32-C6, no eFuse, no Secure Boot or Flash
Encryption. The map was only requested in reading and demonstration mode.
