# Maintain and prepare WaveShare Glycemia publication

The project is hosted in a **private deposit** on GitHub:
[ZerdoumRazine/Waveshare-Glycemie](https://github.com/ZerdoumRazine/Waveshare-Glycemie).
Its visibility must not be changed without the explicit decision of the owner.

This guide concerns the **maintenance of the existing repository**. Do not recreate a
new repository, neither erase its Git history, nor execute an order from
"force push" to update documentation.

## Make a change

1. In GitHub Desktop, select repository `ZerdoumRazine/Waveshare-Glycemie`.
2. Make **Fetch origin**, then **Pull origin** if proposed.
3. Change the sources or documentation in this copy.
4. Review changes, create a commit and use **Push origin**.
5. On GitHub, check the rendering of the README and ties. For a change
   create a branch and a *pull request* rather than modify the program
   direct `main`.

Documents [README French](../README.md) et
[README English](../README.md) must remain consistent.

## Check before proposing a new version

- Run `python tests/run_tests.py`, `python tests/test_depot.py` and
  Other `tests/` ; check also GitHub Actions results.
- Software tests and simulations **do not replace** compilation
  ESP-IDF and equipment testing.
- Never publish personal flash backup, NVS, glucose history,
  logbooks, identifiers or captures showing family data.
- Reread **visibly** each image and verify the license of each image
  third-party component. Automatic secret search tools have limits.
- Confirm that the compiled version matches the sources of the commit tested
  and that the partition table is correct for hardware revision.
- Before wide distribution, have the regulatory status of the software reviewed,
  In particular its warnings: a warning does not constitute certification.

## Downloadable files

Large files do not belong to the Git history of the code.
Releases may contain:
- the programme for **exclusively** the material indicated;
- `GlycemieConnexion.exe` ;
- microSD voice banks and their source information;
- fingerprints SHA-256 files distributed.

Preversion v0.1.0 already exists. Its code and binaries are **historic**:
a subsequent firmware correction requires a new build, tests and
a **new version**; not pretend that the old ZIP was updated.
GitHub automatically offers the source code for each tag.
Do not consider ZIP manually added as more recent than the commit.

## Sharing and publication

A **private** deposit is only accessible to authorized persons.
It is possible to occasionally invite a player from settings
Deposit. Access is never equivalent to a change in visibility.

An opening to the public is not automatic: make a final review of the
secrets, captures, rights, tests, warnings and links before choosing
**Settings → General → Change visibility → Public**. Only the owner
must decide on this action.

## Not to be done

- Do not execute `preparer_razine.py --depot-neuf` on an existing repository.
- Do not delete folder `.git` or rewrite the history for a normal update.
- Do not promise that a new device installation has been validated without actual testing.
- Do not make public a program generated from personal secrets.
