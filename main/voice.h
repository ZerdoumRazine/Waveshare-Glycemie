/*
 * Annonces vocales francaises LOCALES (aucune dependance Internet, aucun appel Google depuis la carte) :
 *
 * - banque principale : fichiers WAV de la microSD. Le francais est a la racine,
 *   /glycemie/audio/voice/ ; les autres langues dans un sous-dossier par code ISO
 *   (/glycemie/audio/voice/en/, /es/, /de/, /it/, /pt/). Si la banque de la langue choisie
 *   n'est pas installee, la banque francaise prend le relais plutot que le silence.
 *   (PCM 16 bits mono 16 kHz, voix Google
 *   fr-FR-Chirp3-HD-Aoede generee une seule fois sur le PC par tools/voice/gen_voice_google.py). Une alerte joue
 *   la phrase d'alerte puis la valeur : alert_low.wav + glucose_078.wav ("Attention, glycemie basse. Glycemie
 *   soixante-dix-huit."). Fichier absent ou carte indisponible : aucune voix, le bip d'alerte deja joue sert de
 *   secours. Les fichiers sont lus dans la tache audio, jamais dans l'interface.
 * - ancienne banque integree au firmware (voix Windows fr-FR, nombres composes mot a mot) : conservee
 *   temporairement jusqu'a validation de la nouvelle voix, jouee seulement par la console (voice_old).
 */
/* Public interface / Interface publique
 * EN: PCM announcement playback and microSD language fallback.
 * FR: Lecture PCM des annonces et repli de langue.
 */
#pragma once


#include <stdbool.h>
#include "alerts.h"

/* Langue changee : la banque vocale de la nouvelle langue sera cherchee a nouveau */
void voice_lang_changed(void);

/* Ancienne banque integree presente dans le firmware */
bool voice_available(void);

/* Annonce associee a une alerte (phrase + valeur eventuelle), jouee apres le son d'alerte */
void voice_announce_alert(alert_type_t type, int value, int volume_pct);

/* TEST VOIX : "Glycemie <nombre>." (glucose_NNN.wav, 120 pour le bouton) ; bip de test si le fichier manque */
void voice_test(int number, int volume_pct);

/* Reponses des commandes vocales (vc_*.wav) ; bip de test si le fichier manque */
typedef enum {
    VOICE_REPLY_SENSOR_CONFIRM = 0, /* "Nouveau capteur. Repetez pour confirmer." */
    VOICE_REPLY_SENSOR_SAVED,       /* "Nouveau capteur enregistre." */
    VOICE_REPLY_CATHETER_CONFIRM,   /* "Nouveau catheter. Repetez pour confirmer." */
    VOICE_REPLY_CATHETER_SAVED,     /* "Nouveau catheter enregistre." */
    VOICE_REPLY_CANCELLED,          /* "Commande annulee." */
    VOICE_REPLY_TIME_UNAVAILABLE,   /* "Heure non synchronisee. Enregistrement impossible." */
    VOICE_REPLY_SENSOR_ASK_CONFIRM, /* K19 : "Nouveau capteur. Dites confirmer capteur." */
    VOICE_REPLY_COUNT,
} voice_reply_t;
void voice_reply(voice_reply_t reply, int volume_pct);

/* Glycemie a la demande : "Glycemie cent quinze." puis la tendance ("Stable.", "En hausse."...) ; donnee ancienne :
 * "Donnees anciennes. Derniere mesure :" puis la valeur, sans tendance ; aucune mesure : vc_no_data.wav */
void voice_say_glucose(int mgdl, uint8_t trend, bool stale, bool available, int volume_pct);

/* Ancienne voix integree (console) : phrase d'alerte, ou type ALERT_TEST : "Test du volume. Glycemie <valeur>
 * milligrammes par decilitre." */
void voice_old_announce(alert_type_t type, int value, int volume_pct);
