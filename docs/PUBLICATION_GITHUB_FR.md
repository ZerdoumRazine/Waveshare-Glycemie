# Publier ce dépôt sur GitHub

Ce dépôt est **prêt localement** mais **n'a pas été publié**. La décision, le compte et le
nom du dépôt appartiennent au développeur principal.

---

## 1. Avant de publier : sept vérifications

### a. Lancer les contrôles automatiques

```powershell
python tests\test_depot.py
python tests\run_tests.py
```

`test_depot.py` vérifie notamment qu'aucune valeur de `private/device_secrets.h` n'apparaît
dans un fichier versionné, qu'aucun chemin absolu de la machine ne subsiste, qu'aucun
binaire de firmware n'est présent et qu'aucun fichier ne dépasse 2 Mo.

### b. Vérifier ce que Git va réellement envoyer

```powershell
git ls-files | Measure-Object            # nombre de fichiers suivis
git ls-files | Select-String "private|\.bin|\.elf|logs"   # doit être vide
```

### c. Vérifier l'historique, pas seulement l'état actuel

Un secret supprimé dans un commit ultérieur **reste** dans l'historique.

```powershell
git log --all --name-only --format="" | Sort-Object -Unique
```

Si un fichier sensible apparaît, le plus simple et le plus sûr est de **repartir d'un
historique neuf** : supprimer `.git`, refaire `git init` et un unique commit initial.

### d. Relire les captures d'écran

`screenshots/` contient de **vraies** captures de la carte. Elles ont été prises en mode
démonstration (données fictives, bandeau « MODE DÉMO ») et les dates de pose réelles sont
masquées sur la page CONSOMMABLES. **Toute capture ajoutée plus tard doit être revérifiée** :
une glycémie réelle, une date de pose, un SSID ou un prénom ne doivent jamais y figurer.

### e. Compléter la licence et les crédits

- `LICENSE` contient `<A COMPLETER : prenom / pseudonyme du developpeur principal>`.
- `CREDITS.md` est un modèle à remplir, ou à supprimer si vous préférez rester anonyme.

Rien dans ce dépôt ne désigne une personne : c'est volontaire. Vous choisissez ce que vous
rendez public.

### f. Vérifier les licences tierces

Lire [LICENCES_TIERCES_FR.md](LICENCES_TIERCES_FR.md). Deux points demandent une décision :

1. `main/voice/voice_fr.pcm` — audio produit par la synthèse vocale de Windows. Le garder
   ou le retirer (il est régénérable en une commande). Le projet ne revendique aucun droit
   dessus.
2. La banque vocale Google n'est **pas** dans le dépôt : ne pas l'y ajouter.

### g. Relire l'avertissement médical

Il doit rester visible en tête du `README.md` et du `README_FR.md`, et dans `LICENSE`.
Ce projet touche à la santé d'un enfant : l'avertissement n'est pas une formalité.

---

## 2. Ne jamais publier

| Élément | Pourquoi |
|---|---|
| `private/device_secrets.h` | identifiants Wi-Fi et Dexcom en clair |
| `build/`, tout `.bin` ou `.elf` compilé | les identifiants y sont **compilés** |
| Sauvegardes de la flash (32 Mo) | peuvent contenir identifiants et données de santé |
| `tools/logs/` | journaux de mise au point contenant des glycémies réelles |
| Export NVS, `consumables.csv`, `history/*.csv` | données de santé et dates de pose réelles |
| Captures montrant une vraie valeur, une date de pose, un SSID, un prénom | données personnelles |
| Banque vocale générée (`tools/voice/bank/`, `tools/voice/google_raw/`) | volume (142 Mo) et conditions d'un service tiers ; les WAV finalisés se publient en **pièce jointe de Release**, le cache brut jamais |
| `main/voice/voice_fr.pcm` | sortie du moteur de synthèse Windows, droits de redistribution non établis (le firmware compile sans) |
| `dist_voix/` | paquets préparés pour une Release : volumineux, hors dépôt |

Le `.gitignore` couvre déjà tous ces cas, mais **le relire une fois** avant de publier.

> ⚠️ **Un `.gitignore` ne nettoie pas une archive ZIP fabriquée en copiant un dossier.**
> Une archive publique se fabrique **uniquement** à partir de ce que Git suit :
>
> ```powershell
> git -C "<racine du depot>" archive --format=zip -o "$env:TEMP\depot-public.zip" HEAD
> ```
>
> Vérifier ensuite la liste : `Expand-Archive`, ou
> `python -c "import zipfile,sys;print('\n'.join(zipfile.ZipFile(sys.argv[1]).namelist()))" <zip>`.
> Aucun `tools/logs/`, aucun `private/`, aucun `.bin` ne doit y figurer.

> Un dépôt public est archivé et indexé. Un secret publié puis supprimé doit être considéré
> comme **compromis** : il faut changer le mot de passe, pas seulement effacer le fichier.

---

## 3. Publier

```powershell
cd "<racine du depot>"
git status
git log --oneline

# Créer le dépôt distant (GitHub CLI), en PRIVÉ d'abord
gh repo create <nom-du-depot> --private --source=. --remote=origin --push
```

Rester **privé** quelques jours est une bonne idée : cela laisse le temps de relire
tranquillement avant de rendre public.

Sans GitHub CLI :

```powershell
git remote add origin https://github.com/<compte>/<nom-du-depot>.git
git branch -M main
git push -u origin main
```

Pour rendre public ensuite : *Settings → General → Danger Zone → Change visibility*.

### Publier les banques vocales (pièces jointes d'une version)

Les six paquets (19 à 25 Mo chacun) ne vont **pas** dans le dépôt : ils sont joints à une version.
Ils sont préparés et vérifiés hors dépôt, dans `dist_voix/` (voir [TUTORIEL_VOIX_FR.md](TUTORIEL_VOIX_FR.md)).

```powershell
gh release create voix-k21 `
    dist_voix\glycemie-voix-*.zip dist_voix\SHA256SUMS-paquets.txt `
    --title "Banques vocales K21 (6 langues)" `
    --notes-file dist_voix\NOTES_RELEASE.md
```

Rappels : indiquer le service et la voix utilisés, joindre `SHA256SUMS-paquets.txt`, ne pas présenter
Google comme partenaire du projet, et conserver l'avertissement médical.

---

## 4. Réglages conseillés du dépôt

- **Description** : « Afficheur familial de glycémie Dexcom G7 sur Waveshare ESP32-P4 7″ —
  projet personnel, pas un dispositif médical ».
- **Sujets** : `esp32-p4`, `esp-idf`, `lvgl`, `dexcom`, `diabetes`, `waveshare`.
- **Langue d'affichage** : GitHub montre `README.md` (anglais) par défaut, qui renvoie vers
  `README_FR.md`.
- Activer *Settings → Code security → Secret scanning* et *Push protection* : GitHub
  bloquera un futur commit contenant un secret reconnu.
- Désactiver les *Issues* si vous ne souhaitez pas de retours.

---

## 5. Après publication

- Vérifier que `private/` est bien **vide** sur la page du dépôt (seul `.gitignore` doit y
  figurer).
- Ouvrir une capture d'écran depuis le site et la regarder en grand : c'est le dernier
  filet de sécurité contre une donnée personnelle oubliée.
- Vérifier qu'aucun fichier n'est anormalement volumineux (onglet *Insights → Repository
  size*).

---

## 6. Si un secret a été publié par erreur

1. **Changer immédiatement** le mot de passe Dexcom et la clé Wi-Fi concernés. C'est la
   seule mesure réellement efficace.
2. Rendre le dépôt privé ou le supprimer.
3. Reconstruire un historique propre (`.git` supprimé, nouveau commit initial) avant de
   republier.

Supprimer le fichier dans un nouveau commit **ne suffit pas** : il reste dans l'historique,
dans les forks et dans les caches.
