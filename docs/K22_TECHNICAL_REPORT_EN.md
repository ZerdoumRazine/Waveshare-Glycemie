# Historical report — preparation of K22 (17 October 2026)

**Archived document, not representative of current status.** It describes the steps
computer preparation prior to material testing and private publication.

Since this report, the firmware K22 was installed and checked on the family apparatus
for the operations described in [limits](LIMITATIONS_EN.md).
The private GitHub repository and its v0.1.0 preversion have also been created.
The first installation on a new card has not yet been validated from end to end.

To start installation, use the [beginner tutorial](BEGINNER_TUTORIAL_EN.md).
The historical results below are not recent tests of the current version.

---

## 1. What has been done, by point of the request

### 1.1 The two connection assistants

**On screen** (`main/ui_setup.c`, 6 steps): language → Wi-Fi network (list of detected networks,
sort by power) → password (keyboard) LVGL, masked characters, "Show" button → account
Dexcom + region (3 buttons) → connection test → **confirmation**. The assistant opens himself
as long as nothing is recorded, and re-opens since LOGAGES → CONNEXION → Edit.

**Menu CONNEXION** (`main/ui_settings.c`, section at y = 1510): permanently accessible, without
network dependence — it only reads internal memory. Show network name and hidden account
(`a***@exemple.fr`), never a password ; **Edit**, ** Clear** (with window)
confirmation), switch **demonstration mode**, and a "test in progress" banner when
access are to the test.

**On PC**: `tools/pc_app/glycemie_connexion.pyw` (tkinter) → `GlycemieConnexion.exe`
(PyInstall, one file, 10.04 MB, without console). End user does install **n Python
or library**. The series dialogue is that of `tools/assistant_config.py`, already tested. The
exchanges are made in a separate wire: the window does not freeze. Construction:
`tools\pc_app\construire_exe.ps1`.

**Port detection**: readable names obtained by PowerShell (`Win32_PnPEntity`), recognition
Link chips (CH343, CP210x, FTDI, CDC...). **If there are several candidates, none are selected**:
the user selects from the list, and the window explains how to recognize the right one
(disconnect/reconnect). On this PC, the map appears as `USB-Enhanced-SERIAL CH343 (COM7)`.

### 1.2 Never replace functioning accesses

Test mechanism**(`credentials_trial_*` in `main/credentials.c`) :

| Step | RAM | Permanent memory (NVS) |
|---|---|---|
| `credentials_trial_begin()` | photographing current accesses | unchanged |
| `credentials_trial_set_wifi/dexcom()` | new values (so used by the connection test) | **unchanged** |
| failure, cancellation, power failure, restart | return to previous accesses | **unchanged** |
| `credentials_trial_commit()` (after successful **and** confirmation) | new values | in writing |

As a result: there is no moment when the device is found without its previous access.
The console applies the same rule: `ident_enregistrer` **refuse** until the last test is
not successful (`ident_enregistrer force` to override, voluntarily explicit).

Identifier protection, frankly: space NVS `ident` separated, block blurred by OR
exclusive with a key derived from the address MAC. **This is not encryption.** Flash Encryption
and Secure Boot are not activated (irreversible, brick risk). Passwords are not
never logged: only masked forms come out of the module (`credentials_masked_*`).

### 1.3 Device protection before installation

`tools/sauvegarde_appareil.py` : identification (chip, revision, flash, partition table), copy
in `private\sauvegardes\<date>\`, **rereading verification** (`esptool verify_flash`) and
prints SHA‐256 in `manifeste.json`. The tool **refuses** any write command or
Explicit blacklist: `erase_flash`, `write_flash`, `burn_efuse`…).

[ROLLBACK_EN.md](ROLLBACK_EN.md) described: what the installation changes and does not change,
interruptions (two restarts, one to two minutes without display), where are current accesses
and how they pass in the new version, the reverse command, and the procedure
Recovery by button **BOOT** if the screen no longer starts — specifying that, in this case,
menu CONNEXION and Windows application is not enough.

**Transfer of current accesses**: they are now *compiled* in firmware since
`private/device_secrets.h`. The new version automatically takes them back as long as the memory
`ident` is empty (`credentials_init()`), so a firmware compiled from the personal folder
Reconnect without any input. No recovery from the device is necessary — so none
password to read again or to transit.

### 1.4 Voice commands and announcements

Changed since K21 and in accordance with the request: `GLY_VOICE_CMD=OFF` default,
`main/voice_cmd_stub.c` provides the same interface **without ever opening the microphone**. Reason: ESP‐SR
recognizes only English and Chinese; the French sentences approached triggered real
false positives ("New sensor" recognized in an English television phrase, score 0.1368–0.1514).

**Reserved**: announcements spoken in all six languages, sound alerts, tactile access to all languages
functions. The six banks (396 files each) are verified: `tests/test_langues_voix.py`
→ 56 checks, 0 failure. Packages ready to copy in `dist_voix\` with manifests, fingerprints and
Notice of provenance (`NOTES_RELEASE.md`, `paquets.json`). **No paid generation was
Relaunched. **

For a personal device, the variant with voice commands is recompiled:
`tools\build.ps1 -Vocal` (verified: 6.19 MB, 26% free).

### 1.5 Public version (Razine)

`tools/preparer_razine.py` reconstruct the published copy **since `git archive HEAD`**: only
files followed by Git are part of it, so never `private\`No newspapers, no binaries. Copy
is replaced ** only after** verification; the deposit of publication `Razine\.git` is kept.

Result: **157 files, 3.90 MB**, containing commented code, tests, tools, sources
Windows application, tutorials, captures, licenses and credits.

`tools/publier_firmware.ps1` compiles the **public** program from this copy (without
`private/device_secrets.h`, therefore without any identifier) and prepares `livraison\` :

| File | Size | SHA‑256 |
|---|---|---|
| `glycemie_display.bin` | 2.25 MB | `eaaa656902d6aa73…dac86e46` |
| `bootloader.bin` | 0.02 MB | `ad079243a1665768…b23cc353c` (see `SHA256SUMS.txt`) |
| `partition-table.bin` | 3 Ko | see `SHA256SUMS.txt` |
| `GlycemieConnexion.exe` | 10.04 MB | `042da82470f6d63f…6d88770b` |
| `installer.ps1` | 2 Ko | see `SHA256SUMS.txt` |

Deposit Archive: `dist_voix\glycemie-depot-public-k22.zip`180 files,
SHA‑256 `ab654adaae8d69db81759e484df15e66e9a0495d7bfcfea65e45a3ce35b8ccd1`. Archive K21, expired,
has been deleted.

** PCM Microsoft** (`main/voice/voice_fr.pcm`) is no longer followed by Git: absent from the archive and
public copy, always present on the disk for a local compilation
(`GLY_VOICE_OLD_PCM`). *Reserve: it still appears in the old recordings of
history; a publication of history would prevail. *

### 1.6 Confidentiality checks

`tools/controle_donnees_perso.py` et `tools/archive_publique.py` have been corrected:

- ** whatever the break** (`re.IGNORECASE`) ;
- The contents of the archive are **extract and then reread** (not just file names);
- files delivered next door (compiled program, `.exe`) are examined with `--livre` ;
- a control that reads too few files, or that cannot compare to actual identifiers,
  returns **2** and says it is inconclusive — it no longer announces success;
- a refused archive is removed**;
- unrecorded changes make **refuse** the archive (it would not match the
  completed);
- Git history is reread **binaries included**.

Results (17 October 2026):

| Monitoring | Result |
|---|---|
| 1606 economic statistics, economic statistics, economic statistics | no ID, no address, no personal path |
| `livraison\glycemie_display.bin` (2.25 MB) | clean |
| `livraison\GlycemieConnexion.exe` (10.04 MB) | clean |
| Inverse control over firmware **personal** | the 4 identifiers are well detected → the tool knows how to see them |
| History Git (345 objects) | Nothing except two local paths in old versions of two documents (corrected since) |
| Addresses of records | a personal address and a hidden GitHub address appear in the recordings — **decision to take before pushing** (see [PUBLICATION_GITHUB_EN.md](PUBLICATION_GITHUB_EN.md)) |
| 9 catches distributed, **one by one** | demo mode, fictitious values, hidden dates, no password, no first name |

Two honest reservations:

1. catches `05_reglages.png` et `06_reglages_langue.png` show the**old** page
   (Voice commands section, phrase "announcements and voice commands follow French")
   : they are to be repeated after updating the screen;
2. no automatic control reads the *visible* content of an image. Human rereading remains
   the last net.

---

## 2. Tests: what was actually executed

| Continue | Nature | Controls | Result |
|---|---|---|---|
| `tests/run_tests.py` → `test_credentials` | **C code actual** compiled on the PC, NVS simulated | 110 | 0 failure |
| `tests/run_tests.py` → `test_glucose_math`, `test_i18n`, `test_consommables` | actual C code | 65 + … | 0 failure |
| `tests/test_assistant_config.py` | **simulation**: dummy apparatus, no serial link | 73 | 0 failure |
| `tests/test_pc_app.py` | Tk window actually built, simulated ports | 26 | 0 failure |
| `tests/test_sd_voice_upload.py` | **simulation**: `--verify-only` Don't write anything | 79 | 0 failure |
| `tests/test_langues_voix.py` | real banks on disk | 56 | 0 failure |
| `tests/test_depot.py` | actual deposit | 42 | 0 failure |
| Personal compilation (`tools\build.ps1`) | **Actual compilation** | — | 3.53 MB, 58 per cent free |
| Personal compilation with voice commands (`-Vocal`) | actual compilation | — | 6.19 MB, 26 per cent free |
| Public compilation (since Razine) | actual compilation | — | 2.25 MB, 73 per cent free |
| Windows application | ** actually set**, captured window | — | start, window correct |

What these tests **do not prove**: nothing was tested on the screen. Touch, connection
real series, Wi-Fi connection, Dexcom authentication, sound ads and persistence
After restarting **remain to check on the device**.

The most important point to check is true: **The test followed by a failure must leave the device
with previous accesses**. This is proven on the PC (test `Essai refused: automatic return to
previous access`), not yet on the screen.

---

## 2 bis. Trials REALLY done on screen (17 October 2026)

Installation: partition `factory` alone (`idf.py app-flash`), 3,534,352 bytes, footprint checked
by esptool. The settings memory, partition table and microSD were not affected.

| Test | How | Result |
|---|---|---|
| Prior storage of memory | Full reading 32 MB + `verify_flash` | checked, recorded footprint; Then reread: 5 compliant partitions, valid start and application, NVS 60 % written |
| History Recovery | `sd_dump` on the 36 files | 36/36 copied, **print CRC-32 checked for each** |
| Fidelity of history | comparison with inventory taken **before** update | 36/36: same number of measurements, **same first and even last line**. 9,973 measures |
| Newspapers | `sd_lire` | 35 files, announced size = received size; a second reading gives the same bytes |
| Voice banks | `vrx_list crc` against the PC bank | 396/396 identical: no sound to be transferred |
| Wi-Fi connection | `info` after restart | connected, **same IP address**, ‐52 dBm |
| Dexcom Reconnection | `info` | in service, fresh measure received |
| Consumable dates | `conso_info`, `stock_info` | sensor and catheter retained by the second, reminders and stocks unchanged |
| Settings | `reglages` | volume, thresholds, night mode kept |
| Identifier | `ident_info` | from the compilation (`origine=compilation`), nothing to grasp |
| Voice commands | `info` | `vcmd=0` : absent, microphone never opened |
| Sound alert and spoken announcement | `test_sound`, `test_voice` | ** understood by the user** |
| Tactile | `touch_info` after use | 23 presses, 11 clicks, 11 scrolls, **0 playback error** |
| Opening the touch assistant | press "Edit", tactile trace | detected press on the right button, then `Essai de nouveaux acces` et `Assistant ouvert (modification)` |
| Search for Wi-Fi networks | Assistant, step 2 | **6 networks displayed**, including its |
| Return without changing anything | "Return" and "Cancel" | `essai=0`, same network, same account, unchanged passwords (27 and 11 characters) |
| Page CONNEXION | capture `snap` | displayed correctly, password nowhere ; the "test in progress" state is displayed during a test |

Two limitations, honestly:

1. **Order `snap` cannot photograph the assistant**: it captures `lv_screen_active()`,
   or the assistant (and the blank confirmation window) live on `lv_layer_top()`. The
   The assistant's pages are therefore missing from the catch; the user provided a photo. Correction
   possible in a next version (capture also the top layer).
2. `tools/serial_capture.py` launched in the background survived the shutdown of his envelope and kept
   serial port; You have to throw it in the foreground, or check that it has finished well.

Text corrected in passage: the help of the section LANGUE announced "voice announcements and
voice commands remain in French" — false since K21 (every language has its bank) and since
K22 (more voice commands). The new text will apply to the next update.

---

## 2 ter. Prepared delivery (18 October 2026)

Nothing was published: no GitHub account used, no repository created, no file sent.

| Element | Où | Content |
|---|---|---|
| Deposit ready to send | `Razine\` | 167 files, 2.2 MB. Publication history **neuf**: one recording, on behalf of Mohamed-Razine Zerdoum, with a masked GitHub address |
| File to be submitted | `paquet_a_remettre\` | 2,612 files, 299.7 MB: `code_source\`, `application_windows\`, `installation_ecran\`, `microSD\`, `tutoriels\`, `telechargements\`, `notices\`, `LISEZMOI.txt` (FR+EN), `SHA256SUMS.txt` |
| Archives for distribution | `paquet_a_remettre	elechargements\` | installation of the screen (1.3 MB), Windows application (11 MB), microSD (130 MB), source code (1.1 MB), fingerprints |
| Complete Archive | `livraison\glycemie-paquet-complet.zip` | the whole package in one file, with its footprint |
| GitHub preparation | `publication_github\` | name of deposit, descriptions FR/EN, presentation, order of catches, description of downloads, version notes, and step by step guide in the browser |

Three real defects found during this preparation, all corrected:

1. **The public program delivered was older than the sources** — compiled two minutes before the
   correction of screenshots, it did not contain it. Preparing the package compares
   now the date of the program at that of each firmware source and refuses to continue.
2. **The publication repository history still contained the old Windows voice** whose
   rights are not established. History starts with the repository: it was redone by one
   registration. The work history was not affected.
3. **The tutorial described a bad tree for sounds**: firmware
   (`main/voice.c`) waits for French **directly** in `glycemie/audio/voice/`, and others
   languages in `en/ es/ de/ it/ pt/`. This is what packages contain; the text was
   Wrong.

The confidentiality check was also specified: search email addresses in
bytes compressed or in audio signal produced only random, and these false alarms
would have ended up masking the real ones.

---

## 3. Files added or modified (K22)

```
main/credentials.c/.h        essai, confirmation, retour en arriere, masquage
main/ui_setup.c              assistant tactile (6 etapes) + confirmation
main/ui_settings.c           section CONNEXION + etat d'essai
main/app_main.c              ident_enregistrer / ident_annuler
main/i18n.c/.h               5 nouveaux textes x 6 langues
tools/assistant_config.py    assistant USB (ligne de commande) + noms de ports lisibles
tools/pc_app/                application Windows : source, construction, captures
tools/sauvegarde_appareil.py sauvegarde verifiee, lecture seule
tools/preparer_razine.py     copie publique depuis git archive
tools/publier_firmware.ps1   programme public + .exe + empreintes
tools/archive_publique.py    controles renforces
tools/controle_donnees_perso.py  idem + historique binaire
tests/hote/test_credentials.c    110 controles
tests/test_assistant_config.py   73 controles
tests/test_pc_app.py             26 controles
docs/BEGINNER_TUTORIAL_EN.md, docs/BEGINNER_TUTORIAL_EN.md
docs/ROLLBACK_EN.md, docs/PUBLICATION_GITHUB_EN.md, docs/K22_TECHNICAL_REPORT_EN.md
```

Records: `93f820f` → `1d570ca`. Nothing was pushed.

---

## 4. Items not resolved

1. **Captures of the assistant's pages**: impossible as long as `snap` only photograph the screen
   (see 2a). To be corrected in a future version, or to be replaced by photos.
2. **Captures to be redone**: `05_reglages.png` et `06_reglages_langue.png` show the old page
   MARKS (voice commands section) and the old language help text.
3. **Photos of equipment**: taken USB‐C, button BOOT, speaker connector, microSD in place.
4. **Historical Git**: the PCM Microsoft and two personal paths are still included; to be decided
   before publishing the history (the archive is clean).
5. **Record address**: to be chosen before the first shipment to GitHub.
6. **Food**: Waveshare does not publish any official value. The tutorial recommends 5 V / 2 A
   not by the manufacturer.
7. **Unsigned Windows app**: SmartScreen will display "Unknown Editor". Signer requests a
   paid certificate; fingerprint SHA‐256 is published instead.
