#!/usr/bin/env python3
# EN: Builds the deliverable folders from clean sources, not from a personal device backup.
# FR: Prépare le paquet complet sans reprendre une sauvegarde personnelle.
r"""Assemble le dossier a remettre : sources, application Windows, installation, microSD, tutoriels.

Organisation produite :

    paquet_a_remettre/
        LISEZMOI.txt              par ou commencer (francais, puis anglais)
        code_source/              export propre du depot public (sans historique)
        application_windows/      GlycemieConnexion.exe + sa notice
        installation_ecran/       programme public + installer.ps1 + procedure exacte
        microSD/                  contenu a copier tel quel sur la carte
            glycemie/audio/voice/ francais a la racine, en/ es/ de/ it/ pt/ pour le reste
        tutoriels/                guides et captures
        telechargements/          les memes elements en ZIP, prets a distribuer
        notices/                  manifestes, provenance et empreintes des banques vocales
        SHA256SUMS.txt            empreinte de chaque fichier du paquet

Ce dossier est destine a etre donne ou publie. Il ne contient JAMAIS : le dossier private\,
un programme compile depuis le dossier personnel, un journal local, une sauvegarde d'appareil,
ni le contenu de la microSD personnelle (les sons viennent des paquets publics de dist_voix\).

Chaque fichier est verifie (tools/controle_donnees_perso.py). Si quelque chose de personnel est
trouve, le paquet est EFFACE : un paquet refuse ne doit pas rester parmi les fichiers a remettre.

Usage :
    python tools\preparer_paquet.py
    python tools\preparer_paquet.py --sans-zip      (plus rapide, sans les archives)
"""
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

# Fichiers d'accompagnement presents dans les paquets vocaux : utiles a lire, inutiles sur la carte.
HORS_CARTE = ('manifest.csv', 'SHA256SUMS.txt', 'PROVENANCE.md', 'LISEZMOI_README.txt')


# Fingerprint the exact sources so a binary can be traced back to its build.
def empreinte_sources(racine):
    """Empreinte du CONTENU de tout ce qui entre dans le firmware : main, components, config, CMake."""
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
    """ZIP reproductible : entrees triees, dates fixes."""
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
    r"""Extrait les six banques publiques aux emplacements attendus par le firmware.

    main/voice.c lit :   /sdcard/glycemie/audio/voice/<fichier>.wav        (francais)
                         /sdcard/glycemie/audio/voice/<langue>/<fichier>.wav
    Les paquets de dist_voix\ sont deja dans cette forme : rien n'est deplace ni renomme.
    """
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

    # Une compilation ancienne ne doit jamais etre prise pour celle de la derniere version.
    # On compare le CONTENU des sources du firmware (pas leur date : regenerer la copie publique
    # les rend toutes "recentes" sans rien changer) a l'empreinte enregistree a la compilation.
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

    # --- 1. code_source : export propre, sans historique ---------------------------------------
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

    # Provenance du programme livre : toujours ecrite, pour que l'on sache de quoi il vient.
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

    # --- 6. Verification AVANT les archives -----------------------------------------------------
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

    # --- 7. telechargements : les memes elements en ZIP -----------------------------------------
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

    # --- 8. Empreintes de tout le paquet --------------------------------------------------------
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


LISEZMOI = """WAVESHARE GLYCEMIE - dossier complet
====================================

Afficheur familial qui montre en grand la glycemie partagee par un capteur Dexcom G7, avec des
annonces parlees en six langues. Projet de Mohamed-Razine Zerdoum. Code sous licence MIT ;
les composants tiers gardent leurs auteurs et leurs licences (voir CREDITS.md et LICENSE).

AVERTISSEMENT
-------------
Affichage complementaire EXPERIMENTAL. Ce n'est pas un dispositif medical. Il peut se tromper,
afficher une valeur en retard ou s'interrompre. Il ne remplace ni le recepteur ni l'application
Dexcom officiels, et AUCUNE DECISION DE TRAITEMENT ne doit se fonder sur ce qu'il affiche.
Aucune certification ni validation medicale n'est revendiquee.

PAR OU COMMENCER
----------------
1. Lire  tutoriels\\docs\\TUTORIEL_DEBUTANT_FR.md  (tout y est, du materiel au depannage).
2. Installer le programme sur l'ecran : dossier  installation_ecran\\  (sa notice explique tout).
3. Saisir ses propres identifiants : sur l'ecran tactile, ou avec  application_windows\\.
4. Copier les sons sur la carte microSD : dossier  microSD\\  (sa notice explique tout).

CE QUE CONTIENT CHAQUE DOSSIER
------------------------------
code_source\\          Tout le code, commente, avec ses tests et ses outils. C'est ce qui part
                      sur GitHub. Rien n'y est personnel.
application_windows\\  GlycemieConnexion.exe : configure l'ecran depuis le PC par le cable USB.
                      Rien a installer (ni Python, ni bibliotheque).
installation_ecran\\   Le programme public de l'ecran (sans aucun identifiant) et son
                      installateur. C'est la PREMIERE installation, complete.
microSD\\              A copier sur la carte memoire : les six banques de voix, aux emplacements
                      attendus par le programme.
tutoriels\\            Les guides (francais et anglais) et les captures d'ecran.
telechargements\\      Les memes elements en archives ZIP, pretes a distribuer.
notices\\              Manifestes, provenance et empreintes des banques vocales (a lire, pas a
                      copier sur la carte).
SHA256SUMS.txt        Empreinte de chaque fichier, pour verifier que rien n'a ete abime.

CE QUI N'A PAS ETE ESSAYE
-------------------------
L'installation, la mise a jour, la reconnexion Wi-Fi et Dexcom, les annonces parlees et
l'ouverture de l'assistant ont ete verifiees sur un appareil reel. La PREMIERE CONFIGURATION
d'un appareil neuf - saisir de nouveaux identifiants, les enregistrer, redemarrer - n'a ete
validee qu'en simulation sur PC. Voir tutoriels\\docs\\LIMITATIONS_FR.md.


WAVESHARE GLYCEMIE - complete package (English)
===============================================

A family display showing, in large digits, the glucose value shared by a Dexcom G7 sensor, with
spoken announcements in six languages. A project by Mohamed-Razine Zerdoum. MIT licence; third
party components keep their own authors and licences.

WARNING
-------
EXPERIMENTAL secondary display. Not a medical device. It can be wrong, show a late value or stop.
It does not replace the official Dexcom receiver or app, and NO TREATMENT DECISION should be based
on what it shows. No medical certification or validation is claimed.

WHERE TO START
--------------
1. Read  tutoriels\\docs\\BEGINNER_TUTORIAL_EN.md  (everything, from hardware to troubleshooting).
2. Install the firmware: folder  installation_ecran\\  (its notice explains each step).
3. Enter your own credentials: on the touchscreen, or with  application_windows\\.
4. Copy the sounds onto the microSD card: folder  microSD\\  (its notice explains how).

WHAT EACH FOLDER HOLDS
----------------------
code_source\\          All the code, commented, with its tests and tools. Nothing personal in it.
application_windows\\  GlycemieConnexion.exe: configures the screen from the PC over USB.
                      Nothing to install (no Python, no libraries).
installation_ecran\\   The public firmware (no credentials at all) and its installer. This is the
                      full first-time installation.
microSD\\              To copy onto the memory card: the six voice banks, in the exact locations
                      the firmware expects.
tutoriels\\            The guides (French and English) and the screenshots.
telechargements\\      The same items as ZIP archives, ready to distribute.
notices\\              Manifests, provenance and checksums of the voice banks (to read, not to
                      copy onto the card).
SHA256SUMS.txt        Checksum of every file, to check nothing was damaged.

NOT YET TESTED
--------------
Installation, update, Wi-Fi and Dexcom reconnection, spoken announcements and opening the setup
wizard were verified on a real device. FIRST-TIME SETUP of a blank device - entering new
credentials, saving them, restarting - has only been validated in simulation on a PC.
"""

NOTICE_APPLICATION = """APPLICATION WINDOWS - GlycemieConnexion.exe
===========================================

A QUOI CA SERT
--------------
A renseigner, essayer, enregistrer ou effacer les acces de l'ecran (reseau Wi-Fi et compte Dexcom
Share) depuis le PC, par le cable USB. C'est une alternative a l'assistant tactile de l'ecran,
utile si taper au doigt est penible.

CE N'EST PAS un programme d'installation : il ne met pas le logiciel sur l'ecran. Pour cela, voir
le dossier  installation_ecran\\ .

COMMENT S'EN SERVIR
-------------------
1. Branchez l'ecran au PC avec son cable USB-C (celui de la prise UART).
2. Double-cliquez sur GlycemieConnexion.exe. Rien a installer : ni Python, ni bibliotheque.
3. "Rechercher" puis choisissez l'ecran dans la liste (USB-Enhanced-SERIAL CH343).
4. "Lire l'etat de l'appareil" montre ce qui est deja enregistre, mots de passe masques.
5. Remplissez le reseau Wi-Fi, puis le compte Dexcom et la region.
6. "Essayer la connexion" : l'appareil essaie, et explique en clair ce qui bloque le cas echeant.
7. "Enregistrer" ne devient actif qu'APRES un essai reussi. Tant que vous n'appuyez pas, les acces
   precedents de l'ecran restent en place.
8. Debranchez une seconde, rebranchez : l'ecran se connecte.

AU PREMIER LANCEMENT
--------------------
Windows peut afficher "Editeur inconnu" (SmartScreen) : Informations complementaires -> Executer
quand meme. Le programme n'est pas signe numeriquement (un certificat coute cher), mais son
empreinte SHA-256 est publiee : verifiez-la avec
    Get-FileHash GlycemieConnexion.exe -Algorithm SHA256

CE QU'IL NE FAIT PAS
--------------------
Il ne va jamais sur Internet, n'ecrit aucun mot de passe sur le PC, et n'affiche jamais un mot de
passe en clair (sauf si vous cochez "Afficher"). Les acces saisis servent uniquement aux
connexions normales de l'appareil : votre box Wi-Fi et les serveurs Dexcom Share.

SI L'ECRAN N'EST PAS DETECTE
----------------------------
- Essayez l'autre prise USB-C de la carte : une seule est reliee au pont serie.
- Beaucoup de cables USB-C ne transportent que le courant : essayez-en un autre.
- Fermez tout autre logiciel qui occupe le port (moniteur serie, idf.py monitor).

Le code source de cette application est dans  code_source\\tools\\pc_app\\ .
"""

NOTICE_INSTALLATION = """INSTALLATION DU PROGRAMME SUR L'ECRAN
=====================================

Ce dossier installe le logiciel sur une carte NEUVE comme sur une carte deja utilisee. C'est une
installation COMPLETE : amorce, table des partitions et programme.

MATERIEL
--------
- Carte Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (ecran tactile 7 pouces, 1024 x 600).
- Un cable USB-C qui transporte les DONNEES (beaucoup ne font que charger).
- La prise USB-C "UART" de la carte : c'est celle qui apparait sur le PC comme
  "USB-Enhanced-SERIAL CH343". L'autre prise USB-C ne sert pas ici.

CE QU'IL FAUT SUR LE PC
-----------------------
- Windows 10 ou 11.
- Python 3 (python.org, cocher "Add python.exe to PATH").
- Puis, une seule fois, dans PowerShell :    py -3 -m pip install esptool

INSTALLER
---------
1. Branchez l'ecran. Attendez deux secondes.
2. Clic droit sur  installer.ps1  ->  Executer avec PowerShell.
3. Comptez deux minutes. Le script affiche "C'est installe." a la fin.
4. L'ecran redemarre et ouvre son assistant de configuration.

Si plusieurs appareils sont branches, le script s'arrete et vous montre la liste. Relancez-le en
indiquant le bon port, par exemple :    .\\installer.ps1 -Port COM7

CE QUE FAIT - ET NE FAIT PAS - L'INSTALLATION
---------------------------------------------
Ecrit : l'amorce (0x2000), la table des partitions (0x8000) et le programme (0x10000).
Ces adresses viennent de la compilation reelle du projet, pas d'une supposition.
N'efface PAS la memoire des reglages (langue, volume, dates de capteur et de catheter, stock).
Ne formate PAS la microSD et n'ecrit rien dessus.
Ne touche PAS au second processeur ESP32-C6 (celui du Wi-Fi).
N'utilise aucune operation irreversible : pas d'effacement general, pas d'eFuse, pas de Secure
Boot, pas de chiffrement de flash.

APRES L'INSTALLATION
--------------------
Le programme ne contient AUCUN identifiant. Chacun saisit les siens :
- sur l'ecran tactile : l'assistant s'ouvre tout seul au premier demarrage ;
- ou depuis le PC : dossier  application_windows\\ .

Les deux choses sont differentes : ICI on installe le logiciel, LA on le configure.

VERIFIER LES FICHIERS
---------------------
SHA256SUMS.txt contient l'empreinte de chaque fichier. Sous Windows :
    Get-FileHash glycemie_display.bin -Algorithm SHA256

REVENIR EN ARRIERE
------------------
Avant d'installer sur un appareil qui fonctionne deja, sauvegardez-le :
voir  tutoriels\\docs\\RETOUR_ARRIERE_FR.md  (sauvegarde verifiee, retour en arriere, et marche a
suivre si l'ecran ne demarre plus).
"""

NOTICE_MICROSD = """CONTENU A COPIER SUR LA CARTE microSD
=====================================

COPIER LE CONTENU DU DOSSIER microSD A LA RACINE DE VOTRE CARTE, PAS LE DOSSIER microSD LUI-MEME.

Autrement dit, apres la copie, la carte doit contenir un dossier  glycemie\\  a sa racine :

    (racine de la carte)\\glycemie\\audio\\voice\\alert_low.wav      <- francais, a la racine
    (racine de la carte)\\glycemie\\audio\\voice\\en\\alert_low.wav   <- anglais
    (racine de la carte)\\glycemie\\audio\\voice\\es\\ ... de\\ ... it\\ ... pt\\

C'est exactement ce que le programme cherche. Le francais est a la racine parce qu'il sert de
secours quand la banque d'une autre langue manque.

Vous pouvez ne copier qu'une seule langue : prenez  glycemie\\audio\\voice\\ (francais) et, si
besoin, le sous-dossier de votre langue. Chaque langue pese environ 20 Mo.

CARTE DEJA UTILISEE
-------------------
N'effacez rien et ne remplacez pas tout le contenu : ajoutez seulement ce qui manque. Les dossiers
glycemie\\history\\ et glycemie\\logs\\ de votre carte contiennent votre historique de mesures.
Ne formatez pas la carte, et refusez si Windows propose de la "reparer".

CARTE NEUVE
-----------
microSD de 8 a 64 Go, formatee en FAT32. exFAT n'est PAS reconnu par le programme.
La carte reellement utilisee et testee pour ce projet est une 64 Go SDXC formatee en FAT32.

SANS LECTEUR DE CARTE
---------------------
La carte peut rester dans l'ecran : les sons passent par le meme cable USB que l'installation.
Il faut Python et le dossier  code_source\\tools\\  :

    py -3 -m pip install pyserial
    py -3 tools\\voice\\sd_voice_upload.py --port COM7 --lang fr --log copie_voix_fr.log

Comptez environ 7 minutes par langue. L'outil se termine par CONFORME quand les 396 fichiers ont
ete ecrits puis relus. Pour verifier sans rien ecrire, ajoutez  --verify-only .

ATTENTION : copier les sons N'INSTALLE PAS le programme de l'ecran. Ce sont deux choses
differentes ; l'installation se fait avec le dossier  installation_ecran\\ .

CE QUI N'EST PAS ICI
--------------------
Aucun historique, aucun journal, aucun enregistrement personnel : ces fichiers sont crees par
l'appareil lui-meme, sur votre carte. Les manifestes, la provenance et les empreintes des banques
sont dans le dossier  notices\\ , a lire mais pas a copier sur la carte.
"""


if __name__ == '__main__':
    sys.exit(main())
