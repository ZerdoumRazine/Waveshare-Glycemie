/*
 * Identifiants de l'utilisateur (Wi-Fi et compte Dexcom Share) saisis SUR L'APPAREIL.
 *
 * Objectif : personne n'a besoin de recompiler le firmware pour mettre ses propres acces.
 * - Stockage : espace NVS "ident", separe des reglages ("settings") et du verrou Dexcom.
 *   Le bloc est brouille par un OU exclusif avec une cle derivee de l'adresse MAC de la carte :
 *   ce n'est PAS un chiffrement (le Flash Encryption n'est pas active sur ce projet), seulement
 *   de quoi eviter qu'un mot de passe se lise en clair dans une copie brute de la memoire.
 * - Les mots de passe ne sont JAMAIS journalises ni affiches : seules des formes masquees sortent
 *   d'ici (credentials_masked_*).
 * - Compatibilite : si la NVS est vide et que le firmware a ete compile avec private/device_secrets.h,
 *   ces valeurs servent de depart (l'appareil d'origine continue de fonctionner sans rien saisir).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define CRED_SSID_MAX   33  /* 32 caracteres + fin de chaine (norme Wi-Fi) */
#define CRED_PASS_MAX   65  /* 64 caracteres + fin de chaine */
#define CRED_USER_MAX   65
#define CRED_REGION_MAX 4   /* "ous", "us", "jp" */

typedef struct {
    char wifi_ssid[CRED_SSID_MAX];
    char wifi_password[CRED_PASS_MAX];
    char dexcom_user[CRED_USER_MAX];
    char dexcom_password[CRED_PASS_MAX];
    char dexcom_region[CRED_REGION_MAX];
} credentials_t;

/* A appeler juste apres settings_init() */
esp_err_t credentials_init(void);

/* Copie complete : reservee aux modules qui se connectent (Wi-Fi, Dexcom). Jamais affichee. */
void credentials_get(credentials_t *out);

bool credentials_has_wifi(void);
bool credentials_has_dexcom(void);
bool credentials_configured(void);   /* Wi-Fi ET Dexcom renseignes */
bool credentials_from_build(void);   /* valeurs heritees de private/device_secrets.h */

/* Enregistrement immediat (apres un essai reussi, ou pour une reprise en console). */
esp_err_t credentials_set_wifi(const char *ssid, const char *password);
esp_err_t credentials_set_dexcom(const char *user, const char *password, const char *region);
esp_err_t credentials_clear(void);   /* efface tout : NVS, memoire et verrou d'authentification */

/*
 * ESSAI DE NOUVEAUX ACCES, sans perdre les anciens.
 *
 * Regle appliquee partout (ecran tactile, console, assistant PC) :
 *   garder les anciens acces -> essayer les nouveaux -> enregistrer seulement apres reussite
 *   ET confirmation.
 *
 * Pendant un essai, les nouvelles valeurs sont utilisees en memoire vive (le test de connexion
 * porte donc bien sur elles) mais RIEN n'est ecrit dans la memoire permanente. Une annulation,
 * une erreur, une coupure de courant ou un redemarrage ramene donc les anciens acces : il n'y a
 * aucun etat intermediaire ou l'appareil se retrouverait sans ses acces precedents.
 */
bool credentials_trial_active(void);
void credentials_trial_begin(void);  /* photographie les acces actuels ; sans effet si deja en cours */
esp_err_t credentials_trial_set_wifi(const char *ssid, const char *password);
esp_err_t credentials_trial_set_dexcom(const char *user, const char *password, const char *region);
esp_err_t credentials_trial_commit(void);  /* enregistre l'essai (apres reussite et confirmation) */
void credentials_trial_cancel(void);       /* revient aux acces precedents, y compris en memoire */

/* Mode demonstration : donnees simulees, aucun compte. Memorise d'un demarrage a l'autre. */
bool credentials_demo(void);
esp_err_t credentials_set_demo(bool on);

/* Formes masquees pour l'ecran et la console (jamais de mot de passe) :
 * "MonReseau", "a***@exemple.fr", "renseigne (12 caracteres)" / "non renseigne". */
void credentials_masked_wifi(char *buf, size_t size);
void credentials_masked_dexcom(char *buf, size_t size);
void credentials_masked_password(char *buf, size_t size, bool wifi);

/* Regions acceptees par Dexcom Share (indice 0..2) : "ous" Europe, "us" Amerique du Nord, "jp" Japon */
const char *credentials_region_code(int index);
const char *credentials_region_label(int index);
int credentials_region_count(void);
int credentials_region_index(const char *code);

void credentials_print(void); /* console : etat masque, jamais de secret */
