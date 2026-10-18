# Dépannage et FAQ

## Avant tout : la console série ne change rien

Ouvrir le port série **ne redémarre pas la carte** et n'écrit rien en flash. C'est
l'opération de diagnostic la plus sûre.

```powershell
python tools\serial_capture.py --port COM7 --duration 60 --send "3:info" --log diag.log
```

L'outil **masque** automatiquement, avant d'écrire le journal, toute valeur présente dans
`private/device_secrets.h` ainsi que tout UUID (session, compte).

Options utiles : `--reset` (redémarre la carte par RTS), `--send "DELAI:commande"`,
`--png capture.png` (décode une capture d'écran `snap`), `--until "<regex>"`.

Ligne d'état renvoyée par `info` :

```
INFO wifi=2 ip=192.168.1.x rssi=-52 internet=2 heure=2 dexcom=3 demo=0 sd=2
     sd_lignes=0 sd_libre_mo=59657 audio=1 derniere=112 tendance=Flat age_s=140
     heap_int=103395 heap_int_min=70360 dma_bloc=32768 psram=26170412 vcmd=0
```

---

## Problèmes de compilation

| Symptôme | Cause et solution |
|---|---|
| `filesystem error: Cannot convert character sequence` | **accent dans le chemin du projet**. Compiler depuis un chemin purement ASCII. |
| `UnicodeDecodeError` dans `ldgen.py` | même cause. |
| `ld.exe: cannot find ...libmultinet.a: No such file or directory` | même cause (chemin accentué vers `managed_components`). |
| `tool xtensa-esp-elf has no installed versions` | l'installation ESP-IDF ne retrouve pas son enregistrement pour ce chemin. `python "$env:IDF_PATH\tools\idf_tools.py" install --targets=esp32p4` |
| `sdkconfig incompatible avec l'ESP32-P4 rev1.3` | la configuration résolue n'est pas en profil pré-v3. Supprimer `sdkconfig` et recompiler ; vérifier que `CMakeLists.txt` ajoute bien `config/esp32p4_rev1_3.defaults` **après** `sdkconfig.defaults`. |
| Une modification de `sdkconfig.defaults` reste sans effet | `sdkconfig` existant prime. Le supprimer, puis recompiler. |
| Avertissement traité comme une erreur | le projet compile avec `-Werror` (ex. `format-truncation`). Corriger l'avertissement, ne pas désactiver l'option. |

---

## La carte ne démarre pas / rien à l'écran

1. Vérifier l'alimentation : un port USB faible peut ne pas suffire avec le rétroéclairage
   à 100 % et le Wi-Fi actif.
2. Ouvrir la console : si le démarrage s'affiche mais pas l'écran, c'est l'initialisation
   EK79007 ou le rétroéclairage.
3. Un binaire compilé pour **rev3.x** ne démarre pas sur une carte **rev1.3** : relancer
   `python tools\verif_avant_flash.py`, qui contrôle les révisions acceptées (v1.0 à v1.99).

---

## Le tactile ne répond pas, ou à côté

- Le mapping validé est `swap_xy = 0`, `mirror_x = 0`, `mirror_y = 0` dans `app_main.c`.
  Les miroirs X/Y de l'exemple Waveshare **inversent** le toucher sur cette carte.
- Diagnostic : `touch_log 1` (RAW / MAPPED / objet LVGL / clic / défilement), `touch_info`,
  `touch_map`. `touch_tap <x> <y>` injecte un appui.

## L'interface se fige

C'était un **gel reproductible** du pilote PPA pendant la rotation 180° des zones
partielles. Le correctif est dans `components/esp_driver_ppa` : s'assurer que ce composant
est bien présent dans le projet. `ui_info` affiche le battement de l'interface, et une
détection `UI_GEL` journalise l'état des tâches si l'affichage ne se rafraîchit plus
pendant 5 s.

En cas de panique ou de watchdog, un **core dump ELF** est émis sur la console avant le
redémarrage. Décodage sur PC :

```powershell
python -m esp_coredump --chip esp32p4 info_corefile -t b64 -c dump.b64 build\glycemie_display.elf
```

---

## Pas de données Dexcom

| Affichage | Piste |
|---|---|
| « DONNÉES ANCIENNES » avec cause « Wi-Fi coupé » | réseau 2,4 GHz, SSID/mot de passe dans `device_secrets.h`, RSSI dans `info` |
| « DONNÉES ANCIENNES » sans coupure visible | le **téléphone** ne publie plus (hors ligne, application fermée, partage désactivé) |
| Rien au tout premier démarrage | normal : le verrou d'authentification est sur INCONNU. Lancer `dexcom_auth` une fois (voir [CONFIGURATION_DEXCOM_FR.md](CONFIGURATION_DEXCOM_FR.md)) |
| État `REFUSE` | identifiants refusés ou compte bloqué ; plus aucune tentative automatique, même après redémarrage. Corriger les identifiants puis réautoriser |

Commandes : `dexcom_status`, `dexcom_auth`, `dexcom_test_session` (simule une session
expirée : la reprise doit être automatique), `wifi_test_disconnect` (coupure Wi-Fi
volontaire : reconnexion attendue en quelques secondes).

Les messages d'erreur Dexcom sont journalisés avec l'étape, le code HTTP et le code Dexcom,
le reste **masqué**.

---

## microSD

| Symptôme | Explication |
|---|---|
| Pastille SD rouge, rien n'est écrit | système de fichiers non reconnu (exFAT par exemple). **Rien n'est formaté automatiquement.** Reformater la carte en FAT32 sur le PC |
| Carte absente ou retirée | démontage propre, nouvelles tentatives espacées (30 s, 1, 2, 5, 10, 30, 60 min). Affichage, Dexcom, alertes et bips continuent ; seule la voix manque |
| Doublons dans l'historique | ne devrait plus arriver : avant d'écrire, la dernière ligne du CSV du jour (ou de la veille) est relue et seules les mesures strictement plus récentes **à la seconde** sont ajoutées. `sd_check` liste doublons, désordre et lignes invalides ; `sd_repair` ne supprime **que** les doublons exacts |

Commandes : `sd_check`, `sd_repair`, `sd_test_eject <s>`, `sd_test_remount <n>`,
`sd_lire <fichier>` (lecture seule d'un fichier de `/glycemie/logs`).
`sd_format EFFACER` n'agit que sur une carte au système de fichiers non reconnu.

---

## Son et voix

| Symptôme | Explication |
|---|---|
| Un bip au lieu d'une phrase | la banque vocale n'est pas sur la microSD, le fichier est absent ou invalide, ou la carte est indisponible. Le bip est le repli prévu |
| Son saturé à 100 % | normal : la limite analogique est atteinte. Utiliser **85 %** |
| Aucun son | volume MUTE, « Alertes sonores » désactivé, ou amplificateur non activé. `audio_test <volume>` |
| Nombre de fichiers vocaux | affiché dans RÉGLAGES > INFORMATIONS |

Test d'alerte complet sans attendre une vraie alerte :
`alert_sim <type> <valeur>` — 0 très basse, 1 basse, 2 aucune donnée, 3 très élevée,
4 élevée, 5 capteur, 6 cathéter. `alert_test off` l'arrête, `alert_info` montre l'état.
Ces alertes de test sont marquées « (TEST) », ne sont jamais journalisées sur la microSD et
ne s'affichent jamais devant une vraie alerte BASSE ou TRÈS BASSE.

> ⚠️ `alert_sim` **fait réellement sonner l'appareil** dans la maison. À éviter la nuit.

---

## Commandes vocales

| Symptôme | Explication |
|---|---|
| L'appareil « parle tout seul » | c'était le cas avec les phrases courtes « Nouveau capteur » / « New sensor » : 5 détections spontanées en 2 jours, toutes de la variante anglaise, déclenchées par des phrases de télévision (« The new center opens on Monday » → score 0,1514). Ces phrases ont été **supprimées** et remplacées par deux phrases différentes. Vérifier le compteur `ignorees=` dans `vcmd_info` |
| Une commande n'est jamais reconnue | relever les scores réels : `vcmd_debug 1` affiche la chaîne de phonèmes décodée et le score à 4 décimales. Ajuster **une variante à la fois** : ajouter beaucoup de variantes dégrade *toute* la reconnaissance |
| L'écoute ne redémarre pas après un plantage | volontaire : « en pause après un redémarrage inattendu ». Réactiver dans RÉGLAGES ou par `vcmd_on` |
| `audio_test` refusé | les micros sont réservés pendant l'écoute |

Commandes : `vcmd_on`, `vcmd_off`, `vcmd_eveil 0|1`, `vcmd_gain <dB>`, `vcmd_info`,
`vcmd_sim <id> [probabilité]`, `vcmd_niveaux 0|1`, `vcmd_debug 0|1`, `vcmd_ajout <1..6> <phonèmes>`.

---

## Consommables

| Question | Réponse |
|---|---|
| Une recompilation efface-t-elle la date de pose ? | **Non.** Elle est en NVS, et un flash n'écrit jamais la NVS. C'est vérifié par un test automatique (`tests/hote/test_consommables.c`) |
| Un rappel peut-il sonner deux fois ? | Non : il est mémorisé en NVS dès sa sonnerie. Un redémarrage ne le rejoue jamais |
| Une date a été enregistrée par erreur | `conso_restore capteur|catheter <epoch> <rappels>` remet exactement la bonne date **sans** ajouter de ligne à `consumables.csv` ; `conso_csv_purge <epoch>` retire les lignes de test |
| Le stock a-t-il été décompté deux fois ? | Non : l'heure du changement sert de marqueur en NVS. `stock_info` montre les marqueurs |

---

## FAQ

**Peut-on afficher en mmol/L ?**
Non, pas aujourd'hui : l'affichage est en mg/dL uniquement.

**Peut-on configurer le Wi-Fi à l'écran ?**
Oui : depuis K22, ouvrez **RÉGLAGES → CONNEXION** ou utilisez l'application Windows `GlycemieConnexion.exe` reliée par USB. La configuration dans `private/device_secrets.h` ne concerne que l'ancienne méthode de compilation personnelle.

**Peut-on l'utiliser avec Nightscout ?**
Non. Le projet lit Dexcom Share directement, et il n'a pas été converti à Nightscout.

**Peut-on l'utiliser sans compte Dexcom ?**
Oui, en mode démonstration (`demo normal`). Toutes les pages fonctionnent avec des données
fictives.

**Que se passe-t-il si Dexcom modifie son service ?**
Share n'est pas une API publique : le service peut s'interrompre sans préavis. C'est une
des raisons pour lesquelles cet afficheur ne remplace pas les alarmes officielles.

**Comment revenir au firmware d'origine de la carte ?**
Si une sauvegarde complète de 32 Mo a été faite avant le premier flash, la réécrire à
l'adresse 0. Sans sauvegarde, il faut repartir d'un exemple Waveshare.
