# Sauvegarder, revenir en arrière, dépanner un écran qui ne démarre plus

Ce document sert **avant** une mise à jour, et **si quelque chose se passe mal**. Il ne concerne que
l'écran (processeur ESP32-P4). Le second processeur, l'ESP32-C6 qui assure le Wi-Fi, **n'est jamais
touché** : ni effacé, ni reprogrammé.

---

## 1. Ce que la mise à jour change, et ce qu'elle ne change pas

**Elle remplace** une seule chose : le programme de l'écran (partition `app`, environ 3,5 Mo).

**Elle ne touche pas :**

| Ce qui est conservé | Où c'est rangé |
|---|---|
| Langue, volume, mode nuit, seuils | mémoire des réglages (`nvs`, espace `settings`) |
| Dates de capteur et de cathéter, stock | mémoire des réglages (`nvs`, espaces consommables) |
| Historique des mesures, journaux | microSD |
| Banques de voix (six langues) | microSD |
| Réseau Wi-Fi et compte Dexcom | voir le point 3 ci-dessous |

**Aucune de ces opérations n'est utilisée** : `erase_flash` (effacement général), écriture d'eFuses,
Secure Boot, chiffrement de la flash, formatage de la microSD. Elles sont irréversibles ou rendent
l'appareil non récupérable : elles ne seront jamais faites sans une autorisation explicite et séparée.

**Interruptions à prévoir :** l'écran redémarre deux fois (une fois pour entrer en mode programmation,
une fois après), et reste éteint pendant l'écriture, environ **une à deux minutes**. Pendant ce temps,
aucune mesure n'est affichée. Le téléphone et le récepteur Dexcom officiels continuent de fonctionner
normalement : ils ne dépendent pas de cet écran.

---

## 2. Sauvegarder AVANT (obligatoire)

Branchez l'écran au PC avec un câble USB-C **de données**, puis :

```bash
python tools\sauvegarde_appareil.py --complet
```

L'outil lit l'appareil, écrit la copie dans `private\sauvegardes\<date>\`, la **relit et la compare**
à l'appareil (`verify_flash`), et enregistre les empreintes SHA-256 dans `manifeste.json`.
Il n'écrit jamais dans l'appareil.

- `--identifier` : identification seule, rien n'est copié.
- sans option : seules les parties utiles (programme, table des partitions, réglages) sont copiées.
- `--complet` : toute la mémoire (32 Mo) — c'est la sauvegarde qui permet de **tout** remettre comme avant.

Ces sauvegardes restent **privées** : le dossier `private\` n'est jamais publié, et les archives
publiques sont vérifiées pour s'en assurer.

> Gardez une copie du dossier de sauvegarde ailleurs que sur le PC (clé USB, disque externe).
> Une sauvegarde sur le seul disque qui peut tomber en panne n'est pas une sauvegarde.

**Sauvegardez aussi la microSD.** Deux façons :

- *avec un lecteur de carte* : écran éteint, carte dans le PC, puis

  ```bash
  python tools\sauvegarde_microsd.py --lecteur E:
  ```

  La carte n'est que lue ; chaque fichier est recopié puis **relu des deux côtés et comparé**.

- *sans lecteur de carte, par le câble USB* : la carte reste dans l'appareil.

  ```bash
  python tools\recuperer_microsd.py --port COM7
  ```

  L'outil fait l'inventaire, récupère les journaux et l'**historique** (commande `sd_dump`,
  présente depuis K22), vérifie l'empreinte CRC-32 de chaque fichier, et compare les sons de la
  carte à une banque déjà présente sur le PC pour ne rien retransférer inutilement.

Dans les deux cas : ne formatez pas la carte, et ne la « réparez » pas si Windows le propose.

---

## 3. Où sont vos accès actuels, et comment ils passent dans la nouvelle version

| Version | Où sont le Wi-Fi et le compte Dexcom |
|---|---|
| Ancienne (avant cette mise à jour) | **écrits dans le programme** au moment de la compilation, depuis `private\device_secrets.h` |
| Nouvelle | **dans la mémoire des réglages** (espace `ident`), saisis sur l'écran ou depuis le PC |

La nouvelle version garde une compatibilité : si la mémoire `ident` est vide et que le programme a été
compilé avec `private\device_secrets.h`, **ces valeurs servent de départ**. Autrement dit, si vous
installez sur votre écran le programme compilé depuis votre propre dossier (qui contient
`private\device_secrets.h`), **il se reconnecte tout seul, sans rien saisir**.

Deux chemins possibles, au choix :

1. **Le plus simple** : installer le programme compilé chez vous (`tools\build.ps1`). Vos accès sont
   repris automatiquement. Vous pourrez ensuite, quand vous voudrez, les enregistrer dans la mémoire
   des réglages depuis l'application Windows ou l'écran tactile, pour ne plus dépendre de la compilation.
2. **Le plus propre** : installer le programme public (sans aucun identifiant), puis saisir vos accès
   avec l'assistant tactile ou l'application Windows. C'est exactement ce que fera une personne qui
   découvre le projet.

Les mots de passe ne sont jamais demandés dans une conversation, jamais écrits dans un journal, et
jamais affichés : l'écran et l'application n'en montrent que la longueur.

---

## 4. Revenir à votre version actuelle

Après une sauvegarde `--complet`, le retour en arrière consiste à réécrire ce qui a été sauvegardé.

1. Branchez l'écran en USB.
2. Ouvrez la fenêtre **ESP-IDF 5.5 PowerShell** (elle définit les variables nécessaires).
3. Remettez le programme sauvegardé :

```bash
python -m esptool --port COM7 --baud 460800 write_flash 0x10000 private\sauvegardes\<date>\app_0x10000.bin
```

Avec une sauvegarde complète, la même commande avec `0` et `flash_complete.bin` remet **tout** :

```bash
python -m esptool --port COM7 --baud 460800 write_flash 0 private\sauvegardes\<date>\flash_complete.bin
```

4. Débranchez, rebranchez : l'écran redémarre sur votre ancienne version.

> Vérifiez le numéro de port (`python tools\assistant_config.py --lister`). Sur ce PC, la carte
> apparaît comme **USB-Enhanced-SERIAL CH343**.

---

## 5. Si l'écran ne démarre plus

Un écran qui reste noir, redémarre en boucle ou n'affiche plus rien **n'est pas perdu** : le mode
programmation est câblé dans le processeur, il ne dépend pas du programme installé.

Dans ce cas, le menu CONNEXION et l'application Windows **ne suffisent pas** : ils supposent que le
programme démarre. Il faut passer par le mode programmation.

1. Débranchez l'alimentation de l'écran.
2. Maintenez le bouton **BOOT** de la carte enfoncé.
3. Rebranchez le câble USB-C **sans relâcher** le bouton, attendez deux secondes, puis relâchez.
4. Le port série doit réapparaître sur le PC (`python tools\assistant_config.py --lister`).
5. Réécrivez votre sauvegarde (point 4 ci-dessus).

Si le port n'apparaît pas : changez de câble (beaucoup de câbles USB-C ne transportent que le courant),
changez de prise USB, et vérifiez que c'est bien la prise USB de la **carte** qui est utilisée.

Si l'écran démarre mais que l'image est noire alors que le son fonctionne, c'est l'affichage ou sa nappe
qu'il faut vérifier, pas le programme.

---

## 6. Ce que cette procédure ne couvre pas

- Une panne matérielle (nappe d'écran, alimentation, carte) : aucune sauvegarde logicielle n'y remédie.
- Une microSD défectueuse : gardez une copie de son contenu, les cartes bon marché s'usent.
- Un compte Dexcom bloqué après trop d'essais : il faut attendre, c'est une protection de Dexcom.
- Le second processeur ESP32-C6 (Wi-Fi) : il n'est ni sauvegardé ni reprogrammé par ces outils, parce
  qu'il n'est pas modifié. S'il tombait en panne, il faudrait le firmware d'origine de Waveshare.

---

*Cet écran est un affichage complémentaire expérimental. Il peut se tromper, être en retard ou
s'interrompre. Il ne remplace pas les dispositifs officiels et ne doit pas guider une décision de
traitement.*
