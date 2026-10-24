#!/usr/bin/env python3
# Exports a shareable source copy without rewriting Git history.
#
# Design note: Refuse to overwrite an existing repository or folder.
# Exports a safe tracked-file snapshot without overwriting an existing repository.
#
"""Prepare a sanitized source export from files actually tracked by Git.

Use a separate destination directory, preserve source history in the actual
repository, and exclude private data or generated local build artifacts."""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import archive_publique as ap          # noqa: E402  (prohibited reasons and expected files)
import controle_donnees_perso as cdp   # noqa: E402


# Run an explicit Git command against the project root.
def git(*args):
    return subprocess.run(['git', '-C', RACINE] + list(args), capture_output=True, text=True,
                          timeout=300, encoding='utf-8', errors='replace')


def enlever_lecture_seule(fonction, chemin, _erreur):
    """Clear the read-only attribute on Git files before removing a temporary copy."""
    try:
        os.chmod(chemin, 0o700)
        fonction(chemin)
    except OSError:
        pass


# Refuse an occupied destination and never rewrite the established GitHub history.
def main():
    parseur = argparse.ArgumentParser()
    parseur.add_argument('--dossier', default=os.path.join(RACINE, 'Razine'))
    parseur.add_argument('--accepter-modifications', action='store_true')
    parseur.add_argument('--depot-neuf', action='store_true',
                         help="ancienne option dangereuse, desactivee depuis la publication")
    args = parseur.parse_args()
    if args.depot_neuf:
        print('REFUS : un depot GitHub existe deja. Ne supprimez pas son historique ; '
              'utilisez une branche et GitHub Desktop pour les mises a jour.')
        return 2

    etat = git('status', '--porcelain')
    if etat.returncode != 0:
        print("Ce dossier n'est pas un depot Git.")
        return 1
    modifies = [l for l in etat.stdout.splitlines() if l and not l.startswith('??')]
    if modifies and not args.accepter_modifications:
        print('REFUS : %d fichier(s) modifie(s) ne sont pas enregistres. La copie publique ne'
              ' correspondrait pas a la version terminee :' % len(modifies))
        for ligne in modifies[:15]:
            print('   ' + ligne)
        print('\nEnregistrez-les (git commit), ou relancez avec --accepter-modifications.')
        return 1

    commit = git('rev-parse', '--short', 'HEAD').stdout.strip()
    cible = os.path.abspath(args.dossier)
    if os.path.commonpath([cible, RACINE]) == cible:
        print('Refus : la destination contiendrait le projet lui-meme.')
        return 1
    if os.path.exists(cible):
        print('REFUS : le dossier de destination existe deja. '
              'Choisissez un NOUVEAU dossier pour ne rien ecraser : %s' % cible)
        return 1

    # --- 1. Content: exactly what Git follows, has the state records ---------------------------
    temporaire = tempfile.mkdtemp(prefix='razine-')
    zip_temp = os.path.join(temporaire, 'depot.zip')
    fait = git('archive', '--format=zip', '-o', zip_temp, 'HEAD')
    if fait.returncode != 0:
        print('git archive a echoue : %s' % fait.stderr.strip())
        shutil.rmtree(temporaire, ignore_errors=True)
        return 1
    extrait = os.path.join(temporaire, 'contenu')
    with zipfile.ZipFile(zip_temp) as zf:
        zf.extractall(extrait)

    # --- 2. Verification AVANT replace existing copy --------------------------------
    secrets = cdp.secrets_reels()
    problemes = []
    noms = []
    for base, _, fichiers in os.walk(extrait):
        for nom in fichiers:
            noms.append(os.path.relpath(os.path.join(base, nom), extrait).replace('\\', '/'))
    import re
    for motif, raison in ap.INTERDITS:
        trouves = [n for n in noms if re.search(motif, n, re.IGNORECASE)]
        if trouves:
            problemes.append('%s : %s' % (raison, ', '.join(trouves[:5])))
    manquants = [n for n in ap.ATTENDUS if n not in noms]
    if manquants:
        problemes.append('fichiers obligatoires absents : %s' % ', '.join(manquants))
    if len(noms) < ap.MINIMUM_FICHIERS:
        problemes.append('copie anormalement petite : %d fichiers' % len(noms))
    trouvailles, lus = ap.controler_contenu(extrait, secrets)
    if lus < ap.MINIMUM_FICHIERS:
        problemes.append('seulement %d fichier(s) ont pu etre lus' % lus)
    for nom, genre, detail in trouvailles:
        problemes.append('contenu de %s : %s (%s)' % (nom, genre, detail))
    if not secrets:
        print('NOTE : aucun secret prive local pour une comparaison exacte. '
              'Les controles de contenu restent heuristiques ; revue humaine necessaire.')

    if problemes:
        print('\n'.join('INTERDIT : %s' % p for p in problemes))
        print('\nRESULTAT : A CORRIGER — la copie publique n\'a PAS ete remplacee.')
        shutil.rmtree(temporaire, ignore_errors=True)
        return 1

    # --- 3. Copy SANS replacement of existing folder or change of history ---------
    shutil.copytree(extrait, cible)
    shutil.rmtree(temporaire, ignore_errors=True)

    total = sum(os.path.getsize(os.path.join(b, f))
                for b, _, fs in os.walk(cible) for f in fs)
    print('Copie publique prete : %s' % cible)
    print('Contenu : %d fichiers, %.2f Mo (depot a l\'etat %s)' % (len(noms), total / 1048576, commit))
    print('Verifications passees : fichiers interdits, identifiants, adresses, chemins personnels.')
    print('\nA faire ensuite, dans l\'ordre :')
    print('   1. regarder les images de screenshots\\ une par une (le controle lit les fichiers,')
    print('      pas les images) ;')
    print('   2. preparer les fichiers a telecharger : tools\\publier_firmware.ps1 ;')
    print('   3. consulter docs\\PUBLICATION_GITHUB_FR.md pour entretenir le depot prive.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
