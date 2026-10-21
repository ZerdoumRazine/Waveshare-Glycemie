#!/usr/bin/env python3
# Packages verified firmware and Windows files with checksums.
# FR: Prépare firmware et application Windows avec leurs empreintes.
# Design note: Release ZIP files must be traced to the tested source commit.
# EN: Packages exact firmware outputs with checksums and Windows installer.
# FR: Assemble les fichiers de l'écran, les empreintes et l'installateur Windows.
"""Prépare une archive d'installation vérifiable à partir d'un build ESP-IDF validé.

Aucun compte utilisateur ni accès réseau. Ne prend jamais de fichiers sous private/.
Les noms de binaires et offsets ciblent uniquement Waveshare ESP32-P4 rev1.3.
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
INSTRUCTIONS = """# WaveShare Glycémie — installation / setup (v0.2.0)

Français
--------
Carte : Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, révision ESP32-P4 v1.3
(pré-v3), flash 32 Mo. Autres révisions : non validées.
1. Lisez docs/TUTORIEL_DEBUTANT_FR.md depuis le dépôt GitHub.
2. Décompressez intégralement cette archive dans un dossier Windows.
3. Branchez le câble USB-C **UART / CH343** et fermez tout moniteur série.
4. Installez Python 3 et esptool : py -3 -m pip install esptool
5. Ouvrez PowerShell dans ce dossier :
     powershell -ExecutionPolicy Bypass -File .\\installer.ps1
6. Le script vérifie les empreintes, la puce et demande votre accord.
7. Après le redémarrage, configurez Wi-Fi et Dexcom sur l'écran, ou
   utilisez GlycemieConnexion.exe téléchargé séparément.

English
-------
Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, ESP32-P4 rev1.3
(pre-v3), 32 MB flash. Other revisions have not been validated.
1. See docs/BEGINNER_TUTORIAL_EN.md in the GitHub repository.
2. Extract this whole archive on Windows.
3. Connect to the UART/CH343 USB-C port; close serial monitors.
4. Install Python 3 and esptool: py -3 -m pip install esptool
5. Run: powershell -ExecutionPolicy Bypass -File .\\installer.ps1
6. The installer checks file hashes, hardware and asks for approval.
7. Set up Wi-Fi and Dexcom using the touchscreen wizard or the
   separately downloaded Windows application.

IMPORTANT : supplementary experimental display, not certified for medical
use. Never rely on it for therapy decisions or primary glucose alarms.
No private credentials are included in the distributed firmware.
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
