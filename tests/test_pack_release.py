#!/usr/bin/env python3
"""Regression test for self-contained, offline v0.2.0 installation archives."""
from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
PACKER = ROOT / "tools" / "pack_release.py"
SOURCE_NAMES = {
    "glycemie_display.bin": "GLYCEMIE-FIRMWARE-TEST",
    "bootloader/bootloader.bin": "BOOTLOADER-TEST",
    "partition_table/partition-table.bin": "PARTITION-TEST",
}


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="test-glycemie-release-") as folder:
        tmp = Path(folder)
        build = tmp / "build"
        for path, value in SOURCE_NAMES.items():
            destination = build / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(value.encode("ascii"))
        exe = tmp / "GlycemieConnexion.exe"
        exe.write_bytes(b"MZ" + b"FAKE-EXECUTABLE-FOR-OFFLINE-TEST")
        out = tmp / "out"
        p = subprocess.run(
            [sys.executable, str(PACKER), "--build", str(build), "--exe", str(exe),
             "--out", str(out)],
            cwd=ROOT, capture_output=True, text=True,
        )
        if p.returncode:
            print("ERREUR: packaging impossible", p.stdout, p.stderr)
            return 1

        archive = out / "glycemie-installation-ecran-v0.2.0.zip"
        if not archive.is_file():
            print("ERREUR: archive absente")
            return 1
        with zipfile.ZipFile(archive) as z:
            names = set(z.namelist())
            expected = {"glycemie_display.bin", "bootloader.bin", "partition-table.bin",
                        "installer.ps1", "LISEZMOI_README.md", "SHA256SUMS.txt"}
            if names != expected:
                print("ERREUR: fichiers inattendus", names ^ expected)
                return 1
            manifest = z.read("SHA256SUMS.txt").decode("ascii")
            for line in manifest.strip().splitlines():
                digest, name = line.split("  ", 1)
                if hashlib.sha256(z.read(name)).hexdigest() != digest:
                    print("ERREUR: empreinte interne invalide", name)
                    return 1
            installer = z.read("installer.ps1").decode("utf-8")
            if not all(x in installer for x in
                       ["Get-FileHash", "Read-Host", "esp32p4", "INSTALLER", "revision"]):
                print("ERREUR: controles indispensables manquants dans l'installateur")
                return 1
            if any(x in " ".join(names).lower() for x in ("secret", "private", "nvs", "log")):
                print("ERREUR: entree privee dans l'archive")
                return 1

        manifest = (out / "SHA256SUMS.txt").read_text(encoding="ascii")
        for line in manifest.strip().splitlines():
            digest, name = line.split("  ", 1)
            if hashlib.sha256((out / name).read_bytes()).hexdigest() != digest:
                print("ERREUR: empreinte de livraison invalide", name)
                return 1
        if exe.read_bytes() != (out / "GlycemieConnexion.exe").read_bytes():
            print("ERREUR: application Windows differente")
            return 1

    print("TESTS OK : archive, sources, hashes, composant Windows et securite offline")
    return 0


if __name__ == "__main__":
    sys.exit(main())
