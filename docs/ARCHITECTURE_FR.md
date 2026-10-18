# Architecture logicielle

Projet ESP-IDF classique : tout le firmware est dans `main/`, un seul composant local dans
`components/`, le reste vient du gestionnaire de composants Espressif.

---

## Flux de données

```
capteur G7
   │  BLE
   ▼
téléphone (application Dexcom)
   │  Internet
   ▼
serveurs Dexcom Share  ──HTTPS──►  dexcom_share.c  ──►  glucose_service.c
                                        (lecture)         │  cache 24 h (288 mesures, PSRAM)
                                                          │  fusion sans doublon, rattrapage
                                                          ▼
                                                     app_state.c
                                         (état partagé + historique, sous mutex)
                                        ┌─────────────┼─────────────┐
                                        ▼             ▼             ▼
                                  ui_*.c (LVGL)   alerts.c     sd_storage.c
                                                      │             │
                                                      ▼             ▼
                                              audio_out.c /     CSV quotidien
                                                 voice.c        + journal
```

Les pages ne font **jamais** de requête réseau : elles lisent le cache partagé.

---

## Modules

| Fichier | Rôle |
|---|---|
| `app_main.c` | démarrage (NVS → état → réglages → capteur → écran/tactile → interface → audio → Wi-Fi → microSD → réseau → Dexcom → alertes), supervision, console de développement |
| `app_state.c` | état partagé et historique (288 mesures) sous mutex ; seule source de vérité pour l'affichage |
| `app_config.h` | toutes les constantes de l'application (cadence, fraîcheur, plages, fuseau) |
| `i18n.c` | langue de l'écran : table des textes en 6 langues, dates longues, `tr()` et `tr_fr()` |
| `wifi_manager.c` | Wi-Fi station via l'ESP32-C6, scan, reconnexion progressive |
| `net_services.c` | SNTP (fuseau France), test DNS + HTTPS neutre, RSSI |
| `dexcom_share.c` | client Dexcom Share natif (d'après *pydexcom*) |
| `dexcom_guard.c` | verrou d'authentification NVS (protection du compte) |
| `glucose_service.c` | cadence de lecture, cache 24 h, fusion, rattrapage, session, mode démonstration |
| `glucose_math.c` | **fonctions pures** : âge, fraîcheur, variation, cadence réelle, prochaine mesure |
| `sensor_session.c` | suivi du capteur G7 : pose, échéance à +10 jours, rappels 24 h / 2 h, NVS |
| `consumables.c` | cathéter (intervalle, rappel 2 h), « maintenant = heure de départ », `consumables.csv` |
| `stock.c` | stock des capteurs et cathéters : NVS, décrémentation unique par changement, seuils, événement local STOCK_LOW |
| `sd_storage.c` | microSD : montage sans formatage, dossiers, CSV, déduplication, journal |
| `audio_out.c` | sons générés, ouverture/fermeture du codec, mesures au micro |
| `voice.c` | lecture des phrases : banque de la langue choisie (`/glycemie/audio/voice/<code>/`), repli sur la banque française, puis sur un bip. L'ancienne voix Windows embarquée n'est jouée que par la commande console `voice_old`, et seulement si `main/voice/voice_fr.pcm` a été compilé (facultatif) |
| `voice_rx.c` | réception d'une banque vocale par la console série (`vrx_dir` choisit le dossier de langue) |
| `voice_cmd.c` | commandes vocales locales ESP-SR (MultiNet7 / WakeNet9L, confirmations, seuils) |
| `alerts.c` | gestionnaire d'alertes : conditions, priorités, répétitions, hystérésis |
| `settings.c` | réglages persistants (NVS) : volume, voix, alertes, seuils, luminosité, nuit, voix |
| `night_mode.c` | écran nuit minimaliste, rétroéclairage, toucher, alertes, aperçu |
| `ui_main.c` | bandeau, navigation, cycle de rafraîchissement |
| `ui_home.c` `ui_graph.c` `ui_today.c` `ui_consumables.c` `ui_settings.c` | les 5 pages |
| `ui_alert.c` | panneau d'alerte plein écran |
| `ui_chart.c` | courbe et exploration tactile |
| `ui_common.c` | polices, jetons de style, cartes « verre », lueurs, anneaux, couleurs |
| `trend_arrow.c` | flèche de tendance dessinée (`LV_EVENT_DRAW_POST`) |
| `touch_diag.c` | diagnostic tactile (journal, appuis synthétiques de test) |
| `components/esp_driver_ppa` | copie du pilote PPA d'ESP-IDF 5.5.5 **avec le correctif du gel** (voir `CORRECTIF.txt`) |

---

## Tâches

| Tâche | Pile | Rôle |
|---|---|---|
| `main` | 10 Ko interne | démarrage puis supervision et console |
| LVGL (BSP) | interne | **toute** l'interface ; aucun autre fil ne touche LVGL |
| glycémie | PSRAM | cadence Dexcom, requêtes HTTPS, fusion du cache |
| alertes | 6 Ko | évaluation des conditions, répétitions, déclenchement son/voix |
| audio | interne | ouverture du codec, lecture PCM ; jamais appelée depuis l'interface |
| microSD | PSRAM | écriture des CSV et du journal, remontage |
| réglages | — | écriture NVS différée (2 s après le dernier changement) |
| `voixcmd` | 16 Ko PSRAM | commandes vocales (si activées), priorité 3 |

Règles respectées partout :

- **tout appel LVGL se fait dans la tâche LVGL** ;
- une lecture en flash ne peut pas se faire depuis une pile en PSRAM (d'où la tâche
  temporaire à pile interne qui charge `srmodels.bin`) ;
- l'écriture NVS n'est jamais faite depuis un callback d'interface.

---

## Interface : principes de rendu

Style « dark glass » 2.5D, **formes LVGL natives uniquement** — aucune image, aucun bitmap :

- fond d'écran en dégradé bleu nuit → noir, peint une seule fois (les pages sont transparentes) ;
- cartes « verre » : dégradé vertical **opaque** (la vraie translucidité ne gagnait rien
  visuellement et coûtait cher), bord fin semi-transparent, ombre courte, second contour
  intérieur discret ;
- halos : LVGL ne dessine l'ombre qu'**autour** d'un objet non opaque — un halo creux
  donnerait un contour. Un halo correct est donc une ombre colorée floue autour d'un petit
  noyau arrondi, ou un objet **sans fond** dont seule l'ombre est teintée ;
- boutons en pilules ; `lv_buttonmatrix_set_button_width` n'accepte que **1 à 15**.

**Coût du rendu** : chaque ombre coûte selon sa largeur (≈ 15 ms par carte à 24 px). Les
ombres ont donc été ramenées à 10 px (cartes) et 16 px (zone glycémie). Rafraîchissement
complet mesuré par page (commande `ui_stress`, mesures historiques de septembre 2026) :
ACCUEIL 112 ms, GRAPHIQUE 116–122 ms, AUJOURD'HUI 137–144 ms, CONSOMMABLES ≈ 130 ms,
RÉGLAGES 77–79 ms. Au repos sur l'accueil : 5 rafraîchissements de 5 ms par seconde.

**Polices** : Montserrat Medium et Bold embarquées (TTF) et rendues par TinyTTF, avec un
cache LRU de 64 glyphes par taille. Ce cache explique une baisse de PSRAM pendant la
première heure, puis une stabilisation : **ce n'est pas une fuite** (vérifié sur une
endurance de 4 h : 185 minutes consécutives sans aucune variation).

---

## Tactile et gel de l'interface — correctif obligatoire

L'adaptateur `esp_lvgl_adapter` tourne l'image de 180° lui-même (LVGL n'est pas tourné) et
le GT911 de cette carte renvoie déjà des coordonnées dans l'orientation affichée. La
configuration validée au doigt est `swap_xy = 0`, `mirror_x = 0`, `mirror_y = 0`
(dans `app_main.c`). Les miroirs X/Y de l'exemple Waveshare inversaient le toucher.

**Gel reproductible** : des appuis rapides bloquaient définitivement la tâche LVGL dans
`ppa_do_operation()` pendant la rotation 180° des zones partielles (mode `TRIPLE_PARTIAL`),
jusqu'au watchdog. Diagnostiqué par core dump. La correction est le correctif officiel
Espressif « PPA hang », reporté dans une **copie locale du composant `esp_driver_ppa`**
(`components/esp_driver_ppa`, voir `CORRECTIF.txt`) ; ESP-IDF reste intact.
Après correctif : 2 322 appuis en 10 min sans gel (avant : gel au 3ᵉ appui).

> **Ne jamais retirer `components/esp_driver_ppa`** ni modifier le mapping tactile sans
> refaire ces essais.

---

## Persistance (NVS)

| Espace | Clés | Contenu |
|---|---|---|
| `settings` | `v1`, `nuit`, `vcmd` | réglages généraux, mode nuit, commandes vocales (blocs séparés) |
| `sensor` | `start`, `remind2` | heure de pose du capteur, rappels déjà sonnés (bit 0 = 24 h, bit 1 = 2 h) |
| `catheter` | `start`, `hours`, `remind2` | changement, intervalle, rappel 2 h déjà sonné |
| `conso` | `log_g7`, `log_cath` | dernière heure déjà écrite dans `consumables.csv` (anti-doublon) |
| `stock` | `q_g7`, `q_cath`, `s_g7`, `s_cath`, `t_g7`, `t_cath` | quantités, seuils, marqueurs de décrémentation |
| `dexcom` | — | verrou d'authentification (empreinte des identifiants, jamais les identifiants) |

Les anciennes clés `remind` sont **lues** pour la conversion depuis les versions
précédentes, mais jamais réécrites.

La partition `nvs` n'est jamais écrite par un flash : voir [HARDWARE_FR.md](HARDWARE_FR.md).

---

## Identifiants

`main/app_secrets.h` inclut `device_secrets.h` **s'il existe** (via `__has_include`) et
fournit sinon des valeurs vides. `main/CMakeLists.txt` ajoute `<racine>/private` aux
include privés uniquement si le fichier y est présent. Le projet compile donc sans
identifiants — l'appareil démarre et reste utilisable en mode démonstration.
