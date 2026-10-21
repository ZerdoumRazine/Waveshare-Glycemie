#!/usr/bin/env python3
# EN: Check that all six language catalogs and audio phrase inventories are consistent.
# FR: Vérifie la cohérence des six catalogues de phrases vocales.
"""Controles de la banque vocale multilingue (tools/voice/langues_voix.py).

Ne genere aucun audio et n'appelle aucun service : ce sont des controles de texte.

Le plus important est le premier : les phrases FRANCAISES doivent rester identiques au manifeste
versionne (assets/banque_vocale_manifeste.csv), qui decrit la banque deja copiee sur la microSD.
Changer un mot en francais rendrait les fichiers de la carte obsoletes sans que rien ne le signale.

Usage : python tests\\test_langues_voix.py
"""
import csv
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'voice'))

import langues_voix as lv  # noqa: E402  (le chemin doit etre ajoute avant l'import)

MANIFESTE = os.path.join(ROOT, 'assets', 'banque_vocale_manifeste.csv')

resultats = []


def check(nom, ok, detail=''):
    resultats.append((nom, bool(ok), detail))


# Detect missing clips or mismatched translated IDs before packaging audio.
def main():
    # 1. Le francais doit correspondre exactement au manifeste de la banque installee
    attendu = {}
    with io.open(MANIFESTE, encoding='utf-8-sig', newline='') as f:
        for row in csv.DictReader(f, delimiter=';'):
            attendu[row['fichier']] = row['texte']
    fr = dict(lv.phrases('fr'))
    manquants = sorted(set(attendu) - set(fr))
    en_trop = sorted(set(fr) - set(attendu))
    differents = sorted(n for n in attendu if n in fr and fr[n] != attendu[n])
    check('francais : memes fichiers que le manifeste de la banque installee',
          not manquants and not en_trop, 'manquants=%s en_trop=%s' % (manquants[:5], en_trop[:5]))
    check('francais : memes textes que le manifeste (la banque de la microSD reste valable)',
          not differents, ', '.join('%s : %r != %r' % (n, fr[n], attendu[n]) for n in differents[:3]))

    # 2. Chaque langue est complete et coherente
    for lang in lv.LANGUES:
        items = lv.phrases(lang)
        noms = [n for n, _ in items]
        textes = [t for _, t in items]
        check('%s : %d fichiers (35 phrases fixes + 361 valeurs)' % (lang, len(items)), len(items) == 396,
              str(len(items)))
        check('%s : aucun nom de fichier en double' % lang, len(set(noms)) == len(noms))
        check('%s : aucun texte vide' % lang, all(t.strip() for t in textes))
        check('%s : memes noms de fichiers que le francais' % lang, set(noms) == set(fr))
        valeurs = [t for n, t in items if n.startswith('glucose_') and n[8].isdigit()]
        check('%s : deux valeurs ne se prononcent jamais pareil' % lang,
              len(set(valeurs)) == len(valeurs))
        trop_long = [n for n, t in items if len(t) > 90]
        check('%s : aucune phrase demesuree (> 90 caracteres)' % lang, not trop_long, str(trop_long[:3]))

    # 3. Nombres en lettres : quelques valeurs de reference, verifiees a la main
    reference = {
        'fr': {40: 'quarante', 71: 'soixante et onze', 80: 'quatre-vingts', 100: 'cent',
               121: 'cent vingt et un', 180: 'cent quatre-vingts', 400: 'quatre cents'},
        'en': {40: 'forty', 71: 'seventy-one', 100: 'one hundred', 121: 'one hundred twenty-one',
               208: 'two hundred eight', 400: 'four hundred'},
        'es': {40: 'cuarenta', 78: 'setenta y ocho', 100: 'cien', 121: 'ciento veintiuno',
               200: 'doscientos', 400: 'cuatrocientos'},
        'de': {40: 'vierzig', 78: 'achtundsiebzig', 100: 'einhundert', 121: 'einhunderteinundzwanzig',
               200: 'zweihundert', 400: 'vierhundert'},
        'it': {40: 'quaranta', 78: 'settantotto', 100: 'cento', 121: 'centoventuno',
               180: 'centottanta', 400: 'quattrocento'},
        'pt': {40: 'quarenta', 78: 'setenta e oito', 100: 'cem', 121: 'cento e vinte e um',
               208: 'duzentos e oito', 400: 'quatrocentos'},
    }
    for lang, cas in reference.items():
        mot = lv.NOMBRE[lang]
        faux = ['%d -> %r (attendu %r)' % (n, mot(n), t) for n, t in sorted(cas.items()) if mot(n) != t]
        check('%s : nombres en lettres corrects' % lang, not faux, ' ; '.join(faux))

    # 4. Les 40 a 400 doivent tous etre ecrivables, sans exception
    for lang in lv.LANGUES:
        try:
            tous = [lv.NOMBRE[lang](n) for n in range(lv.GLUCOSE_MIN, lv.GLUCOSE_MAX + 1)]
            ok = all(t and not t.endswith(' ') for t in tous)
            detail = ''
        except Exception as err:  # noqa: BLE001 - on veut le nom de l'erreur dans le rapport
            ok, detail = False, '%s : %s' % (type(err).__name__, err)
        check('%s : les 361 valeurs de 40 a 400 s\'ecrivent sans erreur' % lang, ok, detail)

    # 5. Une voix Chirp 3 HD est declaree pour chaque langue
    for lang in lv.LANGUES:
        code, voix = lv.VOIX[lang]
        check('%s : voix declaree (%s)' % (lang, voix), 'Chirp3-HD' in voix and voix.startswith(code))

    largeur = max(len(r[0]) for r in resultats)
    echecs = 0
    for nom, ok, detail in resultats:
        echecs += 0 if ok else 1
        print('%-6s %-*s %s' % ('OK' if ok else 'ECHEC', largeur, nom, detail))
    print('\nRESULTAT : %s (%d controles, %d echec(s))'
          % ('BANQUES COHERENTES' if echecs == 0 else 'A CORRIGER', len(resultats), echecs))
    return 0 if echecs == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
