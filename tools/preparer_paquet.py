#!/usr/bin/env python3
# Assembles delivery resources and verifies their contents.
#
# Design note: Only documented public resources should enter a release.
# Builds the deliverable folders from clean sources, not from a personal device backup.
#
"""Assemble the distributable package from public source and verified artifacts.

The output includes source code, the Windows setup utility, firmware installer,
optional microSD speech banks, English manuals and checksums. Build from a
clean export rather than a personal device backup. Scan every candidate file
for private data, and reject incomplete or unsafe releases.

Usage: python tools/preparer_paquet.py [--sans-zip]"""
import argparse
import hashlib
import io
import os
import shutil
import sys
import zipfile

ICI = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(ICI, '..'))
sys.path.insert(0, ICI)
import controle_donnees_perso as cdp  # noqa: E402

SOURCES = os.path.join(ROOT, 'Razine')
LIVRAISON = os.path.join(ROOT, 'livraison')
BANQUES = os.path.join(ROOT, 'dist_voix')

# Accompaniment files in voice packages: useful to read, useless on the map.
HORS_CARTE = ('manifest.csv', 'SHA256SUMS.txt', 'PROVENANCE.md', 'LISEZMOI_README.txt')


# Fingerprint the exact sources so a binary can be traced back to its build.
def empreinte_sources(racine):
    """Hash all firmware inputs, including source, components, configuration and build files."""
    h = hashlib.sha256()
    elements = []
    for dossier in ('main', 'components', 'config'):
        base = os.path.join(racine, dossier)
        for chemin_base, dossiers, fichiers in os.walk(base):
            dossiers.sort()
            for nom in sorted(fichiers):
                chemin = os.path.join(chemin_base, nom)
                elements.append((os.path.relpath(chemin, racine).replace(os.sep, '/'), chemin))
    for nom in ('CMakeLists.txt', 'sdkconfig.defaults', 'partitions.csv', 'dependencies.lock'):
        chemin = os.path.join(racine, nom)
        if os.path.exists(chemin):
            elements.append((nom, chemin))
    for rel, chemin in sorted(elements):
        h.update(rel.encode('utf-8'))
        with open(chemin, 'rb') as f:
            h.update(f.read())
    return h.hexdigest()


def copier(source, destination):
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    shutil.copyfile(source, destination)


def ecrire(chemin, texte):
    os.makedirs(os.path.dirname(chemin), exist_ok=True)
    io.open(chemin, 'w', encoding='utf-8', newline='\r\n').write(texte)


# Write archive members in a stable order to make package contents reproducible.
def zipper(dossier, archive, base=''):
    """Create a deterministic ZIP with sorted entries and fixed timestamps."""
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as zf:
        for racine, dossiers, fichiers in os.walk(dossier):
            dossiers.sort()
            for nom in sorted(fichiers):
                chemin = os.path.join(racine, nom)
                interne = os.path.relpath(chemin, dossier).replace('\\', '/')
                info = zipfile.ZipInfo(base + interne, date_time=(2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                with open(chemin, 'rb') as f:
                    zf.writestr(info, f.read())


# Preserve the one French root directory and five language subfolders expected by firmware.
def construire_microsd(cible, notices):
    """Place all six approved voice banks in the microSD layout required by the firmware."""
    compte = {}
    for nom in sorted(os.listdir(BANQUES)):
        if not (nom.startswith('glycemie-voix-') and nom.endswith('.zip')):
            continue
        langue = nom.split('-')[2]
        with zipfile.ZipFile(os.path.join(BANQUES, nom)) as zf:
            for entree in zf.infolist():
                if entree.is_dir():
                    continue
                base = os.path.basename(entree.filename)
                if base in HORS_CARTE:
                    destination = os.path.join(notices, langue, base)
                else:
                    destination = os.path.join(cible, entree.filename.replace('/', os.sep))
                os.makedirs(os.path.dirname(destination), exist_ok=True)
                with open(destination, 'wb') as f:
                    f.write(zf.read(entree))
                if base not in HORS_CARTE:
                    compte[langue] = compte.get(langue, 0) + 1
    return compte


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dossier', default=os.path.join(ROOT, 'paquet_a_remettre'))
    ap.add_argument('--sans-zip', action='store_true')
    ap.add_argument('--raison-sources', default=None,
                    help="accepter que les sources aient change depuis la compilation, en disant "
                         "pourquoi ; la raison est ecrite dans le paquet (notices/PROVENANCE_PROGRAMME.txt)")
    ap.add_argument('--empreinte', action='store_true',
                    help="afficher seulement l'empreinte des sources du firmware, et quitter")
    args = ap.parse_args()

    if args.empreinte:
        print(empreinte_sources(SOURCES))
        return 0
    cible = os.path.abspath(args.dossier)

    for chemin, quoi in ((os.path.join(SOURCES, 'main', 'app_main.c'), 'les sources publiques'),
                         (os.path.join(LIVRAISON, 'glycemie_display.bin'), "le programme de l'ecran"),
                         (os.path.join(LIVRAISON, 'GlycemieConnexion.exe'), "l'application Windows")):
        if not os.path.exists(chemin):
            print('Manque %s (%s).' % (quoi, chemin))
            print('Lancez d\'abord :  python tools\\preparer_razine.py')
            print('                  powershell -File tools\\publier_firmware.ps1')
            return 1
    if os.path.exists(os.path.join(SOURCES, 'private', 'device_secrets.h')):
        print('ARRET : la copie publique contient des identifiants.')
        return 1

    # An old compilation must never be taken for the last version.
    # We compare the CONTENU firmware sources (not their date: regenerate public copy
    # makes them all "recent" without changing anything) to the fingerprint recorded at the compilation.
    attendue = os.path.join(LIVRAISON, 'sources.sha256')
    actuelle = empreinte_sources(SOURCES)
    if not os.path.exists(attendue):
        print('ARRET : %s est absent : impossible de savoir de quelles sources vient le programme.'
              % attendue)
        print('Recompilez :  powershell -File tools' + os.sep + 'publier_firmware.ps1')
        return 1
    enregistree = io.open(attendue, encoding='utf-8').read().split()[0]
    derogation = None
    if enregistree != actuelle:
        if not args.raison_sources:
            print('ARRET : le programme livre ne vient pas de ces sources.')
            print('   empreinte du programme : %s' % enregistree[:16])
            print('   empreinte des sources  : %s' % actuelle[:16])
            print('Recompilez :  powershell -File tools' + os.sep + 'publier_firmware.ps1')
            print("Ou, si la difference ne touche pas le code compile, dites pourquoi :")
            print('   --raison-sources "..."')
            return 1
        derogation = (enregistree, actuelle, args.raison_sources)
        print('ATTENTION : sources modifiees depuis la compilation. Raison donnee :')
        print('   %s' % args.raison_sources)

    if os.path.isdir(cible):
        shutil.rmtree(cible, ignore_errors=True)
    os.makedirs(cible)

    # --- 1. code_source : own export, without history ---------------------------------------
    shutil.copytree(SOURCES, os.path.join(cible, 'code_source'),
                    ignore=shutil.ignore_patterns('.git', 'build', '__pycache__', '*.pyc'))

    # --- 2. application_windows ----------------------------------------------------------------
    copier(os.path.join(LIVRAISON, 'GlycemieConnexion.exe'),
           os.path.join(cible, 'application_windows', 'GlycemieConnexion.exe'))
    ecrire(os.path.join(cible, 'application_windows', 'LISEZMOI.txt'), NOTICE_APPLICATION)

    # --- 3. installation_ecran -----------------------------------------------------------------
    for nom in ('glycemie_display.bin', 'bootloader.bin', 'partition-table.bin', 'installer.ps1',
                'SHA256SUMS.txt'):
        source = os.path.join(LIVRAISON, nom)
        if os.path.exists(source):
            copier(source, os.path.join(cible, 'installation_ecran', nom))
    ecrire(os.path.join(cible, 'installation_ecran', 'LISEZMOI.txt'), NOTICE_INSTALLATION)

    # --- 4. microSD + notices ------------------------------------------------------------------
    compte = construire_microsd(os.path.join(cible, 'microSD'), os.path.join(cible, 'notices'))
    for nom in ('SHA256SUMS-paquets.txt', 'NOTES_RELEASE.md', 'paquets.json'):
        source = os.path.join(BANQUES, nom)
        if os.path.exists(source):
            copier(source, os.path.join(cible, 'notices', nom))
    ecrire(os.path.join(cible, 'microSD', 'LISEZMOI.txt'), NOTICE_MICROSD)

    # Origin of the book program: always written, so that we know what it comes from.
    provenance = [
        "PROVENANCE DU PROGRAMME DE L'ECRAN",
        '',
        'glycemie_display.bin a ete compile depuis les sources publiques de ce paquet',
        '(dossier code_source), sans aucun identifiant.',
        '',
        'Empreinte du contenu des sources au moment de la compilation :',
        '   %s' % enregistree,
    ]
    if derogation is not None:
        provenance += [
            '',
            'Les sources ont ete modifiees APRES cette compilation :',
            '   empreinte actuelle : %s' % derogation[1],
            '   raison acceptee    : %s' % derogation[2],
            '',
            'Autrement dit : le programme fourni a ete compile depuis une version de ces sources',
            'anterieure a cette modification. Recompilez si vous voulez une correspondance exacte.',
        ]
    else:
        provenance += ['', 'Les sources de ce paquet sont exactement celles de cette compilation.']
    ecrire(os.path.join(cible, 'notices', 'PROVENANCE_PROGRAMME.txt'),
           chr(10).join(provenance) + chr(10))

    # --- 5. tutoriels --------------------------------------------------------------------------
    for nom in ('TUTORIEL_DEBUTANT_FR.md', 'BEGINNER_TUTORIAL_EN.md', 'TUTORIEL_VOIX_FR.md',
                'VOICE_TUTORIAL_EN.md', 'RETOUR_ARRIERE_FR.md', 'LIMITATIONS_FR.md',
                'HARDWARE_FR.md', 'DEPANNAGE_FR.md', 'LICENCES_TIERCES_FR.md',
                'CONFIGURATION_DEXCOM_FR.md', 'UTILISATION_FR.md'):
        source = os.path.join(SOURCES, 'docs', nom)
        if os.path.exists(source):
            copier(source, os.path.join(cible, 'tutoriels', 'docs', nom))
    captures = os.path.join(SOURCES, 'screenshots')
    if os.path.isdir(captures):
        shutil.copytree(captures, os.path.join(cible, 'tutoriels', 'screenshots'))
    for nom in ('LICENSE', 'CREDITS.md', 'README.md', 'README_FR.md'):
        source = os.path.join(SOURCES, nom)
        if os.path.exists(source):
            copier(source, os.path.join(cible, nom))
    ecrire(os.path.join(cible, 'LISEZMOI.txt'), LISEZMOI)

    # --- 6. Verification AVANT archives -----------------------------------------------------
    secrets = cdp.secrets_reels()
    trouves, lus = [], 0
    for base, _, fichiers in os.walk(cible):
        for nom in fichiers:
            chemin = os.path.join(base, nom)
            rel = os.path.relpath(chemin, cible).replace('\\', '/')
            data = cdp.lire(chemin, binaire=True)
            if data is None:
                continue
            lus += 1
            if nom.lower().endswith('.zip'):
                cdp.controler_zip(rel, chemin, secrets, trouves)
            elif rel.lower().endswith(cdp.MEDIA):
                cdp.controler_media(rel, data, secrets, trouves)
            elif rel.lower().endswith(cdp.BINAIRES):
                cdp.controler_binaire(rel, data, secrets, trouves)
            else:
                cdp.controler_texte(rel, data.decode('utf-8', errors='replace'), secrets, trouves)
    if not secrets:
        trouves.append(('(tout le paquet)', 'verification impossible',
                        'private\\device_secrets.h absent'))
    if trouves or lus < 2000:
        for nom, genre, detail in trouves[:20]:
            print('INTERDIT : %s : %s (%s)' % (nom, genre, detail))
        if lus < 2000:
            print('INTERDIT : seulement %d fichier(s) lus : paquet incomplet' % lus)
        shutil.rmtree(cible, ignore_errors=True)
        print('\nRESULTAT : A CORRIGER - le paquet a ete efface.')
        return 1

    # --- 7. telecommunications : the same elements in ZIP -----------------------------------------
    if not args.sans_zip:
        archives = os.path.join(cible, 'telechargements')
        os.makedirs(archives, exist_ok=True)
        zipper(os.path.join(cible, 'microSD'), os.path.join(archives, 'glycemie-microSD.zip'))
        zipper(os.path.join(cible, 'installation_ecran'),
               os.path.join(archives, 'glycemie-installation-ecran.zip'))
        zipper(os.path.join(cible, 'code_source'), os.path.join(archives, 'glycemie-code-source.zip'))
        copier(os.path.join(cible, 'application_windows', 'GlycemieConnexion.exe'),
               os.path.join(archives, 'GlycemieConnexion.exe'))
        lignes = []
        for nom in sorted(os.listdir(archives)):
            with open(os.path.join(archives, nom), 'rb') as f:
                lignes.append('%s  %s' % (hashlib.sha256(f.read()).hexdigest(), nom))
        ecrire(os.path.join(archives, 'SHA256SUMS.txt'), '\n'.join(lignes) + '\n')

    # --- 8. Prints of the whole package --------------------------------------------------------
    lignes, total, nb = [], 0, 0
    for base, _, fichiers in os.walk(cible):
        for nom in sorted(fichiers):
            chemin = os.path.join(base, nom)
            rel = os.path.relpath(chemin, cible).replace('\\', '/')
            if rel == 'SHA256SUMS.txt':
                continue
            with open(chemin, 'rb') as f:
                data = f.read()
            total += len(data)
            nb += 1
            lignes.append('%s  %s' % (hashlib.sha256(data).hexdigest(), rel))
    ecrire(os.path.join(cible, 'SHA256SUMS.txt'),
           '\n'.join(sorted(lignes, key=lambda l: l.split('  ', 1)[1])) + '\n')

    print('Paquet pret : %s' % cible)
    print('%d fichiers, %.1f Mo, tous verifies (aucun identifiant, aucune donnee personnelle).'
          % (nb + 1, total / 1048576))
    print('Banques vocales copiees : %s' % ', '.join('%s=%d' % (k, v) for k, v in sorted(compte.items())))
    for dossier in sorted(os.listdir(cible)):
        chemin = os.path.join(cible, dossier)
        if os.path.isdir(chemin):
            n = sum(len(f) for _, _, f in os.walk(chemin))
            o = sum(os.path.getsize(os.path.join(b, f)) for b, _, fs in os.walk(chemin) for f in fs)
            print('   %-22s %5d fichiers %8.1f Mo' % (dossier, n, o / 1048576))
    return 0


LISEZMOI = """WAVESHARE GLYCEMIE — complete package
====================================

A supplementary family display for glucose readings shared by a Dexcom G7,
with touch controls and optional speech announcements in six languages.
Project by Mohamed-Razine Zerdoum. Original code: MIT licence. Third-party
software and audio retain their own licensing terms.

IMPORTANT SAFETY NOTICE
-----------------------
EXPERIMENTAL DISPLAY, NOT CERTIFIED FOR MEDICAL USE. Its values may be delayed,
incorrect or unavailable. This project does not replace an official Dexcom
receiver, the Dexcom application or their alarms. Never base treatment
decisions on this display.

GETTING STARTED
---------------
1. Read tutoriels/docs/BEGINNER_TUTORIAL_EN.md.
2. Install the firmware using the installation_ecran/ package.
3. Configure Wi-Fi and Dexcom Share using the touchscreen wizard or the
   application_windows/ USB configuration program.
4. Optionally copy the speech banks from microSD/ to the memory card.

PACKAGE CONTENTS
----------------
code_source/         Source code, tests, developer notes and tooling.
application_windows/ GlycemieConnexion.exe, the optional USB setup application.
installation_ecran/  Firmware images and the installation script.
microSD/             Optional six-language speech files for the card.
tutoriels/           English-language guides and demonstration screenshots.
telechargements/     Downloadable ZIP packages.
notices/             Audio provenance, inventories and integrity checks.
SHA256SUMS.txt       SHA-256 hashes for the distributed files.

VALIDATION LIMIT
----------------
Firmware installation, update, reconnection, audio playback and the setup
wizard have been exercised on an existing board. Entering and saving new
credentials on a completely blank board has only been checked in simulation.
See tutoriels/docs/LIMITATIONS_EN.md for details.
"""

NOTICE_APPLICATION = """WINDOWS USB SETUP — GlycemieConnexion.exe
==========================================

PURPOSE
-------
This utility configures the display's Wi-Fi and Dexcom Share credentials over
its UART/CH343 USB connection. It can test and save new settings or clear
credentials. It does NOT install the firmware.

HOW TO USE IT
-------------
1. Connect the board to the PC through its UART/CH343 USB-C port.
2. Open GlycemieConnexion.exe. No Python installation is required.
3. Select "Rechercher" (Search) and choose the detected display.
4. Select "Lire l'etat de l'appareil" (Read device state). Passwords stay masked.
5. Enter Wi-Fi information and the Dexcom Share account and region.
6. Choose "Essayer la connexion" (Try connection) and review its result.
7. "Enregistrer" (Save) activates only after a successful connection trial.
   Existing credentials are kept until the change succeeds and is confirmed.
8. Disconnect and reconnect the display so it starts with the new settings.

WINDOWS SMARTSCREEN
-------------------
The executable is not code-signed. Windows may show an unknown publisher
warning. Verify the published SHA-256 hash before running it:
    Get-FileHash GlycemieConnexion.exe -Algorithm SHA256

PRIVACY
-------
The tool does not contact Dexcom or the Internet directly, and does not
store your passwords on the PC. Credentials are sent over USB to the display
for its own Wi-Fi and Dexcom connections.

IF THE DEVICE IS NOT FOUND
--------------------------
Try a USB-C data cable (some are power-only), verify that Windows lists
USB-Enhanced-SERIAL CH343, and close any other application using its COM port.

Source: code_source/tools/pc_app/.
"""

NOTICE_INSTALLATION = """FIRMWARE INSTALLATION — Waveshare ESP32-P4 7B
============================================

BOARD AND REQUIREMENTS
----------------------
This build targets Waveshare ESP32-P4-WIFI6-Touch-LCD-7B with ESP32-P4 rev 1.3
(pre-v3). Other board revisions are not validated.
Use a USB-C DATA cable on the UART/CH343 port and a Windows 10 or 11 PC.
Install Python 3, then in PowerShell run:
    py -3 -m pip install esptool

INSTALL THE FIRMWARE
--------------------
1. Extract the complete installation archive into one Windows folder.
2. Connect the board's UART/CH343 USB-C port.
3. Close serial monitors or other software using that COM port.
4. Open PowerShell in this folder and run:
    powershell -ExecutionPolicy Bypass -File .\\installer.ps1
5. The script checks the hashes, detects the hardware and asks for approval.
6. If automatic port selection fails, provide a port explicitly, for example:
    powershell -ExecutionPolicy Bypass -File .\\installer.ps1 -Port COM7
7. After reboot, configure Wi-Fi and Dexcom with the touchscreen wizard
   or with the separately supplied GlycemieConnexion.exe program.

WHAT THE INSTALLER CHANGES
--------------------------
It writes the bootloader (0x2000), partition table (0x8000) and firmware
(0x10000). It does not request a full-chip erase, format the microSD or
update the ESP32-C6 Wi-Fi coprocessor. It does not enable irreversible
eFuse, Secure Boot or flash encryption operations.
Back up any previously configured device before installing.

VALIDATE AND RECOVER
--------------------
SHA256SUMS.txt lists integrity hashes for the distributed files. On Windows:
    Get-FileHash glycemie_display.bin -Algorithm SHA256
For backup and recovery guidance, read
tutoriels/docs/ROLLBACK_EN.md.

WARNING: Experimental supplementary display, not a medical alarm system.
"""

NOTICE_MICROSD = """OPTIONAL SPEECH AUDIO — microSD instructions
============================================

The display works without these recordings; alert beeps remain available.
To install speech, copy the CONTENTS of microSD/ to the card root, not the
microSD directory itself.

Expected locations:
    glycemie/audio/voice/alert_low.wav      French fallback bank
    glycemie/audio/voice/en/alert_low.wav   English
    glycemie/audio/voice/es/                Spanish
    glycemie/audio/voice/de/                German
    glycemie/audio/voice/it/                Italian
    glycemie/audio/voice/pt/                Portuguese

The French recordings are stored directly in voice/ so the firmware can use
them as a fallback when the selected language's speech bank is missing.
Each language bank contains 396 WAV files.

EXISTING CARD
-------------
Back up the card and merge files; do not delete glycemie/history/ or
glycemie/logs/. Never accept a Windows prompt to format the card to repair
it. A FAT32 card is required; exFAT is not supported.

WITHOUT A CARD READER
---------------------
Leave the microSD in the display and use the UART/CH343 USB-C connection.
From the source repository on a PC with Python 3, install pyserial:
    py -3 -m pip install pyserial
Then use tools/voice/sd_voice_upload.py with the extracted voice-bank folder
and the appropriate --lang code. See
tutoriels/docs/VOICE_TUTORIAL_EN.md for the complete verified procedure.

Audio files are separate from firmware installation. WAV provenance and
third-party terms are listed in notices/ and in
tutoriels/docs/THIRD_PARTY_LICENSES_EN.md. The audio package contains no
personal medical records or device credentials.
"""


if __name__ == '__main__':
    sys.exit(main())
