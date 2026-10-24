# Project resources

## Speech-bank manifest

The file [banque_vocale_manifeste.csv](banque_vocale_manifeste.csv) lists
396 recording names for the French reference bank. The device expects this
bank in `/glycemie/audio/voice/`; the other five banks use subdirectories
`en/`, `es/`, `de/`, `it/` and `pt/`.

The CSV uses French column headers for compatibility with the existing
generation scripts: `fichier` (filename), `texte` (phrase),
`duree_s` (duration in seconds), `taille_octets` (size in bytes)
and `crc32` (checksum). These are data/schema identifiers, not comments
to translate.

The WAV files are not included in the repository. Optional release banks
were generated with Google Cloud Text-to-Speech using the Chirp 3 HD
Aoede voice; see [third-party licences](../docs/THIRD_PARTY_LICENSES_EN.md).

| File prefix | Purpose |
|---|---|
| `glucose_040` through `glucose_400` | Spoken glucose readings |
| `glucose_low`, `glucose_high` | Readings outside the supported range |
| `alert_*` | Glucose and missing-data alerts |
| `sensor_rappel_*`, `catheter_rappel_*` | Consumables reminders |
| `trend_*` | Trend phrases |
| `vc_*` | Optional voice-command responses |

## Generating a bank

The optional generator requires a Google Cloud account and may incur charges.
To inspect the planned language bank before generation:

```powershell
python tools/voice/gen_voice_google.py --lang en --estimate
python tools/voice/gen_voice_google.py --lang en --check
```

To generate an English WAV bank and transfer it over the board's UART/CH343
USB connection, consult [the voice tutorial](../docs/VOICE_TUTORIAL_EN.md).
Transfer verification includes checksums and a read-back check. Without a
speech bank, the device still sounds alert beeps.

## Fonts and optional legacy audio

Montserrat fonts are stored in `main/fonts/` and carry the SIL Open Font
License 1.1. The third-party licence and author credits must remain intact.

The optional historical `main/voice/voice_fr.pcm` was not distributed
because its redistribution rights were not established. The normal
microSD WAV announcements and fallback beeps do not depend on it.
