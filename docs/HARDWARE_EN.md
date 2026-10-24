# Hardware reference

## Tested board

**Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** — [official documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B) · [manufacturer examples](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B).

| Component | Hardware used during development |
|---|---|
| Main SoC | ESP32-P4 **revision v1.3** (pre-v3 profile) |
| Flash | 32 MB GigaDevice NOR |
| PSRAM | 32 MB |
| Display | 7-inch, 1024 × 600 MIPI-DSI, EK79007 controller |
| Touch | GT911 on I2C address `0x5D` |
| Wi-Fi | ESP32-C6 coprocessor via SDIO / ESP-Hosted |
| Audio | ES8311 codec, NS4150B amplifier, ES7210 microphones |
| microSD | SDMMC slot 0, 4-bit bus |
| PC connection | WCH CH343 USB-UART bridge (`1A86:55D3`) |

Useful BSP pin assignments (do not change without reviewing the manufacturer schematic): touch/audio I2C SCL 8, SDA 7; backlight GPIO 32; display reset GPIO 33; microSD D0–D3 GPIO 39–42, CMD 44, CLK 43; UART GPIO 37/38 at 115200 baud; amplifier enable GPIO 53.

## Chip revision compatibility

This firmware is built for **ESP32-P4 pre-v3 hardware**, with a revision floor of v1.0 and ceiling of v1.99. The configuration overlays `config/esp32p4_rev1_3.defaults` after `sdkconfig.defaults`.

```text
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
```

CMake, a compile-time guard and `tools/verif_avant_flash.py` reject incompatible configurations. **Do not flash this binary to a rev3.x board or another Waveshare model without a separately validated build.**

## Reference flash layout

| Partition | Offset | Size | Purpose |
|---|---|---|---|
| `nvs` | `0x9000` | 24 KB | Persistent settings, dates, counts and account state |
| `phy_init` | `0xF000` | 4 KB | PHY information |
| `factory` | `0x10000` | 8 MB | Main application |
| `storage` | `0x810000` | 7 MB | Reserved storage |
| `model` | `0xF10000` | 4 MB | Optional legacy ESP-SR models |

The partition layout keeps the original NVS offset. Ordinary app flashing does not intentionally erase NVS, although hardware failures can still require restoring a verified backup.

## microSD

The development card was **64 GB SDXC formatted as FAT32**; exFAT is not supported by the installed firmware. Card mounting happens after Wi-Fi startup because of shared SDMMC/SDIO resources. Automatic formatting is explicitly disabled. A mount error must not erase the card.

Typical folders include `glycemie/history/` for daily CSV readings, `glycemie/logs/` for events and consumable logs, and `glycemie/audio/voice/` for optional WAV announcements. Glucose display and fallback alert beeps continue without a card, but CSV persistence and spoken phrases require it.

## Wi-Fi coprocessor

The ESP32-C6 uses its original firmware and is not flashed by this project. It provides Wi-Fi scanning, connection and DHCP services through ESP-Hosted. Failure to query its firmware version does not imply Wi-Fi is unusable.

## Audio and brightness

Historical measurements on the development device showed audible analogue distortion at **100%** output. Approximately **85%** was the practical recommended clean level. These are observations for the tested hardware and speaker, not a guarantee of alarm loudness in another room.

## Memory stability

The Wi-Fi SDIO driver and LVGL drawing use internal DMA-capable memory. Large caches and selected TLS buffers are directed to PSRAM so the internal memory is not exhausted during network and graphical activity. A decrease in free PSRAM while font glyph caches warm up is not automatically a memory leak; use the project's memory diagnostics and stress tests to investigate changes.

## Connecting the board to Windows

Connect the **UART/CH343 USB-C** port with a data-capable cable. In Device Manager, find `USB-Enhanced-SERIAL CH343 (COMx)`. This COM port supports diagnostics, firmware updates and the optional Windows setup application. A specific COM number (such as COM7) is only an example.

For software configuration, see [Installation](INSTALLATION_EN.md) and [Troubleshooting](TROUBLESHOOTING_EN.md). Always keep full-flash backups and real glucose logs private.
