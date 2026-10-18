# Matériel

## La carte

**Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**
· [documentation constructeur](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)
· [dépôt d'exemples](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B)

| Élément | Valeur relevée sur la carte du projet |
|---|---|
| SoC | ESP32-P4, **révision v1.3** (profil pré-v3 obligatoire) |
| Flash | 32 Mo NOR GigaDevice (fabricant `0xC8`, device `0x4019`) |
| PSRAM | 32 Mo |
| Écran | 7″ 1024 × 600 MIPI-DSI, contrôleur **EK79007** |
| Tactile | **GT911** en I²C à l'adresse `0x5D`, sans broches INT ni RST |
| Wi-Fi / BLE | coprocesseur **ESP32-C6** en SDIO via ESP-Hosted (le P4 n'a pas de radio) |
| Audio | codec **ES8311** → amplificateur **NS4150B** → haut-parleur ; micros **ES7210** |
| microSD | slot 0, 4 bits |
| Liaison PC | pont USB-série **WCH CH343** (VID:PID `1A86:55D3`) |
| Sécurité puce | Secure Boot, Flash Encryption et Secure Download **désactivés** |

### Brochage utile (valeurs du BSP, non redéfinies par le projet)

| Fonction | Broches |
|---|---|
| I²C (tactile, codec) | SCL **8**, SDA **7** |
| Rétroéclairage | GPIO **32** (LEDC) |
| Reset écran | GPIO **33** |
| microSD | D0–D3 **39–42**, CMD **44**, CLK **43** |
| Console UART | GPIO **37 / 38**, 115 200 bauds |
| Activation ampli audio | GPIO **53** |

## Révision de la puce — point critique

La carte est une **ESP32-P4 rev1.3**. Le projet ajoute toujours
`config/esp32p4_rev1_3.defaults` **après** `sdkconfig.defaults` (voir `CMakeLists.txt`) :

```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y
```

Trois garde-fous indépendants empêchent de produire un binaire incompatible :

1. `main/CMakeLists.txt` — `FATAL_ERROR` à la configuration CMake ;
2. `main/app_main.c` — `#error` à la compilation ;
3. `tools/verif_avant_flash.py` — relecture de l'image compilée (`image_info` doit annoncer
   des révisions acceptées **v1.0 à v1.99**) avant chaque flash.

Sur une carte d'une autre révision, ce profil doit être adapté **en connaissance de cause** :
un binaire rev3.x ne démarre pas sur une rev1.3.

## Table de partitions

Volontairement **identique à celle du firmware d'origine** de la carte, avec une seule
partition ajoutée **après** les autres (leurs offsets ne bougent pas) :

| Nom | Type | Offset | Taille | Rôle |
|---|---|---|---|---|
| `nvs` | data/nvs | `0x9000` | 24 Ko | réglages, capteur, cathéter, stock, verrou Dexcom |
| `phy_init` | data/phy | `0xF000` | 4 Ko | calibration radio |
| `factory` | app | `0x10000` | 8 Mo | application (≈ 73 % occupés) |
| `storage` | data/spiffs | `0x810000` | 7 Mo | réservé |
| `model` | data/spiffs | `0xF10000` | 4 Mo | modèles ESP-SR (`srmodels.bin`, ≈ 3 Mo) |

Conséquence importante : **un flash ne touche jamais la NVS**. Les dates de pose du capteur
et du cathéter, les stocks et les réglages survivent à une recompilation et à un reflash.

## microSD

Carte en service : **SDXC 64 Go formatée en FAT32** (cluster 32 Ko).

- Montée **après** le démarrage du Wi-Fi : le contrôleur SDMMC est partagé avec la liaison
  SDIO de l'ESP32-C6.
- **Jamais de formatage automatique** (`format_if_mount_failed = false`,
  `CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL` désactivé). Un système de fichiers non reconnu
  (exFAT par exemple) arrête l'écriture ; rien n'est effacé.
- Dossiers utilisés : `/glycemie/history/` (CSV quotidiens), `/glycemie/audio/voice/`
  (banque vocale WAV), `/glycemie/logs/` (journal d'événements, `consumables.csv`).
- Le fonctionnement principal ne dépend jamais de la carte : sans elle, l'affichage, le
  Wi-Fi, Dexcom et les alertes continuent ; seule la voix est remplacée par un bip.

## ESP32-C6

Le firmware d'origine du coprocesseur **n'est jamais modifié** par ce projet. Il répond aux
commandes Wi-Fi d'ESP-Hosted (scan, association, DHCP) mais pas à la lecture de version
(`GetCoprocessorFwVersion`) : la version reste affichée « inconnue », sans conséquence.

## Audio — limite matérielle mesurée

Courbe de volume du DAC ES8311 : 35 % = −18 dB, 50 % = −12 dB, 70 % = −6 dB, 85 % = −3 dB,
100 % = 0 dB. Mesures au micro de la carte (bip 1 kHz) : +6,1 / +6,1 / +2,9 / +3,0 dB en
passant de 35 → 50 → 70 → 85 → 100 %.

À 100 %, un bip à −1,5 dBFS **sature l'étage analogique** (3ᵉ harmonique à −11 dB pour
seulement +1,3 dB utile) : la limite matérielle est atteinte, et pousser le logiciel plus
loin n'ajoute que de la distorsion. **85 % est le niveau propre recommandé.**

Pour un vrai niveau d'alarme, il faudrait un haut-parleur plus sensible en enceinte fermée
(4 ou 8 Ω, 2 à 3 W), voire un amplificateur externe — montage à valider sur le schéma
officiel de la carte.

## Mémoire — contrainte de stabilité

Le pilote SDIO d'ESP-Hosted alloue ses tampons en **RAM interne DMA**, et le BSP garde son
tampon de dessin LVGL (≈ 100 Ko) en RAM interne. Sans précaution, la RAM interne s'épuise
pendant les échanges HTTPS (assertion `sdio_push_data_to_queue ... pkt_rxbuff`).

Le projet impose donc :

- allocations génériques et TLS en PSRAM — `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0`,
  `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y`, `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` ;
- gros tampons explicitement en PSRAM : historique, cache Dexcom, fusion, copie de
  l'interface, capture d'écran, files de la microSD, piles et tampons des commandes vocales.

La console affiche chaque minute la RAM interne libre, son minimum et le plus grand bloc DMA
disponible. Ordre de grandeur observé en fonctionnement normal avec les commandes vocales
actives : **94 à 104 Ko libres**, minimum ≈ 68 Ko, bloc DMA 27–30 Ko, PSRAM ≈ 22 Mo libres.

## Raccordement au PC

Un seul câble **USB-C** entre la carte et le PC suffit pour l'alimentation, le flash et la
console série. Sous Windows, le pont CH343 apparaît comme un port COM (COM7 sur la machine
de développement d'origine) ; il faut le pilote WCH si Windows ne le reconnaît pas.

Pour retrouver le port :

```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' } | Select-Object Name
```

La ligne recherchée ressemble à `USB-Enhanced-SERIAL CH343 (COM7)`.

**Ouvrir le port série ne redémarre pas la carte** et ne modifie rien : c'est l'opération de
diagnostic la plus sûre. Voir [DEPANNAGE_FR.md](DEPANNAGE_FR.md).
