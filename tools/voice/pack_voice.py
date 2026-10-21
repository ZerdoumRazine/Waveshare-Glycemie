#!/usr/bin/env python3
# EN: Processes a legacy Windows voice into short PCM clips for local use only.
# FR: Prépare les anciens extraits PCM Windows sans autoriser leur redistribution.
"""Assemble les extraits vocaux WAV en un bloc PCM integre au firmware.

- rogne les silences de debut et de fin (marge 30 ms) ;
- normalise le niveau : meme niveau efficace pendant la parole pour tous les extraits (TARGET_RMS_DB),
  crete limitee a CEILING_DB par un limiteur a anticipation (gain lisse, jamais d'ecretage) ;
- ecrit firmware/glycemie_display/main/voice/voice_fr.pcm (int16 little-endian, mono, 16 kHz) ;
- genere firmware/glycemie_display/main/voice_clips.h (index des extraits).
"""
import array
import collections
import math
import os
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
WAV_DIR = os.path.join(HERE, 'wav')
MAIN = os.path.join(ROOT, 'main')
OUT_PCM = os.path.join(MAIN, 'voice', 'voice_fr.pcm')
OUT_H = os.path.join(MAIN, 'voice_clips.h')

ORDER = [
    'p_tres_basse', 'p_basse', 'p_elevee', 'p_tres_elevee', 'p_aucune_donnee', 'p_minutes',
    'p_capteur_48h', 'p_capteur_24h', 'p_capteur_12h', 'p_capteur_grace', 'p_capteur_2h', 'p_test',
    'p_glycemie', 'p_mg_dl',
    'w_un', 'w_deux', 'w_trois', 'w_quatre', 'w_cinq', 'w_six', 'w_sept', 'w_huit', 'w_neuf', 'w_dix',
    'w_onze', 'w_douze', 'w_treize', 'w_quatorze', 'w_quinze', 'w_seize', 'w_vingt', 'w_trente',
    'w_quarante', 'w_cinquante', 'w_soixante', 'w_quatre_vingt', 'w_quatre_vingts', 'w_cent', 'w_cents', 'w_et',
]
RATE = 16000
THRESHOLD = 250
MARGIN = int(0.03 * RATE)

TARGET_RMS_DB = -12.0              # niveau efficace vise sur les fenetres de parole (20 ms)
ACTIVE_DB = -40.0                  # fenetre consideree comme parole au-dessus de ce niveau
CEILING_DB = -1.5                  # crete maximale apres limitation (marge avant la pleine echelle)
MAX_REDUCTION_DB = 6.0             # limitation maximale : au-dela, le gain de l'extrait est reduit (voix naturelle)
LOOKAHEAD = int(0.008 * RATE)      # anticipation et lissage du gain : 8 ms
RELEASE_S = 0.100                  # retour du gain : 100 ms
WINDOW = int(0.020 * RATE)


def db(value):
    return 20.0 * math.log10(max(value, 1e-9))


# Read the source audio in the expected PCM format.
def load(path):
    with wave.open(path, 'rb') as w:
        if w.getframerate() != RATE or w.getsampwidth() != 2 or w.getnchannels() != 1:
            raise SystemExit('%s : format inattendu' % path)
        data = array.array('h')
        data.frombytes(w.readframes(w.getnframes()))
        if sys.byteorder != 'little':
            data.byteswap()
        return data


# Remove leading and trailing silence without changing the spoken text.
def trim(samples):
    start = 0
    end = len(samples)
    while start < end and abs(samples[start]) < THRESHOLD:
        start += 1
    while end > start and abs(samples[end - 1]) < THRESHOLD:
        end -= 1
    return samples[max(0, start - MARGIN):min(len(samples), end + MARGIN)]


def active_rms(x):
    """Valeur efficace sur les fenetres de 20 ms au-dessus de ACTIVE_DB (pleine echelle = 1.0)."""
    floor = 10.0 ** (ACTIVE_DB / 10.0)
    energies = []
    for i in range(0, max(1, len(x) - WINDOW + 1), WINDOW):
        chunk = x[i:i + WINDOW]
        energy = sum(v * v for v in chunk) / len(chunk)
        if energy > floor:
            energies.append(energy)
    return math.sqrt(sum(energies) / len(energies)) if energies else 0.0


# Control peaks before embedding clips in a personal firmware build.
def limit(x):
    """Limiteur a anticipation : |sortie| <= plafond sans ecretage.

    gain requis r[i] = min(1, plafond/|x[i]|) ; minimum glissant sur [i-L, i+L], puis moyenne glissante de
    meme largeur : chaque valeur moyennee est <= r[i], donc la crete ne depasse jamais le plafond.
    Le retour du gain est ensuite ralenti (RELEASE_S) sans jamais depasser le gain lisse.
    """
    ceiling = 10.0 ** (CEILING_DB / 20.0)
    n = len(x)
    req = [ceiling / abs(v) if abs(v) > ceiling else 1.0 for v in x]
    L = LOOKAHEAD
    gmin = [1.0] * n
    window = collections.deque()  # indices, gains croissants
    for i in range(n + L):
        j = i
        if j < n:
            while window and req[window[-1]] >= req[j]:
                window.pop()
            window.append(j)
        centre = i - L
        while window and window[0] < centre - L:
            window.popleft()
        if 0 <= centre < n:
            gmin[centre] = req[window[0]]
    prefix = [0.0]
    for g in gmin:
        prefix.append(prefix[-1] + g)
    smooth = [(prefix[min(n, i + L + 1)] - prefix[max(0, i - L)]) / (min(n, i + L + 1) - max(0, i - L))
              for i in range(n)]
    release = 1.0 - math.exp(-1.0 / (RELEASE_S * RATE))
    out = [0.0] * n
    gain = 1.0
    min_gain = 1.0
    for i in range(n):
        gain = min(smooth[i], gain + (1.0 - gain) * release)
        min_gain = min(min_gain, gain)
        out[i] = x[i] * gain
    return out, min_gain


def process(samples):
    clip = trim(samples)
    x = [s / 32768.0 for s in clip]
    rms_in = active_rms(x)
    gain = 10.0 ** ((TARGET_RMS_DB - db(rms_in)) / 20.0) if rms_in > 0 else 1.0
    peak_in = max((abs(v) for v in x), default=0.0)
    if peak_in > 0:
        gain = min(gain, 10.0 ** ((CEILING_DB + MAX_REDUCTION_DB - db(peak_in)) / 20.0))
    x = [v * gain for v in x]
    peak_before = max((abs(v) for v in x), default=0.0)
    y, min_gain = limit(x)
    out = array.array('h', (max(-32768, min(32767, int(round(v * 32767.0)))) for v in y))
    stats = {
        'rms_in': db(rms_in),
        'gain': db(gain),
        'peak_before': db(peak_before),
        'reduction': -db(min_gain),
        'peak_out': db(max((abs(s) for s in out), default=1) / 32768.0),
        'rms_out': db(active_rms([s / 32768.0 for s in out])),
        'clipped': sum(1 for s in out if s in (32767, -32768)),
    }
    return out, stats


# Generate a private legacy bank; these recordings are not part of public releases.
def main():
    os.makedirs(os.path.dirname(OUT_PCM), exist_ok=True)
    blob = array.array('h')
    index = []
    print('%-16s %6s %8s %9s %9s %9s %8s %7s %6s' % ('extrait', 'duree', 'RMS_in', 'gain_dB', 'crete_av',
                                                    'limit_dB', 'crete', 'RMS', 'ecret'))
    worst = 0.0
    for name in ORDER:
        clip, st = process(load(os.path.join(WAV_DIR, name + '.wav')))
        index.append((name, len(blob), len(clip)))
        blob.extend(clip)
        worst = max(worst, st['reduction'])
        print('%-16s %5.2fs %8.1f %+9.1f %+9.1f %9.1f %8.1f %7.1f %6d' % (
            name, len(clip) / RATE, st['rms_in'], st['gain'], st['peak_before'], st['reduction'], st['peak_out'],
            st['rms_out'], st['clipped']))
    if sys.byteorder != 'little':
        blob.byteswap()
    with open(OUT_PCM, 'wb') as f:
        f.write(blob.tobytes())

    lines = [
        '/* Genere par tools/voice/pack_voice.py a partir des extraits WAV (voix Windows fr-FR) - ne pas modifier */',
        '#pragma once',
        '',
        '#include <stdint.h>',
        '',
        'typedef enum {',
    ]
    lines += ['    VOICE_%s,' % name.upper() for name, _, _ in index]
    lines += ['    VOICE_CLIP_COUNT,', '} voice_clip_id_t;', '',
              'typedef struct {', '    uint32_t offset;  /* en echantillons */', '    uint32_t samples;',
              '} voice_clip_t;', '',
              'static const voice_clip_t k_voice_clips[VOICE_CLIP_COUNT] = {']
    lines += ['    {%d, %d}, /* %s */' % (off, n, name) for name, off, n in index]
    lines += ['};', '']
    with open(OUT_H, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines))

    total = len(blob)
    print('%d extraits, %.1f s, %d Ko ; niveau vise %.1f dBFS efficace, crete <= %.1f dBFS, limitation max %.1f dB'
          % (len(index), total / RATE, total * 2 // 1024, TARGET_RMS_DB, CEILING_DB, worst))


if __name__ == '__main__':
    main()
