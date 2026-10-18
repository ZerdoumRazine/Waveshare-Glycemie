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

Le code source de la version correspondante est également fourni
automatiquement par GitHub (« Source code »).

### Son et microSD
Les banques WAV en six langues sont optionnelles ; sans elles, les alarmes
sonores de secours restent disponibles. Les scripts et instructions pour
produire et transférer les banques sont dans
[le tutoriel voix](docs/TUTORIEL_VOIX_FR.md).

**Les enregistrements générés par Google ne sont pas redistribués à nouveau
dans cette livraison automatique.** Vérifier les conditions de diffusion
avant de proposer publiquement les anciennes archives vocales.

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
checksums, and a text file identifying the source commit.
GitHub also automatically provides a source-code archive.

The six-language WAV voice banks are optional. Sound-generation and
microSD instructions remain in
[VOICE_TUTORIAL_EN.md](docs/VOICE_TUTORIAL_EN.md).
Generated Google audio is **not repackaged in this automated release**;
rights to redistribute prior voice archives should be checked before
public distribution.

The complete first-time setup on an unused board has **not yet been
tested end-to-end on hardware**. This is an experimental supplementary
display, not certified for medical use. It must not replace official
Dexcom alarms or be used to make treatment decisions.
