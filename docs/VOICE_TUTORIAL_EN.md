# Spoken announcements: install a language, or build your own audio

Written for someone who has never used Python or Google Cloud.

Two paths, simplest first:

| | For whom | What you need |
|---|---|---|
| **Path A — install** | everyone | a PC and a microSD card reader. 10 minutes. |
| **Path B — build** | changing the wording, adding a voice | Python and a Google Cloud account. 1 hour the first time. |

> ⚠️ This device is an **experimental secondary display**. Announcements may be delayed, missing or
> wrong. They replace neither the official apps, receivers nor alarms, and must not be used alone to
> make a treatment decision.

---

## 1. How it works

The device **does not synthesise speech**. The audio is produced once on a PC, converted to the
format the display reads (WAV, 16-bit PCM, mono, 16 kHz) and copied to the microSD card. The display
then plays those files on its own: **no connection is needed to speak**.

Fetching glucose values is a separate matter, over Wi‑Fi and your Dexcom account
([docs/CONFIGURATION_DEXCOM_FR.md](CONFIGURATION_DEXCOM_FR.md), in French).

**ESP‑IDF is not required** to install or build audio: it only builds the firmware
([docs/INSTALLATION_FR.md](INSTALLATION_FR.md)).

### The six languages

| Language | Code | Voice | Files | Size |
|---|---|---|---|---|
| French | `fr` | `fr-FR-Chirp3-HD-Aoede` | 396 | 22.0 MB |
| US English | `en` | `en-US-Chirp3-HD-Aoede` | 396 | 22.0 MB |
| Spanish | `es` | `es-ES-Chirp3-HD-Aoede` | 396 | 23.1 MB |
| German | `de` | `de-DE-Chirp3-HD-Aoede` | 396 | 23.9 MB |
| Italian | `it` | `it-IT-Chirp3-HD-Aoede` | 396 | 24.8 MB |
| Portuguese (Brazil) | `pt` | `pt-BR-Chirp3-HD-Aoede` | 396 | 26.7 MB |

396 files per language = **35 fixed phrases** (alerts, trends, replies) + **361 values** from 40 to
400 mg/dL.

Changing the language in SETTINGS **creates no file**. If the chosen language has no bank on the
card, the device **falls back to French**; with nothing at all, only a beep is left. **Voice-command recognition is disabled in the public build**; its microphone is not used for listening to commands. Spoken announcements and alerts remain available in all six languages.

---

## 2. Path A — install a ready-made bank

### 2.1 Get the package for your language

The packages are **intended to be supplied as GitHub Release attachments**, separate from the source code. Check [Releases](../../releases) for availability; until a Release is published, these downloads are not yet available.

| Package | Contents | ZIP size |
|---|---|---|
| `glycemie-voix-fr-k21.zip` | French | 19.4 MB |
| `glycemie-voix-en-k21.zip` | US English | 20.2 MB |
| `glycemie-voix-es-k21.zip` | Spanish | 21.2 MB |
| `glycemie-voix-de-k21.zip` | German | 21.4 MB |
| `glycemie-voix-it-k21.zip` | Italian | 22.3 MB |
| `glycemie-voix-pt-k21.zip` | Portuguese (Brazil) | 24.9 MB |

Each package holds the tree to copy, `manifest.csv` (text, duration, size and CRC32 of every file),
`SHA256SUMS.txt`, `PROVENANCE.md` and a short FR/EN how-to.

**Check the ZIP hash before opening it** (PowerShell):

```powershell
Get-FileHash .\glycemie-voix-en-k21.zip -Algorithm SHA256
```

It must match the matching line of `SHA256SUMS-paquets.txt` published next to the packages. If it
differs, do not use the file: the download is incomplete or altered.

### 2.2 Back the card up BEFORE changing anything

1. Power the display off, then remove the microSD card.
2. Insert it in the PC and **copy everything to a backup folder**. The `glycemie/` folder also holds
   your measurement history; it must not be lost.
3. If Windows offers to **format** the card, say **no** and check the drive letter: formatting would
   erase everything. A brand-new card is prepared as FAT32 (see
   [docs/INSTALLATION_FR.md](INSTALLATION_FR.md), microSD section).

### 2.3 Copy the files

Open the ZIP and copy the `glycemie` folder **to the root of the card**, merging with the existing
one. Expected result on the card:

```text
glycemie/
  audio/
    voice/
      glucose_040.wav ...        ← FRENCH, directly here
      alert_low.wav ...
      en/  glucose_040.wav ...   ← English
      es/  ...                   ← Spanish
      de/  ...                   ← German
      it/  ...                   ← Italian
      pt/  ...                   ← Portuguese
  logs/                          ← history: leave alone
```

Three common mistakes:

- do **not** create an `fr/` folder: French lives directly in `voice/`;
- do **not** create `en/voice/`: just `en/`;
- do **not** copy `manifest.csv`, `PROVENANCE.md` or `LISEZMOI_README.txt` onto the card: they are
  only used to verify the package on the PC.

### 2.4 Test

Eject the card safely, put it back **with the display off**, then power on. In SETTINGS, pick the
language, then run the interface's **audio test**.

Listen to a value ("Glucose one hundred twenty"), a trend and a test alert phrase. **Do not create a
real abnormal medical situation to test an alarm.** A successful volume test does not prove that
every alert works.

### 2.5 Copying without removing the card (optional)

The display can receive the files over the USB-serial cable, card in place. You need Python with
`pyserial`, the display powered on, and the right COM port. Close any serial monitor first.

```powershell
$Py = 'tools\voice\.venv_tts\Scripts\python.exe'   # or your own Python
& $Py -B tools\voice\sd_voice_upload.py --port COM7 --lang en --log tools\logs\copy_voice_en.log
```

A transfer takes about 7 minutes per language and ends with `CONFORME`. To **check** a card without
writing anything:

```powershell
& $Py -B tools\voice\sd_voice_upload.py --port COM7 --lang en --verify-only --log tools\logs\check_en.log
```

> `--verify-only` sends read commands only: **no file is written and no folder is created**, even if
> the card is incomplete. For languages other than French this requires up-to-date firmware (the one
> answering `VRX LIST dossier=…`); with older firmware the tool **refuses** instead of writing. Keep
> the logs private: they contain the port name and local paths.

---

## 3. Path B — build your own audio (advanced)

Only needed to **change the wording**, add a language or another voice. The published packages are
enough for normal use.

### 3.1 Go to the project folder

The folder holding `main`, `tools` and `README_FR.md`. Example (adapt the path):

```powershell
Set-Location 'E:\wsg-public'
Test-Path '.\tools\voice\gen_voice_google.py'      # must print True
```

### 3.2 Install Python and the libraries

```powershell
py -3 --version                                     # otherwise: install Python for Windows
py -3 -m venv tools\voice\.venv_tts
$Py = Join-Path (Get-Location) 'tools\voice\.venv_tts\Scripts\python.exe'
& $Py -m pip install numpy google-cloud-texttospeech pyserial
```

Versions used to produce the published packages: **Python 3.12.9, numpy 2.5.3,
google-cloud-texttospeech 2.37.0**. SciPy is not needed (resampling is implemented in the script).
The environment does not need activating: `& $Py` is enough. Re-run the `$Py = ...` line in every new
PowerShell window.

### 3.3 Estimate before paying anything

```powershell
& $Py -B tools\voice\gen_voice_google.py --lang en --estimate
```

This command **does not call Google**. Measured on this version:

| Language | Characters | Indicative cost (USD 30 / million) |
|---|---:|---:|
| fr | 11,941 | USD 0.36 |
| en | 11,450 | USD 0.34 |
| es | 11,800 | USD 0.35 |
| de | 12,011 | USD 0.36 |
| it | 10,492 | USD 0.31 |
| pt | 12,256 | USD 0.37 |
| **Total** | **69,950** | **USD 2.10** |

Google advertises **one million free characters per month** for Chirp 3 HD, but billing must be
enabled and the quota may already be used. The script gives an estimate, not a spending cap: only
your Google account is authoritative.

### 3.4 Set up Google Cloud

1. Create or pick a Google Cloud project, enable billing, then enable the **Cloud Text-to-Speech**
   API ([official guide](https://cloud.google.com/text-to-speech/docs/before-you-begin)).
2. Install the **Google Cloud CLI** ([official download](https://cloud.google.com/sdk/docs/install)).
3. In a new PowerShell window:

```powershell
gcloud init
gcloud auth application-default login
$Project = 'YOUR_PROJECT_ID'        # the ID, not the display name
gcloud auth application-default set-quota-project $Project
```

Sign-in happens in the browser. **Never paste a password or key file into a chat, add it to the
repository, or copy it onto the microSD card.**

### 3.5 Check access, then generate

```powershell
& $Py -B tools\voice\gen_voice_google.py --lang en --check     # lists voices, synthesises nothing
& $Py -B tools\voice\gen_voice_google.py --lang en --only glucose_100.wav   # single trial
& $Py -B tools\voice\gen_voice_google.py --lang en             # the whole bank
```

Everything from `--only` onwards **consumes billable characters**.

Expected: 396 WAV files in `tools/voice/bank/voice/` (French) or `tools/voice/bank/<code>/voice/`
(other languages), plus a `manifest.csv` next to them.

`tools/voice/google_raw/` keeps Google's raw answers so a phrase is never paid for twice. Keep it
**private**; it is useless on the card and must not be published. A new generation is not guaranteed
byte-identical to an old one: keep the WAV files and their manifest if you want a reproducible
distribution.

### 3.6 Changing a text

Texts live in `tools/voice/langues_voix.py`. After editing:

```powershell
python tests\test_langues_voix.py
```

This test rejects any change to the **French** texts that departs from the versioned manifest
(`assets/banque_vocale_manifeste.csv`): otherwise the files already copied onto cards would silently
become obsolete.

---

## 4. What is published, what is not

| File or folder | Role | Published? |
|---|---|---|
| `tools/voice/langues_voix.py` | texts of the six languages, voice names | yes, with the code |
| `tools/voice/gen_voice_google.py` | generation and conversion on the PC | yes |
| `tools/voice/sd_voice_upload.py` | copy to the microSD card | yes |
| `tools/voice/bank/` | final WAV files + manifests | as Release attachments, not in Git |
| `tools/voice/google_raw/` | cache of Google's answers | no (private backup) |
| `tools/voice/.venv_tts/` | installed Python and libraries | no (reinstallable) |
| `main/voice/voice_fr.pcm` | legacy embedded Windows voice (console `voice_old`) | no: redistribution rights not established, see [docs/LICENCES_TIERCES_FR.md](LICENCES_TIERCES_FR.md) |

The firmware builds **with or without** `main/voice/voice_fr.pcm`. Without it, only the legacy
`voice_old` console command becomes unavailable; normal announcements (microSD bank) and the fallback
beep are unchanged.

---

## 5. If it does not work

| Symptom | Most likely cause |
|---|---|
| A beep instead of a phrase | file missing on the card, or card not read: check the tree in § 2.3 |
| Device speaks French while the screen is in English | the `en/` bank is missing: French fallback by design |
| Nothing at all | volume at 0, no card, or empty `glycemie/audio/voice` |
| `--verify-only` refuses for a language | firmware older than the `vrx_dir … lecture` command: rebuild and reflash, or check the card on the PC |
| `Acces API refuse` when generating | billing or API not enabled, or quota project not set (§ 3.4) |

Full troubleshooting (French): [docs/DEPANNAGE_FR.md](DEPANNAGE_FR.md).

---

## Official sources

- Google Cloud Text-to-Speech — getting started: <https://cloud.google.com/text-to-speech/docs/before-you-begin>
- Local authentication (ADC): <https://cloud.google.com/docs/authentication/provide-credentials-adc>
- Pricing: <https://cloud.google.com/text-to-speech/pricing>
- Using the generated audio files: <https://cloud.google.com/text-to-speech/quotas>
- GitHub Release attachments: <https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases>
