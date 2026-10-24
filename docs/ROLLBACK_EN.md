# Backing up and restoring the Waveshare display

This procedure concerns only the **ESP32-P4 display processor**. The project's normal flashing procedure does not replace the ESP32-C6 Wi-Fi coprocessor firmware. Read this guide **before** flashing and keep official Dexcom monitoring available while the supplementary screen is offline.

## What an ordinary application update changes

An application-only update replaces the application partition. It does not intentionally rewrite the NVS configuration partition, the microSD, or the ESP32-C6. Language selection, thresholds, sensor and infusion-set dates, stock, history, and audio should remain intact during a normal app update.

This is not a guarantee against power failure, a damaged card, or incompatible firmware. Never substitute a full-flash erase, eFuse operation, Secure Boot change, flash encryption or automatic microSD formatting for a proper recovery procedure.

## 1. Back up internal flash

Connect the board with a USB-C **data cable** to its UART/CH343 port. The project provides:

```powershell
python tools\sauvegarde_appareil.py --complet
```

The tool performs a read-only backup of the full **32 MB flash**, compares the image against a second device read, and records file hashes and a manifest in a private backup directory. `--identifier` performs device identification only; the normal mode copies selected partitions rather than the entire flash.

**The backup is sensitive**: it can contain personal Wi-Fi/Dexcom details, health readings or settings. Never put it in GitHub, a public Release, or a support issue. Store an additional verified copy on another trusted drive.

## 2. Back up the microSD

With a card reader, power off the screen, remove the card, and use a verified file-by-file copy. The bundled command accepts the selected drive letter:

```powershell
python tools\sauvegarde_microsd.py --lecteur E:
```

Without a reader, leave the card in the powered-on screen and recover its contents over the USB serial cable (supported by K22 and later firmware):

```powershell
python tools\recuperer_microsd.py --port COM7
```

Replace `E:` and `COM7` with the actual drive or port. These tools are designed to inspect and verify data; read the on-screen messages before approving any operation. **Never accept Windows' offer to format or automatically repair a card containing the only history copy.**

## 3. Understand credential compatibility

Older private builds could embed the Wi-Fi and Dexcom details through `private/device_secrets.h`. Current public builds do not embed those details: the user configures the display on the touchscreen or with the Windows USB application, and the accepted settings are stored in NVS.

If an old binary is restored, its earlier credential behavior returns with it. Treat every flash backup and private build as secret-bearing even when its filename looks harmless. The project does not guarantee forensic erasure of credentials previously present in flash.

## 4. Restore an application partition

**Only restore a backup from the same board and a known compatible partition layout.** Record the verified path, port and checksum before proceeding. After entering the board's programming mode, the historical restore command for an application-partition backup is:

```powershell
python -m esptool --port COM7 --baud 460800 write_flash 0x10000 private\sauvegardes\<date>\app_0x10000.bin
```

The command is an example. Replace `<date>` with the actual verified backup folder. A wrong board, path or offset can make the installation unusable.

## 5. Full flash recovery — use only a verified image

Restoring a full 32 MB image is a different and much more invasive operation: it restores **all partitions**, including NVS and private settings, not just the application. The historical command is:

```powershell
python -m esptool --port COM7 --baud 460800 write_flash 0 private\sauvegardes\<date>\flash_complete.bin
```

Use it only when you deliberately want the exact earlier state and have verified that the image belongs to this board. It is **not** the standard update procedure, and it can overwrite newer settings or data.

## 6. Recover a device that no longer boots

The ESP32-P4 bootloader has a hardware programming mode, independent of the installed application:

1. Disconnect power.
2. Hold the board's **BOOT** button.
3. Connect its UART USB-C data cable while continuing to hold BOOT; release after a short interval.
4. Confirm that Windows sees the CH343 COM port.
5. Restore the correct verified image using the procedure above.

If the port is missing, check cable data support, the connector, the USB port and the WCH driver. A display ribbon, board or power-supply failure cannot be fixed by flashing. A non-booting app also means the in-app Connection menu and Windows configuration wizard cannot help until firmware boot is restored.

## Limits

These procedures do not repair physical damage, a failed SD card or an account blocked by Dexcom. They do not back up or reprogram the ESP32-C6. See [Hardware](HARDWARE_EN.md), [Recovery](RECOVERY_EN.md) and [Known limitations](LIMITATIONS_EN.md).

The device is an experimental supplemental display, not a primary medical monitor or treatment-decision tool.
