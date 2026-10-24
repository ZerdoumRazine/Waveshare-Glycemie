#!/usr/bin/env python3
# Creates and verifies a removable-card backup.
#
# Design note: A backup should be checked, not just copied.
# Makes a verified file copy from removable media without writing to the card.
#
"""Copy and VERIFIER the content of the microSD, without ever changing it.

Map n'est que LUE : aucun fichier n'it is written, replaced or erased, and it is never
formatee. If Windows proposes to repair or format it, refuse.

Deroulement :
  1. each file is read, its footprint SHA-256 calculates and then copies in the backup folder;
  2. Then, TOUT is read from both sides and compares (map and copy): this is the verification;
  3. a manifest (manifest.json) and a fingerprint file (SHA256SUMS.txt) are recorded.

The backup goes into private\\sauvegardes\\: a private folder, never published.

Usage :
    python tools\\sauvegarde_microsd.py --lister shows removable drives
    python tools\\sauvegarde_microsd.py --reader E: copy and verify
    python tools\\sauvegarde_microsd.py --reader E: --check-only compare to an existing copy
"""
import argparse
import datetime
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SAUVEGARDES = os.path.join(ROOT, 'private', 'sauvegardes')
BLOC = 1 << 20


# List candidate media; the user must identify the correct card.
def lecteurs_amovibles():
    """[(lettre, nom, taille_Go, libre_Go, systeme_de_fichiers, amovible)] vus par Windows."""
    if sys.platform != 'win32':
        return []
    requete = ("Get-CimInstance Win32_LogicalDisk | ForEach-Object { "
               "'{0}|{1}|{2}|{3}|{4}|{5}' -f $_.DeviceID, $_.VolumeName, $_.Size, $_.FreeSpace, "
               "$_.FileSystem, $_.DriveType }")
    try:
        sortie = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', requete],
                                capture_output=True, text=True, timeout=60).stdout
    except (OSError, subprocess.SubprocessError):
        return []
    lecteurs = []
    for ligne in sortie.splitlines():
        morceaux = ligne.strip().split('|')
        if len(morceaux) != 6 or not morceaux[0].endswith(':'):
            continue
        taille = int(morceaux[2]) if morceaux[2].isdigit() else 0
        libre = int(morceaux[3]) if morceaux[3].isdigit() else 0
        lecteurs.append((morceaux[0], morceaux[1], taille / 1e9, libre / 1e9, morceaux[4],
                         morceaux[5] == '2'))
    return lecteurs


# Compare hashes rather than assuming that matching file sizes mean identical data.
def empreinte(chemin):
    h = hashlib.sha256()
    with open(chemin, 'rb') as f:
        for bloc in iter(lambda: f.read(BLOC), b''):
            h.update(bloc)
    return h.hexdigest()


# Walk files read-only and preserve their relative names.
def parcourir(racine):
    """[(relative path, size)] of all files, read-only."""
    fichiers = []
    for base, dossiers, noms in os.walk(racine):
        dossiers[:] = [d for d in dossiers if d.upper() not in ('SYSTEM VOLUME INFORMATION',
                                                                '$RECYCLE.BIN')]
        for nom in noms:
            chemin = os.path.join(base, nom)
            try:
                taille = os.path.getsize(chemin)
            except OSError:
                continue
            fichiers.append((os.path.relpath(chemin, racine), taille))
    return sorted(fichiers)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lister', action='store_true', help='montrer les lecteurs et quitter')
    ap.add_argument('--lecteur', default=None, help='lettre de la carte, par exemple E:')
    ap.add_argument('--dossier', default=None, help='dossier de sauvegarde (prive par defaut)')
    ap.add_argument('--verifier-seulement', action='store_true',
                    help='comparer la carte a une sauvegarde deja faite, sans rien copier')
    ap.add_argument('--forcer', action='store_true',
                    help='accepter un lecteur non amovible (a eviter)')
    args = ap.parse_args()

    lecteurs = lecteurs_amovibles()
    if args.lister or not args.lecteur:
        print('Lecteurs vus par Windows :\n')
        print('   %-6s %-22s %10s %10s %-8s %s' % ('', 'nom', 'taille', 'libre', 'format', 'amovible'))
        for lettre, nom, taille, libre, fs, amovible in lecteurs:
            print('   %-6s %-22s %7.1f Go %7.1f Go %-8s %s'
                  % (lettre, nom or '(sans nom)', taille, libre, fs, 'oui' if amovible else 'non'))
        if not args.lecteur:
            print('\nRelancez avec la lettre de la carte, par exemple :')
            print('    python tools\\sauvegarde_microsd.py --lecteur E:')
        return 0

    lettre = args.lecteur.rstrip('\\')
    if not lettre.endswith(':'):
        lettre += ':'
    infos = [l for l in lecteurs if l[0].upper() == lettre.upper()]
    if not infos:
        print("Le lecteur %s n'existe pas. Relancez avec --lister." % lettre)
        return 1
    _, nom_volume, taille_go, libre_go, systeme, amovible = infos[0]
    if not amovible and not args.forcer:
        print('%s n\'est pas un lecteur amovible (%s, %.1f Go). Par securite, rien n\'est fait.'
              % (lettre, nom_volume or 'sans nom', taille_go))
        print("Verifiez la lettre, ou ajoutez --forcer si vous etes certain.")
        return 1
    racine = lettre + '\\'
    print('Carte : %s  %s  %.1f Go (%.1f Go libres), format %s'
          % (lettre, nom_volume or '(sans nom)', taille_go, libre_go, systeme))
    if systeme.upper() not in ('FAT32', 'FAT', 'EXFAT'):
        print("ATTENTION : format inattendu (%s). La copie reste possible." % systeme)

    fichiers = parcourir(racine)
    total = sum(t for _, t in fichiers)
    print('Contenu : %d fichiers, %.1f Mo' % (len(fichiers), total / 1048576))
    if not fichiers:
        print('Aucun fichier lu : ce n\'est pas la bonne carte, ou elle est vide. Rien n\'est fait.')
        return 1

    horodatage = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    cible = args.dossier or os.path.join(SAUVEGARDES, horodatage + '_microsd')
    if args.verifier_seulement:
        candidats = sorted(d for d in os.listdir(SAUVEGARDES) if d.endswith('_microsd')) \
            if os.path.isdir(SAUVEGARDES) else []
        if args.dossier is None:
            if not candidats:
                print('Aucune sauvegarde de microSD a verifier.')
                return 1
            cible = os.path.join(SAUVEGARDES, candidats[-1])
        print('Verification par rapport a : %s' % cible)
    else:
        os.makedirs(cible, exist_ok=True)
        print('Copie vers : %s' % cible)

    # --- 1. Copy (card is only read) ---------------------------------------------------
    manifeste = {'date': horodatage, 'lecteur': lettre, 'volume': nom_volume,
                 'format': systeme, 'taille_go': round(taille_go, 2), 'fichiers': []}
    copies = 0
    if not args.verifier_seulement:
        for rel, taille in fichiers:
            source = os.path.join(racine, rel)
            destination = os.path.join(cible, rel)
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            try:
                shutil.copyfile(source, destination)   # reading the map, writing on the PC
            except OSError as err:
                print('   ILLISIBLE : %s (%s)' % (rel, err))
                continue
            copies += 1
            if copies % 50 == 0 or taille > 20 * 1048576:
                print('   %d/%d fichiers...' % (copies, len(fichiers)))
        print('%d fichier(s) copie(s).' % copies)

    # --- 2. Verification: everything is read from DEUX cotes et compare ----------------------------
    print('\nVerification (relecture de la carte et de la copie)...')
    identiques, differents, manquants, illisibles = 0, [], [], []
    for rel, taille in fichiers:
        source = os.path.join(racine, rel)
        destination = os.path.join(cible, rel)
        if not os.path.exists(destination):
            manquants.append(rel)
            continue
        try:
            h_source = empreinte(source)
            h_copie = empreinte(destination)
        except OSError as err:
            illisibles.append('%s (%s)' % (rel, err))
            continue
        if h_source == h_copie:
            identiques += 1
            manifeste['fichiers'].append({'nom': rel.replace('\\', '/'), 'taille': taille,
                                          'sha256': h_source})
        else:
            differents.append(rel)

    lignes = ['%s  %s' % (f['sha256'], f['nom']) for f in manifeste['fichiers']]
    if not args.verifier_seulement:
        io.open(os.path.join(cible, 'SHA256SUMS.txt'), 'w', encoding='utf-8',
                newline='\n').write('\n'.join(lignes) + '\n')
        with io.open(os.path.join(cible, 'manifeste.json'), 'w', encoding='utf-8') as f:
            json.dump(manifeste, f, indent=2, ensure_ascii=False)

    print('   identiques : %d' % identiques)
    if manquants:
        print('   ABSENTS de la copie : %d (%s%s)'
              % (len(manquants), ', '.join(manquants[:3]), ' ...' if len(manquants) > 3 else ''))
    if differents:
        print('   DIFFERENTS : %d (%s%s)'
              % (len(differents), ', '.join(differents[:3]), ' ...' if len(differents) > 3 else ''))
    if illisibles:
        print('   ILLISIBLES : %d (%s)' % (len(illisibles), illisibles[0]))

    print('\n=== RESULTAT ===')
    if manquants or differents or illisibles or identiques != len(fichiers):
        print('SAUVEGARDE INCOMPLETE : %d fichier(s) sur %d verifies.' % (identiques, len(fichiers)))
        print("Relancez la copie. Si un fichier reste illisible, notez son nom : la carte est")
        print('peut-etre fatiguee. Ne la formatez pas, ne la "reparez" pas.')
        return 1
    print('microSD sauvegardee et verifiee : %d fichiers, %.1f Mo' % (identiques, total / 1048576))
    print('   %s' % cible)
    print("La carte n'a pas ete modifiee. Gardez une copie de ce dossier ailleurs (cle USB).")
    return 0


if __name__ == '__main__':
    sys.exit(main())
