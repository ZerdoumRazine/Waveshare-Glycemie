# Maintenir et préparer la publication de WaveShare Glycémie

Le projet est hébergé dans un **dépôt privé** sur GitHub :
[ZerdoumRazine/Waveshare-Glycemie](https://github.com/ZerdoumRazine/Waveshare-Glycemie).
Sa visibilité ne doit pas être modifiée sans décision explicite du propriétaire.

Ce guide concerne la **maintenance du dépôt existant**. Il ne faut ni recréer un
nouveau dépôt, ni effacer son historique Git, ni exécuter une commande de
« force push » pour mettre à jour une documentation.

## Faire une modification

1. Dans GitHub Desktop, sélectionner le dépôt `ZerdoumRazine/Waveshare-Glycemie`.
2. Faire **Fetch origin**, puis **Pull origin** si proposé.
3. Modifier les sources ou la documentation dans cette copie.
4. Examiner les changements, créer un commit et utiliser **Push origin**.
5. Sur GitHub, vérifier le rendu du README et les liens. Pour une modification
   du programme, créer une branche et une *pull request* plutôt que modifier
   directement `main`.

Les documents [README français](../README_FR.md) et
[README anglais](../README.md) doivent rester cohérents.

## Vérifier avant de proposer une nouvelle version

- Exécuter `python tests/run_tests.py`, `python tests/test_depot.py` et les
  autres suites de `tests/` ; vérifier aussi les résultats GitHub Actions.
- Les tests logiciels et simulations **ne remplacent pas** la compilation
  ESP-IDF et les essais sur matériel.
- Ne jamais publier de sauvegarde flash personnelle, NVS, historique de glucose,
  journaux, identifiants ni captures montrant des données familiales.
- Relire **visuellement** chaque image et vérifier la licence de chaque
  composant tiers. Les outils automatiques de recherche de secrets ont des limites.
- Confirmer que la version compilée correspond aux sources du commit testé
  et que la table des partitions est correcte pour la révision matérielle.
- Avant distribution large, faire examiner le statut réglementaire du logiciel,
  notamment ses alertes : un avertissement ne constitue pas une certification.

## Les fichiers téléchargeables

Les fichiers volumineux n'appartiennent pas à l'historique Git du code.
Les Releases peuvent contenir :
- le programme destiné **exclusivement** au matériel indiqué ;
- `GlycemieConnexion.exe` ;
- les banques vocales microSD et leur notice de provenance ;
- les empreintes SHA-256 des fichiers distribués.

La préversion v0.1.0 existe déjà. Son code et ses binaires sont **historiques** :
une correction ultérieure du firmware exige un nouveau build, des tests et
une **nouvelle version** ; ne pas prétendre que l'ancien ZIP a été mis à jour.
GitHub propose automatiquement le code source correspondant à chaque tag.
Ne pas considérer un ZIP ajouté manuellement comme plus récent que le commit.

## Partage et publication

Un dépôt **privé** n'est accessible qu'aux personnes autorisées.
Il est possible d'inviter ponctuellement un lecteur depuis les réglages
du dépôt. L'accès n'est jamais équivalent à un changement de visibilité.

Une ouverture au public n'est pas automatique : faire une dernière revue des
secrets, captures, droits, tests, avertissements et liens avant de choisir
**Settings → General → Change visibility → Public**. Seul le propriétaire
doit décider de cette action.

## À ne pas faire

- Ne pas exécuter `preparer_razine.py --depot-neuf` sur un dépôt existant.
- Ne pas supprimer le dossier `.git` ni réécrire l'historique pour une mise à jour normale.
- Ne pas promettre qu'une installation sur appareil neuf a été validée sans essai réel.
- Ne pas rendre public un programme généré à partir de secrets personnels.
