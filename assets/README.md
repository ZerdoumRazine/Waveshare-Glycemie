# Resources

## `banque_vocale_manifeste.csv`

List of 396 phrases of the French** voice bank, stored on microSD in
`/glycemie/audio/voice/`. The other five languages have the same list of files, translated, in
`/glycemie/audio/voice/<code>/` (`en`, `es`, `de`, `it`, `pt`). Texts and numbers in letters
of each language are in [`../tools/voice/langues_voix.py`](../tools/voice/langues_voix.py).

Columns: `fichier ; texte ; duree_s ; taille_octets ; crc32`.

Files WAV they themselves are **not** in the repository (
by Google Cloud Text-to-Speech, voice `fr-FR-Chirp3-HD-Aoede`). See
[../docs/THIRD_PARTY_LICENSES_EN.md](../docs/THIRD_PARTY_LICENSES_EN.md).

Categories of sentences:

| Prefix | Content |
|---|---|
| `glucose_040` … `glucose_400` | one full sentence per value ("Glycemia seventy-eight.") |
| `glucose_low`, `glucose_high` | off-range ("Glycemia less than forty.) |
| `alert_*` | very low / low / high / very high / no data |
| `sensor_rappel_*`, `catheter_rappel_*` | recalls of consumables |
| `trend_*` | trends ("Stable. "On the rise. »...) |
| `vc_*` | voice control responses |

### Reconstitute the bank

```powershell
# --lang fr (défaut) | en | es | de | it | pt
python tools\voice\gen_voice_google.py --lang en --estimate   # phrases, caractères, coût indicatif
python tools\voice\gen_voice_google.py --lang en --check      # accès à l'API (aucun caractère facturé)
python tools\voice\gen_voice_google.py --lang en              # génération (bank/en/voice)
python tools\voice\sd_voice_upload.py --port COM7 --lang en --log copie_en.log
```

Count approximately 11,500 characters per language, i.e. 0.35 USD at the price Chirp 3 HD — in the million
free characters per month. Each bank weighs 22 MB and takes 5 min to go through the cable
USB-Series.

Copy goes through the **cable USB- series**, without removing microSD: temporary flow rate
921 600 bauds, acquitted blocks, CRC32 checked in memory and then rereaded on the card, and
complete final verification against the manifest. `--verify-only` Check without writing.

A Google Cloud account is required and Google's terms and conditions apply.
**Without a voice bank, the device works**: ads are replaced by a beep.

## Police

Montserrat police are not here but in `main/fonts/`, because they are
** boarded in firmware** (`EMBED_FILES` de `main/CMakeLists.txt`) and rendered by
TinyTTF. Their license (SIL OFL 1.1) is in `main/fonts/OFL.txt`.

## Onboard voice

`main/voice/voice_fr.pcm` 40 withdrawal extracts (1.25 MB), used only when
microSD bank is absent. Origin and Licence Issue:
[../docs/THIRD_PARTY_LICENSES_EN.md](../docs/THIRD_PARTY_LICENSES_EN.md).
