# Licences — code du projet et composants tiers

Le code écrit pour ce projet est sous licence **MIT** (voir [../LICENSE](../LICENSE)).
Cette licence ne couvre **que** ce code : `main/`, `tests/`, `tools/` et `docs/`.

Tout le reste appartient à ses auteurs et conserve sa propre licence. Ce document les
recense. **Aucun composant tiers n'est réattribué au projet.**

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

### Banque vocale microSD — **non distribuée**

Les 396 fichiers WAV de la banque principale (≈ 23 Mo) ont été générés **une seule fois**
avec **Google Cloud Text-to-Speech**, voix `fr-FR-Chirp3-HD-Aoede`. Ils ne sont **pas**
dans le dépôt : taille, et conditions d'utilisation propres à Google Cloud.

Ce qui est fourni à la place :

- `assets/banque_vocale_manifeste.csv` — la liste des phrases (nom de fichier, texte,
  durée, taille, CRC32) ; ce texte est celui du projet ;
- `tools/voice/gen_voice_google.py` — le générateur (code du projet, licence MIT), qui
  reconstitue la banque avec un compte Google Cloud ;
- `tools/voice/sd_voice_upload.py` — la copie sur la microSD par le câble USB-série.

Quiconque reconstruit la banque le fait avec **son propre** compte Google Cloud et sous les
conditions de Google. Coût indicatif relevé à l'époque : 11 181 caractères, soit ≈ 0,34 $
au tarif Chirp 3 HD, dans la gratuité mensuelle.

Sans banque vocale, l'appareil fonctionne : les annonces sont remplacées par un bip.

## 4. Sources d'inspiration (aucun code copié)

Le client Dexcom Share a été écrit en C à partir du **comportement documenté** du protocole,
en s'appuyant sur la lecture de projets libres :

| Projet | Licence | Usage |
|---|---|---|
| [pydexcom](https://github.com/gagebenne/pydexcom) | MIT | référence du protocole Share, identifiants d'application, codes de tendance |
| Gluco-Monitor, Dexcom_Follow, ESP32_Dex | MIT | comportements et séquences d'appels |
| Exemples Waveshare ESP32-P4 | Apache-2.0 / CC0 | initialisation matérielle (écran, tactile, Wi-Fi hébergé) |

**Aucun code n'a été repris** de ForgeUI-P4 ni de dexcom-esp32-monitor.
L'interface graphique est originale : aucune reprise visuelle d'un autre produit.

## 5. Ce qui n'est pas publié

Par choix, et pour protéger des données personnelles de santé :

- les identifiants Wi-Fi et Dexcom (`private/device_secrets.h`) ;
- les sauvegardes de la flash (32 Mo) ;
- les firmwares compilés (`.bin`, `.elf`) — les identifiants y sont inclus ;
- les journaux de mise au point et l'historique glycémique réel ;
- la banque vocale générée.

Voir [PUBLICATION_GITHUB_FR.md](PUBLICATION_GITHUB_FR.md).

## 6. Si vous redistribuez ce projet

1. Conservez `LICENSE` et `main/fonts/OFL.txt`.
2. Conservez les en-têtes de licence Espressif dans `components/esp_driver_ppa/`.
3. Ne redistribuez pas de firmware compilé contenant des identifiants.
4. Vérifiez vous-même les conditions applicables à tout audio généré que vous ajouteriez.
