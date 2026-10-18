#!/usr/bin/env python3
"""Copie la banque vocale (tools/voice/bank) sur la microSD du Waveshare par le cable USB-serie, sans retirer la
carte et sans reseau. Outil temporaire : commandes console vrx_* du firmware (main/voice_rx.c).

- debit temporaire 921600 bauds (retour automatique de la carte a 115200 ; repli a 115200 si le changement echoue) ;
- chaque fichier : blocs de 896 octets acquittes un par un (aucun debordement du tampon de la carte), CRC32 verifie
  par la carte en memoire puis apres relecture sur la microSD ; fichier deja present et identique : saute ;
- verification finale : liste complete relue sur la carte avec CRC32, comparee au manifeste (nombre exact, tailles,
  fichiers manquants ou en trop, integrite) et espace libre. La verification n'envoie QUE des commandes de lecture
  (vrx_dir ... lecture, vrx_list crc) : aucun octet de fichier n'est transmis, meme si la carte est non conforme.
Le port est ouvert sans agir sur DTR/RTS : la carte ne redemarre pas.

Usage (environnement du projet, pyserial) :
  .venv\\Scripts\\python.exe -B tools\\voice\\sd_voice_upload.py --port COM7 --log tools\\logs\\NNN_copie.log
  ... --verify-only    verification seule : aucune ecriture, aucun dossier cree (necessite un firmware qui
                       repond "VRX LIST dossier=..." pour les langues autres que le francais)
"""
import argparse
import csv
import os
import re
import sys
import time
import zlib

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, TOOLS)
from serial_capture import ANSI_RE, load_secrets, redact  # noqa: E402  (masquage identique aux captures)

BANK = os.path.join(HERE, 'bank', 'voice')
MANIFEST = os.path.join(HERE, 'bank', 'manifest.csv')


def configure(lang):
    """Banque locale a copier. Le francais est a la racine (bank/voice), comme a l'origine ;
    les autres langues dans bank/<code>/voice, et sur la carte dans /glycemie/audio/voice/<code>."""
    global BANK, MANIFEST
    base = os.path.join(HERE, 'bank') if lang == 'fr' else os.path.join(HERE, 'bank', lang)
    BANK = os.path.join(base, 'voice')
    MANIFEST = os.path.join(base, 'manifest.csv')
CHUNK = 896
FAST_BAUD = 921600
SLOW_BAUD = 115200


class Link:
    def __init__(self, port, log_path):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = SLOW_BAUD
        self.ser.timeout = 0.05
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()
        self.buffer = b''
        self.secrets = load_secrets()
        self.log = open(log_path, 'w', encoding='utf-8')
        self.start = time.time()

    def note(self, text, echo=True):
        stamp = '%8.2f ' % (time.time() - self.start)
        self.log.write(stamp + text + '\n')
        self.log.flush()
        if echo:
            print(stamp + text, flush=True)

    def send(self, command):
        self.ser.write((command + '\n').encode('ascii'))

    def line(self, timeout):
        """Ligne suivante de la carte (masquee), None apres timeout secondes."""
        end = time.time() + timeout
        while True:
            if b'\n' in self.buffer:
                raw, self.buffer = self.buffer.split(b'\n', 1)
                text = redact(ANSI_RE.sub('', raw.decode('utf-8', errors='replace').rstrip('\r')), self.secrets)
                if not text.startswith('VRX ACK'):
                    self.note('< ' + text, echo=False)
                return text
            if time.time() >= end:
                return None
            # au moins 1 octet (rend la main des son arrivee), puis tout ce qui est deja recu : latence minimale
            waiting = self.ser.in_waiting
            chunk = self.ser.read(waiting if waiting else 1)
            if chunk:
                self.buffer += chunk

    def expect(self, pattern, timeout):
        """Premiere ligne VRX correspondant a pattern (regex) ; les autres lignes (journal) sont ignorees."""
        regex = re.compile(pattern)
        end = time.time() + timeout
        while True:
            remaining = end - time.time()
            if remaining <= 0:
                return None
            text = self.line(remaining)
            if text is None:
                return None
            if text.startswith('VRX '):
                match = regex.match(text)
                if match:
                    return match
                if text.startswith('VRX ERR') or 'ERR' in text.split()[:3]:
                    return text

    def set_baud(self, baud):
        self.ser.baudrate = baud
        time.sleep(0.2)
        self.ser.reset_input_buffer()
        self.buffer = b''

    def close(self):
        self.ser.close()
        self.log.close()


def load_manifest():
    entries = []
    with open(MANIFEST, encoding='utf-8-sig', newline='') as f:
        for row in csv.DictReader(f, delimiter=';'):
            entries.append((row['fichier'], int(row['taille_octets']), row['crc32'].lower()))
    for name, size, crc in entries:
        data = open(os.path.join(BANK, name), 'rb').read()
        if len(data) != size or '%08x' % (zlib.crc32(data) & 0xFFFFFFFF) != crc:
            raise SystemExit('Banque locale incoherente avec le manifeste : %s' % name)
    return entries


def begin(link):
    link.send('')
    time.sleep(0.3)
    link.send('vrx_begin %d' % FAST_BAUD)
    m = link.expect(r'VRX BEGIN OK baud=(\d+) fichiers=(-?\d+) libre_mo=(\d+) crc_test=([0-9a-f]{8})', 10)
    if not hasattr(m, 'group'):
        raise SystemExit('Session refusee par la carte : %s' % m)
    if m.group(4) != '%08x' % zlib.crc32(b'123456789'):
        raise SystemExit('CRC32 de la carte incompatible (%s)' % m.group(4))
    link.note('Carte : %s fichier(s) deja dans /glycemie/audio/voice, %s Mo libres' % (m.group(2), m.group(3)))
    link.set_baud(FAST_BAUD)
    for _ in range(6):
        link.send('vrx_ping')
        if hasattr(link.expect(r'VRX PONG', 0.7), 'group'):
            link.note('Debit %d bauds etabli' % FAST_BAUD)
            return FAST_BAUD
    link.note('Pas de reponse a %d bauds : retour a %d (attente du retour automatique de la carte)' % (FAST_BAUD,
                                                                                                    SLOW_BAUD))
    link.set_baud(SLOW_BAUD)
    time.sleep(32)
    link.send('')
    link.send('vrx_begin %d' % SLOW_BAUD)
    if not hasattr(link.expect(r'VRX BEGIN OK', 10), 'group'):
        raise SystemExit('Session impossible a %d bauds' % SLOW_BAUD)
    return SLOW_BAUD


def put_file(link, name, size, crc, data):
    link.send('vrx_put %s %d %s' % (name, size, crc))
    m = link.expect(r'VRX (SKIP|READY) (\S+)', 15)
    if not hasattr(m, 'group') or m.group(2) != name:
        return 'refus (%s)' % m
    if m.group(1) == 'SKIP':
        return 'SKIP'
    for offset in range(0, size, CHUNK):
        block = data[offset:offset + CHUNK]
        link.ser.write(block)
        ack = link.expect(r'VRX ACK (\d+)', 8)
        if not hasattr(ack, 'group') or int(ack.group(1)) != offset + len(block):
            return 'acquittement manquant a %d (%s)' % (offset, ack)
    m = link.expect(r'VRX OK (\S+) (\d+) ([0-9a-f]{8})', 20)
    if not hasattr(m, 'group') or m.group(1) != name or int(m.group(2)) != size or m.group(3) != crc:
        return 'verification refusee (%s)' % m
    return 'OK'


def choisir_dossier(link, lang, lecture):
    """vrx_dir pour une langue. lecture=True : la carte ne doit RIEN creer (firmware a jour exige)."""
    if lang == 'fr':
        return True
    link.send('vrx_dir %s%s' % (lang, ' lecture' if lecture else ''))
    m = link.expect(r'VRX (DIR|ERR) (\S+) ?(\S*)', 10)
    if not hasattr(m, 'group') or m.group(1) != 'DIR':
        link.note('Sous-dossier %s refuse par la carte : %s' % (lang, m))
        return False
    if lecture and m.group(3) != 'lecture':
        link.note('VERIFICATION refusee : ce firmware ne connait pas "vrx_dir %s lecture" ; il a pu creer le '
                  'dossier, mais aucun fichier n\'a ete envoye. Mettre le firmware a jour.' % lang)
        return False
    link.note('Dossier de la carte : /glycemie/audio/voice/%s%s' % (lang, ' (lecture seule)' if lecture else ''))
    return True


def verify(link, entries, lang='fr'):
    """Verification en LECTURE SEULE : seule la commande vrx_list est envoyee."""
    link.send('vrx_list crc')
    files = {}
    end = time.time() + 300
    summary = None
    while time.time() < end:
        text = link.line(end - time.time())
        if text is None:
            break
        m = re.match(r'VRX FILE (\S+) (-?\d+) ?(\S*)', text)
        if m:
            files[m.group(1)] = (int(m.group(2)), m.group(3))
            continue
        m = re.match(r'VRX LIST (?:dossier=(\S+) )?fichiers=(\d+) octets=(\d+) libre_mo=(\d+)', text)
        if m:
            summary = m
            break
        if text.startswith('VRX LIST ERR'):
            break
    if summary is None:
        link.note('VERIFICATION : liste de la carte incomplete')
        return False
    dossier = summary.group(1)
    attendu = 'racine' if lang == 'fr' else lang
    if dossier is None:
        if lang != 'fr':
            link.note('VERIFICATION impossible : ce firmware liste toujours la racine (pas de "dossier=") ; '
                      'aucune ecriture n\'a ete faite. Mettre le firmware a jour pour verifier %s.' % lang)
            return False
        link.note('VERIFICATION : firmware sans "dossier=" ; les sous-dossiers de langue sont ignores dans la liste')
    elif dossier != attendu:
        link.note('VERIFICATION : la carte a liste "%s" au lieu de "%s"' % (dossier, attendu))
        return False
    # anciens firmwares : les sous-dossiers de langue apparaissent dans la liste (taille -1) ; ce ne sont pas des WAV
    files = {n: v for n, v in files.items() if not (v[0] < 0 and not n.lower().endswith('.wav'))}
    expected = {name: (size, crc) for name, size, crc in entries}
    missing = sorted(set(expected) - set(files))
    extra = sorted(set(files) - set(expected))
    bad_size = sorted(n for n in expected if n in files and files[n][0] != expected[n][0])
    bad_crc = sorted(n for n in expected if n in files and files[n][1] != expected[n][1])
    total = sum(size for size, _ in files.values())
    link.note('VERIFICATION carte : %d fichier(s) (attendus %d), %d octets (attendus %d), %s Mo libres'
              % (len(files), len(expected), total, sum(s for s, _ in expected.values()), summary.group(4)))
    link.note('VERIFICATION manquants=%d en_trop=%d tailles_differentes=%d crc32_differents=%d%s'
              % (len(missing), len(extra), len(bad_size), len(bad_crc),
                 '' if not (missing or extra or bad_size or bad_crc) else ' : ' +
                 ', '.join((missing + extra + bad_size + bad_crc)[:10])))
    return not (missing or extra or bad_size or bad_crc) and len(files) == len(expected)


LINK = Link  # remplacable par les tests (transport simule, aucune liaison serie)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='COM7')
    ap.add_argument('--log', required=True)
    ap.add_argument('--verify-only', action='store_true')
    ap.add_argument('--lang', default='fr', choices=['fr', 'en', 'es', 'de', 'it', 'pt'],
                    help='langue de la banque a copier (fr par defaut : racine de la carte)')
    args = ap.parse_args(argv)

    configure(args.lang)
    entries = load_manifest()
    link = LINK(args.port, args.log)
    ok = False
    try:
        link.note('Banque locale : %d fichiers, %d octets (manifeste verifie)' % (len(entries),
                                                                              sum(e[1] for e in entries)))
        baud = begin(link)
        if not choisir_dossier(link, args.lang, args.verify_only):
            link.send('vrx_end')
            link.expect(r'VRX END', 5)
            link.set_baud(SLOW_BAUD)
            link.note('Session terminee, console a %d bauds ; resultat : ECHEC (aucune ecriture)' % SLOW_BAUD)
            return 1
        if not args.verify_only:
            t0 = time.time()
            sent = skipped = sent_bytes = 0
            for index, (name, size, crc) in enumerate(entries, 1):
                data = open(os.path.join(BANK, name), 'rb').read()
                for attempt in range(3):
                    result = put_file(link, name, size, crc, data)
                    if result in ('OK', 'SKIP'):
                        break
                    link.note('%s : %s, nouvel essai' % (name, result))
                    time.sleep(1.0)
                    link.ser.reset_input_buffer()
                    link.buffer = b''
                    link.send('')
                else:
                    raise SystemExit('%s : copie impossible (%s)' % (name, result))
                if result == 'OK':
                    sent += 1
                    sent_bytes += size
                else:
                    skipped += 1
                if index % 25 == 0 or index == len(entries):
                    elapsed = max(time.time() - t0, 0.001)
                    link.note('%d/%d fichiers (%d copies, %d deja presents), %.0f Ko/s a %d bauds'
                              % (index, len(entries), sent, skipped, sent_bytes / 1024 / elapsed, baud))
            link.note('Copie terminee en %.0f s : %d copies, %d deja presents' % (time.time() - t0, sent, skipped))
        ok = verify(link, entries, args.lang)
        link.send('vrx_end')
        link.expect(r'VRX END', 5)
        link.set_baud(SLOW_BAUD)
        link.note('Session terminee, console a %d bauds ; resultat : %s' % (SLOW_BAUD, 'CONFORME' if ok else 'ECHEC'))
    finally:
        link.close()
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
