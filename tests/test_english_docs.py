#!/usr/bin/env python3
"""Require maintained developer documentation to be English only.

French strings used by the six-language display and speech resources are
intentionally excluded from this check.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
FRENCH_DOC_SUFFIX = "_F" + "R.md"
LINK_PATTERN = re.compile(r"[A-Za-z0-9_./-]+" + re.escape(FRENCH_DOC_SUFFIX), re.I)


def main():
    errors = []
    excludes = {"build", ".git", "managed_components", "__pycache__", ".venv"}
    docs = [p for p in ROOT.rglob("*.md") if not excludes.intersection(p.relative_to(ROOT).parts)]
    for p in docs:
        if p.name.upper().endswith(FRENCH_DOC_SUFFIX.upper()):
            errors.append(f"obsolete Markdown file: {p.relative_to(ROOT)}")
    folders = ("main", "tools", "tests", "docs", "assets", "screenshots", ".github")
    paths = [ROOT / "README.md", ROOT / "CMakeLists.txt"]
    allowed = {".md", ".py", ".pyw", ".ps1", ".yml", ".yaml", ".txt", ".c", ".h"}
    for folder in folders:
        for p in (ROOT / folder).rglob("*"):
            if p.is_file() and p.suffix.lower() in allowed and not excludes.intersection(p.relative_to(ROOT).parts):
                paths.append(p)
    for p in paths:
        if p.resolve() == Path(__file__).resolve() or p.name == "ENGLISH_MIGRATION_REPORT.md":
            continue
        try:
            lines = p.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeError):
            continue
        for line_no, line in enumerate(lines, 1):
            if LINK_PATTERN.search(line):
                errors.append(f"obsolete doc link: {p.relative_to(ROOT)}:{line_no}")
    if errors:
        print("\n".join(errors))
        return 1
    print("OK: No obsolete French-language Markdown pages or references.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
