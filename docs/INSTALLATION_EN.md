# Installation and compilation (Windows 10 / 11)

Duration: about one hour the first time, including the most important download.

---

## 1. Constraints to know before starting

>  ASCII.**
> ESP-IDF fail otherwise, in three distinct places: `ccache` lift
> `filesystem error: Cannot convert character sequence`, `ldgen.py` lift
> `UnicodeDecodeError` by rereading the exit`objdump`, and the link editor finds
> plus pre-compiled librariesESP-SR. **spaces** in the path are accepted.
>
> `C:\dev\waveshare-glycemie` appropriate; a folder containing an accent, for example
> `D:\Projets\Waveshare glycémie`, is not suitable for compiling (the repository may be
> preserved, but compilation must be done from a copy or clone in a path
> ASCII). `tools\build.ps1` automatically handle it.

Other items:

- ESP-IDF and tools (`IDF_TOOLS_PATH`) must also be in a path ASCII.
- Provide 5 GB for ESP-IDF and the compilation chain, plus 2 GB for the project.
- No admin fee is required once the serial driver is installed.

---

## 2. Serial port driver

The map displays a bridge **WCH CH343** If Windows does not recognize it automatically,
install the pilot from [wch.cn](https://www.wch.cn/) (pilot CH343SER). Next:

```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' } | Select-Object Name
```

Record the number displayed (`USB-Enhanced-SERIAL CH343 (COMx)`) : it will replace `COM7` in
all orders that follow.

---

## 3. ESP-IDF 5.5.5

Two possibilities.

### a. Official installer (the simplest)

Download**ESP-IDF Windows Install** on
[dl.espressif.com](https://dl.espressif.com/dl/esp-idf/) and select the **v5.5.5** version.
When installing, select target **ESP32-P4** (it avoids downloading
unnecessary Xtensa strings) and a directory **without accent**, for example
`C:\Espressif`.

Installer adds shortcut " ESP-IDF 5.5 PowerShell": all controls of this
document launch from this window.

### b. ESP-IDF installed in the project

The script `tools\idf_env.ps1` automatically detects an installation placed in
`tools\esp-idf-v5.5.5` (with its tools in `tools\espressif`) and redirect all caches
in `tools\cache`, without touching Windows environment variables:

```powershell
. .\tools\idf_env.ps1
```

Si ESP-IDF is elsewhere, define `IDF_PATH` before the call:

```powershell
$env:IDF_PATH = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
. .\tools\idf_env.ps1
```

> If activation complains of `xtensa-esp-elf` missing is that the installation was
> made for the target ESP32-P4 only and onlyESP-IDF does not find the recording of
> this installation. Correct with:
> `python "$env:IDF_PATH\tools\idf_tools.py" install --targets=esp32p4`

---

## 4. Recover Project

```powershell
git clone <url-du-depot> C:\dev\waveshare-glycemie
cd C:\dev\waveshare-glycemie
```

---

## 5. Private identifiers — old optional method

**To install the public program, do not enter any secrets in the code.** After installation, the touch assistant or Windows app allows you to enter your own Wi-Fi and Dexcom access without recompilation ([beginner tutorial]](BEGINNER_TUTORIAL_EN.md)).

The instructions below relate only to the**old optional method** of compiling a personal program with its integrated identifiers. Never distribute the resulting binaries.


```powershell
copy main\device_secrets.example.h private\device_secrets.h
notepad private\device_secrets.h
```

```c
#define GLY_WIFI_SSID        "MonReseau"      /* Wi-Fi 2,4 GHz */
#define GLY_WIFI_PASSWORD    "..."
#define GLY_DEXCOM_USERNAME  "..."            /* compte Dexcom QUI PARTAGE */
#define GLY_DEXCOM_PASSWORD  "..."
#define GLY_DEXCOM_REGION    "ous"            /* Europe */
```

The folder `private/` is ignored by Git. These values are **compiled in firmware**:
never publish `build/` or `.bin` product.

Dexcom account details : [DEXCOM_SETUP_EN.md](DEXCOM_SETUP_EN.md).
Without Dexcom account, leave the Dexcom fields empty: the device starts and the **mode
demonstration** allows you to try everything (`demo normal` in the console).

---

## 6. Compile

```powershell
idf.py set-target esp32p4
idf.py build
```

Component manager uploads dependencies to the first compilation
(BSP Waveshare, LVGL, ESP- Hosted, ESP-SR...) respecting `dependencies.lock`. Count
several minutes and 2,400 object files.

Expected outcome at the end of the compilation:

```
glycemie_display.bin binary size 0x5d8920 bytes. Smallest app partition is 0x800000 bytes.
0x2276e0 bytes (27%) free.
```

Shortcut provided: `powershell -ExecutionPolicy Bypass -File tools\build.ps1` (add
`-Clean` for one `fullclean` prior).

> After any modification of `sdkconfig.defaults` or `config/esp32p4_rev1_3.defaults`,
> **delete `sdkconfig`** before recompiling, otherwise old values persist.

---

## 7. Check before flashing

```powershell
python tools\verif_avant_flash.py
```

23 security checks on the image produced: target, profile rev1.3, flash size,
PSRAM, partition offsets ** identical to those of the original firmware**, files
actual written (the NVS is never in the list), absence of erasure, sizes of
binary, accepted revisions, no ID in configuration files.

The script is mistaken if only one control fails. **Do not flash in this case. **

---

## 8. Flash

```powershell
idf.py -p COM7 flash monitor
```

Are written: bootloader `0x2000`, partition table `0x8000`, application `0x10000`,
models ESP-SR `0xF10000`. **The NVS is never written** : settings, dates of installation of the
sensor and catheter, stocks and lock Dexcom are kept.

If only the application has changed (current case), `idf.py -p COM7 app-flash` is sufficient and avoids
Rewrite the 3MB of models, instead of 2 min.

**Never** of`erase-flash`, of`erase-region`, eFuse, Secure Boot or Flash
Encryption. The firmware of theESP32- C6 is never hit.

> **Recommended save before the first flash.** Read the entire flash
> (32 MB) allows you to return to the factory firmware:
> `python -m esptool --chip esp32p4 -p COM7 read-flash 0 0x2000000 sauvegarde_32MB.bin`
> This backup may contain private data: keep it **outside the repository**.

---

## 9. First commissioning

1. On startup, the console displays the revision of the chip, the PSRAM, the initialisation of
   the screen, the detection of the GT911, then Wi-Fi, SNTP and Dexcom.
2. Page **REGULATIONS**: adjust volume (85% recommended), brightness, thresholds,
   Night mode, catheter interval.
3. Page **CONSOMMABLES**: press **NOUVEAU CAPTEUR** at the actual time of installation, then
   Inform stocks.
4. Voice commands are **disabled** by default (they consume 6 MB of PSRAM
   and 12 Ko of RAM internal): to be activated in LOGIES if desired.

---

## 10. microSD (optional but recommended)

Format a map in **FAT32** (a 64 GB card must be explicitly formatted in
FAT32, not exFAT), insert it, then check:

```powershell
python tools\serial_capture.py --port COM7 --duration 20 --send "2:sd_check" --log sd.log
```

Without microSD: no history CSV and no voices (a beep replaces the ads). All
remains working.

To install the French voice bank, see [THIRD_PARTY_LICENSES_EN.md](THIRD_PARTY_LICENSES_EN.md)
(the bank is not distributed with the deposit) and `tools\voice\gen_voice_google.py` +
`tools\voice\sd_voice_upload.py`.

---

## 11. Tests

```powershell
python tests\run_tests.py      # tests unitaires sur PC, sans carte
python tests\test_depot.py     # cohérence du dépôt, absence de données privées
```

See [../tests/README.md](../tests/README.md).
