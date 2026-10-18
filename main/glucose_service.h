/*
 * Service glycemie : planifie les lectures Dexcom Share, gere session, erreurs et delais,
 * et maintient un cache des 24 dernieres heures (lectures incrementales, sans reauthentification).
 * Les identifiants jamais valides ne declenchent qu'UNE tentative, sur autorisation explicite.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "app_state.h"

/*
 * Appele apres chaque lecture apportant des mesures reelles nouvelles.
 * `cache` est trie du plus recent au plus ancien ; `previous_newest_ms` est l'horodatage
 * de la mesure la plus recente avant la lecture (0 au demarrage).
 * Jamais appele en mode demonstration.
 */
typedef void (*glucose_new_readings_cb_t)(const glucose_point_t *cache, int count, int64_t previous_newest_ms);

esp_err_t glucose_service_start(void);

/* Assistant de configuration : essai de connexion avec les identifiants enregistres.
 * Le test tourne dans sa propre tache ; l'interface interroge l'etat sans jamais bloquer. */
typedef enum {
    GLY_TEST_INACTIF = 0,
    GLY_TEST_EN_COURS,
    GLY_TEST_REUSSI,
    GLY_TEST_ECHEC,
} gly_test_t;
void glucose_service_test_request(void);                    /* lance un essai (ignore si deja en cours) */
gly_test_t glucose_service_test_state(char *msg, size_t size); /* message pret a afficher (sans secret) */
void glucose_service_set_new_readings_cb(glucose_new_readings_cb_t cb);

/* Secondes depuis le dernier signe de vie de la tache (surveillance par app_main) */
int64_t glucose_service_heartbeat_age_s(void);

/* Autorise la tentative unique d'authentification ; renvoie un compte rendu textuel */
const char *glucose_service_request_auth(void);

/* Test de fiabilite : force une session expiree pour verifier le renouvellement automatique */
const char *glucose_service_test_invalid_session(void);

/* Mode demonstration (console) : off, normal, low, verylow, high, veryhigh, gap, stale */
const char *glucose_service_set_demo(const char *scenario);
