#!/usr/bin/env python3
"""Migrate source comments and developer documents to English using an offline model.

This utility deliberately leaves string literals, identifiers, UI language tables,
speech-bank data and third-party licensed code untouched. Run on a Git branch.
"""
from __future__ import annotations
import ast
from collections import Counter
import io
from pathlib import Path
import re
import tokenize

ROOT = Path(__file__).resolve().parent.parent
MAP = {
 "README_FR.md":"README.md",
 "CREDITS_EN.md":"CREDITS.md",
 "docs/CODE_GUIDE_FR.md":"docs/CODE_GUIDE_EN.md",
 "docs/TUTORIEL_DEBUTANT_FR.md":"docs/BEGINNER_TUTORIAL_EN.md",
 "docs/TUTORIEL_VOIX_FR.md":"docs/VOICE_TUTORIAL_EN.md",
 "docs/ARCHITECTURE_FR.md":"docs/ARCHITECTURE_EN.md",
 "docs/COMMENT_PUBLIER_FR.md":"docs/PUBLICATION_GITHUB_EN.md",
 "docs/CONFIGURATION_DEXCOM_FR.md":"docs/DEXCOM_SETUP_EN.md",
 "docs/DEPANNAGE_FR.md":"docs/TROUBLESHOOTING_EN.md",
 "docs/HARDWARE_FR.md":"docs/HARDWARE_EN.md",
 "docs/INSTALLATION_FR.md":"docs/INSTALLATION_EN.md",
 "docs/LICENCES_TIERCES_FR.md":"docs/THIRD_PARTY_LICENSES_EN.md",
 "docs/LIMITATIONS_FR.md":"docs/LIMITATIONS_EN.md",
 "docs/PUBLICATION_GITHUB_FR.md":"docs/PUBLICATION_GITHUB_EN.md",
 "docs/RAPPORT_K22_FR.md":"docs/K22_TECHNICAL_REPORT_EN.md",
 "docs/RECUPERATION_FR.md":"docs/RECOVERY_EN.md",
 "docs/RETOUR_ARRIERE_FR.md":"docs/ROLLBACK_EN.md",
 "docs/UTILISATION_FR.md":"docs/USER_GUIDE_EN.md",
 "docs/VERSIONS_FR.md":"docs/VERSION_HISTORY_EN.md",
 "assets/README_FR.md":"assets/README.md",
 "screenshots/README_FR.md":"screenshots/README.md",
 "tests/README_FR.md":"tests/README.md",
}
CANONICAL={"README.md","CREDITS_EN.md","docs/CODE_GUIDE_EN.md",
           "docs/BEGINNER_TUTORIAL_EN.md","docs/VOICE_TUTORIAL_EN.md"}
COUNTS=Counter()
TRANSLATOR=None
CACHE={}
CODETOK=re.compile(r"([A-Za-z_]\w*_[A-Za-z_0-9]+|[A-Za-z_]\w*\(\)|"
                   r"(?:[A-Za-z_]\w*/)+[A-Za-z_][\w./-]*|"
                   r"https?://[^\s)]+|\b[A-Z][A-Z0-9_]{2,}\b)")
FR=re.compile(r"[àâçéèêëîïôùûüÿœ]|"
              r"\b(?:les|des|une|dans|pour|avec|sans|apres|avant|"
              r"fichier|fichiers|donnees|donnees|connexion|sauvegarde|"
              r"garder|toujours|jamais|doit|doivent|lequel|lorsque|"
              r"permet|modifie|stockage|n'est|n'ont|l'utilisateur|"
              r"clavier|cliquez|affiche|reglages|reprise)\b",re.I)


def english(text, forced=False):
    global TRANSLATOR
    if not forced and not FR.search(text):
        return text
    if text in CACHE:
        return CACHE[text]
    out=[]
    for part in CODETOK.split(text):
        if not part:
            continue
        if CODETOK.fullmatch(part) or not re.search(r"[A-Za-zÀ-ÿ]",part):
            out.append(part)
        elif not forced and not FR.search(part):
            out.append(part)
        else:
            lead=part[:len(part)-len(part.lstrip())]
            tail=part[len(part.rstrip()):]
            core=part.strip()
            if len(core)<3:
                out.append(part)
            else:
                out.append(lead+TRANSLATOR.translate(core).replace("*/","* /")+tail)
    result="".join(out)
    CACHE[text]=result
    COUNTS["translated_fragments"]+=int(result!=text)
    return result


def comment_line(value):
    if re.match(r"^\s*FR:\s*",value,re.I):
        COUNTS["removed_FR_duplicate_lines"]+=1
        return None
    value=re.sub(r"^\s*EN:\s*","",value,flags=re.I)
    if re.match(r"^(?:SPDX|Copyright|LICENSE|@param|@return|©)",value,re.I):
        return value
    return english(value)


def block(body):
    out=[]
    for line in body.splitlines(keepends=True):
        ending="\n" if line.endswith("\n") else ""
        inner=line[:-1] if ending else line
        m=re.match(r"^(\s*\*?\s?)(.*)$",inner)
        if not m:
            out.append(line)
            continue
        part=comment_line(m.group(2))
        if part is not None:
            out.append(m.group(1)+part+ending)
    return "".join(out)


def cprocess(s,change=True):
    o=[];i=0;last=0
    while i<len(s):
        if s[i] in ('"',"'"):
            q=s[i];i+=1
            while i<len(s):
                if s[i]=="\\":
                    i+=2
                elif s[i]==q:
                    i+=1;break
                else:i+=1
            continue
        if s.startswith("/*",i):
            end=s.find("*/",i+2)
            if end<0:raise RuntimeError("unclosed C comment")
            o.append(s[last:i])
            o.append("/*"+(block(s[i+2:end]) if change else "")+"*/")
            i=end+2;last=i;continue
        if s.startswith("//",i):
            end=s.find("\n",i+2)
            if end<0:end=len(s)
            o.append(s[last:i])
            part=comment_line(s[i+2:end].strip())
            o.append("// "+part if change and part is not None else "//")
            i=end;last=i;continue
        i+=1
    o.append(s[last:])
    return "".join(o)


def dedup_file_headers(s):
    # Two bilingual overview blocks were added at the top of several modules.
    blocks=list(re.finditer(r"/\*[\s\S]*?\*/\s*",s[:1800]))
    if len(blocks)<2 or blocks[0].start()>0:return s
    first=blocks[0].group()
    second=blocks[1].group()
    if ("Design note:" in first or "Design decision:" in first) and (
        "Design note:" in second or "Design decision:" in second):
        s=s[:blocks[1].start()]+s[blocks[1].end():]
        COUNTS["duplicate_headers_removed"]+=1
    return s


def pyprocess(src):
    original=ast.parse(src)
    spans=[0]
    for line in src.splitlines(keepends=True):spans.append(spans[-1]+len(line))
    changes=[]
    for tok in tokenize.generate_tokens(io.StringIO(src).readline):
        if tok.type!=tokenize.COMMENT:continue
        m=re.match(r"(#\s?)(.*)",tok.string,re.S)
        if not m:continue
        val=comment_line(m.group(2))
        new=(m.group(1)+val) if val is not None else "#"
        if new!=tok.string:
            a=spans[tok.start[0]-1]+tok.start[1]
            b=spans[tok.end[0]-1]+tok.end[1]
            changes.append((a,b,new))
    s=src
    for a,b,new in reversed(changes):s=s[:a]+new+s[b:]
    if ast.dump(ast.parse(s),include_attributes=False)!=ast.dump(original,include_attributes=False):
        raise RuntimeError("Python code AST changed")
    return s


def hash_comments(src):
    out=[]
    for line in src.splitlines(keepends=True):
        m=re.match(r"^(\s*#\s?)([^\n]*)(\n?)$",line)
        if m:
            val=comment_line(m.group(2))
            out.append((m.group(1)+val if val is not None else "#")+m.group(3))
        else:out.append(line)
    return "".join(out)


def psprocess(s):
    s=re.sub(r"<#([\s\S]*?)#>",lambda m:"<#"+block(m.group(1))+"#>",s)
    return hash_comments(s)


def markdown(s,force=True):
    out=[];fence=False
    inline=chr(96)
    for line in s.splitlines(keepends=True):
        nl="\n" if line.endswith("\n") else ""
        raw=line[:-1] if nl else line
        if raw.strip().startswith((inline*3,"~~~")):
            fence=not fence;out.append(line);continue
        if fence:
            out.append(line);continue
        parts=raw.split("|") if raw.count("|")>=2 else [raw]
        translated=[]
        for part in parts:
            m=re.match(r"^(\s*(?:#{1,6}\s+|>\s*|[-*+]\s+|\d+\.\s+)*)",part)
            pre=m.group(0)
            body=part[len(pre):]
            if not body.strip() or re.fullmatch(r"[\s\-:]+",body):
                translated.append(part);continue
            # Preserve inline-code expressions and Markdown URL destinations.
            chunks=re.split(r"("+re.escape(inline)+r"[^"+re.escape(inline)+r"]+"+re.escape(inline)+r"|\]\([^)]*\))",body)
            translated.append(pre+"".join(
                c if c.startswith(inline) or c.startswith("](") else english(c,forced=force)
                for c in chunks))
        out.append("|".join(translated)+nl)
    return "".join(out)


def legacy_notice(old,new):
    target=Path(new).name if Path(old).parent==Path(new).parent else new
    return ("# English documentation\n\n"
            "The current guide is maintained in English: ["+Path(new).name+
            "]("+target+").\n"
            "This short compatibility page keeps links from older releases working.\n")


def migrate():
    global TRANSLATOR
    import argostranslate.package
    import argostranslate.translate
    languages=argostranslate.translate.get_installed_languages()
    french=next((x for x in languages if x.code=="fr"),None)
    englishlang=next((x for x in languages if x.code=="en"),None)
    if not french or not englishlang:
        argostranslate.package.update_package_index()
        package=next(x for x in argostranslate.package.get_available_packages()
                     if x.from_code=="fr" and x.to_code=="en")
        argostranslate.package.install_from_path(package.download())
        languages=argostranslate.translate.get_installed_languages()
        french=next(x for x in languages if x.code=="fr")
        englishlang=next(x for x in languages if x.code=="en")
    TRANSLATOR=french.get_translation(englishlang)

    source={x:(ROOT/x).read_text(encoding="utf-8-sig") for x in MAP}
    for old,new in MAP.items():
        if new in CANONICAL:continue
        target=ROOT/new
        if target.exists():raise FileExistsError(str(target))
        target.write_text(markdown(source[old]),encoding="utf-8")
        COUNTS["english_full_guides"]+=1

    # The English credits become the canonical CREDITS.md.
    credits=source["CREDITS_EN.md"]
    credits=credits.replace("[Français](CREDITS.md) | **English**","**English**")
    (ROOT/"CREDITS.md").write_text(credits,encoding="utf-8")
    for old,new in MAP.items():
        (ROOT/old).write_text(legacy_notice(old,new),encoding="utf-8")
        COUNTS["compatibility_links"]+=1

    docs=[ROOT/"README.md",ROOT/"CREDITS.md"]+list((ROOT/"docs").glob("*.md"))
    docs+=list((ROOT/"assets").glob("*.md"))+list((ROOT/"tests").glob("*.md"))
    docs+=list((ROOT/"screenshots").glob("*.md"))
    for path in docs:
        rel=path.relative_to(ROOT).as_posix()
        if rel in MAP:continue
        s=path.read_text(encoding="utf-8")
        for old,new in MAP.items():
            s=s.replace(Path(old).name,Path(new).name)
        if rel=="README.md":
            s=re.sub(r"(?m)^\*Documentation en français.*\n",
                     "Developer documentation is maintained in English; on-device languages are unchanged.\n",s)
            s=s.replace(" · [Guide français](docs/CODE_GUIDE_EN.md)","")
        if rel=="docs/CODE_GUIDE_EN.md":
            s=s.replace("**English** | [Français](CODE_GUIDE_EN.md)","**English only**")
            s=s.replace("English and French","English only")
        path.write_text(s,encoding="utf-8")

    files=[p for group in ("main","tools","tests") for p in (ROOT/group).rglob("*")
           if p.is_file() and p.suffix.lower() in (".c",".h",".py",".pyw",".ps1")]
    files+=[ROOT/"CMakeLists.txt",ROOT/"main/CMakeLists.txt",ROOT/"sdkconfig.defaults",
            ROOT/"partitions.csv"]
    for path in files:
        rel=path.relative_to(ROOT).as_posix()
        if rel=="main/voice_clips.h":continue
        s=path.read_text(encoding="utf-8")
        if path.suffix in (".c",".h"):
            new=cprocess(dedup_file_headers(s))
            if re.sub(r"\s+"," ",cprocess(new,False))!=re.sub(r"\s+"," ",cprocess(s,False)):
                raise RuntimeError("C tokens changed outside comments: "+rel)
        elif path.suffix in (".py",".pyw"):
            new=pyprocess(s)
        elif path.suffix==".ps1":
            new=psprocess(s)
        else:new=hash_comments(s)
        if new!=s:
            path.write_text(new,encoding="utf-8")
            COUNTS["commented_code_files"]+=1
    report=["# English documentation migration","","All developer documentation and comments are in English.",
            "User-interface and audio languages remain unchanged.",
            "Third-party licensed source and font licences were not edited.",
            "Legacy documentation links point to English guides.",
            "This migration must pass automated regressions and ESP-IDF compilation.",""]
    report.extend("- "+k+": "+str(v) for k,v in sorted(COUNTS.items()))
    (ROOT/"docs/ENGLISH_MIGRATION_REPORT.md").write_text("\n".join(report)+"\n",encoding="utf-8")
    print(dict(COUNTS))


if __name__=="__main__":
    import sys
    if sys.argv[1:]!=["--apply"]:
        raise SystemExit("Run only on a disposable branch: python tools/english_docs_migration.py --apply")
    migrate()
