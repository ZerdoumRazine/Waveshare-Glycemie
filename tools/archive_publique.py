#!/usr/bin/env python3
# Builds source archives from a controlled Git snapshot.
#
# Design note: Never archive the personal working directory indiscriminately.
# Exports only tracked Git files and rejects unsafe publication artefacts.
#
r"""Fabrique et controle l'archive publique du depot, a partir de ce que Git suit reellement.

Un .gitignore ne nettoie PAS une archive faite en copiant un dossier : journaux, identifiants ou
firmware personnel s'y retrouvent. Cet outil part donc de `git archive HEAD`, puis :
  - relit le ZIP, nom par nom, et refuse tout element interdit (majuscules comprises) ;
  - EXTRAIT l'archive et examine son CONTENU (identifiants reels, adresses de courriel, chemins
    personnels) avec tools/controle_donnees_perso.py ;
  - verifie que les fichiers obligatoires sont presents et que l'archive n'est pas vide ;
  - verifie que l'archive correspond bien a l'etat termine du projet (rien de non commite) ;
  - controle en plus les fichiers livres a cote (programme compile, application Windows) ;
  - EFFACE l'archive si elle est refusee : une archive refusee ne doit pas rester parmi les
    fichiers prets a partager.

Un controle qui n'a rien pu lire est un ECHEC, jamais une reussite.

Usage :
    python tools\archive_publique.py
    python tools\archive_publique.py --sortie D:\partage
    python tools\archive_publique.py --livre tools\pc_app\dist\GlycemieConnexion.exe
    python tools\archive_publique.py --accepter-modifications   (archive un etat non commite, a eviter)
Code de sortie 0 seulement si tout est propre.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import controle_donnees_perso as cdp  # noqa: E402

# Everything that should JAMAIS exit: IDs, health data, personal binary, caches.
# The reasons apply SANS tenir compte de la casse (.BIN, .Pcm, PRIVATE/... sont vises aussi).
INTERDITS = [
    # private/ stays in the repository with its only .gitignore (the folder must exist): everything else is forbidden
    (r'(^|/)private/(?!\.gitignore$).+', 'identifiants prives'),
    (r'device_secrets\.h$', 'identifiants prives'),
    (r'(^|/)tools/logs/', 'journaux locaux (glycemies reelles, chemins du PC)'),
    (r'(^|/)tools/cache/', 'cache local'),
    (r'(^|/)dist_voix/', 'paquets vocaux (pieces jointes de Release)'),
    (r'(^|/)build/', 'sortie de compilation'),
    (r'(^|/)tests/build/', 'sortie de compilation des tests'),
    (r'(^|/)managed_components/', 'composants telecharges'),
    (r'\.(bin|elf|map)$', 'firmware compile (contient les identifiants)'),
    (r'\.(exe|msi)$', 'programme compile (se refabrique depuis les sources)'),
    (r'\.pcm$', 'audio dont les droits ne sont pas etablis'),
    (r'(^|/)google_raw/', 'cache du service de synthese'),
    (r'(^|/)bank/', 'banque vocale generee'),
    (r'\.venv', 'environnement Python local'),
    (r'(^|/)\.git/', 'historique Git'),
    (r'\.csv\.perso$', 'donnees personnelles'),
    (r'(^|/)sauvegardes?/', 'sauvegarde de l\'appareil personnel'),
]
ATTENDUS = ['README.md', 'README_FR.md', 'LICENSE', 'CREDITS.md', 'CMakeLists.txt',
            'main/app_main.c', 'main/credentials.c', 'main/ui_setup.c',
            'tools/assistant_config.py', 'tools/pc_app/glycemie_connexion.pyw',
            'tools/pc_app/construire_exe.ps1',
            'docs/TUTORIEL_VOIX_FR.md', 'docs/VOICE_TUTORIAL_EN.md',
            'docs/RETOUR_ARRIERE_FR.md', 'screenshots/01_accueil.png']
MINIMUM_FICHIERS = 100   # un depot complet ; en dessous, l'archive est suspecte


def git(*args):
    return subprocess.run(['git', '-C', RACINE] + list(args), capture_output=True, text=True,
                          timeout=300, encoding='utf-8', errors='replace')


# Inspect the actual bytes, including nested archives, instead of trusting filenames.
def controler_contenu(dossier, secrets):
    """Examine le contenu reellement extrait de l'archive. Renvoie (trouvailles, fichiers lus)."""
    trouves, lus = [], 0
    for base, _, fichiers in os.walk(dossier):
        for nom in fichiers:
            chemin = os.path.join(base, nom)
            rel = os.path.relpath(chemin, dossier).replace('\\', '/')
            data = cdp.lire(chemin, binaire=True)
            if data is None:
                continue
            lus += 1
            if rel.lower().endswith(cdp.BINAIRES):
                cdp.controler_binaire(rel, data, secrets, trouves)
            else:
                cdp.controler_texte(rel, data.decode('utf-8', errors='replace'), secrets, trouves)
    return trouves, lus


# Fail closed when the public archive includes a forbidden file or a known private value.
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sortie', default=tempfile.gettempdir())
    ap.add_argument('--nom', default=None, help='nom du fichier ZIP (sinon <dossier>-<commit>.zip)')
    ap.add_argument('--livre', action='append', default=[],
                    help='fichier publie a cote de l\'archive (programme compile, .exe) ; repetable')
    ap.add_argument('--accepter-modifications', action='store_true',
                    help='archiver malgre des modifications non commitees (deconseille)')
    args = ap.parse_args()

    etat = git('status', '--porcelain')
    if etat.returncode != 0:
        print('Ce dossier n\'est pas un depot Git : %s' % etat.stderr.strip())
        return 1
    modifies = [l for l in etat.stdout.splitlines() if l and not l.startswith('??')]
    if modifies and not args.accepter_modifications:
        print('REFUS : %d fichier(s) modifie(s) ne sont pas commites. L\'archive ne correspondrait'
              ' pas a la version terminee :' % len(modifies))
        for l in modifies[:15]:
            print('   ' + l)
        print('\nCommitez ces modifications, ou relancez avec --accepter-modifications.')
        return 1
    if modifies:
        print('ATTENTION : archive faite malgre %d modification(s) non commitee(s).' % len(modifies))

    commit = git('rev-parse', '--short', 'HEAD').stdout.strip()
    nom = args.nom or '%s-%s.zip' % (os.path.basename(RACINE).lower().replace(' ', '-'), commit)
    chemin = os.path.join(args.sortie, nom)
    os.makedirs(args.sortie, exist_ok=True)

    fait = git('archive', '--format=zip', '-o', chemin, 'HEAD')
    if fait.returncode != 0:
        print('git archive a echoue : %s' % fait.stderr.strip())
        return 1

    with zipfile.ZipFile(chemin) as zf:
        noms = zf.namelist()
        octets = sum(i.file_size for i in zf.infolist())
    problemes = []
    if len(noms) < MINIMUM_FICHIERS:
        problemes.append("archive anormalement petite : %d fichiers (moins de %d) : le controle ne "
                         'porterait sur presque rien' % (len(noms), MINIMUM_FICHIERS))
    for motif, raison in INTERDITS:
        trouves = [n for n in noms if re.search(motif, n, re.IGNORECASE)]
        if trouves:
            problemes.append('%s : %s%s' % (raison, ', '.join(trouves[:5]),
                                            ' ...' if len(trouves) > 5 else ''))
    absents = [n for n in ATTENDUS if n not in noms]

    # --- contenu reellement distribue ---------------------------------------------------------
    secrets = cdp.secrets_reels()
    if not secrets:
        problemes.append('private\\device_secrets.h est absent : impossible de verifier que vos '
                         'identifiants ne sont pas dans l\'archive')
    extrait = tempfile.mkdtemp(prefix='archive-controle-')
    trouvailles, lus = [], 0
    try:
        with zipfile.ZipFile(chemin) as zf:
            zf.extractall(extrait)
        trouvailles, lus = controler_contenu(extrait, secrets)
    finally:
        shutil.rmtree(extrait, ignore_errors=True)
    if lus < MINIMUM_FICHIERS:
        problemes.append('seulement %d fichier(s) ont pu etre lus dans l\'archive' % lus)
    for nom_fichier, genre, detail in trouvailles:
        problemes.append('contenu de %s : %s (%s)' % (nom_fichier, genre, detail))

    # --- coded book files (compile program, Windows application) ----------------------
    livres = []
    for livre in args.livre:
        if not os.path.exists(livre):
            problemes.append('fichier livre introuvable : %s' % livre)
            continue
        data = cdp.lire(livre, binaire=True)
        trouves_livre = []
        cdp.controler_binaire(os.path.basename(livre), data, secrets, trouves_livre)
        for nom_fichier, genre, detail in trouves_livre:
            problemes.append('fichier livre %s : %s (%s)' % (nom_fichier, genre, detail))
        livres.append((livre, len(data), hashlib.sha256(data).hexdigest()))

    with open(chemin, 'rb') as f:
        sha = hashlib.sha256(f.read()).hexdigest()

    print('\nArchive : %s' % chemin)
    print('Commit  : %s' % commit)
    print('Contenu : %d fichiers, %.2f Mo decompresses, %.2f Mo compresses, %d fichiers relus'
          % (len(noms), octets / 1048576, os.path.getsize(chemin) / 1048576, lus))
    print('SHA-256 : %s' % sha)
    for livre, taille, empreinte in livres:
        print('Livre   : %-44s %8.2f Mo  %s' % (os.path.basename(livre), taille / 1048576, empreinte))
    images = [n for n in noms if n.lower().endswith(('.png', '.jpg', '.jpeg'))]
    print('Images distribuees (%d) : a regarder une par une avant publication' % len(images))
    for image in images:
        print('   %s' % image)
    if absents:
        print('MANQUANTS : %s' % ', '.join(absents))
    for p in problemes:
        print('INTERDIT  : %s' % p)

    refuse = bool(problemes or absents)
    if refuse:
        try:
            os.remove(chemin)
            print('\nArchive refusee : le fichier a ete supprime pour qu\'il ne soit pas partage'
                  ' par erreur.')
        except OSError as err:
            print('\nArchive refusee ; supprimez-la vous-meme (%s) : %s' % (chemin, err))
    print('\nRESULTAT : %s' % ('A CORRIGER' if refuse else 'ARCHIVE PROPRE'))
    return 1 if refuse else 0


if __name__ == '__main__':
    sys.exit(main())
