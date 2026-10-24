# Tests

Two levels, both executable without card ESP32 :

```powershell
python tests\run_tests.py          # tests unitaires du code C, compilés et exécutés sur PC
python tests\test_langues_voix.py  # cohérence des six banques vocales (texte seulement)
python tests\test_depot.py         # cohérence du dépôt et absence de données privées
```

And, once the firmware is compiled, a third before any flash:

```powershell
python tools\verif_avant_flash.py   # 23 contrôles sur l'image produite
```

---

## `run_tests.py` — PC unit tests

They compile the **real firmware code**, without changing it:

| Continue | Firmware files actually compiled |
|---|---|
| `test_glucose_math` | `main/glucose_math.c`, `main/app_state.c` |
| `test_i18n` | `main/i18n.c` |
| `test_consommables` | `main/sensor_session.c`, `main/consumables.c`, `main/stock.c`, `main/i18n.c` |

Only the material dependencies are replaced, by the plugs of `tests/hote/shims/` :

| Cap | What he does |
|---|---|
| `nvs.h` + `plateforme.c` | **NVS simulated in memory**. A "restart" consists of recalling the initialization functions without deleting the NVS — exactly as at card start |
| `freertos/` | mutex without effect (tests are mono-task) |
| `esp_log.h` | neutralised logging (`TEST_LOGS=1` to see) |
| `esp_heap_caps.h` | `malloc` Regular |
| `stubs_firmware.c` | network time (testable), microSD, event log |
| `compat_hote.h` | `localtime_r`, `gmtime_r`, `strlcpy` for MSVC |

Automatically detected compiler: **gcc**, **clang**, or **MSVC** (Build Tools Visual
Studio, found via `vswhere`). If none is present, the script clearly says it and comes out
in code 2 — he never announces a success he has not achieved.

### What is verified

**`test_glucose_math` - 38 controls**

- Age of the last measure: no measurement, unsynchronised time, measurement of the future;
- freshness thresholds **exactly** at terminals: 359 s = AT JOUR, 360 s = RETARD,
  599 s = RETARD, 600 s = DATA ANCIENNES ;
- actual rate: median intervals, default value when intervals are
  out of the beach 4–6 min;
- variation between two measurements: window 2 min 30 to 7 min 30, terminals included, sign correct;
- displayable variation: masked if only one measure, age unknown or data
  is old;
- next action expected;
- Dexcom trend names, including unknown code;
- shared history: limited to 288 measurements, most recent measurement in mind, number of
  sequence that changes with each update.

**`test_i18n` - 45 checks**

- the 6 languages are declared, with a name and code ISO ;
- ** none of the 179 texts is empty**, in none of the 6 languages;
- **The format specifiers are identical from one language to another**: this is control
  the `%d` lost in translation would read a non-existent argument on the map;
- `tr_fr()` remains in French regardless of the language chosen (microSD and CSV) ;
- off-range identifier: empty chain, no crash;
- correct long dates in the 6 languages, initial capital, "1st" only in French;
- 7 day names exist in each language.

**`test_consommables` - 65 checks**

- G7 sensor: delay = pose + **10 days exactly** (grace does not prolong anything), day
  1–10, promotion, "To CHANGER » to the second;
- reminders **24 h** and **2 h** before, and no recall after the deadline;
- **anti-repetition**: a reported reminder is never replayed;
- **persistence**: after a simulated "restart", the date of installation is identical to the
  second and the reminders already reported do not ring again;
- late registered: the reminders already exceeded are marked made, not
  gust of outdated reminders;
- catheter: 72 h by default, 48 h possible, unanticipated value refused, single reminder 2 h
  before, persistence after restarting;
- stock: never negative, **only one decrementation by change** (no effect replay),
  "not informed" which never becomes 0, bounding to 99, persistence;
- refusal to register a consumable if time is not synchronized — **without overwriting**
  the previous date;
- console restoration: exact date, reminder mask, no history line added.

### Result obtained on 2026-10-15

```
test_glucose_math : 38 controles, 0 echec(s)
test_i18n         : 45 controles, 0 echec(s)
test_consommables : 65 controles, 0 echec(s)
RESULTAT GLOBAL : TOUTES LES SUITES PASSENT
```

---

## `test_langues_voix.py` — voice banks

Do not generate any audio and call no service: these are text checks on
`tools/voice/langues_voix.py`, which describes the 396 sentences of each language.

The most important control is the first: **French sentences must remain identical
versioned manifest** (`assets/banque_vocale_manifeste.csv`), which describes the bank already copied
on the microSD. Changing a word in French would make silently obsolete files of the
map. Other controls check that each language has 396 files with the same names
in French, that no text is empty, that two blood glucose values are never pronounced
like, that the numbers in letters from 40 to 400 write without error, and that a voice Chirp 3 HD
is declared for each language.

Result obtained on 2026-10-15: **56 controls, 0 failure**.

---

## `test_depot.py` — Deposit checks

Don't compile anything and don't access the map:

- presence of expected files;
- **no absolute machine path** in a versioned file;
- ** no identifier**: no `device_secrets.h`, `.gitignore` correct, and comparison
  with the actual values of `private/device_secrets.h` whether this file exists locally;
- `partitions.csv` ** identical** to the original arrangement + partition `model` ;
- profile ESP32-P4 rev1.3 present and applied **after** `sdkconfig.defaults` ;
- coherence `main/CMakeLists.txt`  `.c` actually present, embedded files
  existing;
- **5 pages** reported and page STATS not reintroduced;
- versions of dependencies frozen as historically validated;
- no firmware binary, no file over 2 MB;
- 5 screenshots in real format 1024 × 600.

---

## What is **not** covered here

Honestly, and by construction:

- **Dexcom Share** (HTTPS, authentication, lock) — not insolable without ESP-IDF, and he
  does not exist a test Share server;
- **network** (cut, reconnection, expired session) — validated on the map with
  `wifi_test_disconnect` et `dexcom_test_session` ;
- **microSD** (duplication, removal, winding) — depends on the driver SDMMC ; validated on the
  card with `sd_check`, `sd_repair`, `sd_test_eject` ;
- **LVGL and display** — non-instabable on PC here; verified by actual catches and
  by `ui_stress` on the map;
- ** audio alarms and repetitions** — depend on FreeRTOS clocks and codec; validated
  on the map with `alert_sim`. Only consumables recalls and their anti-repetition
  are covered in the host test;
- **Voice controls** — French synthetic voices almost never trigger the
  French phrases: validation requires a human voice.

See [../docs/LIMITATIONS_EN.md](../docs/LIMITATIONS_EN.md).

---

## Add Suite

1. Write `tests/hote/test_<nom>.c` with `mini_test.h` (`section`, `CHECK`, `CHECK_EQ_I`,
   `test_report`).
2. Add an entry in `SUITES` de `run_tests.py` with the `.c` firmware to compile.
3. If a new platform symbol is missing, add it in `tests/hote/shims/`.

Test binaries are produced in `tests/build/` (ignored by Git).
