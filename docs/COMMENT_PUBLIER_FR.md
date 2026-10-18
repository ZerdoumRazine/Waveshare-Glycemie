# Publier ce projet sur GitHub — marche à suivre

Ce dépôt est **prêt à être publié tel quel**. Il ne contient aucun identifiant, aucune donnée de
santé, aucun binaire compilé. Suivez les étapes dans l'ordre ; comptez vingt minutes la première
fois.

> La copie publiable se fabrique toute seule à partir de ce qui est enregistré dans Git :
> `python tools\preparer_razine.py` remplit le dossier `Razine\`. N'y modifiez jamais un fichier
> à la main : la prochaine préparation l'écraserait. Les corrections se font dans le dépôt.

Si quelque chose ne se passe pas comme décrit, arrêtez-vous et vérifiez — rien ici n'est urgent.

---

## 1. Avant tout : les trois vérifications

Ouvrez PowerShell dans ce dossier (clic droit dans l'explorateur → *Ouvrir dans le Terminal*), puis :

```powershell
python tests\test_depot.py
```

La dernière ligne doit afficher **`DEPOT CONFORME`**. Ce script vérifie notamment qu'aucun mot de
passe ne s'est glissé dans un fichier, qu'aucun chemin de votre ordinateur ne traîne, et qu'aucun
firmware compilé n'est présent.

```powershell
python tests\run_tests.py
python tests\test_langues_voix.py
```

Les deux doivent se terminer par `TOUTES LES SUITES PASSENT` et `BANQUES COHERENTES`.

> Si `run_tests.py` dit qu'il ne trouve pas de compilateur C, ce n'est pas grave pour publier :
> c'est juste que les tests ne peuvent pas s'exécuter sur cet ordinateur.

---

## 2. Compléter deux fichiers

**`LICENSE`** — la ligne `Copyright (c) 2026 Razine` : mettez le nom ou le pseudonyme sous lequel
vous voulez apparaître. C'est le seul endroit du dépôt où votre nom figure obligatoirement.

**`CREDITS.md`** — complétez ou raccourcissez à votre goût. Vous pouvez aussi le supprimer
entièrement si vous préférez rester anonyme.

Rien d'autre dans le dépôt ne vous identifie : ni adresse e-mail, ni compte, ni lien.

---

## 3. Créer le dépôt sur GitHub

1. Allez sur [github.com](https://github.com) et connectez-vous (créez un compte si besoin).
2. En haut à droite : **+** → **New repository**.
3. Remplissez :
   - **Repository name** : `waveshare-glycemie` (ou le nom que vous voulez)
   - **Description** : *Afficheur familial de glycémie Dexcom G7 sur Waveshare ESP32-P4 7″*
   - Cochez **Private** pour commencer. Vous le rendrez public quand vous serez à l'aise.
   - **Ne cochez rien d'autre** : pas de README, pas de .gitignore, pas de licence —
     ils sont déjà dans ce dossier, et GitHub créerait des doublons gênants.
4. **Create repository**.

GitHub affiche alors une page avec des commandes. Gardez-la ouverte.

---

## 4. Envoyer le projet

Toujours dans PowerShell, dans ce dossier :

```powershell
git status
```

Vous devez voir `nothing to commit, working tree clean` : l'historique est déjà prêt.

Si Git vous est inconnu, dites-lui d'abord qui vous êtes (une seule fois sur l'ordinateur) :

```powershell
git config --global user.name "Votre nom"
git config --global user.email "votre.adresse@example.com"
```

> **Votre adresse apparaîtra dans l'historique public.** Chaque enregistrement (*commit*) porte le
> nom et l'adresse configurés ici, et l'historique part avec le dépôt. Si vous ne voulez pas publier
> votre adresse personnelle, utilisez l'adresse masquée que GitHub vous fournit
> (*Settings → Emails → Keep my email addresses private*, de la forme
> `12345678+votrecompte@users.noreply.github.com`) **avant** le premier envoi. Les enregistrements
> déjà faits gardent l'ancienne adresse : les réécrire change tous leurs identifiants, ce n'est
> utile que si le dépôt n'a jamais été envoyé.
>
> L'archive publique fabriquée par `tools\archive_publique.py` ne contient **pas** l'historique :
> c'est une photo du projet, sans aucune adresse.

Puis, en remplaçant `VOTRE-COMPTE` et `waveshare-glycemie` :

```powershell
git remote add origin https://github.com/VOTRE-COMPTE/waveshare-glycemie.git
git branch -M main
git push -u origin main
```

Git ouvrira une fenêtre pour vous connecter à GitHub. Acceptez.

Rafraîchissez la page GitHub : le projet est en ligne.

---

## 5. Soigner la page d'accueil

Sur la page du dépôt, roue dentée à droite de **About** :

- **Description** : *Afficheur familial de glycémie Dexcom G7 sur Waveshare ESP32-P4 7″ — projet
  personnel, pas un dispositif médical.*
- **Topics** : `esp32-p4`, `esp-idf`, `lvgl`, `dexcom`, `diabetes`, `waveshare`, `cgm`

Puis **Settings → Code security** : activez **Secret scanning** et **Push protection**. GitHub
bloquera automatiquement un futur envoi qui contiendrait un mot de passe.

C'est `README.md` (en anglais) qui s'affiche sur la page d'accueil ; il renvoie vers
`README_FR.md` dès la deuxième ligne.

---

## 6. Rendre public

Quand vous êtes prêt : **Settings → General → tout en bas, Danger Zone → Change visibility →
Make public**.

Avant de cliquer, relisez une dernière fois **toutes** les captures d'écran du dossier
`screenshots/` en grand (celles de l'écran et celles de l'application Windows, dans
`screenshots/pc_app/`). Elles ont été prises en mode démonstration ou avec des données inventées —
les valeurs affichées sont fictives, et les dates de pose réelles sont masquées sur la page
CONSOMMABLES. C'est le dernier filet de sécurité : un contrôle automatique lit les fichiers, pas
les images.

---

## 7. Les fichiers prêts à installer (Releases)

Trois choses ne sont **pas** dans le dépôt, parce qu'elles se refabriquent : le programme compilé
de l'écran, l'application Windows et les banques de voix. Elles se publient en **pièces jointes
d'une Release** (page du dépôt → *Releases* → *Draft a new release*) :

| Pièce jointe | Fabriquée par | Pour qui |
|---|---|---|
| `glycemie_display.bin` + `bootloader.bin` + `partition-table.bin` | `tools\build.ps1` depuis le dépôt **public** (sans `private\device_secrets.h`) | installer sans compiler |
| `GlycemieConnexion.exe` | `tools\pc_app\construire_exe.ps1` | configurer l'écran depuis le PC |
| `glycemie-voix-<langue>-k21.zip` (six langues) | déjà préparées dans `dist_voix\` | copier les sons sur la microSD |

Publiez aussi le fichier d'empreintes `SHA256SUMS.txt` : il permet à chacun de vérifier qu'il a bien
reçu les mêmes fichiers que vous.

> Ne publiez **jamais** le programme compilé depuis votre propre dossier : il contient vos
> identifiants. Celui du dépôt public n'en contient aucun — c'est vérifié par
> `tools\controle_donnees_perso.py`.

---

## Ce qu'il ne faut jamais ajouter au dépôt

| À ne pas publier | Pourquoi |
|---|---|
| `private/device_secrets.h` rempli | identifiants Wi-Fi et Dexcom en clair |
| le dossier `build/`, un fichier `.bin` ou `.elf` | **les identifiants y sont compilés** |
| une sauvegarde de la flash (32 Mo) | peut contenir identifiants et données de santé |
| `tools/logs/` | journaux de mise au point, avec des glycémies réelles |
| les fichiers CSV de la microSD | historique glycémique réel |
| les banques vocales (fichiers WAV) | 130 Mo, et audio produit par un service tiers |
| une capture montrant une vraie valeur, une date de pose ou un prénom | données personnelles |

Le fichier `.gitignore` couvre déjà tous ces cas. `python tests\test_depot.py` le revérifie.

> **Si un mot de passe est publié par erreur** : changez-le immédiatement chez Dexcom et sur votre
> box Wi-Fi. C'est la seule mesure efficace. Supprimer le fichier ensuite ne suffit pas : il reste
> dans l'historique, dans les copies et dans les caches.

---

## Et après ?

Les gens qui ont la même carte voudront savoir par où commencer. Tout est déjà écrit :

- `README.md` / `README_FR.md` — ce que fait l'appareil, avec les captures
- `docs/INSTALLATION_FR.md` — installer ESP-IDF, compiler, flasher
- `docs/CONFIGURATION_DEXCOM_FR.md` — le compte Dexcom Share
- `docs/DEPANNAGE_FR.md` — les pannes courantes
- `docs/LIMITATIONS_FR.md` — ce que le projet ne fait pas, dit franchement

Si quelqu'un ouvre une *issue*, vous n'êtes obligé à rien : c'est un projet personnel partagé
gratuitement. Vous pouvez désactiver les *issues* dans **Settings → General → Features**.
