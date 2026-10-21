#!/usr/bin/env python3
# EN: Retrieves recoverable microSD data over a read-only serial protocol.
# FR: Récupère les données microSD par une liaison série en lecture seule.
r"""Recuperer par le cable USB ce que le programme INSTALLE accepte de relire sur la microSD.

Aucun lecteur de carte n'est necessaire : tout passe par la console serie de l'appareil, en
LECTURE SEULE. Rien n'est ecrit, efface ni formate sur la carte.

Ce que le programme actuel permet :
  - l'inventaire complet (sd_check) : tous les fichiers, leur taille, et pour chaque journee de
    mesures le nombre de valeurs, les doublons, la premiere et la derniere ligne ;
  - le contenu des JOURNAUX (/glycemie/logs) : commande sd_lire ;
  - les empreintes CRC32 des fichiers de VOIX (vrx_list crc), de quoi verifier que la banque de la
    carte correspond a celle deja presente sur le PC, sans retransferer les sons ;
  - le contenu des fichiers d'HISTORIQUE (/glycemie/history) seulement si le programme installe
    connait la commande "sd_dump" (ajoutee en K22). Sinon, ces fichiers sont listes mais pas copies :
    l'outil le dit clairement au lieu de faire croire a une sauvegarde complete.

La sauvegarde va dans private\sauvegardes\ : un dossier prive, jamais publie.

Usage :
    python tools\recuperer_microsd.py --port COM7
    python tools\recuperer_microsd.py --port COM7 --banque "D:\\chemin\\vers\\voice\\fr"
"""
import argparse
import base64
import binascii
import datetime
import io
import json
import os
import re
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(ICI, '..'))
sys.path.insert(0, ICI)
import assistant_config as ac  # noqa: E402

SAUVEGARDES = os.path.join(ROOT, 'private', 'sauvegardes')


# Inventory remote files before attempting any copy.
def inventaire(app, attente=120):
    """sd_check : liste tout, en lecture seule. Renvoie (fichiers, resume_csv, brut)."""
    app.envoyer('sd_check')
    lignes, fichiers, csv, entete = [], [], {}, ''
    fin_attendue = 0
    import time
    limite = time.time() + attente
    dernier = time.time()
    while time.time() < limite:
        paquet = app.lire(2)
        if paquet:
            dernier = time.time()
            lignes.extend(paquet)
            for l in paquet:
                if l.startswith('SD_CHECK '):
                    entete = l
                elif l.startswith('SD_FILE '):
                    m = re.match(r'SD_FILE (\S+) (-?\d+) octets', l)
                    if m:
                        fichiers.append({'chemin': m.group(1), 'taille': int(m.group(2))})
                elif l.startswith('SD_CSV '):
                    m = re.match(r'SD_CSV (\S+) (\S+)=(.*)', l)
                    if m:
                        csv.setdefault(m.group(1), {})[m.group(2)] = m.group(3)
                elif l.startswith('SD_DIR '):
                    fin_attendue += 1
        elif time.time() - dernier > 6:
            break            # plus rien ne vient : l'inventaire est termine
    return fichiers, csv, lignes


def lire_journal(app, nom, attente=90):
    """sd_lire : contenu d'un fichier de /glycemie/logs. Renvoie (lignes, octets_annonces)."""
    app.envoyer('sd_lire %s' % nom)
    contenu, octets, fini = [], None, False
    import time
    limite = time.time() + attente
    while time.time() < limite and not fini:
        for l in app.lire(2):
            if l.startswith('SD_LIRE| '):
                contenu.append(l[len('SD_LIRE| '):])
            elif l.startswith('SD_LIRE fin '):
                m = re.search(r'(\d+) lignes, (\d+) octets', l)
                if m:
                    octets = int(m.group(2))
                fini = True
            elif l.startswith('SD_LIRE '):
                fini = True   # refus ou erreur
    return contenu, octets


# Verify each transferred file before accepting it as a usable backup.
def dump_fichier(app, chemin, attente=180):
    """sd_dump (K22) : contenu exact en base64 + CRC32. Renvoie (octets, crc) ou (None, raison)."""
    app.envoyer('sd_dump %s' % chemin)
    morceaux, crc_annonce, taille_annoncee, fini, refus = [], None, None, False, None
    import time
    limite = time.time() + attente
    while time.time() < limite and not fini:
        for l in app.lire(2):
            if l.startswith('SD_DUMP| '):
                morceaux.append(l[len('SD_DUMP| '):])
            elif l.startswith('SD_DUMP fin '):
                m = re.search(r'octets=(\d+) crc=([0-9a-fA-F]+)', l)
                if m:
                    taille_annoncee, crc_annonce = int(m.group(1)), int(m.group(2), 16)
                fini = True
            elif l.startswith('SD_DUMP '):
                refus = l
                fini = True
    if refus is not None or crc_annonce is None:
        return None, refus or 'commande sd_dump absente du programme installe'
    try:
        data = base64.b64decode(''.join(morceaux))
    except (ValueError, binascii.Error) as err:
        return None, 'decodage impossible (%s)' % err
    if len(data) != taille_annoncee:
        return None, 'taille differente (%d recus, %d annonces)' % (len(data), taille_annoncee)
    if (binascii.crc32(data) & 0xFFFFFFFF) != crc_annonce:
        return None, 'empreinte CRC differente'
    return data, crc_annonce


def voix_de_la_carte(app, attente=120):
    """vrx_list crc : {nom: (taille, crc)} des fichiers de voix, sans rien transferer."""
    app.envoyer('vrx_begin')
    app.lire(8, 'VRX BEGIN')
    app.envoyer('vrx_list crc')
    fichiers, fini = {}, False
    import time
    limite = time.time() + attente
    while time.time() < limite and not fini:
        for l in app.lire(2):
            m = re.match(r'VRX FILE (\S+) (\d+) ([0-9a-fA-F]+)', l)
            if m:
                fichiers[m.group(1)] = (int(m.group(2)), int(m.group(3), 16))
            elif l.startswith('VRX LIST '):
                fini = True
    app.envoyer('vrx_end')
    app.lire(4, 'VRX END')
    return fichiers


# Use remote CRCs to check audio without sending the same WAVs again.
def comparer_banque(fichiers_carte, dossier_pc):
    """Compare les empreintes de la carte a celles d'une banque deja presente sur le PC."""
    if not fichiers_carte or not dossier_pc or not os.path.isdir(dossier_pc):
        return None
    identiques, differents, absents = [], [], []
    for nom, (taille, crc) in sorted(fichiers_carte.items()):
        chemin = os.path.join(dossier_pc, nom)
        if not os.path.exists(chemin):
            absents.append(nom)
            continue
        with open(chemin, 'rb') as f:
            data = f.read()
        if len(data) == taille and (binascii.crc32(data) & 0xFFFFFFFF) == crc:
            identiques.append(nom)
        else:
            differents.append(nom)
    return {'identiques': identiques, 'differents': differents, 'absents_du_pc': absents}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default=None)
    ap.add_argument('--banque', default=None,
                    help='dossier de la banque vocale deja presente sur le PC (pour comparaison)')
    ap.add_argument('--dossier', default=None)
    args = ap.parse_args()

    port = args.port or ac.trouver_port()
    if port is None:
        print('Appareil introuvable. Branchez-le, ou precisez --port COM7.')
        return 1
    app = ac.Appareil(ac.ouvrir_port(port))
    horodatage = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    cible = args.dossier or os.path.join(SAUVEGARDES, horodatage + '_microsd')
    os.makedirs(cible, exist_ok=True)
    resume = {'date': horodatage, 'port': port, 'recupere': [], 'non_recupere': []}

    try:
        app.lire(1)
        print('--- Inventaire de la carte (lecture seule) ---')
        fichiers, csv, brut = inventaire(app)
        io.open(os.path.join(cible, 'inventaire_brut.txt'), 'w', encoding='utf-8',
                newline='\n').write('\n'.join(brut) + '\n')
        par_dossier = {}
        for f in fichiers:
            par_dossier.setdefault(os.path.dirname(f['chemin']), []).append(f)
        for dossier in sorted(par_dossier):
            total = sum(f['taille'] for f in par_dossier[dossier] if f['taille'] > 0)
            print('   %-28s %4d fichier(s)  %8.1f Ko' % (dossier, len(par_dossier[dossier]),
                                                         total / 1024))
        resume['inventaire'] = fichiers
        resume['csv'] = csv

        # --- Journaux : relisibles par le programme installe ---------------------------------
        print('\n--- Journaux (/glycemie/logs) ---')
        dossier_logs = os.path.join(cible, 'glycemie', 'logs')
        os.makedirs(dossier_logs, exist_ok=True)
        for f in [f for f in fichiers if '/logs/' in f['chemin']]:
            nom = os.path.basename(f['chemin'])
            contenu, octets = lire_journal(app, nom)
            if octets is None:
                print('   %-24s NON RELU' % nom)
                resume['non_recupere'].append({'chemin': f['chemin'], 'raison': 'sd_lire sans reponse'})
                continue
            texte = '\n'.join(contenu) + ('\n' if contenu else '')
            io.open(os.path.join(dossier_logs, nom), 'w', encoding='utf-8',
                    newline='\n').write(texte)
            exact = len(texte.encode('utf-8')) == octets
            print('   %-24s %6d octets annonces, %d lignes %s'
                  % (nom, octets, len(contenu), 'identique' if exact else '(fins de ligne adaptees)'))
            resume['recupere'].append({'chemin': f['chemin'], 'octets_annonces': octets,
                                       'lignes': len(contenu), 'octet_pour_octet': exact})

        # --- Historique : seulement si le programme connait sd_dump --------------------------
        print('\n--- Historique (/glycemie/history) ---')
        histo = [f for f in fichiers if '/history/' in f['chemin']]
        dossier_histo = os.path.join(cible, 'glycemie', 'history')
        os.makedirs(dossier_histo, exist_ok=True)
        data, raison = dump_fichier(app, histo[0]['chemin']) if histo else (None, 'aucun fichier')
        if data is None:
            print('   IMPOSSIBLE avec le programme installe : %s' % raison)
            print('   %d fichier(s) sont listes et verifies, mais leur contenu ne peut pas sortir'
                  % len(histo))
            for f in histo:
                resume['non_recupere'].append({'chemin': f['chemin'], 'taille': f['taille'],
                                               'raison': raison})
        else:
            open(os.path.join(dossier_histo, os.path.basename(histo[0]['chemin'])), 'wb').write(data)
            resume['recupere'].append({'chemin': histo[0]['chemin'], 'octets': len(data),
                                       'octet_pour_octet': True})
            for f in histo[1:]:
                data, raison = dump_fichier(app, f['chemin'])
                if data is None:
                    print('   %-24s ECHEC : %s' % (os.path.basename(f['chemin']), raison))
                    resume['non_recupere'].append({'chemin': f['chemin'], 'raison': raison})
                    continue
                open(os.path.join(dossier_histo, os.path.basename(f['chemin'])), 'wb').write(data)
                print('   %-24s %6d octets, empreinte verifiee' % (os.path.basename(f['chemin']),
                                                                   len(data)))
                resume['recupere'].append({'chemin': f['chemin'], 'octets': len(data),
                                           'octet_pour_octet': True})

        # --- Voix : comparaison d'empreintes, aucun transfert --------------------------------
        print('\n--- Fichiers de voix : comparaison, sans transfert ---')
        voix = voix_de_la_carte(app)
        print('   %d fichier(s) de voix sur la carte' % len(voix))
        resume['voix_carte'] = {n: {'taille': t, 'crc': '%08x' % c} for n, (t, c) in voix.items()}
        comparaison = comparer_banque(voix, args.banque)
        if comparaison is None:
            print('   (aucune banque du PC indiquee : --banque <dossier> pour comparer)')
        else:
            print('   identiques au PC : %d   differents : %d   absents du PC : %d'
                  % (len(comparaison['identiques']), len(comparaison['differents']),
                     len(comparaison['absents_du_pc'])))
            resume['comparaison_voix'] = comparaison
    finally:
        app.close()

    with io.open(os.path.join(cible, 'resume.json'), 'w', encoding='utf-8') as f:
        json.dump(resume, f, indent=2, ensure_ascii=False)

    print('\n=== RESULTAT ===')
    print('Sauvegarde partielle : %s' % cible)
    print('   recuperes      : %d fichier(s)' % len(resume['recupere']))
    print('   non recuperes  : %d fichier(s)' % len(resume['non_recupere']))
    print("Ce dossier est prive. La carte n'a pas ete modifiee.")
    return 0 if not resume['non_recupere'] else 2


if __name__ == '__main__':
    sys.exit(main())
