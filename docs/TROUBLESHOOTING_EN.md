# Troubleshooting and frequently asked questions

These procedures are for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** project. Always retain the official Dexcom monitoring app and alarms when troubleshooting this supplementary screen.

## First check: serial diagnostics

Connect to the board's **UART/CH343** USB-C port and identify the COM port in Windows Device Manager. Use the project's serial-capture utility:

```powershell
python tools\serial_capture.py --port COM7 --duration 60 --send "3:info" --log diag.log
```

Replace COM7 with your actual port. An ordinary read-only diagnostic session does not flash the board. The `--reset` option and certain explicit diagnostic commands can affect device state, so do not invoke them casually. **Keep the resulting log private**: redaction is not a guarantee that all health information is removed.

The `info` output reports Wi-Fi, Internet, clock, Dexcom, SD card, sound, memory and firmware state. Use those indicators to narrow down the failure rather than replacing everything at once.

## The screen will not start

Confirm that the USB cable supports data and provides stable power, that the correct USB-C connector is used, and that the board is the supported pre-v3 revision. Check the build's partition and revision profile before flashing anything. If the firmware cannot boot, use the ROM programming method and a verified private backup as described in [Rollback](ROLLBACK_EN.md).

Do not attempt a general flash erase, eFuse change, Secure Boot modification or card format as a troubleshooting shortcut.

## Touch coordinates are wrong or the interface freezes

The touch configuration validated on this board uses `swap_xy=0`, `mirror_x=0`, `mirror_y=0`. Changing the example BSP's mirroring options can invert touches. The local `components/esp_driver_ppa` implements the rotated-partial-rendering PPA fix; removing it can reproduce a serious LVGL freeze.

Historical diagnostics included `ui_info`, stress tests and a core-dump analysis. A normal log is preferable to aggressive stress testing when the device is used for family glucose observation.

## No new Dexcom readings

Check whether the **official Dexcom phone application** is uploading data, Share is active, the network is online, and the displayed reading is genuinely recent. Then open **Settings → Connection** and inspect the saved account status. A refused Dexcom login should not be retried rapidly: the authentication guard is designed to prevent account lockouts.

The diagnostic console includes commands such as `dexcom_status`, `dexcom_auth`, `dexcom_test_session`, and `wifi_test_disconnect`. The latter commands deliberately change session or network state: use only in a controlled test, never as routine diagnosis.

## microSD problems

The card must use **FAT32**, including on the 64 GB card used during development. An exFAT card will not be mounted by this firmware. The application deliberately does not auto-format a card.

Diagnostic commands include `sd_check` and the more intrusive `sd_repair`, `sd_test_eject`, and `sd_test_remount`. Take a verified backup before running a repair operation. Logs and real glucose CSV files must not be uploaded to the public repository.

## Missing sound or speech

The alert **beep** is generated independently of the optional speech bank. For spoken announcements, check the speaker, volume and microSD path `glycemie/audio/voice/`. French WAV files live directly in `voice/`; other language clips live in `voice/en/`, `voice/es/`, `voice/de/`, `voice/it/`, or `voice/pt/`.

Use the [voice tutorial](VOICE_TUTORIAL_EN.md) to validate the bank and, where supported, run a read-only checksum comparison. Voice-command recognition is **disabled in the public build**, so microphone wake commands are not a supported troubleshooting path.

## Sensor and infusion-set dates

Check the Consumables page and saved dates before changing them. An unexpected stock decrement or repeated reminder is a software issue to document, not a reason to reset all NVS. Routine reboots and firmware application updates should retain dates and counts.

## Frequently asked questions

**Can it display mmol/L?** No; the current implementation uses mg/dL.

**Can I change Wi-Fi/Dexcom without recompiling?** Yes. Use the touchscreen Settings → Connection wizard or the Windows USB application.

**Does it connect directly to a G7 sensor?** No. It reads the account's Dexcom Share data uploaded by the phone.

**Does it use Nightscout?** No.

**Can I demonstrate it without an account?** Yes. Demo mode uses fictitious readings and is visibly marked as such.

**Can Dexcom Share stop working?** Yes. It is an undocumented service interface and may change without notice. Never depend on this project for primary medical alarms.

See [Installation](INSTALLATION_EN.md), [Recovery](RECOVERY_EN.md) and [Limitations](LIMITATIONS_EN.md) for further detail.
