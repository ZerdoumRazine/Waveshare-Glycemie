# English documentation migration

All developer documentation and comments are in English.
User-interface and audio languages remain unchanged.
Third-party licensed source and font licences were not edited.
Legacy documentation links point to English guides.
This migration must pass automated regressions and ESP-IDF compilation.

- commented_code_files: 118
- compatibility_links: 22
- duplicate_headers_removed: 8
- english_full_guides: 16
- removed_FR_duplicate_lines: 204
- translated_fragments: 2944

## Final English-only cleanup

Mixed-language maintainer comments and Python docstrings were normalized to English. The executable C/Python code, UI labels, voice banks, language tables, protocol fields and third-party source remain unchanged. All Python AST nodes except docstrings were compared, as were C non-comment sources.

- edited_files: 105
- translated_comment_lines: 872
- translated_docstrings: 89
