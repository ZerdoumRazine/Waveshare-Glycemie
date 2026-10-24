# Building and installing WaveShare Glycémie

This guide explains the **developer build from source** on Windows 10/11. Users who only need a working display should follow [the beginner's guide](BEGINNER_TUTORIAL_EN.md) and use the versioned installation ZIP instead of installing a compiler.

## Requirements

- **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**, ESP32-P4 revision v1.3 (pre-v3 build profile).
- A USB-C **data** cable connected to the board's **UART/CH343** connector.
- Windows 10 or 11, Git, Python 3, and Espressif **ESP-IDF v5.5.5**.
- An ASCII-only build path, such as `C:\dev\waveshare-glycemie`. Paths containing accented letters have caused build-tool failures.
- Enough disk space for ESP-IDF, its toolchains and the generated build directory (several gigabytes).

## 1. Verify the serial port

The board's USB-UART bridge is a **WCH CH343**. Check Windows Device Manager under *Ports (COM & LPT)*, or run:

```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' } | Select-Object Name
```

Find `USB-Enhanced-SERIAL CH343 (COMx)` and note its port number. `COM7` in examples is a placeholder, not a fixed port. If Windows does not recognize the bridge, use the appropriate WCH serial driver.

## 2. Install and activate ESP-IDF

Use the official [ESP-IDF Windows installer](https://dl.espressif.com/dl/esp-idf/) for **v5.5.5** and select the **ESP32-P4** target. Install it to a path without accents, for example `C:\Espressif`. Open the ESP-IDF PowerShell shortcut.

If ESP-IDF is already installed elsewhere, you can activate it with the included helper:

```powershell
$env:IDF_PATH = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
. .\tools\idf_env.ps1
```

The script also supports an installation under `tools\esp-idf-v5.5.5`; do not commit a local toolchain or cached dependencies into the source repository.

## 3. Clone the repository

```powershell
git clone https://github.com/ZerdoumRazine/Waveshare-Glycemie.git C:\dev\waveshare-glycemie
cd C:\dev\waveshare-glycemie
```

Run from the desired branch or release tag. Do not mix source files from different versions.

## 4. Handle credentials safely

**Public builds must not embed passwords.** The touchscreen first-run wizard and the optional `GlycemieConnexion.exe` Windows application collect each user's Wi-Fi and Dexcom Share details after installation.

The legacy private-build option uses `main/device_secrets.example.h` as a template. It is intended for local experimentation only:

```powershell
Copy-Item main\device_secrets.example.h private\device_secrets.h
notepad private\device_secrets.h
```

The `private/` directory is ignored by Git. However, any executable built with this file may contain the credentials: **never distribute that firmware, flash backup or build directory**. Leave the file absent when building for someone else.

## 5. Build the firmware

```powershell
idf.py set-target esp32p4
idf.py build
```

The ESP-IDF component manager restores dependencies from `dependencies.lock`. Use `powershell -ExecutionPolicy Bypass -File tools\build.ps1` as a project-specific helper if preferred; its `-Clean` option forces a fresh build.

If you change `sdkconfig.defaults` or `config/esp32p4_rev1_3.defaults`, regenerate the resolved `sdkconfig` instead of assuming old cached settings will automatically update.

## 6. Validate before flashing

```powershell
python tools\verif_avant_flash.py
```

This checks the ESP32-P4 revision profile, flash and PSRAM settings, image compatibility, partition offsets, expected binaries, and other safety conditions. **Do not flash if any check fails.** The CI firmware workflow performs the same checks on a public build without personal credentials.

## 7. Flash the correct board

Make a backup before experimenting, especially when the device already has settings or history. A full flash copy is sensitive and must remain outside GitHub.

```powershell
python -m esptool --chip esp32p4 -p COM7 read-flash 0 0x2000000 backup_32MB.bin
```

To build and flash through ESP-IDF:

```powershell
idf.py -p COM7 flash monitor
```

The reference partition layout contains the bootloader at `0x2000`, partition table at `0x8000`, application at `0x10000`, and optional speech-recognition models at `0xF10000`. For an application-only update, the project historically used `idf.py -p COM7 app-flash` to avoid rewriting the models.

**Never use `erase-flash`, automatic microSD formatting, eFuse changes, Secure Boot or flash encryption as part of this project's ordinary installation.** The ESP32-C6 coprocessor firmware is not modified by this procedure.

## 8. Configure the touchscreen and microSD

On first start, use the touchscreen setup wizard to select a language, Wi-Fi network, Dexcom Share account and region. The wizard tests a new connection before saving it. The public firmware disables voice-command recognition, while prerecorded announcements and alert tones are supported.

A microSD card is optional but needed for daily CSV history and WAV announcements. Use **FAT32**, including for a 64 GB SDXC card; exFAT is not supported by this firmware. Never reformat a card containing history without making a verified backup.

See [Dexcom setup](DEXCOM_SETUP_EN.md), [the voice tutorial](VOICE_TUTORIAL_EN.md), and [hardware details](HARDWARE_EN.md).

## 9. Run regression tests

```powershell
python tests\run_tests.py
python tests\test_depot.py
python tests\test_assistant_config.py
python tests\test_sd_voice_upload.py
```

GitHub Actions runs Linux and Windows suites plus a full ESP-IDF build. A successful compiler run **does not certify medical use** or prove every first-time setup path on an unused board. Those limitations are documented honestly in [Limitations](LIMITATIONS_EN.md).
