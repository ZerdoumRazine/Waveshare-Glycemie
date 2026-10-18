/*
 * Commandes vocales LOCALES avec les micros ES7210 de la carte (ESP-SR 2.5.3 d'Espressif, aucune reconnaissance dans
 * le cloud, aucune connexion).
 *
 * - Commandes : "GLYCEMIE", "ENREGISTRER NOUVEAU CAPTEUR" puis "CONFIRMER CAPTEUR" (K19), "NOUVEAU CATHETER",
 *   reconnues par MultiNet7 (mn7_en) a partir de phonemes ecrits pour la prononciation francaise (Espressif ne
 *   fournit ni MultiNet francais ni WakeNet francais personnalise en libre-service), et en meme temps les
 *   equivalents anglais "GLUCOSE" et "NEW CATHETER". Reponses toujours en francais. K19 (2026-09-21) : les phrases
 *   courtes "NOUVEAU CAPTEUR" / "NEW SENSOR" sont retirees ("New sensor" provoquait les annonces spontanees).
 *   Option : mot d'eveil officiel francais WakeNet9L "Bonjour ESP" avant la commande.
 * - Modeles dans la partition "model" (srmodels.bin genere et flashe par le composant esp-sr, placee apres les
 *   partitions d'origine, au-dela de 16 Mo) : lus une fois en PSRAM puis charges par srmodel_load() ; aucune
 *   dependance a la microSD. Premiere ecoute 60 s apres la mise sous tension ; jamais de demarrage automatique apres
 *   une panique ou un watchdog (pas de boucle de redemarrage possible).
 * - "GLYCEMIE" : reponse immediate avec la voix Chirp de la microSD (valeur + tendance, ou "Donnees anciennes").
 * - Capteur (K19) : "ENREGISTRER NOUVEAU CAPTEUR" ne modifie rien ; reponse "Nouveau capteur. Dites confirmer
 *   capteur." ; seule la phrase DIFFERENTE "CONFIRMER CAPTEUR", entendue dans les 10 s qui suivent la fin de
 *   l'annonce, enregistre l'heure actuelle (NVS, historique microSD) ; sinon annulation automatique, puis
 *   "ENREGISTRER NOUVEAU CAPTEUR" ignore pendant 5 min (aucune annonce repetee). Seuils propres a ces deux phrases,
 *   plus stricts que ceux des commandes de lecture. Le bouton tactile NOUVEAU CAPTEUR ne depend pas de la voix.
 * - "NOUVEAU CATHETER" : une seule detection ne modifie JAMAIS un compteur ; reponse "Repetez pour confirmer",
 *   deuxieme detection de la meme commande dans les 10 s qui suivent la fin de la phrase -> heure actuelle
 *   enregistree, "... enregistre" ; sinon annulation automatique.
 * - Detection suspendue pendant toute lecture audio (alertes, voix, bips) et 0,6 s apres : l'appareil ne reagit pas
 *   a sa propre voix. Modeles remis a zero a la reprise.
 * - Taches : lecture des micros et detection dans la tache "voixcmd" (pile en PSRAM) ; l'enregistrement NVS est fait
 *   par la tache LVGL (ui_request_consumable_new).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    VOICE_CMD_NONE = 0,
    VOICE_CMD_GLUCOSE,   /* "Glycemie" */
    VOICE_CMD_SENSOR,    /* capteur : "Enregistrer nouveau capteur" puis "Confirmer capteur" */
    VOICE_CMD_CATHETER,  /* "Nouveau catheter" */
} voice_cmd_id_t;

/* Au demarrage, apres audio_out_init() : demarre l'ecoute si elle est activee dans les reglages */
esp_err_t voice_cmd_init(void);

/* Reglages modifies (RÉGLAGES ou console) : demarre / arrete l'ecoute, applique le mot d'eveil et le gain */
void voice_cmd_apply_settings(void);

bool voice_cmd_running(void);

/* Texte de la pastille du bandeau superieur ("" si aucune commande en cours) ; tache LVGL */
void voice_cmd_ui_hint(char *buf, size_t size);

/* Etat (RÉGLAGES > COMMANDES VOCALES) : "a l'ecoute", "arretees", "erreur ..." */
const char *voice_cmd_status_text(void);

/* Console */
void voice_cmd_print_info(void);
/* Detection simulee d'une phrase MultiNet (1 glycemie, 3 catheter, 4 glucose, 5 enregistrer nouveau capteur,
 * 6 confirmer capteur) avec sa probabilite : meme chemin qu'une vraie detection (seuil, confirmation, anti-repetition) */
void voice_cmd_simulate(int mn_id, float prob);
/* Test : son PCM 16 bits mono 16 kHz ajoute au signal des micros en temps reel (console vrx_inject) */
bool voice_cmd_inject(const uint8_t *pcm, size_t bytes);
void voice_cmd_set_level_log(bool on);        /* niveau des micros chaque seconde */
void voice_cmd_set_decode_log(bool on);       /* chaine decodee par MultiNet (reglage des phonemes) */
void voice_cmd_add_phrase(int mn_id, const char *phonemes); /* variante temporaire (perdue au redemarrage) */
