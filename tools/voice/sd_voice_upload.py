#!/usr/bin/env python3
# Copies speech files to the microSD over USB serial.
#
# Design note: Verification-only mode must not create or overwrite files.
# Copies existing WAV banks over USB with CRC32 checks and a read-only verification mode.
#
"""Copy the voice bank (tools/voice/bank) on the Waveshare microSD by the cable USB-Series, without removing the
map and no network. Temporary tool: firmware vrx * console commands (main/voice_rx.c).

- temporary debit 921600 bauds (automatic return of the card to 115200; withdrawal to 115200 if the change failed);
- each file: 896 bytes blocks acquit one by one (no card stamp overflow), CRC32 verifies
  by the memory card then after rereading on the microSD; file already present and identical: jump;
- Final verification: complete list reread on the map with CRC32, compares to the manifest (accurate number, sizes,
  missing or surplus files, integrite) and free space. Verification does not send QUE read commands
  (vrx_dir ... reading, vrx_list crc) : no file byte is transmitted, even if the card is not compliant.
The port is open without acting on DTR/RTS The card doesn't start again.

Usage (environnement du projet, pyserial) :
  .venv\\Scripts\\python.exe -B tools\\voice\\sd_voice_upload.py --port COM7 --log tools\\logs\\NNN_copie.log
  ... --verify-only verification alone: no writing, no folder created (needs firmware that
                       answer "VRX LIST dossier=..." for languages other than French)
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


# Select either a developer bank or the public ZIP extracted to a folder.
def configure(lang, sd_root=None):
    """Bank from working files or from the ZIP microSD extracted.

    --sd-root points to a folder containing glycemie/audio/voice, for
    Copy the telecharges directly without restarting a Google generation.
    """
    global BANK, MANIFEST
    if sd_root is not None:
        root = os.path.abspath(sd_root)
        base = os.path.join(root, 'glycemie', 'audio', 'voice')
        BANK = base if lang == 'fr' else os.path.join(base, lang)
        MANIFEST = None  # CRC32, name and size calculated on the WAV reels
    else:
        base = os.path.join(HERE, 'bank') if lang == 'fr' else os.path.join(HERE, 'bank', lang)
        BANK = os.path.join(base, 'voice')
        MANIFEST = os.path.join(base, 'manifest.csv')
    if not os.path.isdir(BANK):
        raise SystemExit('Banque vocale absente : %s' % BANK)
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
        """Next line of the map (mask), None after timeout seconds."""
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
            # at least 1 byte (makes the hand of the sound arrive), then everything that is already received: minimum latency
            waiting = self.ser.in_waiting
            chunk = self.ser.read(waiting if waiting else 1)
            if chunk:
                self.buffer += chunk

    def expect(self, pattern, timeout):
        """First line VRX a pattern correspondent (regex); Other lines (journal) are not known."""
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


# Use the reviewed manifest or calculate CRC32 from the extracted WAVs.
def load_manifest():
    entries = []
    if MANIFEST is None:
        # ZIP public: no need d'un compte Google ou d'A private protest.
        # The five language sub-folders in the French bank are
        # not files and should never be copies as such.
        names = sorted(n for n in os.listdir(BANK) if n.lower().endswith('.wav')
                       and os.path.isfile(os.path.join(BANK, n)))
        if len(names) != 396:
            raise SystemExit('Banque incomplète (%d WAV sur 396) : %s' % (len(names), BANK))
        for name in names:
            if not re.fullmatch(r'[A-Za-z0-9_-]+\.wav', name):
                raise SystemExit('Nom WAV non autorise : %s' % name)
            data = open(os.path.join(BANK, name), 'rb').read()
            entries.append((name, len(data), '%08x' % (zlib.crc32(data) & 0xffffffff)))
    else:
        with open(MANIFEST, encoding='utf-8-sig', newline='') as f:
            for row in csv.DictReader(f, delimiter=';'):
                entries.append((row['fichier'], int(row['taille_octets']), row['crc32'].lower()))
    for name, size, crc in entries:
        data = open(os.path.join(BANK, name), 'rb').read()
        if len(data) != size or '%08x' % (zlib.crc32(data) & 0xFFFFFFFF) != crc:
            raise SystemExit('Banque locale incoherente avec le manifeste : %s' % name)
    return entries


# Negotiate the serial session without erasing the card.
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


# Send each file in bounded chunks and wait for device acknowledgement.
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
    """vrx_dir for a language. read=True: the map must not RIEN create (day firmware requires)."""
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


# Compare names, sizes and CRCs through read-only device commands.
def verify(link, entries, lang='fr'):
    """Verification in LECTURE SEULE : only the command vrx_list is sent."""
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
    # old firmwares: language subfolders appear in the list (size -1); They are not WAV
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


LINK = Link  # replaceable by tests (simultaneous transport, no serial link)


# Never write in --verify-only mode, even when the remote bank is incomplete.
def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='COM7')
    ap.add_argument('--log', required=True)
    ap.add_argument('--verify-only', action='store_true')
    ap.add_argument('--lang', default='fr', choices=['fr', 'en', 'es', 'de', 'it', 'pt'],
                    help='langue de la banque a copier (fr par defaut : racine de la carte)')
    ap.add_argument('--sd-root', default=None, help='dossier extrait du ZIP microSD contenant glycemie/')
    args = ap.parse_args(argv)

    configure(args.lang, args.sd_root)
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
