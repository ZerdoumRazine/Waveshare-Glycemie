#!/usr/bin/env python3
"""Finish English-only developer comments without changing executable behavior."""
from __future__ import annotations

import ast
from collections import Counter
import io
from pathlib import Path
import re
import tokenize

import english_docs_migration as previous

ROOT = Path(__file__).resolve().parent.parent
FRENCH = re.compile(
    r"[àâçéèêëîïôùûüÿœÀÂÇÉÈÊËÎÏÔÙÛÜŸŒ]|"
    r"\b(?:le|la|les|un|une|des|dans|pour|avec|sans|sur|depuis|avant|apres|après|"
    r"lorsque|mais|encore|comme|donc|ainsi|seulement|aucun|aucune|tout|tous|toute|"
    r"cela|cette|ces|ceux|celui|ici|alors|ensuite|entre|etant|étant|doit|doivent|"
    r"ecran|écran|fichier|fichiers|donnees|données|reponse|réponse|fenetre|fenêtre|"
    r"utilisateur|appareil|connexion|sauvegarde|enregistre|enregistré|ancien|ancienne|"
    r"nouveau|nouvelle|mesure|valeur|reglage|reglages|règle|regle|clavier|"
    r"efface|effacer|verifie|vérifie|jamais|toujours|premiere|première|garder|"
    r"supprimer|ajouter|calcul|demarrage|démarrage|retour|voix|capteur|catheter|"
    r"depasse|dépassé|touche|touchez|appuyez|copie|suivant|suivante|"
    r"texte|chaque|vers|meme|même|reste|restant|rien|leurs|"
    r"pourquoi|comment|testee|testé|attendu|mise|jour|sortie|entree|entrée)\b",
    re.IGNORECASE,
)
# Literal UI labels, code paths, technical identifiers and commands must not be translated.
previous.CODETOK = re.compile(
    r"(\x60[^\x60\n]*\x60|\"[^\"\n]*\"|'[^'\n]*'|"
    r"[A-Za-z_]\w*_[A-Za-z_0-9]+|[A-Za-z_]\w*\(\)|"
    r"(?:[A-Za-z_]\w*/)+[A-Za-z_][\w./-]*|"
    r"https?://[^\s)]+|\b[A-Z][A-Z0-9_]{2,}\b)"
)
COUNT = Counter()


def init_translator():
    import argostranslate.package
    import argostranslate.translate
    langs = argostranslate.translate.get_installed_languages()
    french = next((x for x in langs if x.code == "fr"), None)
    english = next((x for x in langs if x.code == "en"), None)
    if french is None or english is None:
        argostranslate.package.update_package_index()
        pkg = next(x for x in argostranslate.package.get_available_packages()
                   if x.from_code == "fr" and x.to_code == "en")
        argostranslate.package.install_from_path(pkg.download())
        langs = argostranslate.translate.get_installed_languages()
        french = next(x for x in langs if x.code == "fr")
        english = next(x for x in langs if x.code == "en")
    previous.TRANSLATOR = french.get_translation(english)


def translate_line(v):
    if not FRENCH.search(v):
        return v
    if re.search(r"\b(?:SPDX-License-Identifier|Copyright|licence text)\b|©", v, re.I):
        return v
    result = previous.english(v, forced=True)
    if result != v:
        COUNT["translated_comment_lines"] += 1
    return result


def comment_line(v):
    if re.match(r"^\s*FR:\s*", v, re.I):
        COUNT["removed_bilingual_duplicate_lines"] += 1
        return None
    v = re.sub(r"^\s*EN:\s*", "", v, flags=re.I)
    return translate_line(v)


def strip_docstrings(tree):
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            if node.body and isinstance(node.body[0], ast.Expr):
                value = node.body[0].value
                if isinstance(value, ast.Constant) and isinstance(value.value, str):
                    value.value = "<documentation>"
    return tree


def char_column(line, byte_column):
    return len(line.encode("utf-8")[:byte_column].decode("utf-8"))


def translate_docstrings(src):
    parsed = ast.parse(src)
    rows = src.splitlines(keepends=True)
    starts = [0]
    for row in rows:
        starts.append(starts[-1] + len(row))
    docs = {}
    for node in ast.walk(parsed):
        if not isinstance(node, (ast.Module, ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            continue
        if not node.body or not isinstance(node.body[0], ast.Expr):
            continue
        obj = node.body[0].value
        if not isinstance(obj, ast.Constant) or not isinstance(obj.value, str):
            continue
        if not FRENCH.search(obj.value):
            continue
        col = char_column(rows[obj.lineno - 1], obj.col_offset)
        docs[(obj.lineno, col)] = obj.value
    edits = []
    for token in tokenize.generate_tokens(io.StringIO(src).readline):
        if token.type != tokenize.STRING or token.start not in docs:
            continue
        try:
            content = ast.literal_eval(token.string)
        except (ValueError, SyntaxError):
            continue
        if not isinstance(content, str) or content != docs[token.start]:
            continue
        translated = "\n".join(translate_line(row) for row in content.split("\n"))
        if translated == content:
            continue
        quote = '"""' if '"""' not in translated else "'''"
        replacement = quote + translated.replace("\\", "\\\\") + quote
        a = starts[token.start[0] - 1] + token.start[1]
        b = starts[token.end[0] - 1] + token.end[1]
        edits.append((a, b, replacement))
        COUNT["translated_docstrings"] += 1
    for a, b, replacement in reversed(edits):
        src = src[:a] + replacement + src[b:]
    ast.parse(src)
    return src


def process_one(p):
    old = p.read_text(encoding="utf-8")
    if p.suffix in (".c", ".h"):
        new = previous.cprocess(old)
        old_clean = previous.cprocess(old, False)
        new_clean = previous.cprocess(new, False)
        if re.sub(r"\s+", " ", old_clean).strip() != re.sub(r"\s+", " ", new_clean).strip():
            raise RuntimeError(f"C source changed outside comments: {p}")
    elif p.suffix in (".py", ".pyw"):
        new = translate_docstrings(previous.pyprocess(old))
        if ast.dump(strip_docstrings(ast.parse(new)), include_attributes=False) != \
           ast.dump(strip_docstrings(ast.parse(old)), include_attributes=False):
            raise RuntimeError(f"Python source changed outside documentation: {p}")
    elif p.suffix == ".ps1":
        new = previous.psprocess(old)
    else:
        new = previous.hash_comments(old)
    if new != old:
        p.write_text(new, encoding="utf-8")
        COUNT["edited_files"] += 1


def run():
    previous.comment_line = comment_line
    init_translator()
    paths = [p for group in ("main", "tools", "tests")
             for p in (ROOT / group).rglob("*")
             if p.is_file() and p.suffix.lower() in (".c", ".h", ".py", ".pyw", ".ps1")]
    paths += [ROOT / "CMakeLists.txt", ROOT / "main/CMakeLists.txt",
              ROOT / "sdkconfig.defaults", ROOT / "partitions.csv"]
    for p in sorted(paths):
        if p.as_posix().endswith("main/voice_clips.h"):
            continue
        process_one(p)
    report = ROOT / "docs/ENGLISH_MIGRATION_REPORT.md"
    existing = report.read_text(encoding="utf-8")
    note = (
        "\n## Final English-only cleanup\n\n"
        "Mixed-language maintainer comments and Python docstrings were normalized to English. "
        "The executable C/Python code, UI labels, voice banks, language tables, protocol fields "
        "and third-party source remain unchanged. All Python AST nodes except docstrings "
        "were compared, as were C non-comment sources.\n\n"
        + "\n".join(f"- {key}: {value}" for key, value in sorted(COUNT.items())) + "\n"
    )
    if "## Final English-only cleanup" not in existing:
        report.write_text(existing.rstrip() + "\n" + note, encoding="utf-8")
    print(dict(COUNT))


if __name__ == "__main__":
    import sys
    if sys.argv[1:] != ["--apply"]:
        raise SystemExit("Run only on a disposable branch: --apply")
    run()
