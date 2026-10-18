# Tests

Deux niveaux, tous les deux exécutables sans carte ESP32 :

```powershell
python tests\run_tests.py          # tests unitaires du code C, compilés et exécutés sur PC
python tests\test_langues_voix.py  # cohérence des six banques vocales (texte seulement)
python tests\test_depot.py         # cohérence du dépôt et absence de données privées
```

Et, une fois le firmware compilé, un troisième avant tout flash :

```powershell
python tools\verif_avant_flash.py   # 23 contrôles sur l'image produite
```

---

## `run_tests.py` — tests unitaires sur PC

Ils compilent le **vrai code du firmware**, sans le modifier :

| Suite | Fichiers du firmware réellement compilés |
|---|---|
| `test_glucose_math` | `main/glucose_math.c`, `main/app_state.c` |
| `test_i18n` | `main/i18n.c` |
| `test_consommables` | `main/sensor_session.c`, `main/consumables.c`, `main/stock.c`, `main/i18n.c` |

Seules les dépendances matérielles sont remplacées, par les bouchons de `tests/hote/shims/` :

| Bouchon | Ce qu'il fait |
|---|---|
| `nvs.h` + `plateforme.c` | **NVS simulée en mémoire**. Un « redémarrage » consiste à rappeler les fonctions d'initialisation sans effacer la NVS — exactement comme au démarrage de la carte |
| `freertos/` | mutex sans effet (les tests sont mono-tâche) |
| `esp_log.h` | journalisation neutralisée (`TEST_LOGS=1` pour la voir) |
| `esp_heap_caps.h` | `malloc` ordinaire |
| `stubs_firmware.c` | heure réseau (pilotable par le test), microSD, journal d'événements |
| `compat_hote.h` | `localtime_r`, `gmtime_r`, `strlcpy` pour MSVC |

Compilateur détecté automatiquement : **gcc**, **clang**, ou **MSVC** (Build Tools Visual
Studio, trouvé via `vswhere`). Si aucun n'est présent, le script le dit clairement et sort
en code 2 — il n'annonce jamais un succès qu'il n'a pas obtenu.

### Ce qui est vérifié

**`test_glucose_math` — 38 contrôles**

- âge de la dernière mesure : absence de mesure, heure non synchronisée, mesure datée du futur ;
- seuils de fraîcheur **exactement** aux bornes : 359 s = À JOUR, 360 s = RETARD,
  599 s = RETARD, 600 s = DONNÉES ANCIENNES ;
- cadence réelle : médiane des intervalles, valeur par défaut quand les intervalles sont
  hors de la plage 4–6 min ;
- variation entre deux mesures : fenêtre 2 min 30 à 7 min 30, bornes incluses, signe correct ;
- variation affichable : masquée si une seule mesure, si l'âge est inconnu ou si la donnée
  est ancienne ;
- prochaine mesure attendue ;
- noms de tendance Dexcom, y compris un code inconnu ;
- historique partagé : borné à 288 mesures, mesure la plus récente en tête, numéro de
  séquence qui change à chaque mise à jour.

**`test_i18n` — 45 contrôles**

- les 6 langues sont déclarées, avec un nom et un code ISO ;
- **aucun des 179 textes n'est vide**, dans aucune des 6 langues ;
- **les spécificateurs de format sont identiques d'une langue à l'autre** : c'est le contrôle
  important, un `%d` perdu en traduction ferait lire un argument inexistant sur la carte ;
- `tr_fr()` reste en français quelle que soit la langue choisie (journal microSD et CSV) ;
- identifiant hors plage : chaîne vide, pas de plantage ;
- dates longues correctes dans les 6 langues, majuscule initiale, « 1er » seulement en français ;
- les 7 noms de jours existent dans chaque langue.

**`test_consommables` — 65 contrôles**

- capteur G7 : échéance = pose + **10 jours exactement** (la grâce ne prolonge rien), jour
  1–10, avancement, « À CHANGER » à la seconde près ;
- rappels **24 h** et **2 h** avant, et absence de rappel après l'échéance ;
- **anti-répétition** : un rappel signalé n'est jamais rejoué ;
- **persistance** : après un « redémarrage » simulé, la date de pose est identique à la
  seconde et les rappels déjà signalés ne sonnent pas à nouveau ;
- pose enregistrée tardivement : les rappels déjà dépassés sont marqués faits, pas de
  rafale de rappels périmés ;
- cathéter : 72 h par défaut, 48 h possible, valeur non prévue refusée, rappel unique 2 h
  avant, persistance après redémarrage ;
- stock : jamais négatif, **une seule décrémentation par changement** (rejeu sans effet),
  « non renseigné » qui ne devient jamais 0, bornage à 99, persistance ;
- refus d'enregistrer un consommable si l'heure n'est pas synchronisée — **sans écraser**
  la date précédente ;
- restauration console : date exacte, masque de rappels, aucune ligne d'historique ajoutée.

### Résultat obtenu le 2026-10-15

```
test_glucose_math : 38 controles, 0 echec(s)
test_i18n         : 45 controles, 0 echec(s)
test_consommables : 65 controles, 0 echec(s)
RESULTAT GLOBAL : TOUTES LES SUITES PASSENT
```

---

## `test_langues_voix.py` — banques vocales

Ne génère aucun audio et n'appelle aucun service : ce sont des contrôles de texte sur
`tools/voice/langues_voix.py`, qui décrit les 396 phrases de chaque langue.

Le contrôle le plus important est le premier : **les phrases françaises doivent rester identiques
au manifeste versionné** (`assets/banque_vocale_manifeste.csv`), qui décrit la banque déjà copiée
sur la microSD. Changer un mot en français rendrait silencieusement obsolètes les fichiers de la
carte. Les autres contrôles vérifient que chaque langue a bien 396 fichiers portant les mêmes noms
qu'en français, qu'aucun texte n'est vide, que deux valeurs de glycémie ne se prononcent jamais
pareil, que les nombres en lettres de 40 à 400 s'écrivent sans erreur, et qu'une voix Chirp 3 HD
est déclarée pour chaque langue.

Résultat obtenu le 2026-10-15 : **56 contrôles, 0 échec**.

---

## `test_depot.py` — contrôles du dépôt

Ne compile rien et n'accède pas à la carte :

- présence des fichiers attendus ;
- **aucun chemin absolu** de machine dans un fichier versionné ;
- **aucun identifiant** : pas de `device_secrets.h`, `.gitignore` correct, et comparaison
  avec les valeurs réelles de `private/device_secrets.h` si ce fichier existe localement ;
- `partitions.csv` **identique** à la disposition d'origine + la partition `model` ;
- profil ESP32-P4 rev1.3 présent et appliqué **après** `sdkconfig.defaults` ;
- cohérence `main/CMakeLists.txt` ↔ fichiers `.c` réellement présents, fichiers embarqués
  existants ;
- **5 pages** déclarées et page STATS non réintroduite ;
- versions de dépendances figées comme historiquement validé ;
- aucun binaire de firmware, aucun fichier de plus de 2 Mo ;
- 5 captures d'écran au format réel 1024 × 600.

---

## Ce qui n'est **pas** couvert ici

Honnêtement, et par construction :

- **Dexcom Share** (HTTPS, authentification, verrou) — non isolable sans ESP-IDF, et il
  n'existe pas de serveur Share de test ;
- **réseau** (coupure, reconnexion, session expirée) — validé sur la carte avec
  `wifi_test_disconnect` et `dexcom_test_session` ;
- **microSD** (déduplication, retrait, remontage) — dépend du pilote SDMMC ; validé sur la
  carte avec `sd_check`, `sd_repair`, `sd_test_eject` ;
- **LVGL et l'affichage** — non instanciable sur PC ici ; vérifié par captures réelles et
  par `ui_stress` sur la carte ;
- **alertes et répétitions audio** — dépendent des horloges FreeRTOS et du codec ; validées
  sur la carte avec `alert_sim`. Seuls les rappels de consommables et leur anti-répétition
  sont couverts en test hôte ;
- **commandes vocales** — les voix de synthèse françaises ne déclenchent presque jamais les
  phrases françaises : la validation demande une voix humaine.

Voir [../docs/LIMITATIONS_FR.md](../docs/LIMITATIONS_FR.md).

---

## Ajouter une suite

1. Écrire `tests/hote/test_<nom>.c` avec `mini_test.h` (`section`, `CHECK`, `CHECK_EQ_I`,
   `test_report`).
2. Ajouter une entrée dans `SUITES` de `run_tests.py` avec les `.c` du firmware à compiler.
3. Si un nouveau symbole de plateforme manque, l'ajouter dans `tests/hote/shims/`.

Les binaires de test sont produits dans `tests/build/` (ignoré par Git).
