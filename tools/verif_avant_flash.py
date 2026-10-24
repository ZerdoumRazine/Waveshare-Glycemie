#!/usr/bin/env python3
# Checks firmware build settings before any device flash.
#
# Design note: Target revision and partition offsets must match the actual board.
# Checks board revision, partition layout and binary headers before flashing.
#
"""Run pre-flash validation checks for the Waveshare ESP32-P4 rev 1.3 build.

Confirm expected firmware outputs, target configuration and partition
assumptions before invoking any installation procedure. This script validates
artifacts; it does not flash the device."""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
PROJECT = ROOT  # the project ESP-IDF is at the root of the deposit
BUILD = os.path.join(PROJECT, 'build')
APP_BIN = os.path.join(BUILD, 'glycemie_display.bin')
BOOT_BIN = os.path.join(BUILD, 'bootloader', 'bootloader.bin')
PART_BIN = os.path.join(BUILD, 'partition_table', 'partition-table.bin')
SECRETS = os.path.join(ROOT, 'private', 'device_secrets.h')

# Provision identical to the original firmware (backup_original/ANALYSE_PARTITIONS.txt), plus partition "model"
# voice controls (ESP-SR) adds 2026-09-15 APRES original partitions (their offsets are interchanged)
ORIGINAL_PARTITIONS = [
    ('nvs', 'data', 'nvs', 0x9000, 0x6000),
    ('phy_init', 'data', 'phy', 0xF000, 0x1000),
    ('factory', 'app', 'factory', 0x10000, 0x800000),
    ('storage', 'data', 'spiffs', 0x810000, 0x700000),
]
MODEL_PARTITION = ('model', 'data', 'spiffs', 0xF10000, 0x400000)
EXPECTED_PARTITIONS = ORIGINAL_PARTITIONS + [MODEL_PARTITION]
MODEL_BIN = os.path.join(BUILD, 'srmodels', 'srmodels.bin')
EXPECTED_FLASH_FILES = {
    0x2000: 'bootloader/bootloader.bin',
    0x8000: 'partition_table/partition-table.bin',
    0x10000: 'glycemie_display.bin',
    0xF10000: 'srmodels/srmodels.bin',
}

results = []


def check(name, ok, detail=''):
    results.append((name, bool(ok), detail))


def to_int(value):
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    return None


def size_to_int(text):
    text = text.strip().upper()
    mult = 1
    if text.endswith('K'):
        mult, text = 1024, text[:-1]
    elif text.endswith('M'):
        mult, text = 1024 * 1024, text[:-1]
    return int(text, 0) * mult


# Read chip revision metadata from the built image, not the filename.
def image_revisions(path):
    # esptool 4.x (environnement ESP-IDF 5.5): the en-tete etendu is only displayed with --version 2
    out = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32p4', 'image_info', '--version', '2', path],
                         capture_output=True, text=True)
    text = out.stdout + out.stderr
    mini = re.search(r'Minimal chip revision:\s*v([\d.]+)', text)
    maxi = re.search(r'Maximal chip revision:\s*v([\d.]+)', text)
    return (mini.group(1) if mini else None, maxi.group(1) if maxi else None, text)


# Reject an incompatible or incomplete build; the check never flashes hardware.
def main():
    # 1. sdkconfig resolu
    cfg = json.load(open(os.path.join(BUILD, 'config', 'sdkconfig.json'), encoding='utf-8'))
    check('Cible esp32p4', cfg.get('IDF_TARGET') == 'esp32p4', str(cfg.get('IDF_TARGET')))
    check('Profil pre-v3 : CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y', cfg.get('ESP32P4_SELECTS_REV_LESS_V3') is True)
    check('Revision minimale v1.0 : CONFIG_ESP32P4_REV_MIN_100=y',
          cfg.get('ESP32P4_REV_MIN_100') is True and to_int(cfg.get('ESP32P4_REV_MIN_FULL')) == 100,
          'REV_MIN_FULL=%s' % cfg.get('ESP32P4_REV_MIN_FULL'))
    check('Aucune configuration rev3.x : CONFIG_ESP32P4_REV_MIN_300 absent', not cfg.get('ESP32P4_REV_MIN_300'))
    check('Revision maximale acceptee v1.99', to_int(cfg.get('ESP32P4_REV_MAX_FULL')) == 199,
          'REV_MAX_FULL=%s' % cfg.get('ESP32P4_REV_MAX_FULL'))
    check('Flash 32 Mo', cfg.get('ESPTOOLPY_FLASHSIZE') == '32MB', str(cfg.get('ESPTOOLPY_FLASHSIZE')))
    check('PSRAM activee (200 MHz)', cfg.get('SPIRAM') is True and to_int(cfg.get('SPIRAM_SPEED')) == 200,
          'SPIRAM_SPEED=%s' % cfg.get('SPIRAM_SPEED'))
    check('Table de partitions a 0x8000', to_int(cfg.get('PARTITION_TABLE_OFFSET')) == 0x8000,
          str(cfg.get('PARTITION_TABLE_OFFSET')))
    check('Secure Boot desactive', not cfg.get('SECURE_BOOT'))
    check('Flash Encryption desactive', not cfg.get('SECURE_FLASH_ENC_ENABLED'))
    check('Aucun formatage automatique de la microSD (BSP)', not cfg.get('BSP_SD_FORMAT_ON_MOUNT_FAIL'))

    # 2. Table de partitions compilee
    gen = os.path.join(os.environ['IDF_PATH'], 'components', 'partition_table', 'gen_esp32part.py')
    out = subprocess.run([sys.executable, gen, PART_BIN], capture_output=True, text=True)
    parts = []
    for line in out.stdout.splitlines():
        if not line.strip() or line.startswith('#'):
            continue
        fields = [f.strip() for f in line.split(',')]
        parts.append((fields[0], fields[1], fields[2], to_int(fields[3]), size_to_int(fields[4])))
    check('Partitions d\'origine inchangees + partition model (ESP-SR) apres elles', parts == EXPECTED_PARTITIONS,
          '; '.join('%s@0x%X/0x%X' % (p[0], p[3], p[4]) for p in parts))
    end = max((p[3] + p[4] for p in parts), default=0)
    check('Partitions contenues dans 32 Mo', end <= 32 * 1024 * 1024, 'fin 0x%X' % end)

    # 3. Arguments de flash
    fa = json.load(open(os.path.join(BUILD, 'flasher_args.json'), encoding='utf-8'))
    files = {int(k, 0): v.replace('\\', '/') for k, v in fa.get('flash_files', {}).items()}
    check('Fichiers flashes : bootloader 0x2000, table 0x8000, application 0x10000, modeles 0xF10000 uniquement '
          '(NVS jamais ecrite)', files == EXPECTED_FLASH_FILES, str({hex(k): v for k, v in files.items()}))
    extra = fa.get('extra_esptool_args', {})
    check('esptool cible esp32p4', extra.get('chip') == 'esp32p4', str(extra.get('chip')))
    write_args = ' '.join(fa.get('write_flash_args', []))
    check('Aucun effacement complet dans les arguments', 'erase' not in write_args.lower(), write_args)
    check('Taille flash des arguments = 32MB', fa.get('flash_settings', {}).get('flash_size') == '32MB',
          str(fa.get('flash_settings')))

    # 4. Binaires
    app_size = os.path.getsize(APP_BIN)
    check('Application < partition factory (8 Mo)', app_size < 0x800000,
          '%d octets (%.1f %% de 8 Mo)' % (app_size, 100.0 * app_size / 0x800000))
    boot_size = os.path.getsize(BOOT_BIN)
    check('Bootloader tient avant 0x8000 (<= 0x6000)', boot_size <= 0x6000, '%d octets' % boot_size)
    model_size = os.path.getsize(MODEL_BIN) if os.path.exists(MODEL_BIN) else -1
    check('Modeles ESP-SR presents et < partition model (4 Mo)', 0 < model_size < MODEL_PARTITION[4],
          '%d octets' % model_size)
    mini, maxi, _ = image_revisions(APP_BIN)
    check('Application : revisions v1.0 a v1.99 (image_info)', mini == '1.0' and maxi == '1.99',
          'min v%s, max v%s' % (mini, maxi))
    mini, maxi, _ = image_revisions(BOOT_BIN)
    check('Bootloader : revisions v1.0 a v1.99 (image_info)', mini == '1.0' and maxi == '1.99',
          'min v%s, max v%s' % (mini, maxi))

    # 5. No secrets in versionable configuration
    secrets = []
    if os.path.exists(SECRETS):
        for m in re.finditer(r'#define\s+(GLY_\w+)\s+"((?:[^"\\]|\\.)*)"', open(SECRETS, encoding='utf-8').read()):
            if m.group(1) != 'GLY_DEXCOM_REGION' and len(m.group(2)) >= 3:
                secrets.append(re.sub(r'\\(.)', r'\1', m.group(2)))
    leaked = []
    for name in ('sdkconfig', 'sdkconfig.defaults', 'README.md', 'dependencies.lock'):
        path = os.path.join(PROJECT, name)
        if os.path.exists(path):
            text = open(path, encoding='utf-8', errors='replace').read()
            leaked += [name for s in secrets if s in text]
    check('Aucun identifiant dans sdkconfig / defaults / README / dependencies.lock', not leaked,
          'fichiers : %s' % sorted(set(leaked)) if leaked else '')

    width = max(len(r[0]) for r in results)
    failed = 0
    for name, ok, detail in results:
        failed += 0 if ok else 1
        print('%-4s %-*s %s' % ('OK' if ok else 'ECHEC', width, name, detail))
    print('\nRESULTAT : %s (%d controles, %d echec(s))' % ('PRET A FLASHER' if failed == 0 else 'FLASH INTERDIT',
                                                         len(results), failed))
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
