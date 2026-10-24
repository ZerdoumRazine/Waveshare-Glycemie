# English documentation migration

All developer documentation and comments are in English.
User-interface and audio languages remain unchanged.
Third-party licensed source and font licences were not edited.
The 21 obsolete *_FR.md pages were deleted, not preserved as redirects.
Release resources and six-language UI/audio content remain unchanged.
The final repository must pass host tests and ESP-IDF compilation.
One-off translation scripts and workflow were removed after use.

- commented_code_files: 118
- compatibility_links_initially_created: 22 (since deleted)
- duplicate_headers_removed: 8
- english_full_guides: 16
- removed_FR_duplicate_lines: 204
- translated_fragments: 2944

## Final English-only cleanup

Mixed-language maintainer comments and Python docstrings were normalized to English. The executable C/Python code, UI labels, voice banks, language tables, protocol fields and third-party source remain unchanged. All Python AST nodes except docstrings were compared, as were C non-comment sources.

- edited_files: 105
- translated_comment_lines: 872
- translated_docstrings: 89
