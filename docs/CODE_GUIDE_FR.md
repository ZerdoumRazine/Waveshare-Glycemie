# Lire et maintenir le code de WaveShare Glycémie

[English](CODE_GUIDE_EN.md) | **Français**

Ce guide présente l'organisation du code. Les commentaires dans les fichiers `main/`,
`tools/` et `tests/` expliquent les responsabilités des modules et les décisions
importantes, notamment les cas d'erreur. Le code reste en C, Python, PowerShell
et CMake ; **les commentaires n'ajoutent pas de fonctionnalité**.

## Où chercher ?

| Besoin | Fichiers principaux | Rôle |
|---|---|---|
| Démarrage et services | `main/app_main.c`, `main/app_state.c` | Initialiser les tâches et exposer des états cohérents |
| Wi-Fi et Internet | `main/wifi_manager.c`, `main/net_services.c` | Piloter le réseau, DNS, HTTPS et l'heure |
| Dexcom Share | `main/dexcom_share.c`, `main/dexcom_guard.c` | Lire Share et limiter les tentatives d'authentification |
| Comptes utilisateur | `main/credentials.c`, `main/ui_setup.c` | Tester de nouveaux accès sans perdre ceux déjà enregistrés |
| Données de glycémie | `main/glucose_service.c`, `main/glucose_math.c` | Synchroniser, dédupliquer et évaluer l'âge des mesures |
| Écrans tactiles | `main/ui_main.c`, `main/ui_*.c`, `main/ui_common.c` | Navigation, graphiques, valeurs, alertes et réglages |
| Capteur et cathéter | `main/sensor_session.c`, `main/consumables.c`, `main/stock.c` | Dates de changement, rappels, stock |
| Alertes et sons | `main/alerts.c`, `main/audio_out.c`, `main/voice.c` | Priorités, bips et annonces WAV sur microSD |
| Langues | `main/i18n.c`, `tools/voice/langues_voix.py` | Textes d'interface et phrases parlées |
| Historique microSD | `main/sd_storage.c`, `tools/recuperer_microsd.py` | Fichiers journaliers, contrôle et récupération |
| Installation Windows | `tools/installer.ps1`, `tools/assistant_config.py`, `tools/pc_app/` | Flash sécurisé et assistant USB |
| Qualité | `tests/`, `.github/workflows/` | Tests logiciels et compilation ESP-IDF |

## Comment lire les commentaires ?

- **Au début d'un fichier** : son rôle, d'abord en anglais puis en français.
- **Avant une fonction délicate** : l'intention ou la règle de sécurité qui justifie le code.
- **Dans une fonction** : garder des précisions uniquement quand la logique est difficile à déduire.
- **Ne pas commenter chaque affectation** : un commentaire redondant devient vite faux.

Exemple : `credentials_clear()` n'efface pas directement le stockage de manière
aveugle. Il vérifie d'abord l'enregistrement d'une configuration vide, puis
traite l'état auxiliaire. La raison est documentée dans `main/credentials.c`.

## Comment vérifier un changement ?

```powershell
python tests/run_tests.py
python tests/test_depot.py
python tests/test_assistant_config.py
python tests/test_sd_voice_upload.py
```

Ces tests s'exécutent sans appareil mais ne remplacent **pas** la compilation
ESP-IDF et les essais physiques. Les workflows GitHub exécutent les tests C/Python
et la compilation de la version publique.

## Fichiers volontairement laissés tels quels

- `components/esp_driver_ppa/` contient du code adapté depuis Espressif :
  conserver ses en-têtes, sa licence Apache-2.0 et les mentions de modifications.
- `main/voice_clips.h` est généré par `tools/voice/pack_voice.py` ; toute
  modification manuelle disparaîtrait lors d'une nouvelle génération.
- Les fichiers binaires, images, polices et catalogues de ressources ne sont
  pas du code à commenter ligne par ligne.

L'afficheur est un **prototype complémentaire non certifié pour un usage médical**.
Ses valeurs et ses alertes ne doivent pas remplacer celles de Dexcom ou servir
à prendre une décision thérapeutique.
