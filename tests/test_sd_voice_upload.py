#!/usr/bin/env python3
# Tests that voice verification never writes to microSD.
#
# These tests use simulations; they do not connect to a patient's device.
# Test read-only verification and audio transfer using a simulated serial device.
#
"""Controles de l'outil de copie de la banque vocale (tools/voice/sd_voice_upload.py).

Le plus important : en mode --verify-only, l'outil ne doit JAMAIS ecrire sur la carte, dans aucune des
six langues, meme si la carte est incomplete ou differente. Les essais utilisent un transport simule
(aucune liaison serie, aucun materiel, aucun fichier reel de banque).

Usage : python tests\\test_sd_voice_upload.py
"""
import io
import os
import sys
import tempfile
import types
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.modules.setdefault('serial', types.ModuleType('serial'))  # pyserial inutile : transport simule
sys.modules['serial'].Serial = object
sys.path.insert(0, os.path.join(ROOT, 'tools', 'voice'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import sd_voice_upload as up  # noqa: E402  (the path must be added before the import)
ORIGINAL_CONFIGURE = up.configure  # restoration after simulated transport

FICHIERS = ['alert_low.wav', 'glucose_100.wav', 'trend_flat.wav']
resultats = []


def check(nom, ok, detail=''):
    resultats.append((nom, bool(ok), detail))


# Use short synthetic clips instead of the real WAV files.
def banque_temporaire(dossier):
    """Petite banque locale (3 fichiers) + manifeste, a la place de la vraie banque."""
    voice = os.path.join(dossier, 'voice')
    os.makedirs(voice, exist_ok=True)
    lignes = ['fichier;texte;duree_s;taille_octets;crc32']
    entrees = []
    for i, nom in enumerate(FICHIERS):
        data = bytes([(i * 7 + j) % 251 for j in range(64)])
        with open(os.path.join(voice, nom), 'wb') as f:
            f.write(data)
        crc = '%08x' % (zlib.crc32(data) & 0xFFFFFFFF)
        lignes.append('%s;texte %d;0.100;%d;%s' % (nom, i, len(data), crc))
        entrees.append((nom, len(data), crc))
    with io.open(os.path.join(dossier, 'manifest.csv'), 'w', encoding='utf-8-sig', newline='') as f:
        f.write('\n'.join(lignes) + '\n')
    return voice, entrees


class CarteSimulee:
    """Repond comme le firmware : vrx_begin / ping / dir / put / list / end."""

    def __init__(self, contenu, dossier_liste=True, sous_dossiers=('en',)):
        self.contenu = {code: dict(fichiers) for code, fichiers in contenu.items()}  # {'racine'|code: {nom: (t, crc)}}
        self.dossier_liste = dossier_liste       # firmware a jour : "VRX LIST dossier=..."
        self.sous_dossiers = sous_dossiers       # old firmwares: lists as large entrees -1
        self.courant = 'racine'
        self.ecritures = []                      # noms demandes en ecriture (vrx_put accepte)
        self.octets_recus = 0
        self.dossiers_crees = []
        self.attendu = 0                         # bytes remaining to be received for the vrx_put en cours

    def commande(self, ligne):
        mots = ligne.split()
        if not mots:
            return []
        if mots[0] == 'vrx_begin':
            return ['VRX BEGIN OK baud=%s fichiers=%d libre_mo=1000 crc_test=cbf43926'
                    % (mots[1] if len(mots) > 1 else '115200', len(self.contenu.get('racine', {})))]
        if mots[0] == 'vrx_ping':
            return ['VRX PONG']
        if mots[0] == 'vrx_dir':
            code = mots[1]
            lecture = len(mots) > 2 and mots[2] == 'lecture'
            if code == '-':
                self.courant = 'racine'
                return ['VRX DIR racine']
            if lecture:
                if not self.dossier_liste:
                    self.dossiers_crees.append(code)      # ancien firmware : ignore "lecture" et cree le dossier
                    self.courant = code
                    return ['VRX DIR %s' % code]
                if not self.contenu.get(code):
                    return ['VRX ERR dossier %s absent ou vide' % code]
                self.courant = code
                return ['VRX DIR %s lecture' % code]
            if code not in self.contenu:
                self.contenu[code] = {}
                self.dossiers_crees.append(code)
            self.courant = code
            return ['VRX DIR %s' % code]
        if mots[0] == 'vrx_put':
            nom, taille, crc = mots[1], int(mots[2]), mots[3]
            present = self.contenu.get(self.courant, {}).get(nom)
            if present == (taille, crc):
                return ['VRX SKIP %s' % nom]
            self.ecritures.append(nom)
            self.attendu = taille
            self.en_cours = (nom, taille, crc)
            return ['VRX READY %s' % nom]
        if mots[0] == 'vrx_list':
            lignes = []
            fichiers = self.contenu.get(self.courant, {})
            for nom, (taille, crc) in sorted(fichiers.items()):
                lignes.append('VRX FILE %s %d %s' % (nom, taille, crc))
            if self.courant == 'racine' and not self.dossier_liste:
                for code in self.sous_dossiers:
                    lignes.append('VRX FILE %s -1 illisible' % code)
            total = sum(t for t, _ in fichiers.values())
            entete = 'dossier=%s ' % self.courant if self.dossier_liste else ''
            lignes.append('VRX LIST %sfichiers=%d octets=%d libre_mo=1000' % (entete, len(fichiers), total))
            return lignes
        if mots[0] == 'vrx_end':
            return ['VRX END']
        return ['VRX ERR commande']

    def binaire(self, data):
        self.octets_recus += len(data)
        self.attendu -= len(data)
        reponses = ['VRX ACK %d' % (self.en_cours[1] - max(self.attendu, 0))]
        if self.attendu <= 0:
            nom, taille, crc = self.en_cours
            self.contenu.setdefault(self.courant, {})[nom] = (taille, crc)
            reponses.append('VRX OK %s %d %s' % (nom, taille, crc))
        return reponses


class SerieSimulee:
    def __init__(self, lien):
        self.lien = lien

    def write(self, data):
        self.lien.recevoir(self.lien.carte.binaire(data))

    def reset_input_buffer(self):
        self.lien.lignes = []


class LienSimule:
    """Meme interface que sd_voice_upload.Link, sans port serie."""

    def __init__(self, port, log_path, carte=None):
        self.carte = carte if carte is not None else LienSimule.carte_par_defaut
        self.ser = SerieSimulee(self)
        self.lignes = []
        self.journal = []
        self.commandes = []
        self.buffer = b''

    def recevoir(self, lignes):
        self.lignes.extend(lignes)

    def note(self, text, echo=False):
        self.journal.append(text)

    def send(self, command):
        if command:
            self.commandes.append(command)
            self.recevoir(self.carte.commande(command))

    def line(self, timeout):
        return self.lignes.pop(0) if self.lignes else None

    def expect(self, pattern, timeout):
        import re
        regex = re.compile(pattern)
        while self.lignes:
            text = self.lignes.pop(0)
            m = regex.match(text)
            if m:
                return m
            if text.startswith('VRX ERR'):
                return text
        return None

    def set_baud(self, baud):
        pass

    def close(self):
        pass


# Replace the serial transport so a regression test cannot write to a physical microSD.
def executer(carte, lang, verify_only):
    """Lance l'outil sur une carte simulee ; retourne (code de sortie, lien)."""
    dossier = tempfile.mkdtemp(prefix='banque_')
    voice, entrees = banque_temporaire(dossier)
    up.configure = lambda _lang, _sd_root=None: None  # banque simulee, meme signature
    up.BANK, up.MANIFEST = voice, os.path.join(dossier, 'manifest.csv')
    liens = []

    def fabrique(port, log_path):
        lien = LienSimule(port, log_path, carte)
        liens.append(lien)
        return lien

    up.LINK = fabrique
    journal = os.path.join(dossier, 'journal.log')
    argv = ['--port', 'SIMULE', '--log', journal, '--lang', lang] + (['--verify-only'] if verify_only else [])
    code = up.main(argv)
    return code, liens[0], entrees


def contenu_identique(entrees):
    return {nom: (taille, crc) for nom, taille, crc in entrees}


# Cover complete, missing, mismatched and older-firmware behaviours.
def main():
    _, entrees = banque_temporaire(tempfile.mkdtemp(prefix='reference_'))
    conforme = contenu_identique(entrees)
    manquant = {n: v for n, v in list(conforme.items())[:-1]}
    different = dict(conforme)
    different[FICHIERS[0]] = (conforme[FICHIERS[0]][0], 'deadbeef')

    for lang in ['fr', 'en', 'es', 'de', 'it', 'pt']:
        cle = 'racine' if lang == 'fr' else lang
        for nom_cas, contenu, attendu_ok in (('carte conforme', conforme, True),
                                             ('fichier manquant', manquant, False),
                                             ('fichier different', different, False)):
            carte = CarteSimulee({cle: contenu})
            code, lien, _ = executer(carte, lang, verify_only=True)
            check('%s / %s : aucune ecriture en --verify-only' % (lang, nom_cas),
                  not carte.ecritures and carte.octets_recus == 0,
                  '%d fichier(s), %d octet(s)' % (len(carte.ecritures), carte.octets_recus))
            check('%s / %s : aucune commande vrx_put' % (lang, nom_cas),
                  not any(c.startswith('vrx_put') for c in lien.commandes))
            check('%s / %s : aucun dossier cree' % (lang, nom_cas), not carte.dossiers_crees,
                  ', '.join(carte.dossiers_crees))
            check('%s / %s : resultat %s' % (lang, nom_cas, 'CONFORME' if attendu_ok else 'ECHEC'),
                  (code == 0) == attendu_ok, 'code %d' % code)

    # Old Firmware (no folder=): explicit refusal for other languages, no writing
    for lang in ['en', 'pt']:
        carte = CarteSimulee({'racine': conforme, lang: conforme}, dossier_liste=False)
        code, lien, _ = executer(carte, lang, verify_only=True)
        check('%s : firmware ancien refuse en lecture seule' % lang, code != 0, 'code %d' % code)
        check('%s : firmware ancien, aucune ecriture de fichier' % lang,
              not carte.ecritures and carte.octets_recus == 0)

    # English on old firmware: list subfolders should not be counted as too many files
    carte = CarteSimulee({'racine': conforme}, dossier_liste=False, sous_dossiers=('en', 'es'))
    code, lien, _ = executer(carte, 'fr', verify_only=True)
    check('fr : firmware ancien, sous-dossiers ignores dans la liste', code == 0, 'code %d' % code)

    # Copie normale : l'outil ecrit bien ce qui manque (le transport simule fonctionne)
    carte = CarteSimulee({'racine': manquant})
    code, lien, entrees_copie = executer(carte, 'fr', verify_only=False)
    check('fr : la copie normale envoie le fichier manquant', carte.ecritures == [FICHIERS[-1]],
          ', '.join(carte.ecritures))
    check('fr : la copie normale se termine CONFORME', code == 0, 'code %d' % code)

    # ZIP public: the script must be able to copy the structure directly
    # glycemie/audio/voice without the cache or the Google generation account.
    up.configure = ORIGINAL_CONFIGURE
    old_bank, old_manifest = up.BANK, up.MANIFEST
    with tempfile.TemporaryDirectory(prefix='sd_root_public_') as sd_root:
        for lang in ('fr', 'en'):
            sd_voice = os.path.join(sd_root, 'glycemie', 'audio', 'voice')
            dest = sd_voice if lang == 'fr' else os.path.join(sd_voice, lang)
            os.makedirs(dest, exist_ok=True)
            for i in range(396):
                name = 'glucose_%03d.wav' % i
                with open(os.path.join(dest, name), 'wb') as output:
                    output.write(b'RIFF' + bytes([i % 251]) * 12)
            up.configure(lang, sd_root)
            entries_public = up.load_manifest()
            check('%s : archive microSD extraite lue sans Google' % lang,
                  len(entries_public) == 396 and up.MANIFEST is None)
            check('%s : CRC32 calcule sur les WAV reels' % lang,
                  all(int(crc, 16) == zlib.crc32(open(os.path.join(dest, name), 'rb').read()) & 0xFFFFFFFF
                      for name, taille, crc in entries_public))
        os.remove(os.path.join(sd_root, 'glycemie', 'audio', 'voice', 'en', 'glucose_395.wav'))
        up.configure('en', sd_root)
        refused = False
        try:
            up.load_manifest()
        except SystemExit:
            refused = True
        check('banque extraite incomplete refusee', refused)
    up.BANK, up.MANIFEST = old_bank, old_manifest

    largeur = max(len(r[0]) for r in resultats)
    echecs = 0
    for nom, ok, detail in resultats:
        echecs += 0 if ok else 1
        print('%-6s %-*s %s' % ('OK' if ok else 'ECHEC', largeur, nom, detail))
    print('\nRESULTAT : %s (%d controles, %d echec(s))'
          % ('COPIE VOCALE SURE' if echecs == 0 else 'A CORRIGER', len(resultats), echecs))
    return 0 if echecs == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
