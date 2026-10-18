# Journal de récupération et de reconstitution du dépôt

**Date : 15 octobre 2026.** Ce document dit exactement d'où vient chaque fichier de ce
dépôt : ce qui a été récupéré à l'identique, ce qui a été réécrit, et ce qui a été produit
pour l'occasion. Il est volontairement détaillé pour qu'on puisse le vérifier.

---

## Point de départ

Le dossier de travail historique avait disparu de son emplacement
d'origine et le projet était considéré comme perdu.

## Ce qui a été retrouvé

Une recherche sur les disques D:, E:, F: et G: portant sur les noms de fichiers
(`glyc`, `waveshare`, `K17`, `sdkconfig`, `partitions.csv`, `idf_component.yml`, `dexcom`)
a localisé le projet **intact** :

un **dossier d'archive du même disque**, sous son nom d'origine
(l'emplacement exact est noté localement : il n'a pas sa place dans un dépôt public).

Le dossier n'avait pas été supprimé : il avait été **déplacé**. Il contenait :

| Élément | État |
|---|---|
| `firmware/glycemie_display/main/` | tous les `.c` / `.h`, dernière modification 2026-09-21 |
| `firmware/glycemie_display/` | `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `dependencies.lock`, `config/`, `components/esp_driver_ppa/` |
| `firmware/snapshots/` | **12 sauvegardes** : `V1_STABLE`, `J2_STABLE_BIN`, `K10` à `K19`, dont le `K17_ACCUEIL_XXL_BIN` recherché |
| `backup_original/` | 3 copies de la flash d'usine de 32 Mo |
| `tools/` | environnement, vérification avant flash, capture série, génération et copie de la banque vocale, **599 journaux numérotés** (jusqu'à 366) et 191 captures d'écran PNG |
| `private/device_secrets.h` | identifiants réels (jamais copiés dans ce dépôt) |
| `PASSATION.md`, `README.md` (344 lignes) | documentation technique d'origine |

Vérifications d'intégrité faites :

- les trois sauvegardes de flash donnent **exactement** le SHA-256 attendu
  `f2ece1fc3dbdfc3bd84db8f3b8a89c40851433f846b819aa8364e813346ce8b8` ;
- le contenu de `main/` est **identique**, fichier par fichier, à celui enregistré dans la
  sauvegarde `K19_VOIX_CAPTEUR_BIN` : les sources sur le disque sont bien celles du dernier
  firmware flashé et validé ;
- le dernier journal est le n° 366 (`366_K19_etat_final.log`) : **aucun développement n'a eu
  lieu après K19**.

En complément, l'historique local de Claude Code
(dossier `.claude\projects` du profil utilisateur, 10 sessions, ≈ 89 Mo, plus
15 notes de mémoire allant jusqu'à K19) a servi à **recouper** l'historique des versions,
les décisions et les mesures. Il n'a pas été nécessaire d'y reconstruire du code : les
sources d'origine étaient disponibles.

---

## Ce qui a été fait pour constituer ce dépôt

Le dossier d'origine n'a **pas** été modifié : il a seulement été lu.

### Récupéré à l'identique (copie exacte)

- `main/` — tous les fichiers `.c` et `.h`, `fonts/`, `voice/`, `idf_component.yml`
- `components/esp_driver_ppa/` — le correctif PPA et son `CORRECTIF.txt`
- `config/esp32p4_rev1_3.defaults`
- `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `dependencies.lock`
- `tools/verif_avant_flash.py`, `tools/serial_capture.py`, `tools/scan_secrets.py`,
  `tools/voice/gen_voice.ps1`, `gen_voice_google.py`, `pack_voice.py`, `sd_voice_upload.py`

### Modifié uniquement pour rendre le projet portable

Quatre fichiers contenaient le chemin absolu de la machine d'origine, **dans des
commentaires ou des docstrings** ; ils ont été réécrits pour ne plus dépendre d'un
emplacement :

| Fichier | Modification |
|---|---|
| `main/CMakeLists.txt` | `../../../private` → `../private` (le projet est maintenant à la racine du dépôt) + commentaire |
| `main/app_secrets.h` | commentaire |
| `main/device_secrets.example.h` | commentaire + commande de copie |
| `components/esp_driver_ppa/CORRECTIF.txt` | origine indiquée comme `$IDF_PATH/components/esp_driver_ppa` |
| `tools/verif_avant_flash.py` | docstring, et `PROJECT` pointe sur la racine du dépôt |
| `tools/voice/pack_voice.py` | chemin de sortie vers `main/` |

**Aucune ligne de logique n'a été modifiée.** Le code compilé est celui de K19.

### Réécrit pour le dépôt

- `tools/idf_env.ps1` — réécrit : détecte la racine du projet depuis sa propre position,
  accepte un ESP-IDF installé ailleurs via `IDF_PATH`, ne touche à aucune variable Windows.
- `.gitignore` — réécrit et élargi (identifiants, binaires, sauvegardes de flash, journaux,
  historique glycémique, environnements locaux).

### Créé pour cette livraison

- `README.md` (anglais), `README_FR.md` (français), `LICENSE`, `CREDITS.md`
- `docs/` — les onze documents de ce dossier
- `tests/` — suite de tests sur PC (voir ci-dessous) et contrôles du dépôt
- `tools/build.ps1`
- `assets/banque_vocale_manifeste.csv` — liste des phrases de la banque vocale
- `screenshots/` — cinq captures réelles

### Volontairement **non** copié dans le dépôt

| Élément | Raison |
|---|---|
| `private/device_secrets.h` | identifiants Wi-Fi et Dexcom réels |
| `backup_original/*.bin` (3 × 32 Mo) | sauvegardes de flash, susceptibles de contenir des données privées |
| `firmware/snapshots/**` (binaires et ELF) | firmwares compilés : les identifiants y sont inclus |
| `tools/logs/` (599 fichiers) | journaux de mise au point, contenant des glycémies réelles |
| `tools/voice/bank/` (23 Mo de WAV) | audio généré par un service tiers, trop volumineux et à la licence distincte |
| `tools/esp-idf-v5.5.5/`, `tools/espressif/`, `.venv/` | outils installés localement |
| `sources/` | dépôts tiers clonés pour étude |

---

## Vérifications réellement exécutées le 2026-10-15

| Vérification | Résultat |
|---|---|
| Compilation ESP-IDF 5.5.5 du dépôt reconstitué | **réussie** — 2 433 cibles, `glycemie_display.bin` = 6 129 952 o, 27 % de la partition libre |
| `tools/verif_avant_flash.py` | **23 / 23 contrôles OK** — « PRÊT À FLASHER » |
| Tests unitaires sur PC (`tests/run_tests.py`) | **103 contrôles, 0 échec** (38 + 65) |
| Comparaison `main/` du dépôt ↔ sauvegarde K19 | identique |
| SHA-256 des trois sauvegardes de flash | conformes à la valeur attendue |
| Captures des 5 pages sur la carte réelle | obtenues par la console `snap`, en mode démonstration |

**Non exécuté** : aucun flash, aucun effacement, aucune écriture NVS, aucun formatage de la
microSD. La carte n'a été utilisée qu'en **lecture** (console série) et en mode
démonstration, qui ne modifie rien de persistant.

---

## Compilation : limite rencontrée

Le dossier de livraison demandé contient un **accent**.
ESP-IDF ne le supporte pas : la compilation échoue successivement dans `ccache`
(`Cannot convert character sequence`), dans `ldgen.py` (`UnicodeDecodeError` sur la sortie
d'`objdump`) et dans l'éditeur de liens (bibliothèques d'ESP-SR introuvables). Une jonction
NTFS ne suffit pas : CMake résout le chemin réel.

La compilation de validation a donc été faite depuis une **copie du dépôt dans un chemin
ASCII**, ce qui est aussi la recommandation officielle d'Espressif. Les fichiers livrés
sont identiques. Cette contrainte est signalée dans le README et dans
[INSTALLATION_FR.md](INSTALLATION_FR.md).

---

## Où se trouve encore l'original

Le dossier d'origine **n'a pas été déplacé, modifié ni supprimé**.
Il reste la référence pour tout ce qui n'est pas publiable : sauvegardes de flash,
journaux, banque vocale, identifiants, et les 12 sauvegardes binaires permettant de revenir
à n'importe quelle version antérieure.
