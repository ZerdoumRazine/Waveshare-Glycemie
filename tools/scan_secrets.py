#!/usr/bin/env python3
# Scans tracked content for credential-like patterns.
#
# Design note: No pattern scan can guarantee every secret has been found.
# Looks for accidentally included credentials without ever echoing secret values.
#
"""Recherche des identifiants prives dans des fichiers texte (journaux, rapports, sources).

N'affiche JAMAIS les valeurs : seulement le nom du champ et le fichier concerne.
Usage : python scan_secrets.py <fichier ou dossier> [...]
Code de sortie 1 si une valeur est trouvee.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SECRETS = os.path.join(ROOT, 'private', 'device_secrets.h')
TEXT_EXT = {'.log', '.txt', '.md', '.c', '.h', '.py', '.json', '.yml', '.yaml', '.csv', '.ps1', ''}


# Load private comparison values only for local scanning.
def load():
    values = {}
    text = open(SECRETS, encoding='utf-8').read()
    for m in re.finditer(r'#define\s+(GLY_\w+)\s+"((?:[^"\\]|\\.)*)"', text):
        if m.group(1) != 'GLY_DEXCOM_REGION' and len(m.group(2)) >= 3:
            values[m.group(1)] = re.sub(r'\\(.)', r'\1', m.group(2))
    return values


# Inspect the requested source files without including excluded build caches.
def files(paths):
    for p in paths:
        if os.path.isdir(p):
            for base, _, names in os.walk(p):
                for n in names:
                    if os.path.splitext(n)[1].lower() in TEXT_EXT:
                        yield os.path.join(base, n)
        elif os.path.isfile(p):
            yield p


# Report locations and categories of matches, not the sensitive strings themselves.
def main():
    secrets = load()
    found = 0
    checked = 0
    for path in files(sys.argv[1:]):
        if os.path.abspath(path) == os.path.abspath(SECRETS):
            continue
        try:
            data = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        checked += 1
        for name, value in secrets.items():
            count = data.count(value)
            if count:
                found += 1
                print('TROUVE %s dans %s (%d occurrence(s))' % (name, path, count))
    print('%d fichier(s) analyse(s), %d correspondance(s)' % (checked, found))
    return 1 if found else 0


if __name__ == '__main__':
    sys.exit(main())
