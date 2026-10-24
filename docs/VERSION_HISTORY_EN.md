# Version History

The first versions (V1 to K19) were developed from **13 to 21 September 2026**.
The project then evolved in October 2026 with the translation of the interface (K20),
ads in six languages (K21) and the touch and Windows connection wizard (K22).

The **preversion GitHub v0.1.0** comes from K22. The family screen has been updated,
but **the installation and first full configuration on a new card**
remain to be validated on hardware. Subsequent changes to the code are
distinguished from this preversion and require a new compilation and testing.

> The measures mentioned (during cooling, RAM, PSRAM) are the historical ** records**
> September 2026 on the real map. They are reported as such and do not
> was re-established when the deposit was made.

---

## V1 — 2026-09-13 · first functional version

Complete chain established: screen EK79007, touch GT911, Wi-Fi via theESP32-C6, SNTP, customer
Dexcom Share native, display of value, trend and age, graphic.

Validated on the map: scan and Wi-Fi association, DHCP, DNS, HTTPS with verification of
certificate, Dexcom Europe authentication, reconnexion after Wi-Fi switch in 6 s,
resume after session invalidation, endurance 19 min without error.

Hard point resolved: exhaustion of the RAM during trade HTTPS
(`sdio_push_data_to_queue ... pkt_rxbuff`), corrected by placing large buffers and
allocations TLS en PSRAM.

## V2 — 2026-09-13 · alerts, microSD, audio

Background CSV daily on microSD, full alert manager, alert panel
fullscreen, audio ES8311 with corrected volume curve.

Endurance 14 min on actual data: 287 measurements read on start, readings
incremental every 5 minutes, capped cache at 288. RAM internal 123–133 Ko free.

## 2026-09-14 · deduplication of history

Cause of duplicates found: Dexcom horodate at **millisecond**, the CSV **second**.
Fixed: Reread the last line and compare to the second.
`sd_repair` cleaned 4 exact duplicates in the 13/09 file and didn't touch anything
other. No duplicate after restarting, simulated card removal, Wi-Fi or
invalidated session.

## 2026-09-14 · interface freeze — patch PPA

Reproducible task gel LVGL in `ppa_do_operation()` during rotation 180°
partial areas, diagnosed by core dump. Espressive Corrective « PPA hang » postponed in
a local copy of the component (`components/esp_driver_ppa`). Result: 2,322 support
10 min without frost (before: freeze on 3rd support). Permanent touch mapping:
`swap_xy = 0, mirror_x = 0, mirror_y = 0`.

## 2026-09-14 · French voice on microSD

Google Bank `fr-FR-Chirp3-HD-Aoede` **only once** generated on PC (373 files
WAV, 16 kHz mono), copied to the card **by the cable USB-series** without removing microSD,
with verification CRC32 file by file. Planned replica: onboard voice, then beep.

## Day2 — 2026-09-15 · night mode

Minimalist night screen according to a schedule, reduced brightness, touch that makes the interface
30 s, priority alerts, overview TEST MODE NUIT. Block NVS separated (`nuit`) for not
touch the block of other settings.

Physical validation by user on 2026-09-15: OK, RAPPEL DANS 30 MINindicator,
automatic closing, tactile navigation, scrolling settings, voice, volume.

## Step K / K10 — 2026-09-15 · consumables and voice commands

- Page **CONSOMMABLES**: G7 sensor and infusion site, single confirmation
  "now = departure time", no date entry.
- **Local voice controls** ESP-SR 2.5.3 (MultiNet7 `mn7_en`, WakeNet9L « Hello ESP »
  optional), partition `model` of 4 MB added **after** original partitions.
- Documented search:API Share does not return **no** sensor session information,
  and API Dexcom v3 / Clarity not suitable → local method assumed.
- Resolved difficulties: models beyond 16 MB not projectable in memory (read in
  PSRAM by an internal stack task), flash playback prohibited from a stack PSRAM,
  crashing on `clean()` a model never used.

Endurance 4 hours from 2026-09-17 (log 221): 0 restart, 0 freeze, 0 watchdog; RAM Internal
97 → 94 KB (min 68); Dexcom 50 readings without error; microSD 49 writings, 0 duplicates.
The fall in PSRAM first hour has been identified as the **glyphs cache
TinyTTF** (185 min then without any variation): **no leak**.

## K11 — 2026-09-17 · filter false voice detections

Real problem: the device "spokes on its own" several times a day.

The MultiNet7 score was determined by **disassembly of binary**:
`score = exp(log-probabilité ÷ (jetons + 2))`, order issued as soon as `score > exp(−2) ≈ 0,135`
(strict test). On 4 a.m.: 6 spontaneous announcements (0.14 to 0.20), all decoded "Glucose".

Minimum Corrective (only `voice_cmd.c` as amended):
"Glycaemia" accepted if `≥ 0,155` ; variant `GLoKbS` from "Glucose" **withdrawn** (real
0.16–0.19 and false 0.14–0.20 inseparable); variant `GLoKbZ` accepted if `> 0,205`.
Each detection is logged with a score of 4 decimals, and discards are counted
(`ignorees=` in `vcmd_info`).

Priority selected, at the request of the user: **Reliability before scope**.

## K12 — 2026-09-17 · visual overhaul 2.5D 'dark glass'

Recast **visual only**: no modified logic. gradient bottom, glass cards,
halos, freshness ring, trend arrow drawn, page AUJOURD'HUI.

Traps LVGL 9.5 documented on this occasion: a shadow is drawn only**around** of a
non opaque object (a hollow halo gives a contour); `lv_buttonmatrix_set_button_width`
accept only 1 to 15.

Processed performance regression: complete refreshment was increased from 75–90 ms to
325–366 ms. Each shadow costs 15 ms per map to 24 px → shadows reduced to 10 and 16 px,
cards made opaque. Measured result: ACCUEIL 162–170 ms.
Endurance 31 min: 0 error, 0 watchdog, 0 freeze.

## K13b — 2026-09-17 · recalls of consumables

- Sensor G7: **maturity = pose + 10 days exactly** (the 12 hours of grace Dexcom does not
  "A" is the first time that a person is aware of his or her health. CHANGER At the end without ringing.
- Catheter: **only one** reminder 2 hours before.
- Each reminder is stored in NVS (`remind2`) **from its ringing** : a restart does not start
  Never play again.
- Former reminders (48 hours, 12 hours, grace, end of grace) deleted. Voice bank increased to 395 files.

Tests: 38 tests passed.

## K14 — 2026-09-18 · stock, tactile graph, adaptive reception

- `main/stock.c` : stock of sensors and catheters in NVS. **Not specified** by default,
  never negative, **only one decrementation by real change** (time of change)
  as marker), low stock thresholds, local event STOCK_LOW.
- Graph: tactile exploration on **real measurements** (never interpolation),
  line, point and bubble.
- Adaptive reception according to state.

Endurance 31 min : 0 restart, 0 watchdog, 0 error PPA, RAM internal 100 Kb stable
(min 70), Dexcom 7/7, microSD 0 duplicates.

## K15 — 2026-09-18 · page STATS is deleted

**Voluntary decision**: page STATS is withdrawn (`ui_stats.c` deleted, `UI_PAGE_STATS`
withdrawn, `gm_stats()` withdrawn). Navigation increases to **5 pages**; AUJOURD'HUI becomes the
only page of statistics.

Sleek reception: 260 px floating value, 190 px ring, 118 px arrow, without cards.
Complete refreshment 112 ms. Stability 16 min: RAM Internal 95 Kb stable (min 68).

> **Do not recreate page STATS.**

## K16 — 2026-09-18 · three-card reception

Reception K15 was considered "too empty". Three zones: map HEURE, map DATE, large map
GLYCEMIA. Alone `ui_home.c` changes. Method adopted here and then retained: ** PIL
on PC scale 1024 × 600, with real fonts and metrics LVGL, before
code**; two faults corrected on model, two on the map.

Refreshment 104 ms (less degraded bottom to repaint).

## K17 — 2026-09-18 · full screen reception * (requested by the voice) *

Accurate user request: "the date at the top next to the green dots, the hour later
high right, higher blood glucose, Wi-Fi/Internet/Dexcom/SD Use nothing."

- Common 5-page banner: **long date left**, mid-state line,
  **hour right**. The four connecting lights are **deleted**.
- ACCUEIL : more time card or date card, ** only one card 992 × 480**, value
  **320 px**, 200 px ring, 128 px arrow, 76 px variation.
- Refreshment 112 ms; RAM internal 94–104 KB (min 68); Application 6 126 880 o.

## K18 — 2026-09-19 · two alterations

- Headband time: 34 → **44 px**.
- Page AUJOURD'HUI : subtitle reduced to **"Since midnight"** (the long history is no longer there
  repeated, it is in the headband).

Validated by tests 345: 5 tabs on the finger, 5 page captures, no truncated text.

## K19 — 2026-09-21 · Secure sensor voice control *(historical version) *

Real problem: spontaneous ads « New sensor... Several times a day.

**Provinced case** (Journal of 18–1909): 5 detections, **all** of the English variant
"New sensor" (scores 0.1390 to 0.1656), never "New sensor"; none
confirmed. Reproduced by sound injection: "The new center opens on Monday" → 0,1514;
« New census data... » → 0.1444.

Corrective:

- the short sentences "New sensor" and "New sensor" are deleted**;
- two sentences **different**: "**Save new sensor**" (threshold ≥ 0,180)
  does not record anything and answers "Say confirm sensor"; only " **Confirm sensor**"
  (threshold ≥ 0,150), **within 10 s** after the announcement, records ;
- **anti-repetition**: After an unconfirmed request, "Save New Sensor" is
  ignored without announcement for 5 minutes;
- level of daily micros at each detection; `vrx_inject` added to test with
  its real; console control `sd_lire` (read only) added.

"Glycaemia" (≥ 0.155), "Glucose" (> 0.205) and the catheter are **unchanged**. The button
touch NOUVEAU CAPTEUR does not depend on voice or anti-repetition.

Validation: 33 functional controls, 0 detection on 24 traps and noise phrases, then
stability test.

Marks of the binary delivered at the time:
`glycemie_display.elf` SHA-256 `609a4d04044ac846803816077b906456f85fb7901d5aec390f0470238b78e342`,
`glycemie_display.bin` SHA-256 `fb475b771f302fb0d2f8575eb5390d7aa538cc4175bfeb11015fafcb8a60cd4c`,
Size 6 131 536 bytes.

---

## K20 — 2026-10-15 · language of the interface

User request: be able to put the screen in another language.

- New Section **LANGUE** in BACKGROUNDS: French, English, Spanish, German, Italian,
  Portuguese — six European languages, all in Latin alphabet, so displayable with fonts
  Montserrat already on board.
- `main/i18n.c/.h` : **179 texts** in 6 languages, in designated initializers (`[S_XXX] = {...}`),
  which makes a misalignment impossible; long dates and days specific to each language;
  `tr()` for the screen, `tr_fr()` for the microSD newspaper, CSV And the console.
- Change **immediate**: the task LVGL reconstruct the five pages, out of the callback button
  (the object carrying this callback is part of what is destroyed). The headband, the warning sign
  and the night screen are not destroyed, only updated.
- Saved in a key ** NVS separate** (`lang`), which does not invalidate the block of other settings.
- Console: `lang` and `lang en|en|es|de|it|pt`.

## K21 — 2026-10-15 · the voice follows the language

User's note: "It's weird to put the texts in English and when she says
high blood sugar levels are said in French."

- **Five new voice banks** of 396 files each (English, Spanish, German, Italian,
  (Google Chirp 3 HD "Aoede")
  22 MB and 11,500 characters per language. Portuguese is in Brazilian voice: Google
  does not offer Chirp 3 HD in European Portuguese.
- `tools/voice/langues_voix.py` Texts and **numbers in letters from 40 to 400** for the six languages
  (Italian elections) `quarantotto`, German composition `einhunderteinundzwanzig`, `cien` /
  `ciento` in Spanish, `cem` / `cento e` in Portuguese...). French is **unchanged byte
  next**: the bank already installed on the microSD remains valid.
- `main/voice.c` : the bank of the chosen language is read in `/glycemie/audio/voice/<code>/` ; if
  it is not installed, **the French bank takes over** rather than silence, and a
  warning is updated once only.
- `main/voice_rx.c` : order `vrx_dir <code>` to copy a bank to its subfolder (created
  if necessary); `sd_storage_mkdir()` added. File names remain short, the console line
  is limited to 47 characters.
- `tools/voice/sd_voice_upload.py --lang <code>` ; verification of a language bank
  each file by `vrx_put`, which answers `SKIP` After rereading and comparing the CRC32.

**Default found and corrected during testing**: field `title` of the table of alerts was
become a text identifier, but six `ESP_LOG` he was still passing to a `%s`. The first
`alert_sim` in English caused a `Load access fault` (dereference of integer 153), core
support dump. The field has been renamed `title_id` so that the compiler imposes the review of each
use, and newspapers go through `tr_fr()`.

** Checks on the map**: language change without crashing, simulated bass alert in English
(sign " LOW GLUCOSE ", microSD newspaper in French), back to the French bank confirmed for
Uninstalled language, Dexcom and Wi-Fi unchanged, NVS preserved.

## K22 — 2026-10-17 · configuration for each user

- Touch assistant: language selection, Wi-Fi, Dexcom account, verification and
  registration after confirmation.
- Windows application `GlycemieConnexion.exe` connected by USB, to complement the tactile.
- Public program **without personal identifiers**, with demonstration mode.
- Recognition of voice commands **deactivated in distributed program**;
  announcements spoken in six languages and sound alerts remain available.
- Non-material checks carried out; the complete test on a new card remains to be done.

## Between K19 et K20

Nothing until 15 October 2026, date of reconstitution of the deposit then of K20 et K21
above. In particular,
functions sometimes mentioned but **never implemented** are not included in this repository:
food balance by IA, additional dashboard, integration with another product,
conversion to Nightscout, display in mmol/L.

## Historical compilation of K19 (not the current version)

The figures below are for **K19**, whose sources were identical
to those of the safeguard `K19_VOIX_CAPTEUR_BIN`. This historical version has been
recompiled the 2026-10-15 with the same battery
(ESP-IDF 5.5.5, same versions resolved in `dependencies.lock`) :

- successful compilation, 2,433 targets;
- `glycemie_display.bin` = **6 129 952 bytes** (27% of partition `factory` free);
- 23/23 `tools/verif_avant_flash.py`.

The difference of 1,584 bytes with the binary of September is explained by the paths of
compiled in the image (the project is no longer in the same place on the disk):
The sources are identical.
