# Historique des versions

Les premières versions (V1 à K19) ont été développées du **13 au 21 septembre 2026**.
Le projet a ensuite évolué en octobre 2026 avec la traduction de l'interface (K20),
les annonces en six langues (K21) et l'assistant de connexion tactile et Windows (K22).

La **préversion GitHub v0.1.0** provient de K22. L'écran familial a été mis à jour,
mais **l'installation et la première configuration complète sur une carte neuve**
restent à valider sur matériel. Les modifications ultérieures du code sont
distinguées de cette préversion et nécessitent une nouvelle compilation et des tests.

> Les mesures citées (durées de rafraîchissement, RAM, PSRAM) sont les relevés **historiques**
> de septembre 2026 sur la carte réelle. Elles sont rapportées telles quelles et n'ont pas
> été refaites lors de la constitution de ce dépôt.

---

## V1 — 2026-09-13 · première version fonctionnelle

Chaîne complète établie : écran EK79007, tactile GT911, Wi-Fi par l'ESP32-C6, SNTP, client
Dexcom Share natif, affichage de la valeur, de la tendance et de l'âge, graphique.

Validé sur la carte : scan et association Wi-Fi, DHCP, DNS, HTTPS avec vérification du
certificat, authentification Dexcom Europe, reconnexion après coupure Wi-Fi en ≈ 6 s,
reprise après invalidation de session, endurance 19 min sans erreur.

Point dur résolu : épuisement de la RAM interne pendant les échanges HTTPS
(`sdio_push_data_to_queue ... pkt_rxbuff`), corrigé en plaçant les gros tampons et les
allocations TLS en PSRAM.

## V2 — 2026-09-13 · alertes, microSD, audio

Historique CSV quotidien sur microSD, gestionnaire d'alertes complet, panneau d'alerte
plein écran, audio ES8311 avec courbe de volume corrigée.

Endurance 14 min sur données réelles : 287 mesures lues au démarrage, lectures
incrémentales toutes les 5 min, cache plafonné à 288. RAM interne 123–133 Ko libres.

## 2026-09-14 · déduplication de l'historique

Cause des doublons trouvée : Dexcom horodate à la **milliseconde**, le CSV à la **seconde**.
Correction : relecture de la dernière ligne et comparaison à la seconde.
`sd_repair` a nettoyé 4 doublons exacts dans le fichier du 13/09 et n'a touché à rien
d'autre. Aucun doublon ensuite après redémarrage, retrait de carte simulé, coupure Wi-Fi ou
session invalidée.

## 2026-09-14 · gel de l'interface — correctif PPA

Gel reproductible de la tâche LVGL dans `ppa_do_operation()` pendant la rotation 180° des
zones partielles, diagnostiqué par core dump. Correctif Espressif « PPA hang » reporté dans
une copie locale du composant (`components/esp_driver_ppa`). Résultat : 2 322 appuis en
10 min sans gel (avant : gel au 3ᵉ appui). Mapping tactile définitif :
`swap_xy = 0, mirror_x = 0, mirror_y = 0`.

## 2026-09-14 · voix française sur microSD

Banque Google `fr-FR-Chirp3-HD-Aoede` générée **une seule fois** sur le PC (373 fichiers
WAV, 16 kHz mono), copiée sur la carte **par le câble USB-série** sans retirer la microSD,
avec vérification CRC32 fichier par fichier. Repli prévu : voix embarquée, puis bip.

## J2 — 2026-09-15 · mode nuit

Écran nuit minimaliste selon un horaire, luminosité réduite, toucher qui rend l'interface
30 s, alertes prioritaires, aperçu TEST MODE NUIT. Bloc NVS séparé (`nuit`) pour ne pas
toucher au bloc des autres réglages.

Validation physique par l'utilisateur le 2026-09-15 : OK, RAPPEL DANS 30 MIN, indicateur,
fermeture automatique, navigation tactile, défilement des réglages, voix, volume.

## Étape K / K10 — 2026-09-15 · consommables et commandes vocales

- Page **CONSOMMABLES** : capteur G7 et site de perfusion, confirmation unique
  « maintenant = heure de départ », aucune saisie de date.
- **Commandes vocales locales** ESP-SR 2.5.3 (MultiNet7 `mn7_en`, WakeNet9L « Bonjour ESP »
  optionnel), partition `model` de 4 Mo ajoutée **après** les partitions d'origine.
- Recherche documentée : l'API Share ne renvoie **aucune** information de session capteur,
  et les API Dexcom v3 / Clarity ne conviennent pas → méthode locale assumée.
- Difficultés résolues : modèles au-delà de 16 Mo non projetables en mémoire (lecture en
  PSRAM par une tâche à pile interne), lecture flash interdite depuis une pile PSRAM,
  plantage sur `clean()` d'un modèle jamais utilisé.

Endurance 4 h du 2026-09-17 (log 221) : 0 redémarrage, 0 gel, 0 watchdog ; RAM interne
97 → 94 Ko (min 68) ; Dexcom 50 lectures sans erreur ; microSD 49 écritures, 0 doublon.
La baisse de PSRAM de la première heure a été identifiée comme le **cache de glyphes
TinyTTF** (185 min ensuite sans aucune variation) : **pas de fuite**.

## K11 — 2026-09-17 · filtre des fausses détections vocales

Problème réel : l'appareil « parlait tout seul » plusieurs fois par jour.

Le score MultiNet7 a été établi par **désassemblage du binaire** :
`score = exp(log-probabilité ÷ (jetons + 2))`, commande émise dès que `score > exp(−2) ≈ 0,135`
(test strict). Sur 4 h : 6 annonces spontanées (0,14 à 0,20), toutes décodées « Glucose ».

Correctif minimal (seul `voice_cmd.c` modifié) :
« Glycémie » acceptée si `≥ 0,155` ; variante `GLoKbS` de « Glucose » **retirée** (vraies
0,16–0,19 et fausses 0,14–0,20 indissociables) ; variante `GLoKbZ` acceptée si `> 0,205`.
Chaque détection est journalisée avec son score à 4 décimales, et les rejets sont comptés
(`ignorees=` dans `vcmd_info`).

Priorité retenue, à la demande de l'utilisateur : **fiabilité avant portée**.

## K12 — 2026-09-17 · refonte visuelle 2.5D « dark glass »

Refonte **visuelle seule** : aucune logique modifiée. Fond en dégradé, cartes « verre »,
halos, anneau de fraîcheur, flèche de tendance dessinée, page AUJOURD'HUI.

Pièges LVGL 9.5 documentés à cette occasion : une ombre n'est dessinée qu'**autour** d'un
objet non opaque (un halo creux donne un contour) ; `lv_buttonmatrix_set_button_width`
n'accepte que 1 à 15.

Régression de performance traitée : le rafraîchissement complet était passé de 75–90 ms à
325–366 ms. Chaque ombre coûte ≈ 15 ms par carte à 24 px → ombres ramenées à 10 et 16 px,
cartes rendues opaques. Résultat mesuré : ACCUEIL 162–170 ms.
Endurance 31 min : 0 erreur, 0 watchdog, 0 gel.

## K13b — 2026-09-17 · rappels des consommables

- Capteur G7 : **échéance = pose + 10 jours exactement** (les 12 h de grâce Dexcom ne
  prolongent rien), rappels **24 h** et **2 h** avant, « À CHANGER » à l'échéance sans sonnerie.
- Cathéter : **un seul** rappel 2 h avant.
- Chaque rappel est mémorisé en NVS (`remind2`) **dès sa sonnerie** : un redémarrage ne le
  rejoue jamais.
- Anciens rappels (48 h, 12 h, grâce, fin de grâce) supprimés. Banque vocale portée à 395 fichiers.

Tests : 38 contrôles passés.

## K14 — 2026-09-18 · stock, graphique tactile, accueil adaptatif

- `main/stock.c` : stock des capteurs et cathéters en NVS. **Non renseigné** par défaut,
  jamais négatif, **une seule décrémentation par changement réel** (l'heure du changement
  sert de marqueur), seuils de stock faible, événement local STOCK_LOW.
- Graphique : exploration tactile sur les **mesures réelles** (jamais d'interpolation),
  trait, point et bulle.
- Accueil adaptatif selon l'état.

Endurance 31 min : 0 redémarrage, 0 watchdog, 0 erreur PPA, RAM interne 100 Ko stable
(min 70), Dexcom 7/7, microSD 0 doublon.

## K15 — 2026-09-18 · la page STATS est supprimée

**Décision volontaire** : la page STATS est retirée (`ui_stats.c` supprimé, `UI_PAGE_STATS`
retiré, `gm_stats()` retiré). La navigation passe à **5 pages** ; AUJOURD'HUI devient la
seule page de statistiques.

Accueil épuré : valeur 260 px flottante, anneau 190 px, flèche 118 px, sans cartes.
Rafraîchissement complet 112 ms. Stabilité 16 min : RAM interne 95 Ko stable (min 68).

> **Ne pas recréer la page STATS.**

## K16 — 2026-09-18 · accueil en trois cartes

L'accueil K15 était jugé « trop vide ». Trois zones : carte HEURE, carte DATE, grande carte
GLYCÉMIE. Seul `ui_home.c` change. Méthode adoptée ici et conservée ensuite : **maquette PIL
sur PC à l'échelle 1024 × 600, avec les vraies polices et les métriques LVGL, avant de
coder** ; deux défauts corrigés sur maquette, deux sur la carte.

Rafraîchissement 104 ms (moins de fond en dégradé à repeindre).

## K17 — 2026-09-18 · accueil plein écran *(demandé à la voix)*

Demande exacte de l'utilisateur : « la date en haut à côté des points verts, l'heure tout en
haut à droite, la glycémie plus grande, les voyants Wi-Fi/Internet/Dexcom/SD servent à rien ».

- Bandeau commun aux 5 pages : **date longue à gauche**, ligne d'état au centre,
  **heure à droite**. Les quatre voyants de liaison sont **supprimés**.
- ACCUEIL : plus de carte heure ni de carte date, **une seule carte 992 × 480**, valeur
  **320 px**, anneau 200 px, flèche 128 px, variation 76 px.
- Rafraîchissement 112 ms ; RAM interne 94–104 Ko (min 68) ; application 6 126 880 o.

## K18 — 2026-09-19 · deux retouches

- Heure du bandeau : 34 → **44 px**.
- Page AUJOURD'HUI : sous-titre réduit à **« Depuis minuit »** (la date longue n'y est plus
  répétée, elle est dans le bandeau).

Validé par les tests 345 : 5 onglets au doigt, captures des 5 pages, aucun texte tronqué.

## K19 — 2026-09-21 · commande vocale du capteur sécurisée *(version historique)*

Problème réel : annonces spontanées « Nouveau capteur… » plusieurs fois par jour.

**Cause prouvée** (journal du 18–19/09) : 5 détections, **toutes** de la variante anglaise
« New sensor » (scores 0,1390 à 0,1656), jamais de « Nouveau capteur » ; aucune n'avait été
confirmée. Reproduit par injection de son : « The new center opens on Monday » → 0,1514 ;
« New census data… » → 0,1444.

Correctif :

- les phrases courtes « Nouveau capteur » et « New sensor » sont **supprimées** ;
- deux phrases **différentes** : « **Enregistrer nouveau capteur** » (seuil ≥ 0,180)
  n'enregistre rien et répond « Dites confirmer capteur » ; seule « **Confirmer capteur** »
  (seuil ≥ 0,150), **dans les 10 s** suivant l'annonce, enregistre ;
- **anti-répétition** : après une demande non confirmée, « Enregistrer nouveau capteur » est
  ignorée sans annonce pendant 5 minutes ;
- niveau des micros journalisé à chaque détection ; `vrx_inject` ajouté pour tester avec du
  son réel ; commande console `sd_lire` (lecture seule) ajoutée.

« Glycémie » (≥ 0,155), « Glucose » (> 0,205) et le cathéter sont **inchangés**. Le bouton
tactile NOUVEAU CAPTEUR ne dépend ni de la voix ni de l'anti-répétition.

Validation : 33 contrôles fonctionnels, 0 détection sur 24 phrases pièges et bruits, puis
test de stabilité.

Empreintes du binaire livré à l'époque :
`glycemie_display.elf` SHA-256 `609a4d04044ac846803816077b906456f85fb7901d5aec390f0470238b78e342`,
`glycemie_display.bin` SHA-256 `fb475b771f302fb0d2f8575eb5390d7aa538cc4175bfeb11015fafcb8a60cd4c`,
taille 6 131 536 octets.

---

## K20 — 2026-10-15 · langue de l'interface

Demande de l'utilisateur : pouvoir mettre l'écran dans une autre langue.

- Nouvelle section **LANGUE** dans RÉGLAGES : français, anglais, espagnol, allemand, italien,
  portugais — six langues européennes, toutes en alphabet latin, donc affichables avec les polices
  Montserrat déjà embarquées.
- `main/i18n.c/.h` : **179 textes** en 6 langues, en initialiseurs désignés (`[S_XXX] = {...}`), ce
  qui rend un désalignement impossible ; dates longues et noms de jours propres à chaque langue ;
  `tr()` pour l'écran, `tr_fr()` pour le journal de la microSD, les CSV et la console.
- Changement **immédiat** : la tâche LVGL reconstruit les cinq pages, hors du callback du bouton
  (l'objet qui porte ce callback fait partie de ce qui est détruit). Le bandeau, le panneau d'alerte
  et l'écran nuit ne sont pas détruits, seulement remis à jour.
- Enregistré dans une **clé NVS séparée** (`lang`), qui n'invalide pas le bloc des autres réglages.
- Console : `lang` et `lang fr|en|es|de|it|pt`.

## K21 — 2026-10-15 · la voix suit la langue

Remarque de l'utilisateur : « ça fait bizarre de mettre les textes en anglais et quand elle dit
glycémie haute, ça se dit en français ».

- **Cinq nouvelles banques vocales** de 396 fichiers chacune (anglais, espagnol, allemand, italien,
  portugais), générées avec le **même timbre** que le français (Google Chirp 3 HD « Aoede »), soit
  ≈ 22 Mo et ≈ 11 500 caractères facturés par langue. Le portugais est en voix brésilienne : Google
  ne propose pas de Chirp 3 HD en portugais européen.
- `tools/voice/langues_voix.py` : textes et **nombres en lettres de 40 à 400** pour les six langues
  (élisions italiennes `quarantotto`, composition allemande `einhunderteinundzwanzig`, `cien` /
  `ciento` en espagnol, `cem` / `cento e` en portugais…). Le français est **inchangé à l'octet
  près** : la banque déjà installée sur la microSD reste valable.
- `main/voice.c` : la banque de la langue choisie est lue dans `/glycemie/audio/voice/<code>/` ; si
  elle n'est pas installée, **la banque française prend le relais** plutôt que le silence, et un
  avertissement est journalisé une seule fois.
- `main/voice_rx.c` : commande `vrx_dir <code>` pour copier une banque dans son sous-dossier (créé
  au besoin) ; `sd_storage_mkdir()` ajouté. Les noms de fichiers restent courts, la ligne console
  étant limitée à 47 caractères.
- `tools/voice/sd_voice_upload.py --lang <code>` ; la vérification d'une banque de langue repasse
  chaque fichier par `vrx_put`, qui répond `SKIP` après relecture et comparaison du CRC32.

**Défaut trouvé et corrigé pendant les essais** : le champ `title` de la table des alertes était
devenu un identifiant de texte, mais six `ESP_LOG` le passaient encore à un `%s`. La première
`alert_sim` en anglais a provoqué un `Load access fault` (déréférencement de l'entier 153), core
dump à l'appui. Le champ a été renommé `title_id` pour que le compilateur impose la revue de chaque
usage, et les journaux passent par `tr_fr()`.

**Vérifications sur la carte** : changement de langue sans plantage, alerte basse simulée en anglais
(panneau « LOW GLUCOSE », journal microSD en français), repli vers la banque française confirmé pour
une langue non installée, Dexcom et Wi-Fi inchangés, NVS conservée.

## K22 — 2026-10-17 · configuration pour chaque utilisateur

- Assistant tactile : choix de la langue, Wi-Fi, compte Dexcom, vérification puis
  enregistrement après confirmation.
- Application Windows `GlycemieConnexion.exe` reliée par USB, en complément du tactile.
- Programme public **sans identifiants personnels**, avec mode démonstration.
- Reconnaissance des commandes vocales **désactivée dans le programme distribué** ;
  les annonces parlées en six langues et les alertes sonores restent disponibles.
- Contrôles hors matériel exécutés ; l'essai complet sur une carte neuve reste à faire.

## Après K19

Rien jusqu'au 15 octobre 2026, date de la reconstitution du dépôt puis de K20 et K21
ci-dessus. En particulier,
les fonctions parfois évoquées mais **jamais implémentées** ne figurent pas dans ce dépôt :
équilibre alimentaire par IA, tableau de bord additionnel, intégration à un autre produit,
conversion vers Nightscout, affichage en mmol/L.

## Compilation historique de K19 (pas celle de la version actuelle)

Les chiffres ci-dessous concernent **K19**, dont les sources étaient identiques
à celles de la sauvegarde `K19_VOIX_CAPTEUR_BIN`. Cette version historique a été
recompilée le 2026-10-15 avec la même pile
(ESP-IDF 5.5.5, mêmes versions résolues dans `dependencies.lock`) :

- compilation réussie, 2 433 cibles ;
- `glycemie_display.bin` = **6 129 952 octets** (27 % de la partition `factory` libres) ;
- 23/23 contrôles de `tools/verif_avant_flash.py`.

L'écart de 1 584 octets avec le binaire de septembre s'explique par les chemins de
compilation inscrits dans l'image (le projet n'est plus au même endroit sur le disque) :
les sources, elles, sont identiques.
