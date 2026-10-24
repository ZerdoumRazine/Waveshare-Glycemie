#!/usr/bin/env python3
# Checks serial microSD recovery against damaged transfers.
#
# These tests use simulations; they do not connect to a patient's device.
# Validate CRC and interrupted-transfer handling with artificial microSD data.
#
"""Controls of microSD recovery by cable (tools/recuperer_microsd.py).

What matters here: a copy n'est declaree bonne que si la taille ET l'footprint announced by
The device matches what was received. A truncated, abysmal transmission, or a program
who does not know the command must be reported, never taken for a successful backup.

Simulating device: no serial connection, no material.

Usage : python tests\\test_recuperation_microsd.py
"""
import base64
import binascii
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import assistant_config as ac          # noqa: E402
import recuperer_microsd as rm         # noqa: E402

resultats = []


def check(nom, ok, detail=''):
    resultats.append((nom, bool(ok), detail))


class AppareilSimule(object):
    """Answer to sd_dump like firmware: base64 lines then size and CRC."""

    def __init__(self, fichiers, tronquer=False, fausser_crc=False, connait=True):
        self.fichiers = fichiers     # {chemin: contenu binaire}
        self.tronquer = tronquer
        self.fausser_crc = fausser_crc
        self.connait = connait
        self.sortie = b''
        self.tampon = b''
        self.recu = []

    def write(self, data):
        self.tampon += data
        while b'\n' in self.tampon:
            brut, self.tampon = self.tampon.split(b'\n', 1)
            self._commande(brut.decode('utf-8').strip())
        return len(data)

    def read(self, taille):
        morceau, self.sortie = self.sortie[:taille], self.sortie[taille:]
        return morceau

    def close(self):
        pass

    def _repondre(self, ligne):
        self.sortie += (ligne + '\r\n').encode('utf-8')

    def _commande(self, cmd):
        self.recu.append(cmd)
        if not cmd.startswith('sd_dump '):
            return
        if not self.connait:
            return                      # old program: no answer
        chemin = cmd[len('sd_dump '):]
        if '..' in chemin or '\\' in chemin:
            self._repondre('SD_DUMP refuse : chemin invalide')
            return
        data = self.fichiers.get(chemin)
        if data is None:
            self._repondre('SD_DUMP %s : introuvable' % chemin)
            return
        envoye = data[:-3] if self.tronquer else data
        for i in range(0, len(envoye), 48):
            self._repondre('SD_DUMP| ' + base64.b64encode(envoye[i:i + 48]).decode('ascii'))
        crc = binascii.crc32(data) & 0xFFFFFFFF
        if self.fausser_crc:
            crc ^= 0xFFFF
        self._repondre('SD_DUMP fin %s octets=%d crc=%08x' % (chemin, len(data), crc))


CSV = ('date;valeur;tendance;variation\n'
       '2026-10-17T08:00:00+02:00;112;Flat;-2\n'
       '2026-10-17T08:05:00+02:00;118;FortyFiveUp;6\n').encode('utf-8')
BINAIRE = bytes(range(256)) * 3


# Reject truncated or damaged downloads rather than calling them valid backups.
def main():
    fichiers = {'/glycemie/history/2026-10-17.csv': CSV, '/glycemie/audio/essai.bin': BINAIRE}

    # --- 1. Fidel copy ---------------------------------------------------------------------------------------
    app = ac.Appareil(AppareilSimule(fichiers))
    data, info = rm.dump_fichier(app, '/glycemie/history/2026-10-17.csv', attente=10)
    check('fichier texte : copie recue', data == CSV, repr(data)[:60])
    check("l'empreinte renvoyee est celle du contenu",
          info == (binascii.crc32(CSV) & 0xFFFFFFFF))

    app = ac.Appareil(AppareilSimule(fichiers))
    data, _ = rm.dump_fichier(app, '/glycemie/audio/essai.bin', attente=10)
    check('fichier binaire : copie exacte, octet pour octet', data == BINAIRE,
          '%d octets' % (len(data) if data else -1))

    # --- 2. Transmission abime : never taken for a success --------------------------------
    app = ac.Appareil(AppareilSimule(fichiers, tronquer=True))
    data, raison = rm.dump_fichier(app, '/glycemie/history/2026-10-17.csv', attente=10)
    check('transmission tronquee : refusee', data is None and 'taille' in (raison or ''), str(raison))

    app = ac.Appareil(AppareilSimule(fichiers, fausser_crc=True))
    data, raison = rm.dump_fichier(app, '/glycemie/history/2026-10-17.csv', attente=10)
    check('empreinte differente : refusee', data is None and 'CRC' in (raison or ''), str(raison))

    # --- 3. Program that does not know the order ------------------------------------------------------------
    app = ac.Appareil(AppareilSimule(fichiers, connait=False))
    data, raison = rm.dump_fichier(app, '/glycemie/history/2026-10-17.csv', attente=3)
    check("ancien programme : dit que la commande est absente",
          data is None and 'absente' in (raison or ''), str(raison))

    # --- 4. Chemins refuses ---------------------------------------------------------------------
    simule = AppareilSimule(fichiers)
    app = ac.Appareil(simule)
    data, raison = rm.dump_fichier(app, '/glycemie/../private/secret.txt', attente=5)
    check('chemin avec ".." : refuse par l\'appareil', data is None and 'refuse' in (raison or ''),
          str(raison))
    data, raison = rm.dump_fichier(app, '/glycemie/history/absent.csv', attente=5)
    check('fichier absent : signale, pas de copie vide', data is None and 'introuvable' in (raison or ''),
          str(raison))

    # --- 5. Comparison of voice banks (no transfer) -----------------------------------
    import tempfile
    dossier = tempfile.mkdtemp(prefix='banque-')
    contenu = {'alert_low.wav': b'RIFF....petit son', 'glucose_100.wav': b'RIFF....autre son'}
    for nom, data_wav in contenu.items():
        open(os.path.join(dossier, nom), 'wb').write(data_wav)
    carte = {nom: (len(d), binascii.crc32(d) & 0xFFFFFFFF) for nom, d in contenu.items()}
    resultat = rm.comparer_banque(carte, dossier)
    check('banque identique : reconnue sans transfert',
          len(resultat['identiques']) == 2 and not resultat['differents'] and
          not resultat['absents_du_pc'])
    carte['glucose_100.wav'] = (99, 0x1234)
    carte['manquant.wav'] = (10, 0x5678)
    resultat = rm.comparer_banque(carte, dossier)
    check('fichier different : signale', resultat['differents'] == ['glucose_100.wav'],
          str(resultat['differents']))
    check('fichier absent du PC : signale', resultat['absents_du_pc'] == ['manquant.wav'],
          str(resultat['absents_du_pc']))

    largeur = max(len(n) for n, _, _ in resultats)
    echecs = 0
    for nom, ok, detail in resultats:
        print('   %-5s %-*s %s' % ('OK' if ok else 'ECHEC', largeur, nom, '' if ok else detail))
        echecs += 0 if ok else 1
    print('\ntest_recuperation_microsd : %d controles, %d echec(s)' % (len(resultats), echecs))
    return 0 if echecs == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
