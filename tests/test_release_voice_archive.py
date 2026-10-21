#!/usr/bin/env python3
# EN: Verify a private audio package only contains the expected six banks and provenance.
# FR: Vérifie les banques vocales et la notice de provenance.
"""Tests sans vrai son ni matériel des gardes-fous de l'archive vocale."""
from __future__ import annotations

import hashlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import wave
import zipfile

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import release_voice_archive as va  # noqa: E402


# Generate harmless artificial audio for tests; no real voice files are needed.
def tiny_wav() -> bytes:
    out = io.BytesIO()
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(16000)
        w.writeframes(b"\x00\x00" * 8)
    return out.getvalue()


# Build a six-language fixture matching the expected archive structure.
def make_source(path: Path, unexpected: bool = False) -> None:
    names = sorted(va.expected_names())
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as dest:
        sound = tiny_wav()
        for lang in va.LANGUAGES:
            prefix = va.BASE if lang == "fr" else va.BASE + lang + "/"
            for filename in names:
                dest.writestr(prefix + filename, sound)
        if unexpected:
            dest.writestr("glycemie/logs/mesures-personnelles.csv", "ne pas diffuser")


# Confirm that a private CSV log in the input archive causes an explicit refusal.
def run() -> int:
    with tempfile.TemporaryDirectory() as folder:
        work = Path(folder)
        original = work / "glycemie-microSD.zip"
        make_source(original)
        digest = hashlib.sha256(original.read_bytes()).hexdigest()
        manifest = work / "SHA256SUMS.txt"
        manifest.write_text(digest + "  glycemie-microSD.zip\n", encoding="ascii")
        out = work / "dist"
        proc = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "release_voice_archive.py"),
             "--archive", str(original), "--historical-sha", str(manifest),
             "--out", str(out)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        if proc.returncode:
            print("ERREUR source valide :", proc.stderr, proc.stdout)
            return 1
        archive = out / "glycemie-microSD-v0.2.0.zip"
        with zipfile.ZipFile(archive) as z:
            if len(z.namelist()) != 2377 or z.testzip():
                print("ERREUR : archive de sortie invalide")
                return 1
            if "PROVENANCE_VOIX.txt" not in z.namelist():
                print("ERREUR : droits/provenance manquants")
                return 1
        expected = (out / "SHA256SUMS-voix.txt").read_text().split("  ", 1)[0]
        if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
            print("ERREUR : contrôle SHA-256")
            return 1

        make_source(original, unexpected=True)
        manifest.write_text(hashlib.sha256(original.read_bytes()).hexdigest()
                            + "  glycemie-microSD.zip\n", encoding="ascii")
        proc = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "release_voice_archive.py"),
             "--archive", str(original), "--historical-sha", str(manifest),
             "--out", str(out)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        if proc.returncode == 0:
            print("ERREUR : un journal privé a été accepté")
            return 1
    print("ARCHIVE VOIX OK : 2376 WAV, SHA-256, droits et refus des journaux")
    return 0


if __name__ == "__main__":
    sys.exit(run())
