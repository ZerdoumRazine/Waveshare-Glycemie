# Limites, précautions et ce qui n'est pas testé

Document volontairement franc. Il distingue ce qui est **vérifié**, ce qui est
**rapporté** (mesuré en septembre 2026 mais non refait depuis) et ce qui n'a
**jamais** été testé.

---

## 1. Avertissement médical

Ce projet est un **afficheur familial complémentaire**, personnel et expérimental.

- **Ce n'est pas un dispositif médical.** Il n'est ni certifié, ni validé cliniquement.
- Il ne doit servir à **aucune décision thérapeutique**.
- Il ne remplace **ni** l'application Dexcom officielle, **ni** ses alarmes, **ni** aucun
  système de surveillance certifié. Les alarmes de sécurité restent celles du capteur et
  du téléphone.
- Les seuils d'alerte sont des choix d'utilisateur, pas des recommandations médicales. Le
  rappel du cathéter (48 h / 72 h) est un rappel pratique, pas une prescription.
- Aucune garantie d'aucune sorte (voir [LICENSE](../LICENSE)).

## 2. Dépendance à Dexcom Share

- **Share n'est pas une API publique documentée.** Le client s'appuie sur le comportement
  observé et sur le travail de *pydexcom*. Un changement côté Dexcom peut interrompre le
  service sans préavis.
- Seule la région **`ous` (Europe)** a été réellement utilisée. La région `us` existe dans
  le code mais **n'a jamais été essayée**.
- La chaîne dépend du **téléphone** : si l'application Dexcom est fermée, hors ligne, ou si
  le partage est désactivé, l'écran affiche « DONNÉES ANCIENNES » — ce qui est le
  comportement voulu, mais ce n'est pas une panne de l'appareil.
- Le verrou d'authentification empêche toute boucle de tentatives, au prix d'une
  autorisation manuelle au premier démarrage (`dexcom_auth`).

## 3. Fonctions absentes (volontairement)

- **Pas de mmol/L** : l'affichage est en mg/dL uniquement.
- **La reconnaissance des commandes vocales et l'écoute du microphone sont exclues du programme public.** Les annonces parlées et alertes restent proposées en six langues depuis les banques WAV de la microSD. L'ancienne variante de reconnaissance vocale est expérimentale et n'est pas fournie dans le programme public.
- Le journal de la microSD, les fichiers CSV et la console restent en français, pour que les
  enregistrements anciens et nouveaux restent comparables.
- La voix portugaise est **brésilienne** : Google ne propose pas de voix Chirp 3 HD en portugais
  européen.
- Les banques vocales ne sont **pas** dans le dépôt (≈ 22 Mo par langue, audio produit par un
  service tiers) : `tools/voice/gen_voice_google.py --lang <code>` les régénère.
- **Configuration sur écran neuf non entièrement validée** : l'assistant tactile et la configuration USB sont présents, mais le parcours complet de saisie, sauvegarde et redémarrage a été vérifié en simulation, pas encore de bout en bout sur une carte vierge.
- **Pas de page STATS** : elle a existé jusqu'à K14 et a été **supprimée** à K15. Elle ne
  doit pas être recréée. AUJOURD'HUI est la seule page de statistiques.
- **Pas de Nightscout**, pas d'API Dexcom v3 / Clarity, pas de détection automatique de
  pose de capteur (l'API Share ne fournit aucune information de session).
- **Pas d'intelligence artificielle, pas d'analyse alimentaire, pas de tableau de bord
  supplémentaire** : ces idées ont pu être évoquées, elles n'ont jamais été implémentées.
- Pas de mise à jour par le réseau (OTA) : le flash se fait par USB.

## 4. Contraintes de compilation

- **Le chemin du projet doit être purement ASCII.** Avec un accent, la compilation échoue
  dans `ccache`, dans `ldgen.py` et à l'édition de liens. Les espaces sont acceptés.
- `IDF_PATH` et `IDF_TOOLS_PATH` doivent eux aussi être dans un chemin ASCII.
- Le projet cible une **ESP32-P4 rev1.3**. Un binaire compilé en profil rev3.x **ne démarre
  pas** sur cette carte. Trois garde-fous l'interdisent, mais une carte d'une autre
  révision demanderait une adaptation réfléchie du profil.
- La compilation est en `-Werror` : un avertissement arrête la construction.

## 5. Matériel

- **Audio** : la limite analogique est atteinte à 100 % de volume (saturation mesurée,
  3ᵉ harmonique à −11 dB pour +1,3 dB utile). **85 % est le maximum propre.** Pour un vrai
  niveau d'alarme, il faudrait un haut-parleur plus sensible en enceinte fermée, voire un
  amplificateur externe — montage non réalisé ni validé.
- **Réserve de stabilité connue** : sous une charge artificielle extrême (24 zones
  redessinées toutes les 20 ms **et** appuis continus), un plantage mémoire a été observé
  **une fois** dans le chemin de rotation partielle de l'adaptateur LVGL. Redémarrage
  automatique en quelques secondes, réglages conservés. Jamais observé en usage normal, ni
  sous chacune de ces charges prise séparément.
- Le composant `components/esp_driver_ppa` contient le correctif du gel de l'interface.
  **Le retirer réintroduit un gel reproductible.**
- Le firmware de l'ESP32-C6 n'est jamais modifié ; sa version reste « inconnue » car il ne
  répond pas à la requête de version.

## 6. Commandes vocales — variante expérimentale non distribuée

La **version publique ne propose pas de reconnaissance de commandes vocales** et n'ouvre pas le microphone pour ce service. La variante historique, fondée sur ESP-SR, a été écartée en raison de sa fiabilité insuffisante et de ses besoins matériels. Les annonces et alertes sonores restent indépendantes et disponibles avec les banques microSD dans les six langues.


## 7. Ce qui est couvert par des tests automatiques

Exécutés et passés le **2026-10-15** sur cette machine :

| Test | Portée | Résultat |
|---|---|---|
| Compilation ESP-IDF 5.5.5 | projet complet | réussie, 6 129 952 o |
| `tools/verif_avant_flash.py` | partitions, profil, image, absence d'identifiant | 23 / 23 |
| `tests/hote/test_glucose_math.c` | âge, fraîcheur, cadence, variation, prochaine mesure, tendances, historique | 38 / 38 |
| `tests/hote/test_consommables.c` | échéances capteur et cathéter, rappels, anti-répétition, persistance NVS après redémarrage, stock | 65 / 65 |
| `tests/test_depot.py` | structure, chemins, secrets, partitions, dépendances, captures | voir la sortie du script |

## 8. Ce qui n'est **pas** couvert par des tests automatiques

- **Dexcom Share** : pas de test automatique, ni hors ligne ni en ligne. Le client HTTPS
  n'est pas isolable sans ESP-IDF, et il n'existe pas de serveur Share de test.
- **Déconnexion / reconnexion réseau** : validé historiquement sur la carte
  (`wifi_test_disconnect`, `dexcom_test_session`, reconnexion en ≈ 6 s), **pas** par un test
  automatique.
- **microSD** : déduplication, retrait et remontage validés historiquement sur la carte
  (`sd_check`, `sd_repair`, `sd_test_eject`), **pas** par un test automatique — le code
  dépend du pilote SDMMC.
- **Composants graphiques** : LVGL n'est pas instanciable sur PC ici. Vérifiés par captures
  d'écran réelles et par la commande `ui_stress` sur la carte.
- **Alertes et répétitions** : la logique de répétition dépend des horloges FreeRTOS et du
  moteur audio ; elle a été validée sur la carte avec `alert_sim`, pas en test hôte. Seule
  la partie « rappels des consommables et anti-répétition » est couverte en test hôte.
- **Audio et voix** : validés à l'oreille et par mesure au micro de la carte.

## 9. Mesures rapportées, non refaites

Les chiffres de performance et de mémoire cités dans la documentation (112 ms de
rafraîchissement de l'accueil, 94–104 Ko de RAM interne, ≈ 22 Mo de PSRAM, endurances de
31 min à 4 h sans erreur) sont les **relevés de septembre 2026** sur la carte réelle. Ils
sont rapportés tels quels, à titre de référence historique : **ils n'ont pas été refaits**
lors de la constitution de ce dépôt.

## 10. Opérations volontairement non effectuées

Lors de la constitution de ce dépôt, aucune opération modifiant la carte n'a été faite :
pas de flash, pas d'effacement, pas d'écriture NVS, pas de formatage de la microSD, pas de
modification du firmware de l'ESP32-C6, pas d'eFuse, pas de Secure Boot ni de Flash
Encryption. La carte n'a été sollicitée qu'en lecture et en mode démonstration.
