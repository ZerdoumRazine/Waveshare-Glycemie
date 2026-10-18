# Installation et compilation (Windows 10 / 11)

Durée : environ une heure la première fois, dont l'essentiel en téléchargement.

---

## 1. Contraintes à connaître avant de commencer

> ⚠️ **Le chemin du projet ne doit contenir aucun caractère accentué ni non ASCII.**
> ESP-IDF échoue sinon, à trois endroits distincts : `ccache` lève
> `filesystem error: Cannot convert character sequence`, `ldgen.py` lève
> `UnicodeDecodeError` en relisant la sortie d'`objdump`, et l'éditeur de liens ne trouve
> plus les bibliothèques précompilées d'ESP-SR. Les **espaces** dans le chemin sont acceptés.
>
> `C:\dev\waveshare-glycemie` convient ; un dossier contenant un accent, par exemple
> `D:\Projets\Waveshare glycémie`, ne convient pas pour compiler (le dépôt peut y être
> conservé, mais la compilation doit se faire depuis une copie ou un clone dans un chemin
> ASCII). `tools\build.ps1` s'en charge automatiquement.

Autres points :

- ESP-IDF et ses outils (`IDF_TOOLS_PATH`) doivent eux aussi être dans un chemin ASCII.
- Prévoir ≈ 5 Go pour ESP-IDF et la chaîne de compilation, plus ≈ 2 Go pour le projet.
- Aucun droit administrateur n'est nécessaire une fois le pilote série installé.

---

## 2. Pilote du port série

La carte expose un pont **WCH CH343**. Si Windows ne le reconnaît pas automatiquement,
installer le pilote depuis [wch.cn](https://www.wch.cn/) (pilote CH343SER). Ensuite :

```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' } | Select-Object Name
```

Noter le numéro affiché (`USB-Enhanced-SERIAL CH343 (COMx)`) : il remplacera `COM7` dans
toutes les commandes qui suivent.

---

## 3. ESP-IDF 5.5.5

Deux possibilités.

### a. Installateur officiel (le plus simple)

Télécharger l'**ESP-IDF Windows Installer** sur
[dl.espressif.com](https://dl.espressif.com/dl/esp-idf/) et choisir la version **v5.5.5**.
Lors de l'installation, sélectionner la cible **ESP32-P4** (cela évite de télécharger les
chaînes Xtensa inutiles) et un répertoire **sans accent**, par exemple
`C:\Espressif`.

L'installateur ajoute un raccourci « ESP-IDF 5.5 PowerShell » : toutes les commandes de ce
document se lancent depuis cette fenêtre.

### b. ESP-IDF installé dans le projet

Le script `tools\idf_env.ps1` détecte automatiquement une installation placée dans
`tools\esp-idf-v5.5.5` (avec ses outils dans `tools\espressif`) et redirige tous les caches
dans `tools\cache`, sans toucher aux variables d'environnement Windows :

```powershell
. .\tools\idf_env.ps1
```

Si ESP-IDF est ailleurs, définir `IDF_PATH` avant l'appel :

```powershell
$env:IDF_PATH = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
. .\tools\idf_env.ps1
```

> Si l'activation se plaint de `xtensa-esp-elf` manquant, c'est que l'installation a été
> faite pour la cible ESP32-P4 seulement et qu'ESP-IDF ne retrouve pas l'enregistrement de
> cette installation. Corriger avec :
> `python "$env:IDF_PATH\tools\idf_tools.py" install --targets=esp32p4`

---

## 4. Récupérer le projet

```powershell
git clone <url-du-depot> C:\dev\waveshare-glycemie
cd C:\dev\waveshare-glycemie
```

---

## 5. Identifiants privés

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

Le dossier `private/` est ignoré par Git. Ces valeurs sont **compilées dans le firmware** :
ne jamais publier `build/` ni un `.bin` produit.

Détails du compte Dexcom : [CONFIGURATION_DEXCOM_FR.md](CONFIGURATION_DEXCOM_FR.md).
Sans compte Dexcom, laisser les champs Dexcom vides : l'appareil démarre et le **mode
démonstration** permet de tout essayer (`demo normal` dans la console).

---

## 6. Compiler

```powershell
idf.py set-target esp32p4
idf.py build
```

Le gestionnaire de composants télécharge les dépendances à la première compilation
(BSP Waveshare, LVGL, ESP-Hosted, ESP-SR…) en respectant `dependencies.lock`. Compter
plusieurs minutes et ≈ 2 400 fichiers objets.

Résultat attendu en fin de compilation :

```
glycemie_display.bin binary size 0x5d8920 bytes. Smallest app partition is 0x800000 bytes.
0x2276e0 bytes (27%) free.
```

Raccourci fourni : `powershell -ExecutionPolicy Bypass -File tools\build.ps1` (ajouter
`-Clean` pour un `fullclean` préalable).

> Après toute modification de `sdkconfig.defaults` ou de `config/esp32p4_rev1_3.defaults`,
> **supprimer `sdkconfig`** avant de recompiler, sinon les anciennes valeurs persistent.

---

## 7. Vérifier avant de flasher

```powershell
python tools\verif_avant_flash.py
```

23 contrôles de sécurité sur l'image produite : cible, profil rev1.3, taille de flash,
PSRAM, offsets des partitions **identiques à ceux du firmware d'origine**, fichiers
réellement écrits (la NVS n'est jamais dans la liste), absence d'effacement, tailles des
binaires, révisions acceptées, absence d'identifiant dans les fichiers de configuration.

Le script sort en erreur si un seul contrôle échoue. **Ne pas flasher dans ce cas.**

---

## 8. Flasher

```powershell
idf.py -p COM7 flash monitor
```

Sont écrits : bootloader `0x2000`, table de partitions `0x8000`, application `0x10000`,
modèles ESP-SR `0xF10000`. **La NVS n'est jamais écrite** : réglages, dates de pose du
capteur et du cathéter, stocks et verrou Dexcom sont conservés.

Si seule l'application a changé (cas courant), `idf.py -p COM7 app-flash` suffit et évite de
réécrire les 3 Mo de modèles (≈ 80 s au lieu de ≈ 2 min).

**Jamais** d'`erase-flash`, d'`erase-region`, d'eFuse, de Secure Boot ni de Flash
Encryption. Le firmware de l'ESP32-C6 n'est jamais touché.

> **Sauvegarde recommandée avant le premier flash.** Lire l'intégralité de la flash
> (32 Mo) permet de revenir au firmware d'usine :
> `python -m esptool --chip esp32p4 -p COM7 read-flash 0 0x2000000 sauvegarde_32MB.bin`
> Cette sauvegarde peut contenir des données privées : la garder **hors du dépôt**.

---

## 9. Première mise en service

1. Au démarrage, la console affiche la révision de la puce, la PSRAM, l'initialisation de
   l'écran, la détection du GT911, puis le Wi-Fi, SNTP et Dexcom.
2. Page **RÉGLAGES** : régler le volume (85 % recommandé), la luminosité, les seuils, le
   mode nuit, l'intervalle du cathéter.
3. Page **CONSOMMABLES** : appuyer sur **NOUVEAU CAPTEUR** au moment réel de la pose, puis
   renseigner les stocks.
4. Les commandes vocales sont **désactivées** par défaut (elles consomment ≈ 6 Mo de PSRAM
   et ≈ 12 Ko de RAM interne) : à activer dans RÉGLAGES si souhaité.

---

## 10. microSD (optionnelle mais recommandée)

Formater une carte en **FAT32** (une carte de 64 Go doit être formatée explicitement en
FAT32, pas en exFAT), l'insérer, puis vérifier :

```powershell
python tools\serial_capture.py --port COM7 --duration 20 --send "2:sd_check" --log sd.log
```

Sans microSD : pas d'historique CSV et pas de voix (un bip remplace les annonces). Tout le
reste fonctionne.

Pour installer la banque vocale française, voir [LICENCES_TIERCES_FR.md](LICENCES_TIERCES_FR.md)
(la banque n'est pas distribuée avec le dépôt) et `tools\voice\gen_voice_google.py` +
`tools\voice\sd_voice_upload.py`.

---

## 11. Tests

```powershell
python tests\run_tests.py      # tests unitaires sur PC, sans carte
python tests\test_depot.py     # cohérence du dépôt, absence de données privées
```

Voir [../tests/README_FR.md](../tests/README_FR.md).
