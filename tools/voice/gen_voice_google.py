#!/usr/bin/env python3
# Generates optional language recordings with Google Cloud TTS.
#
# Design note: Generation may incur API fees; avoid running it accidentally.
# Builds spoken WAV banks locally from project phrases using Google Cloud TTS.
#
"""Banque vocale Google Cloud Text-to-Speech pour glycemie_display, en six langues.

Meme timbre partout (Chirp 3 HD "Aoede") : francais, anglais, espagnol, allemand, italien,
portugais (voix pt-BR, Google ne propose pas de Chirp 3 HD en pt-PT). Les textes et les nombres
en lettres sont dans langues_voix.py.

Generation UNE SEULE FOIS sur le PC avec l'API officielle (bibliotheque google-cloud-texttospeech) et les
identifiants par defaut deja configures par gcloud (ADC) : ils ne sont jamais lus, copies ni affiches par ce
script. Les WAV produits sont ensuite copies sur la microSD (/glycemie/audio/voice/) et lus localement par
la carte : aucune connexion Internet ni appel Google depuis l'ESP32.

Usage (environnement isole tools\\voice\\.venv_tts) :
  python -B gen_voice_google.py --lang en --estimate   phrases, caracteres envoyes, cout (aucune requete)
  python -B gen_voice_google.py --lang en --check      acces a l'API et presence de la voix
  python -B gen_voice_google.py --lang en [--only a,b] generation des manquants, conversion, manifeste

Sortie : francais dans bank/voice (inchange, la banque deja installee reste valable), les autres
langues dans bank/<code>/voice. Sur la microSD : /glycemie/audio/voice pour le francais,
/glycemie/audio/voice/<code> pour les autres.

Chaine audio :
  Google LINEAR16 (frequence native de la voix, 24 kHz) -> cache google_raw/ (jamais redemande si valide)
  -> reechantillonnage polyphase 16 kHz (filtre Kaiser, -80 dB au-dela de 8 kHz)
  -> silences de debut/fin rognes -> niveau identique a l'ancienne banque (-12 dBFS efficace pendant la parole,
     crete <= -1,5 dBFS par limiteur a anticipation, limitation <= 6 dB, aucun echantillon ecrete)
  -> bank/voice/<nom>.wav (PCM 16 bits, mono, 16 kHz) + bank/manifest.csv (nom ; texte ; duree ; taille ; crc32)
"""
import argparse
import csv
import io
import math
import os
import sys
import time
import wave
import zlib

import numpy as np

import langues_voix

LANG = 'fr'                        # fixe par configure(), voir main()
VOICE_NAME = langues_voix.VOIX['fr'][1]
LANGUAGE = langues_voix.VOIX['fr'][0]
PRICE_PER_MILLION_USD = 30.0       # Chirp 3 HD (tarif public), 1 million de caracteres gratuits par mois

HERE = os.path.dirname(os.path.abspath(__file__))
RAW_DIR = os.path.join(HERE, 'google_raw')
BANK_DIR = os.path.join(HERE, 'bank')
OUT_DIR = os.path.join(BANK_DIR, 'voice')
MANIFEST = os.path.join(BANK_DIR, 'manifest.csv')


def configure(lang):
    """Choisit la voix et les dossiers de la langue demandee.

    Le francais garde exactement les chemins d'origine (bank/voice, bank/manifest.csv,
    google_raw) : la banque deja generee et deja copiee sur la microSD reste utilisable telle
    quelle. Les autres langues vivent dans un sous-dossier portant leur code."""
    global LANG, VOICE_NAME, LANGUAGE, RAW_DIR, BANK_DIR, OUT_DIR, MANIFEST
    LANGUAGE, VOICE_NAME = langues_voix.VOIX[lang]
    LANG = lang
    if lang == 'fr':
        RAW_DIR = os.path.join(HERE, 'google_raw')
        BANK_DIR = os.path.join(HERE, 'bank')
    else:
        RAW_DIR = os.path.join(HERE, 'google_raw', lang)
        BANK_DIR = os.path.join(HERE, 'bank', lang)
    OUT_DIR = os.path.join(BANK_DIR, 'voice')
    MANIFEST = os.path.join(BANK_DIR, 'manifest.csv')

RATE = 16000                       # frequence du lecteur audio existant (ES8311, AUDIO_SAMPLE_RATE)
LEAD_S = 0.030                     # margin retained before speech
TAIL_S = 0.060                     # margin retained after speech
SILENCE_DB = -50.0                 # trame de 10 ms consideree comme silence en dessous de ce niveau
FADE_S = 0.005
TARGET_RMS_DB = -12.0              # memes cibles que tools/voice/pack_voice.py (niveau deja valide sur la carte)
ACTIVE_DB = -40.0
CEILING_DB = -1.5
MAX_REDUCTION_DB = 6.0
LOOKAHEAD_S = 0.008
RELEASE_S = 0.100
MIN_DURATION_S = 0.4
MAX_DURATION_S = 6.0

# Generate the fixed alerts and number phrases for the selected language.
def phrases():
    """(nom de fichier, texte) dans l'ordre de generation, pour la langue configuree."""
    return langues_voix.phrases(LANG)


# ----------------------------------------------------------------------------------------------------------------
# WAV


def read_wav_bytes(data):
    """(echantillons int16 mono, frequence) ou None si le WAV n'est pas du PCM 16 bits mono lisible."""
    try:
        with wave.open(io.BytesIO(data), 'rb') as w:
            if w.getsampwidth() != 2 or w.getnchannels() != 1 or w.getcomptype() != 'NONE':
                return None
            frames = w.readframes(w.getnframes())
            rate = w.getframerate()
    except (wave.Error, EOFError):
        return None
    samples = np.frombuffer(frames, dtype='<i2').astype(np.int16)
    return (samples, rate) if len(samples) > 0 else None


def read_wav_file(path):
    try:
        with open(path, 'rb') as f:
            return read_wav_bytes(f.read())
    except OSError:
        return None


def wav_bytes(samples, rate):
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(samples.astype('<i2').tobytes())
    return buf.getvalue()


# ----------------------------------------------------------------------------------------------------------------
# Traitement


def db(value):
    return 20.0 * math.log10(max(float(value), 1e-9))


# Convert the service output to the sample rate required by the ESP32 codec.
def resample(x, rate_in, rate_out):
    """Reechantillonnage polyphase (insertion de zeros, filtre passe-bas Kaiser, decimation)."""
    if rate_in == rate_out:
        return x.astype(np.float64)
    g = math.gcd(rate_in, rate_out)
    up, down = rate_out // g, rate_in // g
    fs_up = rate_in * up
    nyquist_out = min(rate_in, rate_out) / 2.0
    transition = 0.1 * nyquist_out                   # 7.2 -> 8 kHz for a 16 kHz output
    cutoff = nyquist_out - transition / 2.0
    attenuation = 80.0
    beta = 0.1102 * (attenuation - 8.7)
    taps = int(math.ceil((attenuation - 8.0) / (2.285 * 2.0 * math.pi * transition / fs_up))) | 1
    n = np.arange(taps) - (taps - 1) / 2.0
    h = (2.0 * cutoff / fs_up) * np.sinc(2.0 * cutoff / fs_up * n) * np.kaiser(taps, beta) * up
    stuffed = np.zeros(len(x) * up)
    stuffed[::up] = x
    delay = (taps - 1) // 2
    filtered = np.convolve(stuffed, h)[delay:delay + len(stuffed)]
    return filtered[::down]


def trim(x, rate):
    frame = int(0.010 * rate)
    count = len(x) // frame
    if count == 0:
        return x
    rms = np.sqrt(np.mean(x[:count * frame].reshape(count, frame) ** 2, axis=1))
    active = np.nonzero(rms > 10.0 ** (SILENCE_DB / 20.0))[0]
    if len(active) == 0:
        return x
    start = max(0, active[0] * frame - int(LEAD_S * rate))
    end = min(len(x), (active[-1] + 1) * frame + int(TAIL_S * rate))
    y = x[start:end].copy()
    fade = min(int(FADE_S * rate), len(y) // 2)
    if fade > 0:
        ramp = 0.5 - 0.5 * np.cos(np.linspace(0.0, math.pi, fade))
        y[:fade] *= ramp
        y[-fade:] *= ramp[::-1]
    return y


def active_rms(x, rate):
    window = int(0.020 * rate)
    count = len(x) // window
    if count == 0:
        return float(np.sqrt(np.mean(x ** 2))) if len(x) else 0.0
    energy = np.mean(x[:count * window].reshape(count, window) ** 2, axis=1)
    energy = energy[energy > 10.0 ** (ACTIVE_DB / 10.0)]
    return float(np.sqrt(np.mean(energy))) if len(energy) else 0.0


# Keep decoded audio in a safe amplitude range to reduce clipping.
def limit(x, rate):
    """Limiteur a anticipation (meme principe que pack_voice.py) : |sortie| <= plafond, jamais d'ecretage.

    gain requis r = min(1, plafond/|x|) ; minimum glissant sur [i-L, i+L] puis moyenne glissante de meme largeur
    (chaque valeur moyennee est <= r[i]) ; retour du gain ralenti (RELEASE_S) sans depasser le gain lisse.
    """
    ceiling = 10.0 ** (CEILING_DB / 20.0)
    n = len(x)
    L = int(LOOKAHEAD_S * rate)
    magnitude = np.abs(x)
    req = np.where(magnitude > ceiling, ceiling / np.maximum(magnitude, 1e-12), 1.0)
    padded = np.pad(req, L, constant_values=1.0)
    gmin = np.lib.stride_tricks.sliding_window_view(padded, 2 * L + 1).min(axis=1)
    prefix = np.concatenate(([0.0], np.cumsum(gmin)))
    idx = np.arange(n)
    lo = np.maximum(0, idx - L)
    hi = np.minimum(n, idx + L + 1)
    smooth = (prefix[hi] - prefix[lo]) / (hi - lo)
    release = 1.0 - math.exp(-1.0 / (RELEASE_S * rate))
    gain = np.empty(n)
    g = 1.0
    for i, s in enumerate(smooth.tolist()):
        g = min(s, g + (1.0 - g) * release)
        gain[i] = g
    return x * gain, float(gain.min()) if n else 1.0


def process(raw, raw_rate):
    x = resample(raw.astype(np.float64) / 32768.0, raw_rate, RATE)
    x = trim(x, RATE)
    rms_in = active_rms(x, RATE)
    gain = 10.0 ** ((TARGET_RMS_DB - db(rms_in)) / 20.0) if rms_in > 0 else 1.0
    peak_in = float(np.max(np.abs(x))) if len(x) else 0.0
    if peak_in > 0:
        gain = min(gain, 10.0 ** ((CEILING_DB + MAX_REDUCTION_DB - db(peak_in)) / 20.0))
    y, min_gain = limit(x * gain, RATE)
    out = np.clip(np.round(y * 32767.0), -32767, 32767).astype(np.int16)
    stats = {
        'gain_db': db(gain),
        'reduction_db': -db(min_gain),
        'peak_dbfs': db(np.max(np.abs(out.astype(np.int32))) / 32768.0),
        'rms_dbfs': db(active_rms(out.astype(np.float64) / 32768.0, RATE)),
        'clipped': int(np.count_nonzero(np.abs(out.astype(np.int32)) >= 32767)),
    }
    return out, stats


# ----------------------------------------------------------------------------------------------------------------
# Google Cloud


def make_client():
    from google.cloud import texttospeech  # import difference: --estimate works without network
    return texttospeech, texttospeech.TextToSpeechClient()


def synthesize(tts, client, text):
    request = {
        'input': tts.SynthesisInput(text=text),
        'voice': tts.VoiceSelectionParams(language_code=LANGUAGE, name=VOICE_NAME),
        'audio_config': tts.AudioConfig(audio_encoding=tts.AudioEncoding.LINEAR16),
    }
    from google.api_core import exceptions
    delay = 5.0
    for attempt in range(6):
        try:
            return client.synthesize_speech(**request, timeout=60).audio_content
        except (exceptions.ResourceExhausted, exceptions.ServiceUnavailable, exceptions.DeadlineExceeded,
                exceptions.InternalServerError) as err:
            if attempt == 5:
                raise
            print('  nouvel essai dans %.0f s (%s)' % (delay, type(err).__name__))
            time.sleep(delay)
            delay = min(delay * 2, 60.0)
    return None


# Reuse the local cache only when the stored voice and text still match.
def raw_is_valid(name, text):
    base = os.path.join(RAW_DIR, os.path.splitext(name)[0])
    try:
        with open(base + '.txt', encoding='utf-8') as f:
            if f.read() != VOICE_NAME + '\n' + text:
                return None
    except OSError:
        return None
    decoded = read_wav_file(base + '.wav')
    if decoded is None or len(decoded[0]) < MIN_DURATION_S * decoded[1]:
        return None
    return decoded


def save_raw(name, text, data):
    base = os.path.join(RAW_DIR, os.path.splitext(name)[0])
    with open(base + '.wav', 'wb') as f:
        f.write(data)
    with open(base + '.txt', 'w', encoding='utf-8', newline='\n') as f:
        f.write(VOICE_NAME + '\n' + text)


# ----------------------------------------------------------------------------------------------------------------


# Calculate likely billable text length without contacting Google.
def estimate(items):
    chars = sum(len(text) for _, text in items)
    print('Langue : %s ; voix : %s' % (LANG, VOICE_NAME))
    print('Phrases : %d ; caracteres envoyes a Google Cloud : %d (moyenne %.1f)' % (len(items), chars,
                                                                                  chars / len(items)))
    print('Cout indicatif : %.2f USD au tarif Chirp 3 HD (%.0f USD / million), 1 million gratuit par mois'
          % (chars * PRICE_PER_MILLION_USD / 1e6, PRICE_PER_MILLION_USD))
    for name, text in items[:14] + items[-3:]:
        print('  %-22s %s' % (name, text))
    return chars


def check():
    tts, client = make_client()
    voices = client.list_voices(language_code=LANGUAGE).voices
    match = [v for v in voices if v.name == VOICE_NAME]
    if not match:
        print('VOIX INTROUVABLE : %s (%d voix %s disponibles)' % (VOICE_NAME, len(voices), LANGUAGE))
        return False
    v = match[0]
    print('Acces API OK ; voix %s disponible, frequence native %d Hz, langues %s'
          % (v.name, v.natural_sample_rate_hertz, ','.join(v.language_codes)))
    return True


# Generate only missing or requested files, preserving the existing bank when possible.
def generate(items, only):
    os.makedirs(RAW_DIR, exist_ok=True)
    os.makedirs(OUT_DIR, exist_ok=True)
    tts = client = None
    requested = 0
    requested_chars = 0
    rows = []
    worst_reduction = 0.0
    for index, (name, text) in enumerate(items, 1):
        if only and name not in only:
            continue
        decoded = raw_is_valid(name, text)
        if decoded is None:
            if client is None:
                tts, client = make_client()
            data = synthesize(tts, client, text)
            decoded = read_wav_bytes(data or b'')
            if decoded is None:
                raise SystemExit('%s : reponse Google illisible (LINEAR16 attendu)' % name)
            save_raw(name, text, data)
            requested += 1
            requested_chars += len(text)
            time.sleep(0.2)
        samples, stats = process(*decoded)
        data = wav_bytes(samples, RATE)
        path = os.path.join(OUT_DIR, name)
        try:
            with open(path, 'rb') as f:
                unchanged = f.read() == data
        except OSError:
            unchanged = False
        if not unchanged:
            with open(path, 'wb') as f:
                f.write(data)
        # verification: the written file is read as a WAV PCM 16 bits mono 16 kHz de duree plausible
        check_back = read_wav_file(path)
        if check_back is None or check_back[1] != RATE:
            raise SystemExit('%s : WAV final illisible' % name)
        duration = len(check_back[0]) / RATE
        if not MIN_DURATION_S <= duration <= MAX_DURATION_S or stats['clipped']:
            raise SystemExit('%s : duree %.2f s ou ecretage %d' % (name, duration, stats['clipped']))
        worst_reduction = max(worst_reduction, stats['reduction_db'])
        rows.append((name, text, '%.3f' % duration, str(len(data)), '%08x' % (zlib.crc32(data) & 0xFFFFFFFF)))
        print('%3d/%d %-22s %.2f s %6d o  gain %+5.1f dB  limitation %4.1f dB  crete %5.1f dBFS  %s%s'
              % (index, len(items), name, duration, len(data), stats['gain_db'], stats['reduction_db'],
                 stats['peak_dbfs'], text, '' if unchanged else '  [ecrit]'))
    if not only:
        expected = {name for name, _ in items}
        extra = [f for f in os.listdir(OUT_DIR) if f not in expected]
        if extra:
            raise SystemExit('Fichiers inattendus dans %s : %s' % (OUT_DIR, ', '.join(sorted(extra))))
        with open(MANIFEST, 'w', encoding='utf-8-sig', newline='') as f:
            writer = csv.writer(f, delimiter=';')
            writer.writerow(['fichier', 'texte', 'duree_s', 'taille_octets', 'crc32'])
            writer.writerows(rows)
    total = sum(int(r[3]) for r in rows)
    print('Requetes Google : %d (%d caracteres) ; fichiers : %d ; taille totale : %d octets (%.1f Mo) ; '
          'duree totale %.1f s ; limitation max %.1f dB' % (requested, requested_chars, len(rows), total,
                                                            total / 1048576, sum(float(r[2]) for r in rows),
                                                            worst_reduction))
    if not only:
        print('Manifeste : %s' % MANIFEST)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--lang', default='fr', choices=langues_voix.LANGUES,
                    help='langue de la banque (fr par defaut)')
    ap.add_argument('--estimate', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--only', default='', help='liste de fichiers separes par des virgules (essai)')
    args = ap.parse_args()
    configure(args.lang)
    items = phrases()
    if args.estimate:
        estimate(items)
        return
    if args.check:
        sys.exit(0 if check() else 1)
    only = {name.strip() for name in args.only.split(',') if name.strip()}
    unknown = only - {name for name, _ in items}
    if unknown:
        raise SystemExit('Fichiers inconnus : %s' % ', '.join(sorted(unknown)))
    generate(items, only)


if __name__ == '__main__':
    main()
