#!/usr/bin/env python3
# Repackages validated speech WAVs and their provenance.
#
# Design note: Reject unrelated files such as logs or personal histories.
# Validates original six-language WAVs and writes an audio-only release ZIP.
#
"""Control and repaquette only WAV the microSD of the project.

Enter: historical v0.1.1.0 microSD package, downloaded from the PRIVATE Release.
Output: v0.2.0 microSD archive, without user file, cache or log,
and footprint SHA-256 independent.
This program does not contact Dexcom or Google, and does not create any audio files.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
from pathlib import Path, PurePosixPath
import re
import stat
import wave
import zipfile

ROOT = Path(__file__).resolve().parent.parent
LANGUAGES = ("fr", "en", "es", "de", "it", "pt")
WAV_NAME = re.compile(r"^[A-Za-z0-9_-]+\.wav$", re.ASCII)
BASE = "glycemie/audio/voice/"
NOTICE = """WaveShare Glycémie — speech audio provenance
===========================================

This archive contains 2,376 WAV files for optional spoken announcements
(396 recordings in each of six languages), arranged for the display's
microSD card.

These recordings were generated with Google Cloud Text-to-Speech using
Chirp 3 HD Aoede voices. No Google speech synthesis model is included.
Google documents use of generated audio in applications and media, subject
to the applicable terms and laws:
https://docs.cloud.google.com/text-to-speech/docs/basics
https://docs.cloud.google.com/text-to-speech/quotas
https://cloud.google.com/terms

The recordings are application audio resources, NOT original source code
licensed under this project's MIT licence. No rights in Google's speech
models, services or trademarks are claimed. This independent project is
not affiliated with or endorsed by Google.

The repository provides a separate generator so users can create their own
recordings with their own Google Cloud account. The display's alert beeps
continue to work without the WAV files. This experimental supplementary
glucose display is not certified for medical use.
"""


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fp:
        for block in iter(lambda: fp.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


# Reject an input ZIP whose hash differs from the historical release manifest.
def check_original_hash(original: Path, manifest: Path) -> None:
    lines = manifest.read_text(encoding="utf-8-sig").splitlines()
    found = [s.split("  ", 1)[0].strip().lower()
             for s in lines if s.strip().endswith("  " + original.name)]
    if len(found) != 1 or not re.fullmatch(r"[a-f0-9]{64}", found[0]):
        raise ValueError("Empreinte originale introuvable dans SHA256SUMS.txt")
    if digest(original) != found[0]:
        raise ValueError("L'archive originale ne correspond pas au SHA-256 historique")


# Use the reviewed voice manifest as an allowlist for each language.
def expected_names() -> set[str]:
    with (ROOT / "assets" / "banque_vocale_manifeste.csv").open(
        "r", encoding="utf-8-sig", newline=""
    ) as fp:
        names = {row["fichier"] for row in csv.DictReader(fp, delimiter=";")}
    if len(names) != 396 or not all(WAV_NAME.fullmatch(name) for name in names):
        raise ValueError("Manifeste des 396 WAV de référence invalide")
    return names


# Require the PCM format actually supported by the display.
def check_wav(data: bytes, name: str) -> None:
    try:
        with wave.open(io.BytesIO(data), "rb") as reader:
            if (reader.getnchannels(), reader.getsampwidth(), reader.getframerate()) != (1, 2, 16000):
                raise ValueError("Format WAV différent de PCM 16 bits mono 16 kHz")
            if reader.getnframes() <= 0:
                raise ValueError("Fichier WAV vide")
    except (wave.Error, EOFError) as exc:
        raise ValueError("WAV invalide " + name) from exc


# Allow only the expected 2,376 recordings and a freshly written provenance notice.
def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--historical-sha", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    check_original_hash(args.archive, args.historical_sha)
    names = expected_names()
    args.out.mkdir(parents=True, exist_ok=True)
    output = args.out / "glycemie-microSD-v0.2.0.zip"

    counts = {lang: set() for lang in LANGUAGES}
    with zipfile.ZipFile(args.archive, "r") as source, zipfile.ZipFile(
        output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True
    ) as dest:
        for info in source.infolist():
            if info.is_dir():
                continue
            # The old ZIP also includes a download package created
            # local. Never reuse it in the published package:
            # a neutral provenance statement is regenerated below.
            if info.filename in ("LISEZMOI.txt",):
                continue
            path = PurePosixPath(info.filename)
            if not info.filename.startswith(BASE) or ".." in path.parts:
                raise ValueError("Fichier inattendu dans la microSD : " + info.filename)
            if stat.S_ISLNK(info.external_attr >> 16):
                raise ValueError("Lien symbolique interdit : " + info.filename)
            tail = info.filename[len(BASE):].split("/")
            if len(tail) == 1:
                lang, filename = "fr", tail[0]
            elif len(tail) == 2 and tail[0] in LANGUAGES[1:]:
                lang, filename = tail
            else:
                raise ValueError("Dossier vocal imprévu : " + info.filename)
            if not WAV_NAME.fullmatch(filename) or filename not in names:
                raise ValueError("Nom WAV non prévu : " + info.filename)
            if filename in counts[lang]:
                raise ValueError("Doublon WAV : " + info.filename)
            counts[lang].add(filename)
            if info.file_size > 500_000 or info.file_size < 44:
                raise ValueError("Taille WAV anormale : " + info.filename)
            data = source.read(info)  # zipfile checks CRC32 while reading
            check_wav(data, info.filename)
            dest.writestr(info.filename, data)
        for lang in LANGUAGES:
            if counts[lang] != names:
                raise ValueError(
                    f"Banque {lang} incomplète : {len(counts[lang])}/396, "
                    f"{len(names - counts[lang])} absents"
                )
        dest.writestr("PROVENANCE_VOIX.txt", NOTICE.encode("utf-8"))

    with zipfile.ZipFile(output) as verified:
        bad = verified.testzip()
        if bad is not None:
            raise ValueError("ZIP produit corrompu : " + bad)
        if len(verified.namelist()) != 2377:
            raise ValueError("Nombre de fichiers inattendu")
    checksum = args.out / "SHA256SUMS-voix.txt"
    checksum.write_text(f"{digest(output)}  {output.name}\n", encoding="ascii")
    print("Archive vérifiée : 6 × 396 WAV PCM mono 16 kHz + provenance FR/EN")
    print(f"Fichier : {output} — {output.stat().st_size} octets")
    print(f"SHA-256 : {digest(output)}")


if __name__ == "__main__":
    main()
