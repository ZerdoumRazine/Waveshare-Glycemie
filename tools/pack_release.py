#!/usr/bin/env python3
# Packages verified firmware and Windows files with checksums.
#
# Design note: Release ZIP files must be traced to the tested source commit.
# Packages exact firmware outputs with checksums and Windows installer.
#
"""Prepare an archive d'installation vérifiable à partir d'a build ESP-IDF validated.

No user account or network access. Never take files under private/.
The binary and offset names only target Waveshare ESP32-P4 rev1.3.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parent.parent
VERSION = "v0.2.0"
BINARIES = {
    "glycemie_display.bin": "glycemie_display.bin",
    "bootloader/bootloader.bin": "bootloader.bin",
    "partition_table/partition-table.bin": "partition-table.bin",
}
INSTRUCTIONS = """# WaveShare Glycémie — firmware installation (v0.2.0)

Hardware: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, ESP32-P4 rev 1.3
(pre-v3), 32 MB flash. Other revisions have not been validated.

1. Read docs/BEGINNER_TUTORIAL_EN.md in the GitHub repository.
2. Extract this entire archive to a Windows folder.
3. Connect a USB-C data cable to the board's UART/CH343 port.
4. Close any serial monitors. Install Python 3 and esptool:
    py -3 -m pip install esptool
5. Open PowerShell in the extracted folder and run:
    powershell -ExecutionPolicy Bypass -File .\\installer.ps1
6. The installer checks file hashes and the chip revision and asks for
   explicit approval before flashing. Do not bypass its checks.
7. After restart, enter your own Wi-Fi and Dexcom Share credentials on the
   touchscreen or using the separately downloaded GlycemieConnexion.exe.

The firmware contains no personal credentials. It is an EXPERIMENTAL
supplementary display, not a certified medical device or a primary
glucose alert system. Never rely on it for treatment decisions.
"""


# Hash in bounded chunks so the packager handles large firmware safely.
def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


# Refuse private credentials and verify every file before writing the release ZIP.
def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True, help="ESP-IDF build/ directory")
    parser.add_argument("--exe", type=Path, required=True, help="tested Windows executable")
    parser.add_argument("--out", type=Path, required=True, help="release output directory")
    args = parser.parse_args()

    build = args.build.resolve()
    exe = args.exe.resolve()
    out = args.out.resolve()
    if not exe.is_file() or exe.name != "GlycemieConnexion.exe":
        raise SystemExit("Executable Windows absent ou nom incorrect")
    if (ROOT / "private" / "device_secrets.h").exists():
        raise SystemExit("REFUS : ce depot contient des identifiants personnels")

    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="glycemie-release-") as temp:
        src = Path(temp)
        for rel, name in BINARIES.items():
            binary = build / rel
            if not binary.is_file() or binary.stat().st_size <= 0:
                raise SystemExit(f"Binaire absent ou vide : {binary}")
            shutil.copy2(binary, src / name)

        installer = ROOT / "tools" / "installer.ps1"
        if not installer.is_file():
            raise SystemExit("Installateur manquant")
        shutil.copy2(installer, src / "installer.ps1")
        (src / "LISEZMOI_README.md").write_text(INSTRUCTIONS, encoding="utf-8")

        names = list(BINARIES.values()) + ["installer.ps1", "LISEZMOI_README.md"]
        hashes = "\n".join(f"{sha256(src / name)}  {name}" for name in names) + "\n"
        (src / "SHA256SUMS.txt").write_text(hashes, encoding="ascii")

        archive = out / f"glycemie-installation-ecran-{VERSION}.zip"
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for name in sorted(names + ["SHA256SUMS.txt"]):
                z.write(src / name, arcname=name)
        with zipfile.ZipFile(archive) as z:
            bad = z.testzip()
            if bad is not None:
                raise SystemExit(f"Archive ZIP corrompue : {bad}")
            if sorted(z.namelist()) != sorted(names + ["SHA256SUMS.txt"]):
                raise SystemExit("Contenu inattendu dans l'archive")

    shutil.copy2(exe, out / "GlycemieConnexion.exe")
    expected = [archive.name, "GlycemieConnexion.exe"]
    (out / "SHA256SUMS.txt").write_text(
        "\n".join(f"{sha256(out / name)}  {name}" for name in expected) + "\n",
        encoding="ascii",
    )
    (out / "SOURCE_COMMIT.txt").write_text(
        "WaveShare Glycémie " + VERSION + "\n"
        + "Commit source: " + os.environ.get("GITHUB_SHA", "unknown") + "\n"
        + "Nouveaux programmes compilés sur GitHub Actions après vérifications.\n",
        encoding="utf-8",
    )
    print("Paquet préparé, sans donnée personnelle:")
    for path in sorted(out.iterdir()):
        print(f"  {path.name} {path.stat().st_size} octets {sha256(path)}")


if __name__ == "__main__":
    main()
