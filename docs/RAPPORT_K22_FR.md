# Rapport technique — K22 : configuration par l'utilisateur, application Windows, version publique

État au **17 octobre 2026**, dépôt de travail local, branche `main`.
Ce rapport est le détail technique ; le résumé lisible est dans la conversation et dans
[TUTORIEL_DEBUTANT_FR.md](TUTORIEL_DEBUTANT_FR.md).

**Rien n'a été installé sur l'appareil, rien n'a été publié.** Tout ce qui suit a été vérifié sur le
PC : compilation, tests automatiques, essais simulés. Les essais sur l'écran restent à faire.

---

## 1. Ce qui a été fait, par point de la demande

### 1.1 Les deux assistants de connexion

**Sur l'écran** (`main/ui_setup.c`, 6 étapes) : langue → réseau Wi‑Fi (liste des réseaux détectés,
tri par puissance) → mot de passe (clavier LVGL, caractères masqués, bouton « Afficher ») → compte
Dexcom + région (3 boutons) → essai de connexion → **confirmation**. L'assistant s'ouvre tout seul
tant que rien n'est enregistré, et se rouvre depuis RÉGLAGES → CONNEXION → Modifier.

**Menu CONNEXION** (`main/ui_settings.c`, section à y = 1510) : accessible en permanence, sans
dépendance réseau — il ne lit que la mémoire interne. Affiche le nom du réseau et le compte masqué
(`a***@exemple.fr`), jamais un mot de passe ; boutons **Modifier**, **Effacer** (avec fenêtre de
confirmation), interrupteur **mode démonstration**, et un bandeau « essai en cours » quand des
accès sont à l'essai.

**Sur le PC** : `tools/pc_app/glycemie_connexion.pyw` (tkinter) → `GlycemieConnexion.exe`
(PyInstaller, un seul fichier, 10,04 Mo, sans console). L'utilisateur final n'installe **ni Python
ni bibliothèque**. Le dialogue série est celui de `tools/assistant_config.py`, déjà testé. Les
échanges se font dans un fil séparé : la fenêtre ne se fige pas. Construction :
`tools\pc_app\construire_exe.ps1`.

**Détection du port** : noms lisibles obtenus par PowerShell (`Win32_PnPEntity`), reconnaissance des
puces de liaison (CH343, CP210x, FTDI, CDC…). **S'il y a plusieurs candidats, aucun n'est choisi** :
l'utilisateur sélectionne dans la liste, et la fenêtre explique comment reconnaître le bon
(débrancher / rebrancher). Sur ce PC, la carte apparaît comme `USB-Enhanced-SERIAL CH343 (COM7)`.

### 1.2 Ne jamais remplacer des accès qui fonctionnent

Mécanisme d'**essai** (`credentials_trial_*` dans `main/credentials.c`) :

| Étape | Mémoire vive | Mémoire permanente (NVS) |
|---|---|---|
| `credentials_trial_begin()` | photographie des accès actuels | inchangée |
| `credentials_trial_set_wifi/dexcom()` | nouvelles valeurs (donc utilisées par l'essai de connexion) | **inchangée** |
| échec, annulation, coupure de courant, redémarrage | retour aux accès précédents | **inchangée** |
| `credentials_trial_commit()` (après réussite **et** confirmation) | nouvelles valeurs | écrite |

Conséquence : il n'existe aucun instant où l'appareil se retrouve sans ses accès précédents.
La console applique la même règle : `ident_enregistrer` **refuse** tant que le dernier essai n'est
pas réussi (`ident_enregistrer force` pour passer outre, volontairement explicite).

Protection des identifiants, dite franchement : espace NVS `ident` séparé, bloc brouillé par OU
exclusif avec une clé dérivée de l'adresse MAC. **Ce n'est pas un chiffrement.** Le Flash Encryption
et le Secure Boot ne sont pas activés (irréversibles, risque de brique). Les mots de passe ne sont
jamais journalisés : seules des formes masquées sortent du module (`credentials_masked_*`).

### 1.3 Protection de l'appareil avant installation

`tools/sauvegarde_appareil.py` : identification (puce, révision, flash, table des partitions), copie
dans `private\sauvegardes\<date>\`, **vérification par relecture** (`esptool verify_flash`) et
empreintes SHA‑256 dans `manifeste.json`. L'outil **refuse** toute commande d'écriture ou
d'effacement (liste noire explicite : `erase_flash`, `write_flash`, `burn_efuse`…).

[RETOUR_ARRIERE_FR.md](RETOUR_ARRIERE_FR.md) décrit : ce que l'installation change et ne change pas,
les interruptions (deux redémarrages, une à deux minutes sans affichage), où sont les accès actuels
et comment ils passent dans la nouvelle version, la commande de retour en arrière, et la procédure
de récupération par le bouton **BOOT** si l'écran ne démarre plus — en précisant que, dans ce cas,
le menu CONNEXION et l'application Windows ne suffisent pas.

**Transfert des accès actuels** : ils sont aujourd'hui *compilés* dans le firmware depuis
`private/device_secrets.h`. La nouvelle version les reprend automatiquement tant que la mémoire
`ident` est vide (`credentials_init()`), donc un firmware compilé depuis le dossier personnel se
reconnecte sans aucune saisie. Aucune récupération depuis l'appareil n'est nécessaire — donc aucun
mot de passe à relire ni à faire transiter.

### 1.4 Commandes vocales et annonces

Inchangé depuis K21 et conforme à la demande : `GLY_VOICE_CMD=OFF` par défaut,
`main/voice_cmd_stub.c` fournit la même interface **sans jamais ouvrir le micro**. Raison : ESP‑SR
ne reconnaît que l'anglais et le chinois ; les phrases françaises approchées déclenchaient de vrais
faux positifs (« New sensor » reconnu dans une phrase anglaise de télévision, score 0,1368–0,1514).

**Conservés** : annonces parlées dans les six langues, alertes sonores, accès tactile à toutes les
fonctions. Les six banques (396 fichiers chacune) sont vérifiées : `tests/test_langues_voix.py`
→ 56 contrôles, 0 échec. Paquets prêts à copier dans `dist_voix\` avec manifestes, empreintes et
notice de provenance (`NOTES_RELEASE.md`, `paquets.json`). **Aucune génération payante n'a été
relancée.**

Pour un appareil personnel, la variante avec commandes vocales se recompile :
`tools\build.ps1 -Vocal` (vérifiée : 6,19 Mo, 26 % libres).

### 1.5 Version publique (Razine)

`tools/preparer_razine.py` reconstruit la copie publiable **depuis `git archive HEAD`** : seuls les
fichiers suivis par Git en font partie, donc jamais `private\`, ni journaux, ni binaires. La copie
n'est remplacée **qu'après** vérification ; le dépôt de publication `Razine\.git` est conservé.

Résultat : **157 fichiers, 3,90 Mo**, contenant le code commenté, les tests, les outils, les sources
de l'application Windows, les tutoriels, les captures, les licences et les crédits.

`tools/publier_firmware.ps1` compile le programme **public** depuis cette copie (sans
`private/device_secrets.h`, donc sans aucun identifiant) et prépare `livraison\` :

| Fichier | Taille | SHA‑256 |
|---|---|---|
| `glycemie_display.bin` | 2,25 Mo | `eaaa656902d6aa73…dac86e46` |
| `bootloader.bin` | 0,02 Mo | `ad079243a1665768…b23cc353c` (voir `SHA256SUMS.txt`) |
| `partition-table.bin` | 3 Ko | voir `SHA256SUMS.txt` |
| `GlycemieConnexion.exe` | 10,04 Mo | `042da82470f6d63f…6d88770b` |
| `installer.ps1` | 2 Ko | voir `SHA256SUMS.txt` |

Archive du dépôt : `dist_voix\glycemie-depot-public-k22.zip`, 180 fichiers,
SHA‑256 `ab654adaae8d69db81759e484df15e66e9a0495d7bfcfea65e45a3ce35b8ccd1`. L'archive K21, périmée,
a été supprimée.

**Le PCM Microsoft** (`main/voice/voice_fr.pcm`) n'est plus suivi par Git : absent de l'archive et
de la copie publique, toujours présent sur le disque pour une compilation locale
(`GLY_VOICE_OLD_PCM`). *Réserve : il figure encore dans les anciens enregistrements de
l'historique ; une publication de l'historique l'emporterait.*

### 1.6 Contrôles de confidentialité

`tools/controle_donnees_perso.py` et `tools/archive_publique.py` ont été corrigés :

- extensions reconnues **quelle que soit la casse** (`re.IGNORECASE`) ;
- le contenu de l'archive est **extrait puis relu** (pas seulement les noms de fichiers) ;
- les fichiers livrés à côté (programme compilé, `.exe`) sont examinés avec `--livre` ;
- un contrôle qui lit trop peu de fichiers, ou qui ne peut pas comparer aux identifiants réels,
  renvoie **2** et dit qu'il n'est pas concluant — il n'annonce plus de réussite ;
- une archive refusée est **supprimée** ;
- des modifications non enregistrées font **refuser** l'archive (elle ne correspondrait pas à la
  version terminée) ;
- l'historique Git est relu **binaires compris**.

Résultats (17 octobre 2026) :

| Contrôle | Résultat |
|---|---|
| 160 fichiers de l'archive publique, contenu relu | aucun identifiant, aucune adresse, aucun chemin personnel |
| `livraison\glycemie_display.bin` (2,25 Mo) | propre |
| `livraison\GlycemieConnexion.exe` (10,04 Mo) | propre |
| Contrôle à l'envers sur le firmware **personnel** | les 4 identifiants sont bien détectés → l'outil sait les voir |
| Historique Git (245 objets) | rien, hormis deux chemins locaux dans d'anciennes versions de deux documents (corrigés depuis) |
| Adresses des enregistrements | une adresse personnelle et une adresse masquée GitHub apparaissent dans les enregistrements — **décision à prendre avant de pousser** (voir [COMMENT_PUBLIER_FR.md](COMMENT_PUBLIER_FR.md)) |
| 9 captures distribuées, **regardées une par une** | mode démonstration, valeurs fictives, dates masquées, aucun mot de passe, aucun prénom |

Deux réserves honnêtes :

1. les captures `05_reglages.png` et `06_reglages_langue.png` montrent l'**ancienne** page RÉGLAGES
   (section des commandes vocales, phrase « les annonces et commandes vocales suivent le français »)
   : elles sont à refaire après la mise à jour de l'écran ;
2. aucun contrôle automatique ne lit le contenu *visible* d'une image. La relecture humaine reste
   le dernier filet.

---

## 2. Tests : ce qui a réellement été exécuté

| Suite | Nature | Contrôles | Résultat |
|---|---|---|---|
| `tests/run_tests.py` → `test_credentials` | **code C réel** compilé sur le PC, NVS simulée | 110 | 0 échec |
| `tests/run_tests.py` → `test_glucose_math`, `test_i18n`, `test_consommables` | code C réel | 65 + … | 0 échec |
| `tests/test_assistant_config.py` | **simulation** : appareil factice, aucune liaison série | 73 | 0 échec |
| `tests/test_pc_app.py` | fenêtre Tk réellement construite, ports simulés | 26 | 0 échec |
| `tests/test_sd_voice_upload.py` | **simulation** : `--verify-only` n'écrit rien | 79 | 0 échec |
| `tests/test_langues_voix.py` | banques réelles sur le disque | 56 | 0 échec |
| `tests/test_depot.py` | dépôt réel | 42 | 0 échec |
| Compilation personnelle (`tools\build.ps1`) | **compilation réelle** | — | 3,53 Mo, 58 % libres |
| Compilation personnelle avec commandes vocales (`-Vocal`) | compilation réelle | — | 6,19 Mo, 26 % libres |
| Compilation publique (depuis Razine) | compilation réelle | — | 2,25 Mo, 73 % libres |
| Application Windows | **lancée réellement**, fenêtre capturée | — | démarre, fenêtre correcte |

Ce que ces essais **ne prouvent pas** : rien n'a été essayé sur l'écran. Le tactile, la liaison
série réelle, la connexion Wi‑Fi, l'authentification Dexcom, les annonces sonores et la persistance
après redémarrage **restent à vérifier sur l'appareil**.

Le point le plus important à vérifier en vrai : **l'essai suivi d'un échec doit laisser l'appareil
avec ses accès précédents**. C'est prouvé sur le PC (test `Essai refusé : retour automatique aux
accès précédents`), pas encore sur l'écran.

---

## 2 bis. Essais RÉELLEMENT faits sur l'écran (17 octobre 2026)

Installation : partition `factory` seule (`idf.py app-flash`), 3 534 352 octets, empreinte vérifiée
par esptool. La mémoire des réglages, la table des partitions et la microSD n'ont pas été touchées.

| Essai | Comment | Résultat |
|---|---|---|
| Sauvegarde préalable de la mémoire | lecture complète 32 Mo + `verify_flash` | vérifiée, empreinte enregistrée ; relue ensuite : 5 partitions conformes, amorce et application valides, NVS écrite à 60 % |
| Récupération de l'historique | `sd_dump` sur les 36 fichiers | 36/36 copiés, **empreinte CRC-32 vérifiée pour chacun** |
| Fidélité de l'historique | comparaison avec l'inventaire pris **avant** la mise à jour | 36/36 : même nombre de mesures, **même première et même dernière ligne**. 9 973 mesures |
| Journaux | `sd_lire` | 35 fichiers, taille annoncée = taille reçue ; une seconde lecture donne les mêmes octets |
| Banques vocales | `vrx_list crc` contre la banque du PC | 396/396 identiques : aucun son à retransférer |
| Reconnexion Wi-Fi | `info` après redémarrage | connecté, **même adresse IP**, ‑52 dBm |
| Reconnexion Dexcom | `info` | en service, mesure fraîche reçue |
| Dates de consommables | `conso_info`, `stock_info` | capteur et cathéter conservés à la seconde, rappels et stocks inchangés |
| Réglages | `reglages` | volume, seuils, mode nuit conservés |
| Identifiants | `ident_info` | repris de la compilation (`origine=compilation`), rien à saisir |
| Commandes vocales | `info` | `vcmd=0` : absentes, micro jamais ouvert |
| Alerte sonore et annonce parlée | `test_sound`, `test_voice` | **entendues par l'utilisateur** |
| Tactile | `touch_info` après usage | 23 appuis, 11 clics, 11 défilements, **0 erreur de lecture** |
| Ouverture de l'assistant au toucher | appui sur « Modifier », trace tactile | appui détecté sur le bon bouton, puis `Essai de nouveaux acces` et `Assistant ouvert (modification)` |
| Recherche des réseaux Wi-Fi | assistant, étape 2 | **6 réseaux affichés**, dont le sien |
| Retour sans rien changer | « Retour » puis « Annuler » | `essai=0`, même réseau, même compte, mots de passe inchangés (27 et 11 caractères) |
| Page CONNEXION | capture `snap` | affichée correctement, mot de passe nulle part ; l'état « essai en cours » s'affiche pendant un essai |

Deux limites rencontrées, honnêtement :

1. **La commande `snap` ne peut pas photographier l'assistant** : elle capture `lv_screen_active()`,
   or l'assistant (et la fenêtre de confirmation d'effacement) vivent sur `lv_layer_top()`. Les
   pages de l'assistant manquent donc aux captures ; l'utilisateur a fourni une photo. Correction
   possible dans une prochaine version (capturer aussi la couche supérieure).
2. `tools/serial_capture.py` lancé en arrière-plan a survécu à l'arrêt de son enveloppe et a gardé
   le port série ; il faut le lancer au premier plan, ou vérifier qu'il s'est bien terminé.

Texte corrigé au passage : l'aide de la section LANGUE annonçait encore « les annonces vocales et
les commandes vocales restent en français » — faux depuis K21 (chaque langue a sa banque) et depuis
K22 (plus de commandes vocales). Le nouveau texte s'appliquera à la prochaine mise à jour.

---

## 2 ter. Livraison préparée (18 octobre 2026)

Rien n'a été publié : aucun compte GitHub utilisé, aucun dépôt créé, aucun fichier envoyé.

| Élément | Où | Contenu |
|---|---|---|
| Dépôt prêt à envoyer | `Razine\` | 167 fichiers, 2,2 Mo. Historique de publication **neuf** : un seul enregistrement, au nom de Mohamed-Razine Zerdoum, avec une adresse GitHub masquée |
| Dossier à remettre | `paquet_a_remettre\` | 2 612 fichiers, 299,7 Mo : `code_source\`, `application_windows\`, `installation_ecran\`, `microSD\`, `tutoriels\`, `telechargements\`, `notices\`, `LISEZMOI.txt` (FR+EN), `SHA256SUMS.txt` |
| Archives à distribuer | `paquet_a_remettre	elechargements\` | installation de l'écran (1,3 Mo), application Windows (11 Mo), microSD (130 Mo), code source (1,1 Mo), empreintes |
| Archive complète | `livraison\glycemie-paquet-complet.zip` | tout le paquet en un fichier, avec son empreinte |
| Préparation GitHub | `publication_github\` | nom du dépôt, descriptions FR/EN, présentation, ordre des captures, description des téléchargements, notes de version, et le guide pas à pas dans le navigateur |

Trois défauts réels trouvés pendant cette préparation, tous corrigés :

1. **Le programme public livré était plus ancien que les sources** — compilé deux minutes avant la
   correction des captures d'écran, il ne la contenait pas. La préparation du paquet compare
   désormais la date du programme à celle de chaque source du firmware et refuse de continuer.
2. **L'historique du dépôt de publication contenait encore l'ancienne voix Windows** dont les
   droits ne sont pas établis. L'historique part avec le dépôt : il a été refait d'un seul
   enregistrement. L'historique de travail, lui, n'a pas été touché.
3. **Le tutoriel décrivait une mauvaise arborescence pour les sons** : le firmware
   (`main/voice.c`) attend le français **directement** dans `glycemie/audio/voice/`, et les autres
   langues dans `en/ es/ de/ it/ pt/`. C'est ce que contiennent les paquets ; le texte, lui, était
   faux.

Le contrôle de confidentialité a aussi été précisé : chercher des adresses de courriel dans des
octets compressés ou dans du signal audio ne produisait que du hasard, et ces fausses alertes
auraient fini par masquer les vraies.

---

## 3. Fichiers ajoutés ou modifiés (K22)

```
main/credentials.c/.h        essai, confirmation, retour en arriere, masquage
main/ui_setup.c              assistant tactile (6 etapes) + confirmation
main/ui_settings.c           section CONNEXION + etat d'essai
main/app_main.c              ident_enregistrer / ident_annuler
main/i18n.c/.h               5 nouveaux textes x 6 langues
tools/assistant_config.py    assistant USB (ligne de commande) + noms de ports lisibles
tools/pc_app/                application Windows : source, construction, captures
tools/sauvegarde_appareil.py sauvegarde verifiee, lecture seule
tools/preparer_razine.py     copie publique depuis git archive
tools/publier_firmware.ps1   programme public + .exe + empreintes
tools/archive_publique.py    controles renforces
tools/controle_donnees_perso.py  idem + historique binaire
tests/hote/test_credentials.c    110 controles
tests/test_assistant_config.py   73 controles
tests/test_pc_app.py             26 controles
docs/TUTORIEL_DEBUTANT_FR.md, docs/BEGINNER_TUTORIAL_EN.md
docs/RETOUR_ARRIERE_FR.md, docs/COMMENT_PUBLIER_FR.md, docs/RAPPORT_K22_FR.md
```

Enregistrements : `93f820f` → `1d570ca`. Rien n'a été poussé.

---

## 4. Points non résolus

1. **Captures des pages de l'assistant** : impossibles tant que `snap` ne photographie que l'écran
   de base (voir 2 bis). À corriger dans une prochaine version, ou à remplacer par des photos.
2. **Captures à refaire** : `05_reglages.png` et `06_reglages_langue.png` montrent l'ancienne page
   RÉGLAGES (section des commandes vocales) et l'ancien texte d'aide de la langue.
3. **Photos du matériel** : prises USB‑C, bouton BOOT, connecteur du haut‑parleur, microSD en place.
4. **Historique Git** : le PCM Microsoft et deux chemins personnels y figurent encore ; à décider
   avant de publier l'historique (l'archive, elle, est propre).
5. **Adresse des enregistrements** : à choisir avant le premier envoi vers GitHub.
6. **Alimentation** : Waveshare ne publie aucune valeur officielle. Le tutoriel recommande 5 V / 2 A
   en le signalant comme une recommandation du projet, pas du constructeur.
7. **Application Windows non signée** : SmartScreen affichera « Éditeur inconnu ». Signer demande un
   certificat payant ; l'empreinte SHA‑256 est publiée à la place.
