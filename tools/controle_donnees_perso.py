#!/usr/bin/env python3
# Checks release candidates for likely personal data.
# FR: Vérifie la présence possible de données personnelles.
# Design note: Automated checks reduce risk but do not replace a human review.
# EN: Scans public files, binaries and ZIP contents for known private data.
# FR: Contrôle les sources, exécutables et ZIP avant publication.
r"""Controle des donnees personnelles dans ce qui serait publie.

Examine, sans rien modifier :
  - tous les fichiers suivis par git (le depot public est une copie de HEAD) ;
  - l'historique git complet, blob par blob (utile seulement si le depot est pousse en ligne) ;
  - le programme compile (build/glycemie_display.bin, et la copie publique si elle existe) ;
  - les captures d'ecran (metadonnees, pas le contenu de l'image).

Recherche : les identifiants reels lus dans private/device_secrets.h (jamais affiches), les adresses
de courriel, les chemins personnels Windows, les noms de machine, et les valeurs de glycemie exportees.

Usage :
    python tools\controle_donnees_perso.py
    python tools\controle_donnees_perso.py --historique      (ajoute l'analyse de l'historique git)
Code de sortie 0 si rien de personnel n'est trouve dans ce qui serait publie.
"""
import argparse
import io
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# Mots des credits volontaires : ils doivent RESTER (auteur, dedicace au fils, composants tiers).
CREDITS_VOULUS = ('Razine', 'razine')

BINAIRES = ('.png', '.jpg', '.jpeg', '.bin', '.elf', '.map', '.zip', '.pcm', '.wav', '.ico')
# Fichiers dont le contenu est du signal ou de la compression : voir controler_media().
MEDIA = ('.png', '.jpg', '.jpeg', '.wav', '.pcm', '.mp3', '.ogg', '.ico')

COURRIEL = re.compile(r'[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}')
CHEMINS = [
    (re.compile(r'[A-Za-z]:\\\\?Users\\\\?[^\\\\/\s"\']+', re.I), 'chemin du dossier utilisateur Windows'),
    (re.compile(r'[A-Za-z]:[\\/]Nouveau dossier', re.I), 'chemin "Nouveau dossier" (dossier d\'archive local)'),
    (re.compile(r'[A-Za-z]:[\\/]Waveshare', re.I), 'chemin local du projet'),
]
# Adresses attendues et autorisees : composants tiers, adresses neutres, et les adresses de contact
# des autorites de certification contenues dans le paquet de certificats racine d'ESP-IDF
# (esp_crt_bundle, liste Mozilla) : elles viennent des certificats eux-memes, pas du projet.
COURRIELS_AUTORISES = re.compile(r'(noreply@|@users\.noreply\.github\.com|@exemple\.|@example\.|'
                                 r'@espressif\.com|@lvgl\.io|'
                                 r'support@waveshare|@dexcom\.com$|'
                                 r'@e-szigno\.hu|@netlock\.|@firmaprofesional\.com|@disig\.sk|'
                                 r'@sk\.ee|@certsign\.ro|@trustfactory\.|@pki\.)', re.I)


def git(*args):
    out = subprocess.run(['git'] + list(args), cwd=ROOT, capture_output=True, text=True,
                         encoding='utf-8', errors='replace')
    return out.stdout


# Read known local secrets only for comparison; never include them in the report.
def secrets_reels():
    """Chaines a rechercher, lues dans private/device_secrets.h. Jamais affichees."""
    chemin = os.path.join(ROOT, 'private', 'device_secrets.h')
    valeurs = {}
    if not os.path.exists(chemin):
        return valeurs
    for ligne in io.open(chemin, encoding='utf-8', errors='replace'):
        m = re.match(r'\s*#define\s+(GLY_\w+)\s+"([^"]*)"', ligne)
        if m and len(m.group(2)) >= 4 and m.group(1) != 'GLY_DEXCOM_REGION':
            valeurs[m.group(1)] = m.group(2)
    return valeurs


def fichiers_suivis():
    return [f for f in git('ls-files').splitlines() if f.strip()]


def lire(chemin, binaire=False):
    try:
        with open(chemin, 'rb') as f:
            data = f.read()
    except OSError:
        return None
    return data if binaire else data.decode('utf-8', errors='replace')


def controler_texte(nom, texte, secrets, trouves):
    for cle, valeur in secrets.items():
        if valeur in texte:
            trouves.append((nom, 'IDENTIFIANT REEL (%s)' % cle, '(valeur masquee)'))
    for adresse in set(COURRIEL.findall(texte)):
        if not COURRIELS_AUTORISES.search(adresse):
            trouves.append((nom, 'adresse de courriel', adresse))
    for motif, libelle in CHEMINS:
        for brut in set(motif.findall(texte)):
            trouves.append((nom, libelle, brut))
    # valeurs de glycemie exportees : lignes CSV "date;valeur;tendance"
    if re.search(r'\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}.{0,4}[;,]\s*\d{2,3}\s*[;,]', texte):
        trouves.append((nom, 'donnees de glycemie exportees', 'ligne de type journal/CSV'))


def chercher_secrets(nom, data, secrets, trouves):
    """Cherche UNIQUEMENT les identifiants reels : pas d'heuristique sur des octets quelconques."""
    for cle, valeur in secrets.items():
        for encodage in ('utf-8', 'utf-16-le'):
            if valeur.encode(encodage) in data:
                trouves.append((nom, 'IDENTIFIANT REEL (%s, %s)' % (cle, encodage), '(valeur masquee)'))


def controler_media(nom, data, secrets, trouves, metadonnees=4096):
    """Fichier son ou image : les octets utiles sont du signal ou de la compression.

    Y chercher des adresses de courriel ou des chemins ne produit que du hasard (une voix de 20 s en
    contient forcement une par hasard), et ces fausses alertes finiraient par masquer les
    vraies. On cherche donc les identifiants reels dans TOUT le fichier, et les adresses et chemins
    seulement dans les zones de metadonnees : le debut et la fin.
    """
    chercher_secrets(nom, data, secrets, trouves)
    if metadonnees <= 0:
        return
    bords = data[:metadonnees] + data[-metadonnees:]
    texte = bords.decode('latin-1', errors='replace')
    for adresse in set(COURRIEL.findall(texte)):
        if not COURRIELS_AUTORISES.search(adresse):
            trouves.append((nom, 'adresse de courriel (metadonnees)', adresse))
    for motif, libelle in CHEMINS:
        for brut in set(motif.findall(texte)):
            trouves.append((nom, libelle + ' (metadonnees)', brut))


# Inspect entries inside the archive, not the compressed byte stream.
def controler_zip(nom, chemin, secrets, trouves):
    """Examine une archive entree par entree.

    Les octets COMPRESSES sont du hasard : y chercher des adresses de courriel ou des chemins
    produirait de fausses alertes (et masquerait les vraies). On regarde donc les noms des entrees,
    le texte decompresse, et les identifiants reels dans tout le reste.
    """
    import zipfile
    try:
        with zipfile.ZipFile(chemin) as zf:
            for entree in zf.infolist():
                interne = '%s!%s' % (nom, entree.filename)
                for motif, libelle in CHEMINS:
                    for brut in set(motif.findall(entree.filename)):
                        trouves.append((interne, libelle, brut))
                if entree.file_size == 0 or entree.file_size > 8 * 1024 * 1024:
                    continue
                contenu = zf.read(entree)
                if entree.filename.lower().endswith(('.txt', '.csv', '.md', '.json', '.log', '.ini')):
                    controler_texte(interne, contenu.decode('utf-8', errors='replace'), secrets, trouves)
                else:
                    chercher_secrets(interne, contenu, secrets, trouves)
    except (OSError, zipfile.BadZipFile) as err:
        trouves.append((nom, 'archive illisible', str(err)))


# Search a built binary for sensitive strings before considering it distributable.
def controler_binaire(nom, data, secrets, trouves):
    for cle, valeur in secrets.items():
        for encodage in ('utf-8', 'utf-16-le'):
            if valeur.encode(encodage) in data:
                trouves.append((nom, 'IDENTIFIANT REEL (%s, %s)' % (cle, encodage), '(valeur masquee)'))
    texte = data.decode('latin-1', errors='replace')
    for adresse in set(COURRIEL.findall(texte)):
        if not COURRIELS_AUTORISES.search(adresse):
            trouves.append((nom, 'adresse de courriel', adresse))
    for motif, libelle in CHEMINS:
        for brut in set(motif.findall(texte)):
            trouves.append((nom, libelle, brut))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--historique', action='store_true', help="analyse aussi l'historique git")
    ap.add_argument('--binaire', action='append', default=[],
                    help='programme compile a examiner en plus (chemin complet, repetable)')
    ap.add_argument('--attendu-personnel', action='store_true',
                    help='essai de controle : le binaire DOIT contenir les identifiants (code 0 si trouves)')
    args = ap.parse_args()

    secrets = secrets_reels()
    print('Identifiants reels disponibles pour la recherche : %d %s'
          % (len(secrets), '(lus dans private/device_secrets.h, jamais affiches)' if secrets
             else '-> private/device_secrets.h absent : la recherche des identifiants est impossible'))

    trouves = []

    print('\n--- 1. Fichiers suivis par git (contenu du depot public) ---')
    suivis = fichiers_suivis()
    textes = 0
    for rel in suivis:
        chemin = os.path.join(ROOT, rel)
        if not os.path.exists(chemin):
            continue
        if rel.lower().endswith(BINAIRES):
            data = lire(chemin, binaire=True)
            if data is None:
                pass
            elif rel.lower().endswith('.zip'):
                controler_zip(rel, chemin, secrets, trouves)
            elif rel.lower().endswith(MEDIA):
                controler_media(rel, data, secrets, trouves)
            else:
                controler_binaire(rel, data, secrets, trouves)
        else:
            texte = lire(chemin)
            if texte is not None:
                textes += 1
                controler_texte(rel, texte, secrets, trouves)
    print('%d fichiers suivis, dont %d fichiers texte' % (len(suivis), textes))
    # Un controle qui n'a rien lu n'est pas une reussite : il ne prouve rien.
    rien_lu = len(suivis) < 50 or textes < 30
    if rien_lu:
        print('ANOMALIE : trop peu de fichiers lus pour que ce controle veuille dire quelque chose.')

    print('\n--- 2. Programme compile ---')
    for rel in ('build/glycemie_display.bin', 'Razine/build/glycemie_display.bin',
                'dist/glycemie_display.bin') + tuple(args.binaire):
        chemin = rel if os.path.isabs(rel) else os.path.join(ROOT, rel)
        if os.path.exists(chemin):
            data = lire(chemin, binaire=True)
            avant = len(trouves)
            if rel.lower().endswith('.zip'):
                controler_zip(rel, chemin, secrets, trouves)   # entree par entree, pas en vrac
            elif rel.lower().endswith(MEDIA):
                controler_media(rel, data, secrets, trouves)
            else:
                controler_binaire(rel, data, secrets, trouves)
            print('%-38s %8d octets  %s' % (rel, len(data),
                                            'propre' if len(trouves) == avant else 'A VOIR'))
        else:
            print('%-38s absent' % rel)

    print('\n--- 3. Captures d\'ecran (metadonnees) ---')
    for rel in [f for f in suivis if f.lower().endswith(('.png', '.jpg', '.jpeg'))]:
        data = lire(os.path.join(ROOT, rel), binaire=True)
        if data is None:
            continue
        avant = len(trouves)
        controler_binaire(rel, data, secrets, trouves)
        marque = 'propre' if len(trouves) == avant else 'A VOIR'
        print('%-38s %8d octets  %s' % (rel, len(data), marque))
    print("Rappel : le CONTENU affiche sur ces images (valeurs de glycemie) n'est pas analysable ici.")

    if args.historique:
        print('\n--- 4. Historique git (seulement utile si le depot est pousse en ligne) ---')
        objets = [l.split(' ', 1) for l in git('rev-list', '--all', '--objects').splitlines() if ' ' in l]
        print('%d objets nommes dans l\'historique' % len(objets))
        suspects = 0
        for sha, nom in objets:
            avant = len(trouves)
            if nom.lower().endswith(BINAIRES):
                # Les anciens fichiers binaires comptent aussi : un firmware commite par erreur
                # contiendrait les identifiants.
                brut = subprocess.run(['git', 'cat-file', '-p', sha], cwd=ROOT,
                                      capture_output=True).stdout
                controler_binaire('historique:' + nom, brut, secrets, trouves)
            else:
                controler_texte('historique:' + nom, git('cat-file', '-p', sha), secrets, trouves)
            suspects += 1 if len(trouves) != avant else 0
        print('%d objets (texte et binaires) avec une trouvaille' % suspects)
        auteurs = sorted(set(git('log', '--all', '--format=%ae|%ce').splitlines()))
        print('Adresses des commits : %s' % ', '.join(sorted({a for ligne in auteurs
                                                              for a in ligne.split('|') if a})))

    print('\n=== RESULTAT ===')
    if rien_lu:
        print('CONTROLE NON CONCLUANT : trop peu de fichiers lus. Ne publiez rien sur cette base.')
        return 2
    if not secrets:
        print('CONTROLE INCOMPLET : private\\device_secrets.h est absent, donc la presence de vos')
        print('identifiants reels n\'a PAS pu etre verifiee. Les autres controles ont ete faits.')
        return 2
    if not trouves:
        print('Rien de personnel trouve dans ce qui a ete examine ci-dessus.')
        print("Ce controle ne lit ni le contenu visible des images, ni ce qui n'est pas dans ces")
        print('fichiers : il reduit le risque, il ne le supprime pas.')
        return 0
    par_fichier = {}
    for nom, genre, detail in trouves:
        par_fichier.setdefault(nom, []).append((genre, detail))
    for nom in sorted(par_fichier):
        print('\n%s' % nom)
        for genre, detail in sorted(set(par_fichier[nom])):
            print('   %-32s %s' % (genre, detail))
    print('\n%d trouvaille(s) dans %d fichier(s).' % (len(trouves), len(par_fichier)))
    return 1


if __name__ == '__main__':
    sys.exit(main())
