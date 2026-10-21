#!/usr/bin/env python3
# Captures serial diagnostics while redacting known secrets.
# FR: Récupère les diagnostics série en masquant les secrets connus.
# Design note: Logs may contain health data; keep them out of the public repository.
# EN: Captures serial diagnostics while redacting known credentials and personal details.
# FR: Capture les diagnostics série en masquant les données privées.
"""Capture du journal serie de l'ESP32-P4 avec masquage des secrets.

Aucune ecriture en flash : ouverture du port, reset optionnel par RTS (comme esptool hard_reset),
lecture, commandes console optionnelles ("info", "snap").
Les valeurs de private/device_secrets.h et tout UUID (session, compte) sont masques
avant affichage et avant ecriture du journal.
"""
import argparse
import base64
import datetime
import os
import re
import struct
import sys
import time
import zlib

import serial

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SECRETS = os.path.join(ROOT, 'private', 'device_secrets.h')
UUID_RE = re.compile(r'[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}')
ANSI_RE = re.compile(r'\x1b\[[0-9;]*m')


def load_secrets():
    values = []
    if os.path.exists(SECRETS):
        text = open(SECRETS, encoding='utf-8').read()
        for m in re.finditer(r'#define\s+(GLY_\w+)\s+"((?:[^"\\]|\\.)*)"', text):
            if m.group(1) != 'GLY_DEXCOM_REGION' and len(m.group(2)) >= 3:
                values.append(re.sub(r'\\(.)', r'\1', m.group(2)))
    return sorted(set(values), key=len, reverse=True)


# Mask known secrets before a log line reaches disk or console output.
def redact(line, secrets):
    for s in secrets:
        line = line.replace(s, '[SECRET]')
    return UUID_RE.sub('[UUID]', line)


def write_png(path, width, height, rgb):
    rows = b''.join(b'\x00' + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(rows, 6)))
        f.write(chunk(b'IEND', b''))


# Decode a board screenshot separately from text logs.
def decode_snapshot(width, height, b64_lines):
    data = base64.b64decode(''.join(b64_lines))
    rgb = bytearray()
    for i in range(0, len(data) - 3, 4):
        count, color = struct.unpack_from('<HH', data, i)
        r = (color >> 11) & 0x1F
        g = (color >> 5) & 0x3F
        b = color & 0x1F
        rgb += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))) * count
    expected = width * height * 3
    if len(rgb) < expected:
        rgb += bytes(expected - len(rgb))
    return bytes(rgb[:expected])


# Open the port conservatively and redact every captured diagnostic line.
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default='COM7')
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('--duration', type=float, default=30.0, help='duree maximale de capture (s)')
    ap.add_argument('--reset', action='store_true', help='redemarrer la carte (RTS) au debut')
    ap.add_argument('--until', default=None, help='arreter apres une ligne correspondant a cette regex')
    ap.add_argument('--after-until', type=float, default=2.0, help='secondes lues apres --until')
    ap.add_argument('--send', action='append', default=[], help='commande console "DELAI:commande", ex. 20:snap')
    ap.add_argument('--trigger', action='append', default=[],
                    help='"REGEX|DELAI:commande|DELAI:commande" : commandes envoyees apres la 1re ligne correspondante')
    ap.add_argument('--log', required=True, help='fichier journal (masque)')
    ap.add_argument('--png', default=None, help='fichier PNG pour la capture d\'ecran')
    ap.add_argument('--quiet', action='store_true', help='ne pas recopier le journal sur la sortie')
    args = ap.parse_args()

    secrets = load_secrets()
    sends = []
    for item in args.send:
        delay, _, cmd = item.partition(':')
        sends.append([float(delay), cmd, False])
    triggers = []
    for item in args.trigger:
        parts = item.split('|')
        steps = []
        for step in parts[1:]:
            delay, _, cmd = step.partition(':')
            steps.append((float(delay), cmd))
        triggers.append([re.compile(parts[0]), steps, False])

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = args.baud
    ser.timeout = 0.2
    ser.dtr = False
    ser.rts = False
    ser.open()

    if args.reset:
        ser.dtr = False
        ser.rts = True
        ser.dtr = ser.dtr
        time.sleep(0.2)
        ser.rts = False
        ser.dtr = ser.dtr

    start = time.time()
    stop_at = start + args.duration
    buffer = b''
    snap = None
    snap_lines = []
    snap_count = 0
    out = open(args.log, 'w', encoding='utf-8')
    try:
        while time.time() < stop_at:
            now = time.time() - start
            for s in sends:
                if not s[2] and now >= s[0]:
                    ser.write((s[1] + '\n').encode())
                    s[2] = True
            chunk = ser.read(4096)
            if not chunk:
                continue
            buffer += chunk
            while b'\n' in buffer:
                raw, buffer = buffer.split(b'\n', 1)
                line = ANSI_RE.sub('', raw.decode('utf-8', errors='replace').rstrip('\r'))
                if line.startswith('SNAP_BEGIN'):
                    parts = line.split()
                    snap = (int(parts[1]), int(parts[2]))
                    snap_lines = []
                    continue
                if snap is not None and line.startswith('S:'):
                    snap_lines.append(line[2:].strip())
                    continue
                if line.startswith('SNAP_END') and snap is not None:
                    if args.png:
                        snap_count += 1
                        base, ext = os.path.splitext(args.png)
                        path = args.png if snap_count == 1 else '%s_%d%s' % (base, snap_count, ext)
                        write_png(path, snap[0], snap[1], decode_snapshot(snap[0], snap[1], snap_lines))
                        line = 'SNAP_END -> %s (%d lignes base64)' % (path, len(snap_lines))
                    snap = None
                safe = redact(line, secrets)
                stamp = '%7.2f ' % (time.time() - start)
                out.write(stamp + safe + '\n')
                out.flush()
                if not args.quiet:
                    print(stamp + safe)
                if args.until and re.search(args.until, safe):
                    stop_at = min(stop_at, time.time() + args.after_until)
                for trig in triggers:
                    if not trig[2] and trig[0].search(safe):
                        trig[2] = True
                        base_t = time.time() - start
                        for delay, cmd in trig[1]:
                            sends.append([base_t + delay, cmd, False])
    finally:
        out.close()
        ser.close()


if __name__ == '__main__':
    main()
