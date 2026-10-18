# Licences — code du projet et composants tiers

Les **contributions originales** du projet sont diffusées sous licence **MIT**
(voir [../LICENSE](../LICENSE)). Elles se trouvent notamment dans `main/`,
`tests/`, `tools/` et `docs/` ; certains de ces dossiers contiennent aussi
ou adaptent du code tiers, soumis à **sa propre licence**.

Ce document recense ces composants et mentions obligatoires.
**Aucun code tiers n'est réattribué au projet.**

---

## 1. Code tiers *compilé avec* le projet mais **non versionné ici**

Ces composants sont téléchargés par le gestionnaire de composants Espressif à la première
compilation, aux versions figées dans `dependencies.lock`. Ils n'existent pas dans le dépôt.

| Composant | Version | Licence | Origine |
|---|---|---|---|
| ESP-IDF | 5.5.5 | Apache-2.0 | Espressif |
| `waveshare/esp32_p4_wifi6_touch_lcd_7b` (BSP) | 3.0.1 | Apache-2.0 | Waveshare |
| `espressif/esp_lvgl_adapter` | 0.6.x | Apache-2.0 | Espressif |
| `lvgl/lvgl` | 9.5.0 | MIT | LVGL LLC |
| `espressif/esp_lcd_ek79007` | 2.0.2~1 | Apache-2.0 | Espressif |
| `espressif/esp_lcd_touch_gt911` | 1.2.1 | Apache-2.0 | Espressif |
| `espressif/esp_codec_dev` | 1.5.11 | Apache-2.0 | Espressif |
| `espressif/esp_hosted` | 1.4.x | Apache-2.0 | Espressif |
| `espressif/esp_wifi_remote` | 1.2.5 | Apache-2.0 | Espressif |
| `espressif/esp-sr` | 2.5.3 | Apache-2.0 (bibliothèques et modèles : conditions Espressif) | Espressif |
| `espressif/esp-dl`, `esp-dsp`, `dl_fft`, `cjson` | voir `dependencies.lock` | Apache-2.0 / MIT | Espressif |

> Les **modèles** de reconnaissance vocale (WakeNet, MultiNet) sont fournis par Espressif
> avec esp-sr et restent soumis à ses conditions. Ils ne sont pas redistribués ici : ils
> sont téléchargés et empaquetés à la compilation.

## 2. Code tiers **présent dans le dépôt**

### `components/esp_driver_ppa/` — Apache-2.0 (Espressif)

Copie du composant `esp_driver_ppa` d'**ESP-IDF v5.5.5**, modifiée par l'application du
correctif officiel Espressif « PPA freeze fix » (une seule fonction,
`ppa_srm_transaction_on_picked()`). Les en-têtes de licence d'origine sont conservés ;
la modification est décrite dans `components/esp_driver_ppa/CORRECTIF.txt`.

**Ce code appartient à Espressif, pas au projet.** Il est inclus parce que le projet ne
compile pas correctement sans lui et qu'il ne faut pas modifier l'installation d'ESP-IDF.

## 3. Ressources

### Polices Montserrat — SIL Open Font License 1.1

`main/fonts/Montserrat-Medium.ttf` et `Montserrat-Bold.ttf`, embarquées dans le firmware et
rendues par TinyTTF. Le texte complet de la licence est dans `main/fonts/OFL.txt`, comme
l'exige l'OFL. Auteurs : Julieta Ulanovsky et contributeurs.

### Ancienne voix Windows — `main/voice/voice_fr.pcm` : **facultative et non distribuée**

40 extraits de parole (1,25 Mo, PCM 16 bits mono 16 kHz) générés **localement sur un PC
Windows** avec la voix française du système (`System.Speech`, Microsoft). Ils ne sont joués
que par la commande console historique `voice_old` : les annonces normales viennent de la
banque microSD, et le repli sans banque est un **bip**.

**Décision (16 octobre 2026) :** ces échantillons sont une sortie du moteur de synthèse de
Windows ; leur redistribution relèverait des conditions d'utilisation de Microsoft, que ce
projet n'a pas établies. Le fichier est donc **retiré de la copie publiée**, et le firmware
le rend **facultatif** :

- `main/CMakeLists.txt` n'embarque `voice/voice_fr.pcm` que s'il existe et définit
  `GLY_VOICE_OLD_PCM` en conséquence ;
- sans le fichier, la compilation réussit (vérifié) ; `voice_old` répond simplement que
  l'ancienne voix est absente de cette compilation ;
- pour le retrouver sur son propre PC Windows :
  `powershell -ExecutionPolicy Bypass -File tools\voice\gen_voice.ps1`
  puis `python tools\voice\pack_voice.py`, et le replacer dans `main/voice/`.

Le projet ne revendique aucun droit sur cet audio.

### Banques vocales microSD — hors dépôt, redistribution à vérifier avant ouverture publique

Les 396 fichiers WAV par langue (six langues, ≈ 20 à 25 Mo chacune) ont été générés **une seule
fois sur un PC** avec **Google Cloud Text-to-Speech** (voix `*-Chirp3-HD-Aoede`), à partir des
textes du projet. L'appareil ne contacte jamais Google : il lit ces fichiers sur sa microSD.

Les sons ne sont **pas dans le dépôt Git**. Une archive microSD a été préparée et
placée dans la préversion privée v0.1.0. **Cette distribution antérieure ne suffit pas à
établir les droits de redistribution au public** : une vérification des conditions contractuelles
applicables au service et au compte de génération reste nécessaire. La v0.2.0 ne regénère
ni ne recopie les WAV dans ses nouveaux téléchargements. Chaque ancienne archive contient
une notice de provenance (`PROVENANCE.md`), un manifeste et des empreintes. Cette notice
indique notamment :

- le projet n'est ni affilié à Google ni approuvé par Google ; Google Cloud, Chirp et les voix
  citées appartiennent à Google ;
- les sons proviennent de fichiers générés avec un service tiers. La **diffusion publique**
  doit être évaluée au regard des conditions Google Cloud applicables ; aucune licence
  autonome sur les modèles vocaux ni permission générale de redistribution n'est présumée ;
- **la licence MIT du code ne couvre pas ces enregistrements** ;
- qui préfère les refaire avec son propre compte Google trouve la marche à suivre dans
  [TUTORIEL_VOIX_FR.md](TUTORIEL_VOIX_FR.md), parcours B.

Ce qui est fourni dans le dépôt pour les reconstruire :

- `assets/banque_vocale_manifeste.csv` — la liste des phrases (nom de fichier, texte,
  durée, taille, CRC32) ; ce texte est celui du projet ;
- `tools/voice/gen_voice_google.py` — le générateur (code du projet, licence MIT), qui
  reconstitue la banque avec un compte Google Cloud ;
- `tools/voice/sd_voice_upload.py` — la copie sur la microSD par le câble USB-série.

Quiconque reconstruit la banque le fait avec **son propre** compte Google Cloud et sous les
conditions de Google. Coût indicatif relevé à l'époque : 11 181 caractères, soit ≈ 0,34 $
au tarif Chirp 3 HD, dans la gratuité mensuelle.

Sans banque vocale, l'appareil fonctionne : les annonces sont remplacées par un bip.

## 4. Code tiers repris dans ce projet

Deux passages d'exemples officiels et un jeu de constantes de protocole proviennent de projets
tiers. Leur origine et leur licence sont écrites **dans les fichiers concernés** ; ce tableau les
récapitule.

| Origine | Licence | Ce qui est repris | Où c'est cité |
|---|---|---|---|
| Exemple Waveshare `09_lvgl_demo_v9` (© Espressif) | Apache-2.0 | le bloc d'initialisation de l'écran (`bsp_display_cfg_t`, `bsp_display_start_with_config`) | `main/app_main.c` |
| Exemple Waveshare / Espressif `05_wifistation` | Apache-2.0 | le squelette de mise en route du Wi-Fi (gestionnaires d'événements, attente par groupe d'événements) | `main/wifi_manager.c` |
| [pydexcom](https://github.com/gagebenne/pydexcom) — © Gage Benne | MIT | les constantes du protocole Dexcom Share : adresses du service, identifiants d'application, chemins d'API | `main/dexcom_share.c` |
| `components/esp_driver_ppa/` (© Espressif, ESP-IDF 5.5.5) | Apache-2.0 | composant recopié, une fonction modifiée | en-têtes SPDX des fichiers + `CORRECTIF.txt` |

Les noms de tendance renvoyés par l'API Dexcom (`Flat`, `DoubleUp`…) sont des **valeurs du
protocole** : tout client doit employer les mêmes chaînes.

Le reste du code — interface, alertes, suivi des consommables, mode nuit, voix, assistants de
connexion, stockage microSD, tests — a été écrit pour ce projet et est publié sous licence MIT
(voir `LICENSE`). D'autres clients Dexcom libres ont été consultés pendant la conception ; une
comparaison automatique du code n'y a trouvé **aucun fichier ni aucune fonction repris**, et ils
ne sont donc pas cités ici. L'interface graphique est originale.

### Si vous redistribuez

Apache-2.0 demande de conserver les mentions de copyright et de licence, et de signaler les
modifications : c'est fait dans les fichiers eux-mêmes et dans
`components/esp_driver_ppa/CORRECTIF.txt`. MIT demande de conserver la mention de copyright :
elle est dans `main/dexcom_share.c` pour pydexcom, et dans `LICENSE` pour le code du projet.

## 5. Ce qui n'est pas publié

Par choix, et pour protéger des données personnelles de santé :

- les identifiants Wi-Fi et Dexcom (`private/device_secrets.h`) ;
- les sauvegardes de la flash et de la microSD de l'appareil personnel ;
- les firmwares compilés **depuis le dossier personnel** — les identifiants y sont inclus ;
  le programme publié, lui, est compilé depuis ces sources-ci, sans aucun identifiant ;
- les journaux de mise au point et l'historique glycémique réel ;
- les banques vocales : trop volumineuses pour un dépôt, elles sont fournies **à part**
  (voir la section 3).

Voir [PUBLICATION_GITHUB_FR.md](PUBLICATION_GITHUB_FR.md).

## 6. Si vous redistribuez ce projet

1. Conservez `LICENSE` et `main/fonts/OFL.txt`.
2. Conservez les en-têtes de licence Espressif dans `components/esp_driver_ppa/`.
3. Ne redistribuez pas de firmware compilé contenant des identifiants.
4. Vérifiez vous-même les conditions applicables à tout audio généré que vous ajouteriez.
