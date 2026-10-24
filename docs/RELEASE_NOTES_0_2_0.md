# WaveShare Glycémie — v0.2.0 (préversion / pre-release)

**Français**

Cette version est issue de sources compilées et testées pour le
**Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**, ESP32-P4 révision v1.3.

### Ce qui change
- Gestion plus prudente des erreurs de mémoire NVS pour préserver les réglages.
- Correction de l'effacement des identifiants Wi-Fi/Dexcom : confirmation réelle,
  maintien des accès en cas d'échec d'enregistrement, nettoyage de l'espace
  d'authentification correct, prévention du retour des secrets intégrés par
  d'anciennes compilations.
- Tests supplémentaires et automatisation des compilations sous Linux/Windows.
- Tutoriels français et anglais, crédits et licences révisés.
- Installateur USB : vérification des empreintes SHA-256 et de la révision
  matérielle avant toute installation, avec confirmation obligatoire.

### Fichiers joints
- **glycemie-installation-ecran-v0.2.0.zip** : programme, amorce, table des
  partitions, installateur USB et empreintes SHA-256.
- **GlycemieConnexion.exe** : saisie des accès par USB depuis Windows.
- **SHA256SUMS.txt** : empreintes des deux téléchargements ci-dessus.
- **SOURCE_COMMIT.txt** : commit des sources utilisé pour les constructions.
- **glycemie-microSD-v0.2.0.zip** : six banques vocales WAV pour la microSD,
  396 fichiers par langue, préparés à partir des ressources générées pour ce projet.
- **SHA256SUMS-voix.txt** : empreinte SHA-256 du ZIP microSD.

Le code source de la version correspondante est également fourni
automatiquement par GitHub (« Source code »).

### Son et microSD

Les six langues parlées sont proposées dans le fichier microSD séparé.
L'appareil fonctionne également sans les WAV : les bips restent disponibles.
Les instructions de copie avec ou sans lecteur sont dans le
[tutoriel des voix](https://github.com/ZerdoumRazine/Waveshare-Glycemie/blob/main/docs/VOICE_TUTORIAL_EN.md).

Les voix ont été synthétisées avec **Google Cloud Text-to-Speech**.
Google autorise l'utilisation des fichiers audio générés dans des applications
ou des médias, dans le respect de ses conditions et des lois applicables
([documentation officielle](https://docs.cloud.google.com/text-to-speech/quotas)).
Ces WAV sont distribués **uniquement comme ressources de cette application** ;
ils ne sont **pas** couverts par la licence MIT du code et aucun droit sur
les modèles ou marques de Google n'est revendiqué.
Le projet n'est ni affilié à Google ni approuvé par Google.

### Limites importantes
Première installation et configuration complètes sur **appareil neuf**
non validées sur matériel réel. L'application et les compilations ont passé
des tests automatisés, qui ne remplacent pas les essais d'intégration.
L'appareil est un affichage expérimental complémentaire, non certifié
pour un usage médical. Ne jamais l'utiliser pour décider d'un traitement
ou pour remplacer les alarmes officielles Dexcom.

---

**English**

This pre-release targets the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**
with ESP32-P4 revision v1.3 (pre-v3).

Changes: safer NVS error handling, corrected Wi-Fi/Dexcom credential
removal, additional regression tests, updated bilingual documentation,
and a USB installer with SHA-256 checks, hardware-revision checks,
and explicit confirmation.

Downloads: the firmware installation ZIP, Windows configuration EXE,
the six-language microSD WAV ZIP, checksums, and a file identifying the source commit.
GitHub also automatically provides a source-code archive.

The optional six-language WAV bank is provided as
`glycemie-microSD-v0.2.0.zip` with a separate SHA-256 checksum.
Without audio files, the display continues to produce alert beeps.
For installation see the
[English voice tutorial](https://github.com/ZerdoumRazine/Waveshare-Glycemie/blob/main/docs/VOICE_TUTORIAL_EN.md).

These files were generated using **Google Cloud Text-to-Speech**.
Google permits the use of generated audio files in applications and media
subject to its terms and applicable law
([official guidance](https://docs.cloud.google.com/text-to-speech/quotas)).
They are provided solely as this display's audio resources, **not under
the MIT source-code licence**. No rights in Google's models or brands
are claimed; the project is not affiliated with or endorsed by Google.

The complete first-time setup on an unused board has **not yet been
tested end-to-end on hardware**. This is an experimental supplementary
display, not certified for medical use. It must not replace official
Dexcom alarms or be used to make treatment decisions.
