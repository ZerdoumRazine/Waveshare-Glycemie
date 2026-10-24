# Troubleshooting and FAQ

## First of all: the serial console doesn't change anything

Open serial port **does not restart the card** and writes nothing in flash. It's
the safest diagnostic operation.

```powershell
python tools\serial_capture.py --port COM7 --duration 60 --send "3:info" --log diag.log
```

The **mask** tool automatically, before writing the log, any value present in
`private/device_secrets.h` and all UUID (session, account).

Useful Options: `--reset` (starts the card again by RTS), `--send "DELAI:commande"`,
`--png capture.png` (decodes a screenshot `snap`), `--until "<regex>"`.

Status line returned by `info` :

```
INFO wifi=2 ip=192.168.1.x rssi=-52 internet=2 heure=2 dexcom=3 demo=0 sd=2
     sd_lignes=0 sd_libre_mo=59657 audio=1 derniere=112 tendance=Flat age_s=140
     heap_int=103395 heap_int_min=70360 dma_bloc=32768 psram=26170412 vcmd=0
```

---

## Compilation problems

| Symptoms | Cause and solution |
|---|---|
| `filesystem error: Cannot convert character sequence` | ** accent in the project path**. Compile from a pure path ASCII. |
| `UnicodeDecodeError` in `ldgen.py` | same cause. |
| `ld.exe: cannot find ...libmultinet.a: No such file or directory` | same cause `managed_components`). |
| `tool xtensa-esp-elf has no installed versions` | installation ESP-IDF Do not find his recording for this path. `python "$env:IDF_PATH\tools\idf_tools.py" install --targets=esp32p4` |
| `sdkconfig incompatible avec l'ESP32-P4 rev1.3` | the solved configuration is not in pre-v3 profile. Remove `sdkconfig` and recompile; verify that `CMakeLists.txt` add well `config/esp32p4_rev1_3.defaults` **after** `sdkconfig.defaults`. |
| An amendment to `sdkconfig.defaults` remains ineffective | `sdkconfig` existing premium. Remove it, then recompile it. |
| Warning treated as an error | the project compiles with `-Werror` (e.g. `format-truncation`). Correct the warning, do not disable the option. |

---

## Card does not start / nothing on screen

1. Check power supply: a port USB low may not be enough with backlighting
   100% and active Wi-Fi.
2. Open console: if boot is displayed but not screen, it is initialization
   EK79007 or backlighting.
3. A binary compiled for **rev3.x** does not start on a **rev1.3** card: restart
   `python tools\verif_avant_flash.py`, which controls accepted revisions (v1.0 to v1.99).

---

## Touch does not respond, or next door

- Validated mapping is `swap_xy = 0`, `mirror_x = 0`, `mirror_y = 0` in `app_main.c`.
  Mirrors X/Y from the example Waveshare ** reverse** touch it on this map.
- Diagnostic: `touch_log 1` (RAW / MAPPED / object LVGL / click / scroll), `touch_info`,
  `touch_map`. `touch_tap <x> <y>` injects a support.

## The interface freezes

It was a reproducible **gel** pilot PPA during rotation 180° of zones
partial. The patch is in `components/esp_driver_ppa` : ensure that this component
is well present in the project. `ui_info` displays the beat of the interface, and a
detection `UI_GEL` logs the status of the tasks if the display no longer refreshes
5 seconds.

In case of panic or watchdog, a **core dump ELF** is issued on the console before the
restart. Decoding on PC:

```powershell
python -m esp_coredump --chip esp32p4 info_corefile -t b64 -c dump.b64 build\glycemie_display.elf
```

---

## No Dexcom data

| View | Track |
|---|---|
| ‘Data ANCIENNES » with "Wi-Fi coupe" cause | 2.4 GHz network, SSID/mot in `device_secrets.h`, RSSI in `info` |
| ‘Data ANCIENNES » without visible cut-off | the **telephone** no longer publishes (offline, closed application, sharing disabled) |
| Nothing at the very first start | normal: authentication lock is on INCONNU. Launch `dexcom_auth` once (see [DEXCOM_SETUP_EN.md](DEXCOM_SETUP_EN.md)) |
| State `REFUSE` | refused identifiers or blocked account; no automatic attempt, even after restarting. Correct IDs and then reauthorize |

Commands: `dexcom_status`, `dexcom_auth`, `dexcom_test_session` (similar one session)
expired: recovery must be automatic), `wifi_test_disconnect` (Wi-Fi cutting
voluntary: reconnection expected in seconds).

Dexcom error messages are logged with step, code HTTP and the Dexcom code,
the rest **masked**.

---

## microSD

| Symptoms | Explanation |
|---|---|
| Red SD tablet, nothing written | Unrecognized file system (exFAT for example). **Nothing is automatically formatted.** Reformat the card in FAT32 on the PC |
| Card missing or withdrawn | clean disassembly, new attempts spaced (30 s, 1, 2, 5, 10, 30, 60 min). Display, Dexcom, alerts and beeps continue; Only voice is missing |
| Doubts in History | should no longer arrive: before writing, the last line of the CSV the day (or the day before) is reread and only the strictly more recent measurements **to the second** are added. `sd_check` list duplicates, disorder and invalid lines; `sd_repair` deletes **only** exact duplicates |

Commands: `sd_check`, `sd_repair`, `sd_test_eject <s>`, `sd_test_remount <n>`,
`sd_lire <fichier>` (read only a file of `/glycemie/logs`).
`sd_format EFFACER` only acts on a map to the unrecognized file system.

---

## Sound and voice

| Symptoms | Explanation |
|---|---|
| A beep instead of a sentence | the voice bank is not on the microSD, the file is missing or invalid, or the card is unavailable. The beep is the expected withdrawal |
| 100% saturated sound | normal: the analog limit is reached. Use **85 %** |
| No sound | volume MUTE, "Audible Alerts" deactivated, or non-activated amplifier. `audio_test <volume>` |
| Number of vocal files | displayed in FRANÇAIS > INFORMATIONS |

Full alert test without waiting for a real alert:
`alert_sim <type> <valeur>` — 0 very low, 1 bass, 2 no data, 3 very high,
4 high, 5 sensor, 6 catheter. `alert_test off` Stop it, `alert_info` Show the state.
These test alerts are marked "(TEST)", are never logged on microSD and
never appear before a real alert BASSE or VERY BASSE.

> ⚠️ `alert_sim` ** actually sounds the device** in the house. To avoid the night.

---

## Voice commands

| Symptoms | Explanation |
|---|---|
| The apparatus "spokes by itself" | This was the case with the short sentences "New sensor" / "New sensor": 5 spontaneous detections in 2 days, all of the English variant, triggered by television phrases ("The new center opens on Monday" → score 0.1514). These sentences have been removed** and replaced by two different sentences. Check the counter `ignorees=` in `vcmd_info` |
| An order is never recognized | raise actual scores: `vcmd_debug 1` displays the decoded phoneme string and the score at 4 decimal places. Adjust **one variant at a time**: add many variants degraded *all* recognition |
| Listening doesn't start after a crash | voluntary: "in pause after an unexpected restart". Reactivating in REGAGES or by `vcmd_on` |
| `audio_test` refused | microphones are reserved during listening |

Commands: `vcmd_on`, `vcmd_off`, `vcmd_eveil 0|1`, `vcmd_gain <dB>`, `vcmd_info`,
`vcmd_sim <id> [probabilité]`, `vcmd_niveaux 0|1`, `vcmd_debug 0|1`, `vcmd_ajout <1.6> <phonemes>`.

---

## Consumers

| Question | Answer |
|---|---|
| Does a recompilation erase the date of installation? | **No.** She's in NVS, and a flash never writes the NVS. This is verified by an automatic test (`tests/hote/test_consommables.c`) |
| Can a reminder ring twice? | No: it is stored in NVS From his bell. A restart never replays it |
| A date was recorded by mistake | `conso_restore capteur|catheter <epoch> <recall>` remet exactement la bonne date **sans** ajouter de ligne à `consumerable.csv` ; `conso_csv_purge <epoch>` removes the test lines |
| Has the stock been counted twice? | No: the time of change serves as a marker in NVS. `stock_info` shows the markers |

---

## FAQ

**Can be displayed in mmol/L ?**
No, not today: the display is in mg/dL Only.

**Can we configure Wi-Fi on the screen? **
Yes: since K22, open **REGAGES → CONNEXION** or use Windows application `GlycemieConnexion.exe` connected by USB. The configuration in `private/device_secrets.h` concerns only the old method of personal compilation.

**Can we use it with Nightscout? **
No. The project reads Dexcom Share directly, and it was not converted to Nightscout.

**Can we use it without a Dexcom account? **
Yes, in demonstration mode (`demo normal`). All pages work with data
fictitious.

**What happens if Dexcom changes its service? **
Share is not a API public: service may be interrupted without notice. It's a
reasons why this display does not replace official alarms.

**How to return to the original card firmware? **
If a full 32MB backup was made before the first flash, rewrite it to
Address 0. Without backup, we have to start from a Waveshare example.
