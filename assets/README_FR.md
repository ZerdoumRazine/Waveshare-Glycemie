# Ressources

## `banque_vocale_manifeste.csv`

Liste des 396 phrases de la banque vocale **française**, stockée sur la microSD dans
`/glycemie/audio/voice/`. Les cinq autres langues ont la même liste de fichiers, traduite, dans
`/glycemie/audio/voice/<code>/` (`en`, `es`, `de`, `it`, `pt`). Les textes et les nombres en lettres
de chaque langue sont dans [`../tools/voice/langues_voix.py`](../tools/voice/langues_voix.py).

Colonnes : `fichier ; texte ; duree_s ; taille_octets ; crc32`.

Les fichiers WAV eux-mêmes ne sont **pas** dans le dépôt (≈ 23 Mo, et ce sont des sorties
de Google Cloud Text-to-Speech, voix `fr-FR-Chirp3-HD-Aoede`). Voir
[../docs/LICENCES_TIERCES_FR.md](../docs/LICENCES_TIERCES_FR.md).

Catégories de phrases :

| Préfixe | Contenu |
|---|---|
| `glucose_040` … `glucose_400` | une phrase complète par valeur (« Glycémie soixante-dix-huit. ») |
| `glucose_low`, `glucose_high` | hors plage (« Glycémie inférieure à quarante. ») |
| `alert_*` | alertes très basse / basse / élevée / très élevée / aucune donnée |
| `sensor_rappel_*`, `catheter_rappel_*` | rappels des consommables |
| `trend_*` | tendances (« Stable. », « En hausse. »…) |
| `vc_*` | réponses des commandes vocales |

### Reconstituer la banque

```powershell
# --lang fr (défaut) | en | es | de | it | pt
python tools\voice\gen_voice_google.py --lang en --estimate   # phrases, caractères, coût indicatif
python tools\voice\gen_voice_google.py --lang en --check      # accès à l'API (aucun caractère facturé)
python tools\voice\gen_voice_google.py --lang en              # génération (bank/en/voice)
python tools\voice\sd_voice_upload.py --port COM7 --lang en --log copie_en.log
```

Comptez environ 11 500 caractères par langue, soit ≈ 0,35 USD au tarif Chirp 3 HD — dans le million
de caractères gratuits par mois. Chaque banque pèse ≈ 22 Mo et met ≈ 5 min à passer par le câble
USB-série.

La copie passe par le **câble USB-série**, sans retirer la microSD : débit temporaire
921 600 bauds, blocs acquittés, CRC32 vérifié en mémoire puis relu sur la carte, et
vérification finale complète contre le manifeste. `--verify-only` vérifie sans écrire.

Un compte Google Cloud est nécessaire et les conditions de Google s'appliquent.
**Sans banque vocale, l'appareil fonctionne** : les annonces sont remplacées par un bip.

## Polices

Les polices Montserrat ne sont pas ici mais dans `main/fonts/`, parce qu'elles sont
**embarquées dans le firmware** (`EMBED_FILES` de `main/CMakeLists.txt`) et rendues par
TinyTTF. Leur licence (SIL OFL 1.1) est dans `main/fonts/OFL.txt`.

## Voix embarquée

`main/voice/voice_fr.pcm` : 40 extraits de repli (1,25 Mo), utilisés seulement quand la
banque microSD est absente. Origine et question de licence :
[../docs/LICENCES_TIERCES_FR.md](../docs/LICENCES_TIERCES_FR.md).
