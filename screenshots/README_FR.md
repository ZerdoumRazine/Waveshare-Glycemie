# Captures d'écran

**Ce sont de vraies captures de la carte**, pas des maquettes ni des simulations.

Elles ont été obtenues le **15 octobre 2026** par la commande console `snap`, qui transmet
le tampon d'affichage par le câble USB-série ; `tools/serial_capture.py --png` les décode
en PNG. Format natif de l'écran : **1024 × 600**.

| Fichier | Page |
|---|---|
| `01_accueil.png` | ACCUEIL |
| `02_graphique.png` | GRAPHIQUE (6 h) |
| `03_aujourdhui.png` | AUJOURD'HUI |
| `04_consommables.png` | CONSOMMABLES |
| `05_reglages.png` | RÉGLAGES (haut de page) |
| `06_reglages_langue.png` | RÉGLAGES : choix de la langue et INFORMATIONS (17 octobre 2026) |
| `07_accueil_anglais.png` | ACCUEIL, interface en anglais |
| `08_reglages_connexion.png` | RÉGLAGES : section **CONNEXION** (voir, modifier, effacer, mode démonstration) |
| `pc_app/01_fenetre_connexion.png` | Application Windows, fenêtre vide |
| `pc_app/02_essai_reussi.png` | Application Windows après un essai réussi |

## Aucune donnée de santé réelle

Les captures ont été prises en **mode démonstration** (`demo normal`), qui remplace tout
l'historique de 24 h par une courbe **synthétique** générée par le firmware. C'est pour
cela que le bandeau rouge **MODE DÉMO** est visible en haut de chaque image : la valeur
affichée, la courbe et toutes les statistiques de la page AUJOURD'HUI sont **fictives**.

Le mode démonstration n'écrit rien sur la microSD et ne modifie aucune donnée persistante.

## Une valeur masquée dans `06_reglages_langue.png`

Vraie capture du 17 octobre 2026. L'adresse IP locale de l'appareil est recouverte d'un bandeau
gris, comme l'appareil le fait lui-même pour les dates de pose. Le bandeau « AUCUNE DONNÉE » visible
en haut est normal : un capteur venait d'être posé et chauffait encore.

## Deux valeurs remplacées dans `08_reglages_connexion.png`

Cette capture-là est bien une vraie capture de l'écran, prise le **17 octobre 2026** après la mise
à jour. Mais la page CONNEXION affiche le nom du réseau Wi-Fi et le compte Dexcom de l'appareil :
ces deux valeurs ont été **remplacées par des exemples** (`MaBox-Salon` et `m***@exemple.fr`) avant
publication. Tout le reste de l'image est inchangé. L'original n'est pas publié.

Le mot de passe, lui, n'apparaît nulle part : l'appareil ne l'affiche jamais, sur aucune page.

## Les captures de l'application Windows

`pc_app/01` et `pc_app/02` sont de vraies fenêtres de `GlycemieConnexion.exe`, photographiées sur
le PC. Les identifiants qu'on y voit sont **inventés** : ils viennent de `tools/pc_app/demo_capture.pyw`,
qui remplit la fenêtre avec des exemples sans ouvrir aucun port série.

## Un masquage sur une seule image

Le mode démonstration ne touche pas aux consommables : la page CONSOMMABLES affichait donc
les **vraies dates** de pose du capteur et de changement du cathéter. Ce sont des
informations personnelles de santé.

Sur `04_consommables.png`, elles ont été remplacées par des **bandeaux gris visibles** :
date de pose, jour en cours, temps restant, échéance, dépassement. Les libellés, la mise en
page, les boutons et les stocks sont intacts. Rien n'a été inventé ni remplacé par une
fausse valeur : les zones masquées se voient.

Les quatre autres images ne sont pas retouchées.

## Refaire ces captures

```powershell
python tools\serial_capture.py --port COM7 --duration 190 `
  --send "2:demo normal" `
  --send "6:page 0"  --send "10:snap" `
  --send "36:page 1" --send "40:snap" `
  --send "66:page 2" --send "70:snap" `
  --send "96:page 3" --send "100:snap" `
  --send "126:page 4" --send "130:snap" `
  --send "158:demo off" --send "162:page 0" `
  --log capture.log --png page.png
```

Un `snap` met une dizaine de secondes à être transmis : laisser au moins 25 s entre deux.
`demo off` à la fin est important : il rend la main aux données réelles.

> ⚠️ **Avant d'ajouter une capture au dépôt** : vérifier qu'elle ne montre aucune glycémie
> réelle, aucune date de pose, aucun SSID, aucun prénom. Le bandeau MODE DÉMO est le
> meilleur indice qu'une capture est publiable ; la page CONSOMMABLES demande en plus un
> masquage manuel.
