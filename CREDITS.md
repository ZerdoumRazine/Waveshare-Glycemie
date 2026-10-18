# Crédits

> Les crédits ci-dessous sont ceux voulus par la famille. Les composants tiers gardent leurs
> propres auteurs et leurs propres licences : rien de ce qu'ils ont écrit n'est attribué ici à
> quelqu'un d'autre (voir [docs/LICENCES_TIERCES_FR.md](docs/LICENCES_TIERCES_FR.md)).

---

## Conception et développement

**Développeur principal et initiateur du projet** — **Mohamed-Razine Zerdoum**

L'idée, le besoin, la conception et la quasi-totalité du développement viennent de lui :
choix du matériel, architecture, intégration Dexcom Share, interface, alertes, suivi des
consommables, mode nuit, commandes vocales, et les quinze itérations successives
(V1 → K19) menées entre le 13 et le 21 septembre 2026, chacune compilée, flashée et
vérifiée sur la carte réelle.

Les décisions de conception les plus marquantes lui appartiennent :

- une seule information dominante sur l'accueil, lisible de plusieurs mètres ;
- la suppression de la page STATS et des voyants de liaison, jugés encombrants ;
- la priorité donnée à la **fiabilité** sur la portée pour les commandes vocales
  (« je préfère devoir répéter une commande plutôt que d'entendre l'appareil parler seul ») ;
- l'exigence qu'un redémarrage ou une recompilation ne perde jamais une date de pose.

## Assistance

Assistance technique par Claude (Anthropic) pendant le développement : mise au point,
diagnostics (gel PPA, doublons microSD, analyse des faux déclenchements vocaux), mesures,
tests, documentation. La direction du projet, les arbitrages et la validation sur matériel
réel sont restés du côté du développeur principal.

## Remerciements

- **Waveshare** — carte ESP32-P4-WIFI6-Touch-LCD-7B, BSP et exemples.
- **Espressif** — ESP-IDF, ESP-Hosted, ESP-SR, et le correctif du gel PPA.
- **LVGL** — bibliothèque graphique.
- **pydexcom** (Gage Benne) et les projets Dexcom libres — référence du protocole Share.
- **Julieta Ulanovsky et contributeurs** — polices Montserrat (SIL OFL 1.1).

Détail des licences : [docs/LICENCES_TIERCES_FR.md](docs/LICENCES_TIERCES_FR.md).

---

*Projet personnel, développé pour un usage familial réel. Ce n'est pas un dispositif
médical.*
