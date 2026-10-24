# Repository recovery record — 15 October 2026

**Historical report.** This records how the original K19 project was located and reconstructed into a portable Git repository. Later K20–K22 work, security fixes and the v0.2.0 release were added **after** this recovery. It would be incorrect to describe today's sources as byte-for-byte identical to K19.

## What happened

The historical working directory had been moved and was initially believed to be lost. A search across available local drives for Waveshare, glucose and ESP-IDF filenames found an intact archive directory. Its precise machine path is deliberately excluded from public documentation.

The recovered material included all K19 `.c` and `.h` sources; CMake, partition and ESP-IDF configuration; the locally patched Espressif PPA component; 12 historical firmware snapshots; original 32 MB flash backups; diagnostic logs; development screenshots; private account settings; and handover notes.

## How the sources were identified

- The K19 `main/` source tree matched the `K19_VOIX_CAPTEUR_BIN` snapshot in the original recovery comparison.
- Three original 32 MB flash backups were checked against recorded SHA-256 fingerprints.
- Historical logs and local development-session notes were used to reconstruct the chronology and previous decisions.
- The last original K19 diagnostic session was dated September 2026. Subsequent published features came later.

These observations describe the **recovery on 15 October 2026**; they are not new checks against the current v0.2.0 binary.

## Files incorporated into the initial repository

The original C firmware, dependency lockfile, hardware configuration, font licences, patched PPA driver and applicable build tools were recovered. Machine-specific absolute paths were adjusted so the project could be built elsewhere. Separate `.gitignore`, introductory documents, test suites, release tooling, and demo screenshots were created for the sharable repository.

The original private work directory was not intended to be overwritten by the packaging step. Later revisions now use normal Git branches, commits and pull requests rather than recreating publication history.

## What was deliberately excluded

- Real `private/device_secrets.h` credentials.
- Raw flash/NVS backups and old executable snapshots that might embed those credentials.
- Serial logs and measurement history containing real glucose readings.
- Local virtual environments, installed ESP-IDF toolchains and cloned third-party source trees.
- Raw voice-generation caches; only documented application audio packages were prepared separately.

## Historical verification

During the 15 October reconstruction, ESP-IDF 5.5.5 built the recovered K19 sources, pre-flash checks passed, host tests ran, and the five pages were captured on the real display in demo mode. No reflash, NVS write, SD format or eFuse operation was part of that **recovery-only** audit.

The path containing an accented character caused build-tool failures on Windows. Validation therefore used an ASCII-only build directory. The same constraint is documented for present-day developers in [Installation](INSTALLATION_EN.md).

## Current source of truth

For current behavior, use the `main` branch and the tagged Release rather than relying on this historical report. See [Version history](VERSION_HISTORY_EN.md), [the developer walkthrough](DEVELOPER_WALKTHROUGH.md), and [Known limitations](LIMITATIONS_EN.md). The original private archives are neither included nor required to read the public repository.
