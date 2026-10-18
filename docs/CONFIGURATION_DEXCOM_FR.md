# Configuration Dexcom Share

L'appareil lit les mesures via **Dexcom Share**, le service qui permet à une personne
suivie de partager ses glycémies avec des proches. Il ne dialogue jamais avec le capteur, et
n'écrit **jamais** rien côté Dexcom : toutes les requêtes sont en lecture.

Chaîne complète : capteur G7 → application Dexcom du téléphone → serveurs Dexcom Share →
Internet → Wi-Fi → écran.

---

## 1. Côté Dexcom, avant tout

1. Dans l'application Dexcom du téléphone qui porte le capteur, activer **Partage**
   (« Share ») et créer au moins un abonné (« Follower »). Le partage doit être **actif** :
   sans abonné, le service Share ne publie rien.
2. Le téléphone doit rester connecté à Internet : c'est lui qui pousse les mesures vers
   Dexcom. Si le téléphone est hors ligne, l'écran affichera « DONNÉES ANCIENNES ».

## 2. Quel compte utiliser

> Le firmware s'authentifie avec le compte **qui partage** (le *publisher*), c'est-à-dire
> celui de la personne portant le capteur — **pas** un compte d'abonné.

**Méthode recommandée (K22) :** sur l'écran, ouvrir **RÉGLAGES → CONNEXION**
ou suivre l'assistant qui s'affiche au premier démarrage. Saisir son propre
compte Dexcom Share et choisir la région. La connexion est essayée avant
confirmation ; les identifiants n'ont pas à être intégrés au code.

Une application Windows `GlycemieConnexion.exe` permet également cette
configuration par le câble USB.

**Ancienne méthode pour développeurs uniquement :** un firmware personnalisé
peut intégrer des valeurs dans `private/device_secrets.h`, hors dépôt.
**Ne jamais distribuer le binaire** d'un tel firmware.

## 3. Région

| Région | Valeur | Serveur |
|---|---|---|
| Europe / hors États-Unis | `"ous"` | `shareous1.dexcom.com` |
| États-Unis | `"us"` | `share2.dexcom.com` |

**Seule la région `ous` a été réellement utilisée et validée.** Le code accepte `us`, mais
ce chemin n'a jamais été essayé sur un compte américain.

Identifiant d'application utilisé : `d89443d2-327c-4a6f-89e5-496bbb0317db`, celui de
*pydexcom* (`DEXCOM_APPLICATION_ID_OUS = DEXCOM_APPLICATION_ID_US`). L'identifiant
`d8665ade-…` que l'on trouve dans d'anciens projets Arduino est celui du **Japon** : il ne
fonctionne pas ici.

## 4. Verrou d'authentification — à lire avant le premier démarrage

Dexcom bloque temporairement un compte après plusieurs échecs d'authentification. Pour
qu'un bug ou une boucle de redémarrage ne verrouille jamais le compte, `main/dexcom_guard.c`
enregistre en NVS l'état de l'authentification, lié à une **empreinte courte** des
identifiants (jamais les identifiants eux-mêmes) :

| État | Comportement |
|---|---|
| **INCONNU** | aucune authentification tant qu'une tentative unique n'est pas autorisée |
| **TENTATIVE EN COURS** | enregistré *avant* l'envoi ; après un redémarrage, aucune nouvelle tentative |
| **VALIDE** | fonctionnement normal : connexion au démarrage, renouvellement de session automatique |
| **REFUSE** | identifiants refusés ou compte bloqué : plus aucune tentative, même après redémarrage |
| **NON CONCLUANT** | identifiants envoyés sans verdict (réseau ou serveur) : plus de tentative automatique |

**Avec l'assistant K22**, l'utilisateur déclenche lui-même une tentative de
connexion et peut confirmer l'enregistrement lorsque le test réussit :
la console n'est **pas nécessaire** à l'installation normale.

**Sur l'ancienne méthode de compilation**, le verrou d'authentification
peut imposer une autorisation manuelle. La commande console de
diagnostic historique est :

```powershell
python tools\serial_capture.py --port COM7 --duration 60 --send "3:dexcom_status" --send "6:dexcom_auth" --log dexcom.log
```

- `dexcom_status` affiche l'état courant.
- `dexcom_auth` autorise **une** tentative, uniquement si l'état est INCONNU.

Si la requête n'a jamais quitté la carte (DNS, TCP ou TLS en échec), ce n'est pas comptée
comme une tentative : l'état revient à INCONNU. Changer les identifiants dans
`device_secrets.h` remet l'état à INCONNU (une nouvelle autorisation sera nécessaire).

## 5. Ce que lit l'appareil

Trois appels de l'API Share, tous en lecture :

1. `AuthenticatePublisherAccount`
2. `LoginPublisherAccountById`
3. `ReadPublisherLatestGlucoseValues`

Champs utilisés : `WT` (horodatage de la mesure), `ST`, `DT`, `Value` (mg/dL), `Trend`.

Politique de lecture (cache de 24 h en PSRAM, jusqu'à 288 mesures, fusion sans doublon) :

- au démarrage : 24 h complètes (1 440 min) ;
- ensuite : lecture courte à chaque nouvelle mesure attendue (dernière mesure + 5 min 20 s),
  jamais moins de 30 s entre deux requêtes ;
- rattrapage des 3 dernières heures toutes les heures ;
- relecture complète des 24 h toutes les 6 heures ;
- les pages GRAPHIQUE et AUJOURD'HUI n'interrogent **jamais** Dexcom : elles lisent le cache.

En cas de coupure réseau ou de session expirée, la reconnexion est automatique (observée en
≈ 6 s après une coupure Wi-Fi volontaire). Les mesures manquées sont rattrapées au retour,
sans doublon dans l'historique CSV.

## 6. Journalisation et vie privée

- Aucun identifiant, mot de passe, jeton de session ni UUID n'est écrit dans les journaux,
  sur la microSD ou à l'écran. Les messages d'erreur Dexcom sont journalisés avec l'étape,
  le code HTTP et le code Dexcom, le reste étant **masqué**.
- `tools/serial_capture.py` masque en plus, à la volée, toute valeur présente dans
  `private/device_secrets.h` et tout UUID, avant d'écrire le journal.
- Le SSID Wi-Fi n'est pas journalisé non plus.

## 7. Sans compte Dexcom

Le mode démonstration permet d'installer, d'essayer et de présenter l'appareil sans aucun
compte : il génère 24 h de données **fictives** en mémoire, avec un bandeau « MODE DÉMO » à
l'écran, et n'écrit rien sur la microSD.

```
demo normal      courbe réaliste autour de 150 mg/dL
demo low         descente vers 64
demo verylow     descente vers 48 (déclenche l'alerte très basse)
demo high        montée vers 265
demo veryhigh    montée vers 330
demo gap         trou d'une heure dans l'historique
demo stale       dernière mesure vieille de 18 min (DONNÉES ANCIENNES)
demo sparse      10 h de mesures sur 24 h (couverture insuffisante)
demo off         retour aux données réelles (les données simulées sont effacées)
```

## 8. Avertissement

Dexcom Share n'est **pas** une API publique documentée. Ce projet s'appuie sur le
comportement observé et sur le travail de *pydexcom* : un changement côté Dexcom peut
interrompre le service sans préavis. C'est l'une des raisons pour lesquelles cet afficheur
ne doit jamais remplacer les alarmes officielles du capteur et du téléphone.
