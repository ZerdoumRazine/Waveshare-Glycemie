#!/usr/bin/env python3
# Compiles and runs hardware-independent C regression tests.
#
# Design note: Simulated components are not a substitute for hardware validation.
# Compile actual firmware C modules against host stubs before executing their tests.
#
r"""Compile et execute les tests unitaires du firmware sur le PC (sans carte ESP32).

Les tests compilent le VRAI code C de main/ : glucose_math.c, app_state.c, sensor_session.c,
consumables.c et stock.c. Seules les dependances materielles (NVS, FreeRTOS, journal, microSD)
sont remplacees par les bouchons de tests/hote/shims.

Usage :
    python tests\run_tests.py              compile puis execute toutes les suites
    python tests\run_tests.py --liste      affiche seulement ce qui serait compile

Compilateur : gcc, clang ou MSVC (Build Tools Visual Studio). Aucun ESP-IDF requis.
Code de sortie 0 seulement si toutes les suites passent.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
MAIN = os.path.join(ROOT, 'main')
HOTE = os.path.join(ROOT, 'tests', 'hote')
SHIMS = os.path.join(HOTE, 'shims')
OUT = os.path.join(ROOT, 'tests', 'build')

SUITES = {
    'test_glucose_math': [
        os.path.join(HOTE, 'test_glucose_math.c'),
        os.path.join(MAIN, 'glucose_math.c'),
        os.path.join(MAIN, 'app_state.c'),
        os.path.join(SHIMS, 'plateforme.c'),
    ],
    'test_i18n': [
        os.path.join(HOTE, 'test_i18n.c'),
        os.path.join(MAIN, 'i18n.c'),
    ],
    'test_credentials': [
        os.path.join(HOTE, 'test_credentials.c'),
        os.path.join(MAIN, 'credentials.c'),
        os.path.join(SHIMS, 'plateforme.c'),
    ],
    'test_consommables': [
        os.path.join(HOTE, 'test_consommables.c'),
        os.path.join(MAIN, 'i18n.c'),
        os.path.join(MAIN, 'sensor_session.c'),
        os.path.join(MAIN, 'consumables.c'),
        os.path.join(MAIN, 'stock.c'),
        os.path.join(SHIMS, 'plateforme.c'),
        os.path.join(SHIMS, 'stubs_firmware.c'),
    ],
}

INCLUDES = [MAIN, HOTE, SHIMS]
FORCE_INCLUDE = os.path.join(SHIMS, 'compat_hote.h')


# Detect the Visual Studio toolchain when GCC and Clang are unavailable.
def trouver_msvc():
    """Renvoie (environnement, chemin de cl.exe) pour MSVC, ou (None, None)."""
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
                           'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    if not os.path.exists(vswhere):
        return None, None
    out = subprocess.run([vswhere, '-all', '-products', '*', '-sort', '-format', 'json'],
                         capture_output=True, text=True)
    try:
        entries = json.loads(out.stdout or '[]')
    except json.JSONDecodeError:
        return None, None
    for entry in entries:
        vcvars = os.path.join(entry['installationPath'], 'VC', 'Auxiliary', 'Build', 'vcvars64.bat')
        if os.path.exists(vcvars):
            # shell=True: cmd.exe keeps quotes from the path (list of arguments: it removes them)
            dump = subprocess.run(f'"{vcvars}" >nul 2>&1 && set',
                                  capture_output=True, text=True, shell=True)
            # Variable Names Windows are not case-sensitive: normalize
            # (sinon "Path" du vidage et "PATH" d'os.environ coexistent et cl.exe reste introuvable).
            env = {k.upper(): v for k, v in os.environ.items()}
            for line in dump.stdout.splitlines():
                if '=' in line:
                    k, v = line.split('=', 1)
                    env[k.upper()] = v
            cl = shutil.which('cl.exe', path=env.get('PATH', ''))
            if cl:
                return env, cl
    return None, None


# Fail the suite on compilation errors, not just assertion failures.
def compiler_et_lancer(nom, sources, env, kind, cc):
    os.makedirs(OUT, exist_ok=True)
    exe = os.path.join(OUT, nom + ('.exe' if os.name == 'nt' else ''))
    if kind == 'msvc':
        cmd = [cc, '/nologo', '/W3', '/std:c17', '/D_CRT_SECURE_NO_WARNINGS',
               '/FI', FORCE_INCLUDE] + [f'/I{d}' for d in INCLUDES] + sources + \
              ['/Fe:' + exe, '/Fo:' + OUT + os.sep]
    else:
        # _POSIX_C_SOURCE : en C11 strict, gcc/clang n'exposent pas localtime_r (teste sous GCC 14)
        cmd = [cc, '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Wno-unused-parameter',
               '-include', FORCE_INCLUDE] + [f'-I{d}' for d in INCLUDES] + sources + ['-o', exe]
    build = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=OUT)
    if build.returncode != 0:
        print(f'=== {nom} : COMPILATION ECHOUEE')
        print(build.stdout[-6000:])
        print(build.stderr[-6000:])
        return 1
    run = subprocess.run([exe], capture_output=True, text=True, env=env)
    print(f'=== {nom}')
    print(run.stdout)
    if run.stderr.strip():
        print(run.stderr)
    return run.returncode


# Run all independent C suites and report a nonzero code for any failure.
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--liste', action='store_true', help='afficher les suites sans compiler')
    args = ap.parse_args()

    if args.liste:
        for nom, srcs in SUITES.items():
            print(nom)
            for s in srcs:
                print('   ', os.path.relpath(s, ROOT))
        return 0

    env, kind, cc = dict(os.environ), None, None
    for c in ('gcc', 'clang'):
        trouve = shutil.which(c)
        if trouve:
            kind, cc = c, trouve
            break
    if kind is None:
        env_msvc, cl = trouver_msvc()
        if env_msvc is not None:
            env, kind, cc = env_msvc, 'msvc', cl
    if kind is None:
        print('Aucun compilateur C trouve (gcc, clang ou Build Tools Visual Studio).')
        print('Les tests n\'ont PAS ete executes.')
        return 2

    print(f'Compilateur : {kind} ({cc})\n')
    total = 0
    for nom, sources in SUITES.items():
        total += 1 if compiler_et_lancer(nom, sources, env, kind, cc) else 0
    print('\n==================================================')
    print('RESULTAT GLOBAL :', 'TOUTES LES SUITES PASSENT' if total == 0 else f'{total} suite(s) en echec')
    return 0 if total == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
