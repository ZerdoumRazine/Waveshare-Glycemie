#!/usr/bin/env python3
r"""Controles du depot avant publication : coherence du projet et absence de donnees privees.

Ne compile rien et n'accede pas a la carte. A lancer depuis la racine du projet :
    python tests\test_depot.py
Code de sortie 0 seulement si tous les controles passent.
"""
import io
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# Fichiers et dossiers exclus du depot (voir .gitignore) : ils ne sont pas controles.
IGNORES = {'build', 'managed_components', 'tools/espressif', 'tools/esp-idf-v5.5.5', 'tools/cache',
           'tools/logs', 'tests/build', '.git', '.venv', '__pycache__', 'private'}
TEXTE = {'.c', '.h', '.py', '.ps1', '.md', '.txt', '.csv', '.yml', '.yaml', '.json', '.lock', '.defaults', ''}

resultats = []


def check(nom, ok, detail=''):
    resultats.append((nom, bool(ok), detail))


def fichiers_versionnes():
    """Ce que Git publierait reellement ; a defaut (pas de depot Git), tout l'arbre sauf les exclusions."""
    try:
        sortie = subprocess.run(['git', '-C', ROOT, 'ls-files', '-z'], capture_output=True, timeout=60)
        suivis = [n for n in sortie.stdout.decode('utf-8').split('\0') if n]
    except (OSError, subprocess.SubprocessError):
        suivis = []
    if suivis:
        for rel in suivis:
            chemin = os.path.join(ROOT, rel.replace('/', os.sep))
            if os.path.exists(chemin):
                yield rel, chemin
        return
    for base, dirs, names in os.walk(ROOT):
        rel_base = os.path.relpath(base, ROOT).replace('\\', '/')
        dirs[:] = [d for d in dirs
                   if d not in IGNORES and f'{rel_base}/{d}'.lstrip('./') not in IGNORES]
        for n in names:
            rel = os.path.relpath(os.path.join(base, n), ROOT).replace('\\', '/')
            if rel.split('/')[0] in IGNORES:
                continue
            yield rel, os.path.join(base, n)


def lire(path):
    try:
        return io.open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return ''


def main():
    tous = list(fichiers_versionnes())
    textes = {rel: lire(p) for rel, p in tous if os.path.splitext(rel)[1].lower() in TEXTE}

    # 1. Structure attendue
    for chemin in ('CMakeLists.txt', 'partitions.csv', 'sdkconfig.defaults', 'dependencies.lock',
                   '.gitignore', 'LICENSE', 'README.md', 'README_FR.md',
                   'config/esp32p4_rev1_3.defaults', 'main/CMakeLists.txt',
                   'main/device_secrets.example.h', 'private/.gitignore'):
        check(f'present : {chemin}', os.path.exists(os.path.join(ROOT, chemin)))

    # 2. Aucun chemin absolu propre a une machine
    # Seuls les chemins propres a la machine de developpement d'origine sont fautifs ;
    # les exemples generiques des documents (C:\Espressif, C:\dev\..., C:\Program Files\...)
    # sont legitimes et doivent rester lisibles.
    motif_abs = re.compile(r'[A-Za-z]:[\\/]Users[\\/]|Waveshare_Glyc' + 'emie', re.I)
    # Ce fichier contient le motif lui-meme : il ne peut pas etre son propre contre-exemple.
    fautifs = sorted(rel for rel, t in textes.items()
                     if rel != 'tests/test_depot.py' and motif_abs.search(t))
    check('aucun chemin absolu de cette machine dans les fichiers versionnes', not fautifs, ', '.join(fautifs))

    # 3. Identifiants prives
    check('aucun device_secrets.h dans le depot',
          not any(rel.endswith('device_secrets.h') and not rel.endswith('example.h') for rel, _ in tous))
    gitignore = textes.get('.gitignore', '')
    check('.gitignore exclut /private/', '/private/' in gitignore)
    check('.gitignore exclut le dossier build', '/build/' in gitignore)
    check('.gitignore exclut sdkconfig (genere, peut refleter la configuration locale)',
          re.search(r'^sdkconfig$', gitignore, re.M) is not None)

    secrets_reels = os.path.join(ROOT, 'private', 'device_secrets.h')
    valeurs = []
    if os.path.exists(secrets_reels):
        for m in re.finditer(r'#define\s+(GLY_\w+)\s+"((?:[^"\\]|\\.)*)"', lire(secrets_reels)):
            if m.group(1) != 'GLY_DEXCOM_REGION' and len(m.group(2)) >= 3:
                valeurs.append(re.sub(r'\\(.)', r'\1', m.group(2)))
    fuite = sorted({rel for rel, t in textes.items() for v in valeurs if v in t})
    check('aucune valeur de private/device_secrets.h dans les fichiers versionnes'
          + ('' if valeurs else ' (aucun fichier local a comparer)'), not fuite, ', '.join(fuite))

    exemple = textes.get('main/device_secrets.example.h', '')
    vides = re.findall(r'#define\s+GLY_(?:WIFI_SSID|WIFI_PASSWORD|DEXCOM_USERNAME|DEXCOM_PASSWORD)\s+""', exemple)
    check('le modele device_secrets.example.h ne contient que des valeurs vides', len(vides) == 4,
          f'{len(vides)}/4 champs vides')

    # 4. Table de partitions identique a celle du firmware d'origine (+ partition model)
    attendu = [('nvs', '0x9000', '0x6000'), ('phy_init', '0xf000', '0x1000'),
               ('factory', '', '8M'), ('storage', '', '7M'), ('model', '0xF10000', '4M')]
    lignes = [l for l in textes.get('partitions.csv', '').splitlines()
              if l.strip() and not l.strip().startswith('#')]
    lues = []
    for l in lignes:
        ch = [c.strip() for c in l.split(',')]
        lues.append((ch[0], ch[3], ch[4]))
    check('partitions.csv : disposition d\'origine + partition model', lues == attendu, str(lues))

    # 5. Profil de revision ESP32-P4 rev1.3 (obligatoire sur cette carte)
    profil = textes.get('config/esp32p4_rev1_3.defaults', '')
    check('profil pre-v3 : CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y', 'CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y' in profil)
    check('profil pre-v3 : CONFIG_ESP32P4_REV_MIN_100=y', 'CONFIG_ESP32P4_REV_MIN_100=y' in profil)
    racine_cmake = textes.get('CMakeLists.txt', '')
    check('le profil de revision est ajoute apres sdkconfig.defaults',
          racine_cmake.find('sdkconfig.defaults') < racine_cmake.find('esp32p4_rev1_3.defaults'))
    check('garde-fou de revision dans main/CMakeLists.txt',
          'CONFIG_ESP32P4_SELECTS_REV_LESS_V3' in textes.get('main/CMakeLists.txt', ''))

    # 6. Sources : tout ce qui est compile existe, tout ce qui existe est compile
    cm = textes.get('main/CMakeLists.txt', '')
    bloc = cm.split('SRCS', 1)[1].split('INCLUDE_DIRS', 1)[0] if 'SRCS' in cm else ''
    # Les sources citees dans SRCS, plus celles choisies par une variable (ex. GLY_VOICE_CMD_SRC,
    # qui vaut voice_cmd.c ou voice_cmd_stub.c selon l'option de compilation).
    declares = set(re.findall(r'"([A-Za-z0-9_]+\.c)"', bloc))
    declares |= set(re.findall(r'set\([A-Z_]+_SRC\s+"([A-Za-z0-9_]+\.c)"\)', cm))
    check('les sources optionnelles passent bien par une variable de CMake',
          '${GLY_VOICE_CMD_SRC}' in bloc, bloc.strip()[:60])
    # CMake garde en cache ce qu'on lui donne une fois : si build.ps1 ne passait l'option que
    # pour -Vocal, la compilation suivante garderait les commandes vocales sans le dire.
    construction = textes.get('tools/build.ps1', '')
    check("build.ps1 impose toujours l'etat des commandes vocales (cache CMake)",
          "'-DGLY_VOICE_CMD=ON'" in construction and "'-DGLY_VOICE_CMD=OFF'" in construction)
    sur_disque = {n for n in os.listdir(os.path.join(ROOT, 'main')) if n.endswith('.c')}
    check('tous les .c declares existent', not (declares - sur_disque), str(sorted(declares - sur_disque)))
    check('aucun .c oublie dans la compilation', not (sur_disque - declares), str(sorted(sur_disque - declares)))
    embed = re.findall(r'"((?:fonts|voice)/[^"]+)"', cm)
    # voice/voice_fr.pcm est FACULTATIF : CMakeLists ne l'embarque que s'il existe (droits Microsoft non
    # etablis, voir docs/LICENCES_TIERCES_FR.md). Tous les autres fichiers embarques doivent etre presents.
    obligatoires = [f for f in embed if f != 'voice/voice_fr.pcm']
    manquants = [f for f in obligatoires if not os.path.exists(os.path.join(ROOT, 'main', f))]
    check('les fichiers embarques obligatoires (polices) sont presents', not manquants, str(manquants))
    check('l\'ancienne voix Windows reste facultative dans la compilation',
          'GLY_VOICE_OLD_PCM' in cm and 'if(EXISTS' in cm)

    # 7. Interface : 5 pages, la page STATS a ete retiree volontairement
    pages = textes.get('main/ui_pages.h', '')
    noms = re.findall(r'UI_PAGE_([A-Z]+)', pages)
    check('5 pages declarees (ACCUEIL, GRAPHIQUE, AUJOURD\'HUI, CONSOMMABLES, REGLAGES)',
          'UI_PAGE_COUNT' in pages and len(set(noms) - {'COUNT'}) == 5, str(sorted(set(noms) - {'COUNT'})))
    check('la page STATS n\'est pas reintroduite',
          not os.path.exists(os.path.join(ROOT, 'main', 'ui_stats.c')) and 'UI_PAGE_STATS' not in pages)

    # 8. Versions de la pile logicielle historiquement validees
    comp = textes.get('main/idf_component.yml', '')
    for nom, motif in (('ESP-IDF >= 5.5.5', r'version:\s*">=5\.5\.5"'),
                       ('BSP Waveshare 3.0.1', r'version:\s*"==3\.0\.1"'),
                       ('LVGL 9.5.0', r'version:\s*"==9\.5\.0"'),
                       ('esp_wifi_remote 1.2.5', r'version:\s*"==1\.2\.5"'),
                       ('esp_hosted 1.4.x', r'version:\s*"1\.4\.\*"'),
                       ('ESP-SR 2.5.3', r'version:\s*"==2\.5\.3"')):
        check(f'dependance figee : {nom}', re.search(motif, comp) is not None)
    check('dependencies.lock fourni (versions resolues reproductibles)',
          len(textes.get('dependencies.lock', '')) > 1000)

    # 9. Rien de compile ni de volumineux dans le depot
    gros = sorted((rel, os.path.getsize(p)) for rel, p in tous if os.path.getsize(p) > 2 * 1024 * 1024)
    check('aucun fichier versionne de plus de 2 Mo', not gros, str(gros))
    binaires = sorted(rel for rel, _ in tous if os.path.splitext(rel)[1].lower() in ('.bin', '.elf', '.obj', '.a'))
    check('aucun binaire de firmware dans le depot', not binaires, ', '.join(binaires))

    # 10. Captures d'ecran : vraies captures de la carte (1024 x 600)
    dossier = os.path.join(ROOT, 'screenshots')
    pngs = sorted(n for n in os.listdir(dossier) if n.endswith('.png')) if os.path.isdir(dossier) else []
    check('au moins une capture par page', len(pngs) >= 5, ', '.join(pngs))
    mauvaises = []
    for n in pngs:
        with open(os.path.join(dossier, n), 'rb') as f:
            entete = f.read(24)
        if entete[:8] != b'\x89PNG\r\n\x1a\n' or int.from_bytes(entete[16:20], 'big') != 1024 \
                or int.from_bytes(entete[20:24], 'big') != 600:
            mauvaises.append(n)
    check('captures au format reel de l\'ecran (1024 x 600)', not mauvaises, ', '.join(mauvaises))

    largeur = max(len(r[0]) for r in resultats)
    echecs = 0
    for nom, ok, detail in resultats:
        echecs += 0 if ok else 1
        print('%-6s %-*s %s' % ('OK' if ok else 'ECHEC', largeur, nom, detail))
    print('\nRESULTAT : %s (%d controles, %d echec(s))'
          % ('DEPOT CONFORME' if echecs == 0 else 'A CORRIGER', len(resultats), echecs))
    return 0 if echecs == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
