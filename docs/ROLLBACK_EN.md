# Save, go back, troubleshoot a screen that no longer starts

This document serves **before** an update, and **if something goes wrong**. It concerns only
screen (processor ESP32-P4. The second processor, theESP32-C6 that provides Wi-Fi, **is never
** affected: neither erased nor reprogrammed.

---

## 1. What the update changes, and what it does not change

**It replaces** one thing: the screen program (partition) `app`, about 3.5 MB).

**It does not affect:**

| What is preserved | Where it's tidy |
|---|---|
| Language, volume, night mode, thresholds | settings memory (`nvs`, space `settings`) |
| Sensor and catheter dates, stock | settings memory (`nvs`, consumable spaces) |
| History of measurements, newspapers | microSD |
| Voice banks (six languages) | microSD |
| Wi-Fi network and Dexcom account | see item 3 below |

** None of these transactions are used**: `erase_flash` (general erasure), eFuse writing,
Secure Boot, flash encryption, microSD formatting. They are irreversible or
the non-recoverable device: they will never be done without explicit and separate authorization.

**Interruptions to be expected:** the screen restarts twice (once to enter programming mode,
Once after), and remain off during writing, about **one to two minutes**. Meanwhile,
No measurements are displayed. Official Dexcom phone and receiver continue to work
normally: they do not depend on this screen.

---

## 2. Save AVANT (mandatory)

Connect the screen to the PC with a cable USB-C **of data**, then:

```bash
python tools\sauvegarde_appareil.py --complet
```

The tool reads the device, writes the copy in `private\sauvegardes\<date>\`, the **reliance and comparison**
Apparatus (`verify_flash`), and records fingerprints SHA-256 in `manifeste.json`.
He never writes in the camera.

- `--identifier` : identification alone, nothing is copied.
- without option: only useful parts (program, partition table, settings) are copied.
- `--complet` : all memory (32 MB) — it is the backup that allows **all** to put back as before.

These backups remain **private**: the folder `private\` is never published, and archives
are verified to ensure this.

> Keep a copy of the backup folder away from the PC (key USB, external disk).
> A backup on the only disk that can fail is not a backup.

**Save the microSD as well.** Two ways:

- *with card reader*: screen turned off, card in PC, then

  ```bash
  python tools\sauvegarde_microsd.py --lecteur E:
  ```

  The map is only read; each file is copied and then **rereaded on both sides and compared**.

- *without card reader, via cable USB*: the card remains in the camera.

  ```bash
  python tools\recuperer_microsd.py --port COM7
  ```

  The tool makes inventory, recovers logs and**history** (command `sd_dump`,
  present since K22), checks the print CRC-32 of each file, and compares the sounds of the
  card to a bank already present on the PC so as not to transfer anything unnecessarily.

In both cases: do not format the card, and do not "repair" it if Windows offers it.

---

## 3. Where are your current accesses, and how they pass in the new version

| Version | Where are Wi-Fi and Dexcom account |
|---|---|
| Old (before this update) | ** written in the program** at the time of compilation, since `private\device_secrets.h` |
| New | **in the memory of settings** (space `ident`), entered on screen or from PC |

The new version maintains compatibility: if memory `ident` is empty and the program has been
compiled with `private\device_secrets.h`, ** these values serve as the starting point**. In other words, if you
Install on your screen the program compiled from your own folder (which contains
`private\device_secrets.h`), **he reconnects himself, without grasping anything**.

Two possible paths:

1. **The simplest**: install the compiled program at home (`tools\build.ps1`). Your access is
   automatic. You can then, when you want, save them in memory
   settings from the Windows app or touch screen, so you don't depend on the compilation anymore.
2. **Cleanest**: install the public program (without any identifier), then enter your accesses
   with the touch assistant or Windows application. This is exactly what a person who
   discover the project.

Passwords are never requested in a conversation, never written in a newspaper, and
Never displayed: the screen and application show only the length.

---

## 4. Back to your current version

After a backup `--complet`, the reverse is to rewrite what has been saved.

1. Connect the screen to USB.
2. Open window **ESP-IDF 5.5 PowerShell** (it defines the necessary variables).
3. Return the saved program:

```bash
python -m esptool --port COM7 --baud 460800 write_flash 0x10000 private\sauvegardes\<date>\app_0x10000.bin
```

With a complete backup, the same command with `0` et `flash_complete.bin` gives **all**:

```bash
python -m esptool --port COM7 --baud 460800 write_flash 0 private\sauvegardes\<date>\flash_complete.bin
```

4. Unplug, reconnect: the screen restarts on your old version.

> Check the port number (`python tools\assistant_config.py --lister`). On this PC, the map
> appears as **USBEnhanced-SERIAL CH343**.

---

## 5. If screen no longer starts

A screen that remains black, reboots in loop or displays nothing ** is not lost**: the mode
programming is wired in the processor, it does not depend on the installed program.

In this case, the menu CONNEXION and Windows app ** is not enough**: they assume that the
program starts. You have to go through the programming mode.

1. Disconnect the power from the screen.
2. Keep the button **BOOT** of the card pressed.
3. Reconnect cable USB-C **without releasing** the button, wait two seconds, then release.
4. The serial port must reappear on the PC (`python tools\assistant_config.py --lister`).
5. Rewrite your backup (point 4 above).

If the port does not appear: change the cable (many cables) USB-C only transport the current),
change take USB, and check that it's the catch USB **card** used.

If the screen starts but the image is black while the sound works, it is the display or its tablecloth
We need to check, not the program.

---

## 6. What this procedure does not cover

- A hardware failure (screen, power, card): no software backup fixes it.
- A defective microSD: keep a copy of its contents, cheap cards wear out.
- A Dexcom account blocked after too many tests: you have to wait, it is a Dexcom protection.
- The second processor ESP32-C6 (Wi-Fi): it is neither saved nor reprogrammed by these tools, because
  it is not modified. If it broke down, we'd need Waveshare's original firmware.

---

*This screen is an experimental complementary display. He may be wrong, late, or
Stop. It does not replace official arrangements and should not guide a decision to
treatment. *
