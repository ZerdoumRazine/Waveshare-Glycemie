# Les annonces vocales : installer une langue, ou fabriquer ses propres sons

Ce guide est écrit pour quelqu'un qui n'a jamais utilisé Python ni Google Cloud.

Deux parcours, du plus simple au plus technique :

| | Pour qui | Ce qu'il faut |
|---|---|---|
| **Parcours A — installer** | tout le monde | un PC, un lecteur de carte microSD. 10 minutes. |
| **Parcours B — fabriquer** | changer les textes, ajouter une voix | Python et un compte Google Cloud. 1 heure la première fois. |

> ⚠️ Cet appareil est un **affichage complémentaire expérimental**. Les annonces peuvent être
> retardées, absentes ou incorrectes. Elles ne remplacent ni les applications, ni les récepteurs,
> ni les alertes officiels, et ne doivent pas servir seules à une décision de traitement.

---

## 1. Comment ça marche

Les sons **ne sont pas fabriqués par l'appareil**. Ils sont produits une fois sur un PC, convertis
au format que l'écran sait lire (WAV, PCM 16 bits, mono, 16 kHz), puis copiés sur la microSD.
Ensuite, l'écran lit ces fichiers tout seul : **aucune connexion n'est nécessaire pour parler**.

La récupération des glycémies passe, elle, par le Wi‑Fi et le compte Dexcom : c'est un sujet
séparé ([docs/CONFIGURATION_DEXCOM_FR.md](CONFIGURATION_DEXCOM_FR.md)).

**ESP‑IDF n'est pas nécessaire** pour installer ou fabriquer des sons : il ne sert qu'à compiler le
firmware ([docs/INSTALLATION_FR.md](INSTALLATION_FR.md)).

### Les six langues

| Langue | Code | Voix utilisée | Fichiers | Taille |
|---|---|---|---|---|
| Français | `fr` | `fr-FR-Chirp3-HD-Aoede` | 396 | 22,0 Mo |
| Anglais américain | `en` | `en-US-Chirp3-HD-Aoede` | 396 | 22,0 Mo |
| Espagnol | `es` | `es-ES-Chirp3-HD-Aoede` | 396 | 23,1 Mo |
| Allemand | `de` | `de-DE-Chirp3-HD-Aoede` | 396 | 23,9 Mo |
| Italien | `it` | `it-IT-Chirp3-HD-Aoede` | 396 | 24,8 Mo |
| Portugais (Brésil) | `pt` | `pt-BR-Chirp3-HD-Aoede` | 396 | 26,7 Mo |

Les 396 fichiers d'une langue = **35 phrases fixes** (alertes, tendances, réponses) + **361 valeurs**
de 40 à 400 mg/dL.

Changer la langue dans RÉGLAGES **ne crée aucun fichier**. Si la langue choisie n'a pas sa banque sur
la carte, l'appareil **replie sur le français** ; s'il n'y a rien du tout, il ne reste qu'un bip.
**La version publique n'écoute pas les commandes vocales** : le microphone de reconnaissance est désactivé. Les annonces parlées et les alertes restent disponibles dans les six langues.

---

## 2. Parcours A — installer une banque prête à l'emploi

### 2.1 Télécharger les six langues (un seul ZIP)

Dans la [Release v0.2.0](https://github.com/ZerdoumRazine/Waveshare-Glycemie/releases/tag/v0.2.0),
téléchargez **`glycemie-microSD-v0.2.0.zip`** (ressource audio optionnelle).
Cette archive contient les **six banques complètes**, soit 396 WAV par langue :
français, anglais, espagnol, allemand, italien et portugais brésilien.

Téléchargez également **`SHA256SUMS-voix.txt`** pour vérifier l'intégrité :

```powershell
Get-FileHash .\glycemie-microSD-v0.2.0.zip -Algorithm SHA256
```

Comparez la valeur affichée à l'empreinte du fichier `SHA256SUMS-voix.txt`.
N'utilisez pas un ZIP dont l'empreinte ne correspond pas.

**Décompressez complètement** le ZIP dans un dossier du PC : il contient
`glycemie/audio/voice/` prêt à copier sur la carte mémoire, plus une notice
`PROVENANCE_VOIX.txt`. Les fichiers audio proviennent de Google Cloud
Text-to-Speech et ne sont pas soumis à la licence MIT du code :
[licences et provenance](LICENCES_TIERCES_FR.md).

### 2.2 Sauvegarder la carte AVANT de la modifier

1. Éteindre l'écran, puis retirer la microSD.
2. La mettre dans le PC et **copier tout son contenu dans un dossier de sauvegarde**.
   Le dossier `glycemie/` contient aussi l'historique des mesures : il ne doit pas être perdu.
3. Si Windows propose de **formater** la carte, répondre **non** et vérifier le lecteur : un
   formatage effacerait tout. Une carte neuve se prépare en FAT32
   (voir [docs/INSTALLATION_FR.md](INSTALLATION_FR.md), section microSD).

### 2.3 Copier les fichiers

Ouvrir le ZIP et copier le dossier `glycemie` **à la racine de la carte**, en fusionnant avec celui
qui existe déjà. Résultat attendu sur la carte :

```text
glycemie/
  audio/
    voice/
      glucose_040.wav ...        ← FRANÇAIS, directement ici
      alert_low.wav ...
      en/  glucose_040.wav ...   ← anglais
      es/  ...                   ← espagnol
      de/  ...                   ← allemand
      it/  ...                   ← italien
      pt/  ...                   ← portugais
  logs/                          ← historique : ne pas toucher
```

Trois pièges fréquents :

- **ne pas** créer de dossier `fr/` : le français vit directement dans `voice/` ;
- **ne pas** créer `en/voice/` : seulement `en/` ;
- **ne pas** copier `PROVENANCE_VOIX.txt` sur la carte : cette notice reste à côté du ZIP
  sur le PC (le dossier `glycemie/` suffit).

### 2.4 Tester

Éjecter la carte proprement (icône « Retirer le périphérique »), la remettre **écran éteint**, puis
rallumer. Dans RÉGLAGES, choisir la langue, puis lancer le **test audio** de l'interface.

Écouter une valeur (« Glycémie cent vingt »), une tendance et une phrase d'alerte de test.
**Ne pas provoquer une vraie situation médicale anormale pour tester une alarme.** Un test de volume
réussi ne prouve pas que toutes les alertes fonctionnent.

### 2.5 Copier sans retirer la carte (facultatif)

L'écran peut recevoir les fichiers par le câble USB‑série, carte en place. Il faut un PC avec Python
et `pyserial`, l'écran allumé, et le bon port COM. Fermer tout moniteur série avant.

```powershell
$Py = 'tools\voice\.venv_tts\Scripts\python.exe'   # ou votre Python avec pyserial
& $Py -B tools\voice\sd_voice_upload.py --port COM7 --lang fr --sd-root "C:\VoixWaveshare" --log copie_voix_fr.log
```

Remplacez `C:\VoixWaveshare` par le dossier où vous avez décompressé le ZIP.
Le transfert dure environ 7 minutes par langue et se termine par `CONFORME`. Pour **vérifier** une carte
sans rien écrire :

```powershell
& $Py -B tools\voice\sd_voice_upload.py --port COM7 --lang en --sd-root "C:\VoixWaveshare" --verify-only --log verif_en.log
```

> `--verify-only` n'envoie que des commandes de lecture : **aucun fichier n'est écrit, aucun dossier
> n'est créé**, même si la carte est incomplète. Pour les langues autres que le français, cela exige
> un firmware à jour (celui qui répond `VRX LIST dossier=…`) ; avec un firmware plus ancien, l'outil
> **refuse** la vérification au lieu d'écrire. Les journaux produits restent privés : ils contiennent
> le nom du port et des chemins locaux.

---

## 3. Parcours B — fabriquer ses propres sons (avancé)

À faire seulement pour **changer les textes**, ajouter une langue ou une autre voix. Les fichiers
publiés suffisent pour un usage normal.

### 3.1 Se placer dans le dossier du projet

Le dossier qui contient `main`, `tools` et `README_FR.md`. Exemple (adapter le chemin) :

```powershell
Set-Location 'E:\wsg-public'
Test-Path '.\tools\voice\gen_voice_google.py'      # doit afficher True
```

### 3.2 Installer Python et les bibliothèques

```powershell
py -3 --version                                     # sinon : installer Python pour Windows
py -3 -m venv tools\voice\.venv_tts
$Py = Join-Path (Get-Location) 'tools\voice\.venv_tts\Scripts\python.exe'
& $Py -m pip install numpy google-cloud-texttospeech pyserial
```

Versions utilisées pour produire les paquets publiés : **Python 3.12.9, numpy 2.5.3,
google-cloud-texttospeech 2.37.0**. SciPy n'est pas nécessaire (le rééchantillonnage est écrit dans
le script). L'environnement n'a pas besoin d'être « activé » : `& $Py` suffit. Après avoir ouvert une
nouvelle fenêtre PowerShell, refaire la ligne `$Py = ...`.

### 3.3 Estimer avant de payer quoi que ce soit

```powershell
& $Py -B tools\voice\gen_voice_google.py --lang fr --estimate
```

Cette commande **n'appelle pas Google**. Valeurs mesurées sur cette version :

| Langue | Caractères | Coût indicatif (30 USD / million) |
|---|---:|---:|
| fr | 11 941 | 0,36 USD |
| en | 11 450 | 0,34 USD |
| es | 11 800 | 0,35 USD |
| de | 12 011 | 0,36 USD |
| it | 10 492 | 0,31 USD |
| pt | 12 256 | 0,37 USD |
| **Total** | **69 950** | **2,10 USD** |

Google annonce **un million de caractères gratuits par mois** pour Chirp 3 HD, mais la facturation
doit être activée et le quota peut déjà avoir été consommé. Le script donne une estimation, pas un
plafond de dépenses : seul votre compte Google fait foi.

### 3.4 Configurer Google Cloud

1. Créer ou choisir un projet Google Cloud, activer la facturation, puis activer l'API
   **Cloud Text-to-Speech** ([guide officiel](https://cloud.google.com/text-to-speech/docs/before-you-begin)).
2. Installer **Google Cloud CLI** ([téléchargement officiel](https://cloud.google.com/sdk/docs/install)).
3. Dans une nouvelle fenêtre PowerShell :

```powershell
gcloud init
gcloud auth application-default login
$Projet = 'VOTRE_IDENTIFIANT_DE_PROJET'     # l'identifiant, pas le nom affiché
gcloud auth application-default set-quota-project $Projet
```

La connexion se fait dans le navigateur. **Aucun mot de passe ni fichier de clé ne doit être collé
dans une discussion, ajouté au dépôt ou copié sur la microSD.**

### 3.5 Vérifier l'accès, puis générer

```powershell
& $Py -B tools\voice\gen_voice_google.py --lang fr --check      # liste les voix, ne synthétise rien
& $Py -B tools\voice\gen_voice_google.py --lang fr --only glucose_100.wav   # un seul essai
Invoke-Item '.\tools\voice\bank\voice\glucose_100.wav'
& $Py -B tools\voice\gen_voice_google.py --lang fr              # la banque complète
```

Les commandes à partir de `--only` **consomment des caractères facturables**.

Résultats attendus : 396 WAV dans `tools/voice/bank/voice/` (français) ou
`tools/voice/bank/<code>/voice/` (autres langues), plus un `manifest.csv` à côté.

Le dossier `tools/voice/google_raw/` conserve les réponses brutes de Google pour ne pas repayer une
phrase déjà produite. Il est utile **en privé**, inutile sur la carte, et ne doit pas être publié.
Une nouvelle génération n'est pas garantie identique octet pour octet à l'ancienne : garder les WAV
et leur manifeste si l'on veut une distribution reproductible.

### 3.6 Changer un texte

Les textes sont dans `tools/voice/langues_voix.py`. Après modification :

```powershell
python tests\test_langues_voix.py
```

Ce test refuse toute modification des **textes français** qui s'écarterait du manifeste versionné
(`assets/banque_vocale_manifeste.csv`) : sans cela, les fichiers déjà copiés sur les cartes
deviendraient silencieusement obsolètes.

---

## 4. Ce qui est publié, ce qui ne l'est pas

| Fichier ou dossier | Rôle | Publié ? |
|---|---|---|
| `tools/voice/langues_voix.py` | textes des six langues, noms des voix | oui, avec le code |
| `tools/voice/gen_voice_google.py` | génération et conversion sur le PC | oui |
| `tools/voice/sd_voice_upload.py` | copie vers la microSD | oui |
| `tools/voice/bank/` | WAV finaux + manifestes | en pièces jointes d'une Release, pas dans Git |
| `tools/voice/google_raw/` | cache des réponses Google | non (sauvegarde privée) |
| `tools/voice/.venv_tts/` | Python et bibliothèques installés | non (réinstallables) |
| `main/voice/voice_fr.pcm` | ancienne voix Windows embarquée (console `voice_old`) | non : droits de redistribution non établis, voir [docs/LICENCES_TIERCES_FR.md](LICENCES_TIERCES_FR.md) |

Le firmware compile **avec ou sans** `main/voice/voice_fr.pcm`. Sans ce fichier, seule la commande
console historique `voice_old` devient indisponible ; les annonces normales (banque microSD) et le
bip de secours ne changent pas.

---

## 5. Si ça ne marche pas

| Symptôme | Cause la plus fréquente |
|---|---|
| Un bip au lieu d'une phrase | fichier absent sur la carte, ou carte non lue : vérifier l'arborescence du § 2.3 |
| L'appareil parle français alors que l'écran est en anglais | la banque `en/` manque : repli prévu sur le français |
| Rien du tout | volume à 0, carte absente, ou dossier `glycemie/audio/voice` vide |
| `--verify-only` refuse pour une langue | firmware plus ancien que la commande `vrx_dir … lecture` : recompiler et reflasher, ou vérifier la carte sur le PC |
| `Acces API refuse` à la génération | facturation ou API non activée, ou projet de quota non associé (§ 3.4) |

Diagnostic complet : [docs/DEPANNAGE_FR.md](DEPANNAGE_FR.md).

---

## Sources officielles

- Google Cloud Text-to-Speech — démarrage : <https://cloud.google.com/text-to-speech/docs/before-you-begin>
- Authentification locale (ADC) : <https://cloud.google.com/docs/authentication/provide-credentials-adc>
- Tarifs : <https://cloud.google.com/text-to-speech/pricing>
- Utilisation des fichiers audio produits : <https://cloud.google.com/text-to-speech/quotas>
- Pièces jointes des versions GitHub : <https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases>
