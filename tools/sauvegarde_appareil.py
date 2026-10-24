#!/usr/bin/env python3
# Backs up the firmware flash for recovery purposes.
#
# Design note: The backup is private because it may contain Wi-Fi, Dexcom and health data.
# Reads a complete flash backup before maintenance without changing the device.
#
"""Identify and back up a Waveshare device before firmware installation.

Use read-only esptool commands to save the permitted flash regions and inspect
partition layout. Never write, erase, format or modify the device as part of a
backup. Handle saved dumps as sensitive because they may contain credentials."""
import argparse
import datetime
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SAUVEGARDES = os.path.join(ROOT, 'private', 'sauvegardes')

# Areas saved by default (partition addresses.csv, reread on device if possible).
ZONES_PAR_DEFAUT = [
    ('bootloader', 0x0, 0x8000),
    ('table_partitions', 0x8000, 0x1000),
]


def esptool_cmd(impose=None):
    """Select the ESP-IDF esptool when installed, otherwise use the Python module."""
    candidats = []
    if impose:
        candidats.append([impose] if impose.lower().endswith('.exe') else [sys.executable, impose])
    bases = [os.environ.get('IDF_TOOLS_PATH'), os.path.join(ROOT, 'tools', 'espressif')]
    if os.environ.get('IDF_PATH'):
        bases.append(os.path.join(os.path.dirname(os.environ['IDF_PATH']), 'espressif'))
    for base in bases:
        if not base:
            continue
        env = os.path.join(base, 'python_env')
        if os.path.isdir(env):
            for sous in sorted(os.listdir(env)):
                # environmental python ESP-IDF, with the esptool module: more reliable than
                # esptool.exe, which needs this environment to start
                py = os.path.join(env, sous, 'Scripts', 'python.exe')
                if os.path.exists(py):
                    candidats.append([py, '-m', 'esptool'])
    trouve = shutil.which('esptool.py') or shutil.which('esptool')
    if trouve:
        candidats.append([trouve])
    candidats.append([sys.executable, '-m', 'esptool'])
    for cmd in candidats:
        try:
            out = subprocess.run(cmd + ['version'], capture_output=True, text=True, timeout=120)
            if out.returncode == 0:
                return cmd, (out.stdout.strip().splitlines() or ['?'])[0]
        except (OSError, subprocess.SubprocessError):
            continue
    return None, None


def lancer(cmd, args, timeout=900):
    """Run an allowed read-only esptool command and return its status and output."""
    interdits = {'erase_flash', 'erase_region', 'write_flash', 'burn_efuse', 'write_mem',
                 'erase-flash', 'erase-region', 'write-flash', 'burn-efuse'}
    if interdits & set(args):
        raise SystemExit('Commande interdite dans cet outil de sauvegarde : %s' % ' '.join(args))
    out = subprocess.run(cmd + args, capture_output=True, text=True, timeout=timeout,
                         encoding='utf-8', errors='replace')
    return out.returncode, (out.stdout or '') + (out.stderr or '')


# Inspect partition metadata to confirm the hardware layout.
def lire_partitions(chemin_bin):
    """Parse the flash partition table into name, type, subtype, address and size records."""
    entrees = []
    with open(chemin_bin, 'rb') as f:
        data = f.read()
    for i in range(0, len(data), 32):
        bloc = data[i:i + 32]
        if len(bloc) < 32 or bloc[:2] != b'\xaa\x50':
            if bloc[:2] == b'\xeb\xeb':  # fin de table (MD5 ou remplissage)
                break
            continue
        type_, sous = bloc[2], bloc[3]
        adresse = int.from_bytes(bloc[4:8], 'little')
        taille = int.from_bytes(bloc[8:12], 'little')
        nom = bloc[12:28].split(b'\x00', 1)[0].decode('ascii', 'replace')
        entrees.append((nom, type_, sous, adresse, taille))
    return entrees


def sha256(chemin):
    h = hashlib.sha256()
    with open(chemin, 'rb') as f:
        for bloc in iter(lambda: f.read(1 << 20), b''):
            h.update(bloc)
    return h.hexdigest()


# Read the flash and compare checksums before reporting a backup as complete.
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', default=None, help='COM7 par exemple ; detecte sinon')
    ap.add_argument('--vitesse', default='460800', help='vitesse de lecture (460800 par defaut)')
    ap.add_argument('--identifier', action='store_true', help='identification seule, aucune copie')
    ap.add_argument('--complet', action='store_true', help='copie TOUTE la memoire flash (plus long)')
    ap.add_argument('--dossier', default=None, help='dossier de sauvegarde (prive par defaut)')
    ap.add_argument('--esptool', default=None, help='chemin d\'esptool si la detection echoue')
    args = ap.parse_args()

    cmd, version = esptool_cmd(args.esptool)
    if cmd is None:
        print("esptool est introuvable. Ouvrez la fenetre « ESP-IDF 5.5 PowerShell » (elle definit")
        print('IDF_TOOLS_PATH), ou installez esptool :   py -3 -m pip install esptool')
        return 2
    print('esptool : %s' % version)

    port = args.port
    if port is None:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        try:
            import assistant_config as ac
            port = ac.trouver_port()
        except Exception:                                    # noqa: BLE001
            port = None
    if port is None:
        print("Port introuvable : branchez le cable USB-C puis relancez avec --port COM7.")
        print("La liste des ports :  python tools\\assistant_config.py --lister")
        return 1
    base = ['--port', port, '--baud', args.vitesse]
    print('Appareil sur %s' % port)

# --- 1. Read-only identification
    print('\n--- Identification ---')
    code, sortie = lancer(cmd, base + ['flash_id'], timeout=180)
    if code != 0:
        print(sortie[-1500:])
        print("\nL'appareil n'a pas repondu. Verifiez le cable (un cable de DONNEES, pas seulement")
        print('de charge), la prise USB de la carte, et fermez tout moniteur serie ouvert.')
        return 1
    infos = {}
    for motif, cle in ((r'Chip is (.+)', 'puce'), (r'Features: (.+)', 'options'),
                       (r'Crystal is (.+)', 'quartz'), (r'MAC: (.+)', 'adresse_mac'),
                       (r'Detected flash size: (\S+)', 'taille_flash'),
                       (r'Manufacturer: (\S+)', 'fabricant_flash'), (r'Device: (\S+)', 'modele_flash')):
        trouve = re.search(motif, sortie)
        if trouve:
            infos[cle] = trouve.group(1).strip()
    for cle, valeur in infos.items():
        print('   %-16s %s' % (cle, valeur))
    taille_flash = infos.get('taille_flash', '')
    octets_flash = int(taille_flash[:-2]) * 1024 * 1024 if taille_flash.endswith('MB') else 0

    horodatage = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    dossier = args.dossier or os.path.join(SAUVEGARDES, horodatage)
    os.makedirs(dossier, exist_ok=True)

    # Table of scores: Always read, it serves everything else
    table = os.path.join(dossier, 'table_partitions.bin')
    code, sortie = lancer(cmd, base + ['read_flash', '0x8000', '0x1000', table], timeout=300)
    partitions = lire_partitions(table) if code == 0 else []
    if partitions:
        print('\n--- Partitions de votre appareil ---')
        print('   %-12s %-10s %12s %12s' % ('nom', 'type', 'adresse', 'taille'))
        for nom, type_, sous, adresse, taille in partitions:
            genre = {0: 'app', 1: 'donnees'}.get(type_, str(type_))
            print('   %-12s %-10s %12s %12s' % (nom, genre, hex(adresse), '%d Ko' % (taille // 1024)))
    else:
        print('\nTable des partitions illisible : la sauvegarde complete reste possible (--complet).')

    if args.identifier:
        print('\nIdentification seule : rien n\'a ete copie. L\'appareil n\'a pas ete modifie.')
        if code == 0:
            os.remove(table)
            try:
                os.rmdir(dossier)
            except OSError:
                pass
        return 0

    # --- 2. Backup -------------------------------------------------------------------------
    print('\n--- Sauvegarde dans %s ---' % dossier)
    print("Rien n'est ecrit dans l'appareil : il n'est que relu.")
    fichiers = []
    if args.complet and octets_flash:
        cible = os.path.join(dossier, 'flash_complete.bin')
        print('Copie de toute la memoire (%s) : cela peut prendre plusieurs minutes…' % taille_flash)
        code, sortie = lancer(cmd, base + ['read_flash', '0', hex(octets_flash), cible], timeout=3600)
        if code != 0:
            print(sortie[-1500:])
            return 1
        fichiers.append(('flash_complete.bin', 0, octets_flash))
    else:
        zones = list(ZONES_PAR_DEFAUT)
        for nom, type_, sous, adresse, taille in partitions:
            # useful applications and data; one skips large empty partitions
            if nom in ('nvs', 'otadata', 'phy_init') or type_ == 0:
                zones.append((nom, adresse, taille))
        for nom, adresse, taille in zones:
            cible = os.path.join(dossier, '%s_%s.bin' % (nom, hex(adresse)))
            print('   %-16s %10s  (%d Ko)' % (nom, hex(adresse), taille // 1024))
            code, sortie = lancer(cmd, base + ['read_flash', hex(adresse), hex(taille), cible],
                                  timeout=1800)
            if code != 0:
                print(sortie[-1200:])
                return 1
            fichiers.append((os.path.basename(cible), adresse, taille))

    # --- 3. Verification ---------------------------------------------------------------------
    print('\n--- Verification (relecture et comparaison avec l\'appareil) ---')
    manifeste = {'date': horodatage, 'port': port, 'appareil': infos, 'fichiers': [],
                 'partitions': [{'nom': n, 'type': t, 'sous_type': s, 'adresse': a, 'taille': l}
                                for n, t, s, a, l in partitions]}
    tout_ok = True
    for nom, adresse, taille in fichiers:
        chemin = os.path.join(dossier, nom)
        empreinte = sha256(chemin)
        reel = os.path.getsize(chemin)
        code, sortie = lancer(cmd, base + ['verify_flash', hex(adresse), chemin], timeout=1800)
        ok = code == 0 and reel == taille
        tout_ok = tout_ok and ok
        print('   %-28s %s  %s' % (nom, 'verifie' if ok else 'A REVOIR', empreinte[:16]))
        if not ok:
            print(sortie[-800:])
        manifeste['fichiers'].append({'nom': nom, 'adresse': adresse, 'taille': taille,
                                      'sha256': empreinte, 'verifie': ok})
    with io.open(os.path.join(dossier, 'manifeste.json'), 'w', encoding='utf-8') as f:
        json.dump(manifeste, f, indent=2, ensure_ascii=False)

    print('\n=== RESULTAT ===')
    if not tout_ok:
        print("Au moins un fichier n'a pas ete verifie : NE METTEZ PAS A JOUR l'appareil.")
        print('Relancez la sauvegarde (eventuellement avec --vitesse 115200).')
        return 1
    print('Sauvegarde complete et verifiee : %s' % dossier)
    print("Elle reste privee (dossier private\\, jamais publie). Gardez-en une copie ailleurs.")
    print('Pour revenir en arriere, voir docs\\ROLLBACK_EN.md.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
