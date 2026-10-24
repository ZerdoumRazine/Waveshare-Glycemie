# Maintaining and publishing WaveShare Glycémie

The repository is owned by **Mohamed-Razine Zerdoum**. Publishing source code, updating a private Release, and making the repository visible to everyone are **three different actions**. Only the owner should change repository visibility.

## Update the repository

1. In GitHub Desktop, open `ZerdoumRazine/Waveshare-Glycemie` and run **Fetch origin**, then **Pull origin** if offered.
2. Create a branch for the change. Do not rewrite, delete or recreate the existing Git history.
3. Edit source, documentation or tests. Check the diff for accidental changes to passwords, histories, generated assets and vendor notices.
4. Commit and push the branch; open a pull request. Merge only after review and the appropriate GitHub Actions checks.

## Prepare a release

Run C/Python regressions, Windows application tests, and the ESP-IDF firmware build. Confirm the target board and revision, the partition layout, absence of embedded personal credentials, and SHA-256 checksums. The installer and ZIP must correspond to the exact tested source commit.

Published Release assets are **snapshots**. A documentation commit does not update an older firmware ZIP. If executable code changes, make a new version rather than silently replacing a tested historical package.

The optional six-language Google-generated WAV bank is distributed separately with provenance notes; do not apply the project's MIT source licence to those recordings. Read [Third-party licences](THIRD_PARTY_LICENSES_EN.md).

## Share a private project

A private repository can be shown to selected GitHub accounts by inviting a collaborator with the minimum required access. Do not send login credentials, repository tokens or private flash backups to a reviewer.

## Before switching to public visibility

Verify all Markdown links, screenshots, credits, licences and Release attachments. Check for personal glucose readings and accounts in **all committed history and binaries**, not only the current visible files. A secret accidentally pushed must be rotated; deleting the file alone is not sufficient.

Retain the statements that the project is an experimental supplementary display and that first-time installation on an untouched board was not validated end to end. Do not advertise medical certification.

To change visibility, use the owner's GitHub repository settings. **This document does not authorize an automated public release.**

## Avoid

- Force-pushing a rewritten Git history for routine comments.
- Running the old `--depot-neuf` option against an existing repository.
- Publishing private `device_secrets.h`, NVS, serial logs or actual measurement CSV files.
- Updating the ESP32-C6 or using erase-flash/eFuse tools as a documentation step.
