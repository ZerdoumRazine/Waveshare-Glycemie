# Crédits

> Les crédits ci-dessous sont ceux voulus par la famille. Les composants tiers gardent leurs
> propres auteurs et leurs propres licences : rien de ce qu'ils ont écrit n'est attribué ici à
> quelqu'un d'autre (voir [docs/LICENCES_TIERCES_FR.md](docs/LICENCES_TIERCES_FR.md)).

---

## Conception et développement

**Initiateur et responsable du projet** — **Mohamed-Razine Zerdoum**

Le projet existe parce qu'il en avait besoin, et il est ce qu'il est parce qu'il l'a dirigé :

- le **besoin** et le cahier des charges : un afficheur lisible de plusieurs mètres, dans la
  cuisine, qui ne remplace rien mais qui évite d'aller chercher un téléphone ;
- le **choix du matériel** et sa mise en œuvre réelle : carte, écran, haut-parleur, microSD ;
- les **décisions de conception**, y compris celles qui ont retiré des fonctions plutôt que
  d'en ajouter (voir plus bas) ;
- les **seuils, les textes et les comportements d'alerte**, réglés sur un usage quotidien réel ;
- la **validation sur le matériel** : quinze itérations successives (V1 → K19) entre le 13 et
  le 21 septembre 2026, chacune compilée, installée et éprouvée sur la carte, puis les versions
  K20 à K22 (six langues, assistants de connexion, version publique).

Les décisions de conception les plus marquantes lui appartiennent :

- une seule information dominante sur l'accueil, lisible de plusieurs mètres ;
- la suppression de la page STATS et des voyants de liaison, jugés encombrants ;
- la priorité donnée à la **fiabilité** sur la portée pour les commandes vocales
  (« je préfère devoir répéter une commande plutôt que d'entendre l'appareil parler seul ») ;
- l'exigence qu'un redémarrage ou une recompilation ne perde jamais une date de pose.

## Comment ce code a été écrit

Honnêtement : **une grande partie du code, des tests et de la documentation a été produite avec
l'aide d'outils d'assistance automatisée**, à partir des demandes, des choix et des corrections du
responsable du projet. Il n'a pas écrit personnellement chaque ligne, et ce fichier ne le prétend
pas.

Ce qui lui revient, et qui n'est pas automatisable : le besoin, la direction, les arbitrages, les
réglages issus d'un usage quotidien, et la vérification de chaque version sur l'appareil réel —
c'est-à-dire tout ce qui fait que ce projet existe et qu'il fonctionne chez lui.

Le code produit de cette façon est publié tel quel, sous licence MIT, avec ses limites écrites
noir sur blanc dans [docs/LIMITATIONS_FR.md](docs/LIMITATIONS_FR.md).

## Remerciements

- **Waveshare** — carte ESP32-P4-WIFI6-Touch-LCD-7B, BSP et exemples.
- **Espressif** — ESP-IDF, ESP-Hosted, ESP-SR, et le correctif du gel PPA.
- **LVGL** — bibliothèque graphique.
- **pydexcom** (Gage Benne) — constantes du protocole Dexcom Share (MIT).
- **Julieta Ulanovsky et contributeurs** — polices Montserrat (SIL OFL 1.1).

Détail des licences : [docs/LICENCES_TIERCES_FR.md](docs/LICENCES_TIERCES_FR.md).

---

*Projet personnel, développé pour un usage familial réel. Ce n'est pas un dispositif
médical.*
