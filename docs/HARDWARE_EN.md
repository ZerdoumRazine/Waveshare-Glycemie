# Equipment

## The map

**Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**
· [manufacturer documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)
· [submission of examples](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B)

| Element | Value on the project map |
|---|---|
| SoC | ESP32-P4, **revision v1.3** (mandatory pre-v3 profile) |
| Flash | 32 MB NOR GigaDevice (manufacturer) `0xC8`, device `0x4019`) |
| PSRAM | 32 MB |
| Screen | 7′′ 1024 × 600 MIPI-DSI, Controller **EK79007** |
| Tactile | **GT911** in I2C at the address `0x5D`, without pins INT ni RST |
| Wi-Fi / BLE | coprocessor **ESP32-C6** in SDIO via ESP- Hosted (P4 has no radio) |
| Audio | codec **ES8311** → amplifier **NS4150B** → speaker; microphones **ES7210** |
| microSD | slot 0, 4 bits |
| PC link | bridge USB- series **WCH CH343** (VID:PID `1A86:55D3`) |
| Smart security | Secure Boot, Flash Encryption and Secure Download **disabled** |

### Useful broaching (values of BSP, not redefined by the project)

| Function | Brooches |
|---|---|
| I2C (tactile, codec) | SCL **8**, SDA **7** |
| Retrolighting | GPIO **32** (LEDC) |
| Reset screen | GPIO **33** |
| microSD | D0–D3 **39–42**, CMD **44**, CLK **43** |
| Console UART | GPIO * 37 / 38**, 115 200 |
| Audio amp activation | GPIO **53** |

## Chip revision — critical point

The map is a **ESP32-P4 rev1.3**. The project always adds
`config/esp32p4_rev1_3.defaults` **after** `sdkconfig.defaults` (see `CMakeLists.txt`) :

```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
```

Three independent safeguards prevent the production of an incompatible binary:

1. `main/CMakeLists.txt` — `FATAL_ERROR` the CMake configuration;
2. `main/app_main.c` — `#error` compilation;
3. `tools/verif_avant_flash.py` — rereading the compiled image (`image_info` must announce
   accepted revisions **v1.0 to v1.99**) before each flash.

On a map of another revision, this profile must be adapted **with knowledge**:
a rev3.x binary does not start on a rev1.3.

## Table of partitions

Voluntary ** identical to the original firmware** of the card, with only one
partition added **after** the others (their offsets do not move):

| Name | Type | Offset | Size | Role |
|---|---|---|---|---|
| `nvs` | data/nvs | `0x9000` | 24 Ko | settings, sensor, catheter, stock, Dexcom lock |
| `phy_init` | data/phy | `0xF000` | 4 KB | Radio calibration |
| `factory` | app | `0x10000` | 8 MB | application (73% occupied) |
| `storage` | data/spiffs | `0x810000` | 7 MB | reserved |
| `model` | data/spiffs | `0xF10000` | 4 MB | models ESP-SR(`srmodels.bin`, 3 MB) |

Important consequence: **a flash never touches the NVS** The date of installation of the sensor
and catheter, stocks and settings survive a recompilation and reflection.

## microSD

Card in use: **SDXC 64 GB formatted in FAT32** (cluster 32 Kb).

- Mounted **after** the start of Wi-Fi: the controller SDMMC is shared with the link
  SDIO of theESP32-C6.
- **Never automatic formatting** (`format_if_mount_failed = false`,
  `CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL` deactivated). Unrecognized file system
  (exFAT for example) stops writing; Nothing is erased.
- Files used: `/glycemie/history/` (CSV daily), `/glycemie/audio/voice/`
  (voice bank) WAV), `/glycemie/logs/` (event log, `consumables.csv`).
- The main operation never depends on the card: without it, the display, the
  Wi-Fi, Dexcom and alerts continue; Only the voice is replaced by a beep.

## ESP32-C6

The original firmware of the coprocessor ** is never modified** by this project. He responds to the
Wi-Fi commandsESP- Hosted (scan, association, DHCP) but not at version reading
(`GetCoprocessorFwVersion`) : the version remains "unknown", without consequence.

## Audio — measured hardware limit

Volume curve DAC ES8311 35 per cent = -18 dB, 50 per cent = -12 dB, 70 per cent = -6 dB, 85 per cent = -3 dB,
100 % = 0 dB. Micro-map measurements (bip 1 kHz): +6.1 / +6.1 / +2.9 / +3.0 dB
going from 35 → 50 → 70 → 85 → 100%.

At 100%, one beep at -1.5 dBFS **saturates the analog stage** (3rd harmonic to -11 dB for
only +1.3 dB useful): the hardware limit is reached, and push the software more
far adds only distortion. **85 % is the recommended clean level. **

For a real level of alarm, it would require a more sensitive speaker in closed enclosure
(4 or 8 
official card.

## Memory — Stability constraint

The pilot SDIO d'ESPHosted allocates its buffers in **RAM Internal DMA**, and BSP keep his
drawing buffer LVGL 100 Kb in RAM internal. Without precaution, the RAM internal exhaustion
during trade HTTPS (assertion `sdio_push_data_to_queue ... pkt_rxbuff`).

The project therefore requires:

- generic allowances and TLS en PSRAM — `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0`,
  `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y`, `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` ;
- large buffers explicitly in PSRAM : history, Dexcom cache, fusion, copy of
  the interface, screen capture, microSD files, batteries and buffers of voice commands.

The console displays the RAM free internal, minimum sound and largest block DMA
available. Order of magnitude observed in normal operation with voice controls
active: **94 to 104 KB free**, minimum  DMA 27–30 Ko, PSRAM 22 MB free.

## PC connection

Single cable **USB-C** between the card and the PC is enough for power, flash and
Serial console. Under Windows, the bridge CH343 appears as a port COM (COM7 on the machine
of origin); We need the pilot. WCH If Windows doesn't recognize it.

To find the port:

```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' } | Select-Object Name
```

The desired line looks like `USB-Enhanced-SERIAL CH343 (COM7)`.

**Opening serial port does not restart the card** and does not change anything: this is the operation of
Safest diagnosis. See [TROUBLESHOOTING_EN.md](TROUBLESHOOTING_EN.md).
