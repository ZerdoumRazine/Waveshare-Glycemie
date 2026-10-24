/* Exercises credential persistence and simulated NVS failures.
 * Host tests use simulated dependencies, never personal readings.
 */
/*
 * Controls of the ID module (main/credentials.c) compiles as is on the PC.
 *
 * What these tests guarantee, without material:
 *   - the user can enter, reread, modify and delete accesses without recompiling;
 *   - invalid seizures (too long, unknown region) are refused without any damage;
 *   - IDs survive a reboot (NVS simulee) ;
 *   - there is no clear password in what would be written in flash;
 *   - no password appears in text posters or newspapers (masks).
 *
 * The values used here are fictional.
 */
/* Exercise credential validation, persistence, erasure and failed-write recovery.
 */
#include <stdio.h>
#include <string.h>

#include "credentials.h"
#include "mini_test.h"
#include "nvs.h"

#define SSID_TEST   "BoxSalon"
#define MDP_WIFI    "mot-de-passe-du-wifi-2024"
#define USER_TEST   "antoine.dupont@exemple.fr"
#define MDP_DEXCOM  "MonMotDePasseDexcom!42"

/* Search for a suite of bytes (the password) in a binary block. */
static int contient(const void *bloc, size_t taille, const char *motif)
{
    const char *p = (const char *)bloc;
    const size_t n = strlen(motif);
    if (bloc == NULL || n == 0 || taille < n) {
        return 0;
    }
    for (size_t i = 0; i + n <= taille; i++) {
        if (memcmp(p + i, motif, n) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Chaine de (CRED_PASS_MAX 1) characters: the longest accepted; +1 must be refused. */
static void remplir(char *buf, size_t len, char c)
{
    memset(buf, c, len);
    buf[len] = '\0';
}

int main(void)
{
    char buf[128];
    credentials_t c;

    section("Premiere mise en route : rien d'enregistre");
    fake_nvs_reset();
    CHECK_EQ_I(credentials_init(), ESP_OK, "initialisation sur une memoire vierge");
    CHECK(!credentials_has_wifi(), "aucun reseau Wi-Fi enregistre");
    CHECK(!credentials_has_dexcom(), "aucun compte Dexcom enregistre");
    CHECK(!credentials_configured(), "l'assistant de premiere mise en route doit s'ouvrir");
    CHECK(!credentials_demo(), "mode demonstration inactif par defaut");
    credentials_get(&c);
    CHECK(strcmp(c.dexcom_region, "ous") == 0, "region par defaut : Europe (ous)");

    section("Saisies refusees : rien n'est enregistre a moitie");
    CHECK_EQ_I(credentials_set_wifi(NULL, MDP_WIFI), ESP_ERR_INVALID_ARG, "nom de reseau absent");
    remplir(buf, CRED_SSID_MAX, 'x'); /* CRED_SSID_MAX characters: one of too many */
    CHECK_EQ_I(credentials_set_wifi(buf, MDP_WIFI), ESP_ERR_INVALID_ARG, "nom de reseau trop long");
    remplir(buf, CRED_PASS_MAX, 'y');
    CHECK_EQ_I(credentials_set_wifi(SSID_TEST, buf), ESP_ERR_INVALID_ARG, "mot de passe Wi-Fi trop long");
    CHECK(!credentials_has_wifi(), "apres des refus, toujours aucun reseau enregistre");
    CHECK_EQ_I(credentials_set_dexcom(USER_TEST, MDP_DEXCOM, "fr"), ESP_ERR_INVALID_ARG,
               "region inconnue refusee");
    CHECK(!credentials_has_dexcom(), "apres un refus, toujours aucun compte enregistre");

    section("Saisie acceptee : limites exactes");
    remplir(buf, CRED_SSID_MAX - 1, 'x');
    CHECK_EQ_I(credentials_set_wifi(buf, MDP_WIFI), ESP_OK, "nom de reseau de longueur maximale accepte");
    remplir(buf, CRED_PASS_MAX - 1, 'y');
    CHECK_EQ_I(credentials_set_wifi(SSID_TEST, buf), ESP_OK, "mot de passe de longueur maximale accepte");

    section("Configuration complete par l'utilisateur");
    CHECK_EQ_I(credentials_set_wifi(SSID_TEST, MDP_WIFI), ESP_OK, "reseau Wi-Fi enregistre");
    CHECK_EQ_I(credentials_set_dexcom(USER_TEST, MDP_DEXCOM, "us"), ESP_OK, "compte Dexcom enregistre");
    CHECK(credentials_configured(), "l'appareil est configure : plus d'assistant au demarrage");
    CHECK(!credentials_from_build(), "identifiants saisis, pas herites de la compilation");
    credentials_get(&c);
    CHECK(strcmp(c.wifi_ssid, SSID_TEST) == 0, "le nom du reseau est relu a l'identique");
    CHECK(strcmp(c.wifi_password, MDP_WIFI) == 0, "le mot de passe Wi-Fi est relu a l'identique");
    CHECK(strcmp(c.dexcom_user, USER_TEST) == 0, "le compte Dexcom est relu a l'identique");
    CHECK(strcmp(c.dexcom_password, MDP_DEXCOM) == 0, "le mot de passe Dexcom est relu a l'identique");
    CHECK(strcmp(c.dexcom_region, "us") == 0, "la region choisie est conservee");

    section("Redemarrage : les identifiants reviennent");
    memset(&c, 0, sizeof(c));
    CHECK_EQ_I(credentials_init(), ESP_OK, "relecture apres redemarrage");
    credentials_get(&c);
    CHECK(credentials_configured(), "toujours configure apres redemarrage");
    CHECK(strcmp(c.wifi_ssid, SSID_TEST) == 0, "nom du reseau conserve");
    CHECK(strcmp(c.wifi_password, MDP_WIFI) == 0, "mot de passe Wi-Fi conserve");
    CHECK(strcmp(c.dexcom_user, USER_TEST) == 0, "compte Dexcom conserve");
    CHECK(strcmp(c.dexcom_password, MDP_DEXCOM) == 0, "mot de passe Dexcom conserve");
    CHECK(!credentials_from_build(), "origine : saisie de l'utilisateur");

    section("Rien en clair dans ce qui est ecrit en flash");
    size_t taille = 0;
    const void *brut = fake_nvs_blob("ident", "v1", &taille);
    CHECK(brut != NULL && taille > 0, "un bloc est bien enregistre (%d octets)", (int)taille);
    CHECK(!contient(brut, taille, MDP_WIFI), "le mot de passe Wi-Fi n'est pas lisible dans le bloc");
    CHECK(!contient(brut, taille, MDP_DEXCOM), "le mot de passe Dexcom n'est pas lisible dans le bloc");
    CHECK(!contient(brut, taille, USER_TEST), "le compte Dexcom n'est pas lisible dans le bloc");
    CHECK(!contient(brut, taille, SSID_TEST), "le nom du reseau n'est pas lisible dans le bloc");

    section("Textes affiches : noms visibles, mots de passe masques");
    credentials_masked_wifi(buf, sizeof(buf));
    CHECK(strcmp(buf, SSID_TEST) == 0, "le nom du reseau reste lisible pour l'utilisateur : %s", buf);
    credentials_masked_dexcom(buf, sizeof(buf));
    CHECK(strcmp(buf, "a***@exemple.fr") == 0, "compte Dexcom masque : %s", buf);
    CHECK(strstr(buf, "ntoine") == NULL, "le nom complet n'apparait pas");
    credentials_masked_password(buf, sizeof(buf), true);
    CHECK(strstr(buf, MDP_WIFI) == NULL, "mot de passe Wi-Fi absent du texte affiche : %s", buf);
    CHECK(strstr(buf, "25") != NULL, "seule la longueur est indiquee (%s)", buf);
    credentials_masked_password(buf, sizeof(buf), false);
    CHECK(strstr(buf, MDP_DEXCOM) == NULL, "mot de passe Dexcom absent du texte affiche : %s", buf);
    credentials_masked_dexcom(buf, sizeof(buf));
    CHECK(buf[0] == 'a' && strncmp(buf + 1, "***", 3) == 0, "seule la premiere lettre du compte est montree");

    section("Regions proposees a l'ecran");
    CHECK_EQ_I(credentials_region_count(), 3, "trois regions");
    CHECK_EQ_I(credentials_region_index("ous"), 0, "Europe en premier");
    CHECK_EQ_I(credentials_region_index("us"), 1, "Amerique du Nord");
    CHECK_EQ_I(credentials_region_index("jp"), 2, "Japon");
    CHECK_EQ_I(credentials_region_index("zz"), -1, "region inconnue signalee");
    CHECK(strcmp(credentials_region_code(-1), "ous") == 0, "indice hors limites : Europe par defaut");
    CHECK(strcmp(credentials_region_code(99), "ous") == 0, "indice hors limites : Europe par defaut");
    CHECK(credentials_region_label(0) != NULL && credentials_region_label(0)[0] != '\0',
          "chaque region a un libelle : %s", credentials_region_label(0));

    section("Mode demonstration : aucun compte necessaire");
    CHECK_EQ_I(credentials_set_demo(true), ESP_OK, "mode demonstration active");
    CHECK(credentials_demo(), "actif");
    CHECK_EQ_I(credentials_init(), ESP_OK, "redemarrage");
    CHECK(credentials_demo(), "le mode demonstration survit au redemarrage");
    CHECK_EQ_I(credentials_set_demo(false), ESP_OK, "mode demonstration coupe");
    CHECK(!credentials_demo(), "inactif");

    section("Essai de nouveaux acces : les anciens restent enregistres");
    fake_nvs_reset();
    CHECK_EQ_I(credentials_init(), ESP_OK, "memoire vierge");
    CHECK_EQ_I(credentials_set_wifi(SSID_TEST, MDP_WIFI), ESP_OK, "acces de depart : Wi-Fi");
    CHECK_EQ_I(credentials_set_dexcom(USER_TEST, MDP_DEXCOM, "ous"), ESP_OK, "acces de depart : Dexcom");
    const int ecritures = fake_nvs_write_count();
    CHECK(!credentials_trial_active(), "aucun essai en cours au depart");
    credentials_trial_begin();
    CHECK(credentials_trial_active(), "essai ouvert");
    CHECK_EQ_I(credentials_trial_set_wifi("AutreBox", "autre-mot-de-passe"), ESP_OK, "nouveau Wi-Fi a l'essai");
    CHECK_EQ_I(credentials_trial_set_dexcom("bruno@exemple.fr", "AutreMotDePasse", "us"), ESP_OK,
               "nouveau compte a l'essai");
    credentials_get(&c);
    CHECK(strcmp(c.wifi_ssid, "AutreBox") == 0, "le test de connexion portera bien sur le nouveau reseau");
    CHECK(strcmp(c.dexcom_user, "bruno@exemple.fr") == 0, "et sur le nouveau compte");
    CHECK_EQ_I(fake_nvs_write_count(), ecritures, "RIEN n'a ete ecrit en memoire permanente");
    brut = fake_nvs_blob("ident", "v1", &taille);
    CHECK(!contient(brut, taille, "AutreBox"), "le bloc enregistre ne contient pas encore le nouveau reseau");

    section("Essai refuse : retour automatique aux acces precedents");
    credentials_trial_cancel();
    CHECK(!credentials_trial_active(), "essai termine");
    credentials_get(&c);
    CHECK(strcmp(c.wifi_ssid, SSID_TEST) == 0, "ancien reseau retrouve");
    CHECK(strcmp(c.wifi_password, MDP_WIFI) == 0, "ancien mot de passe Wi-Fi retrouve");
    CHECK(strcmp(c.dexcom_user, USER_TEST) == 0, "ancien compte retrouve");
    CHECK(strcmp(c.dexcom_password, MDP_DEXCOM) == 0, "ancien mot de passe Dexcom retrouve");
    CHECK(strcmp(c.dexcom_region, "ous") == 0, "ancienne region retrouvee");
    CHECK_EQ_I(fake_nvs_write_count(), ecritures, "aucune ecriture pour revenir en arriere");
    CHECK(credentials_configured(), "l'appareil reste configure : il n'a jamais perdu ses acces");

    section("Coupure de courant pendant un essai : equivaut a une annulation");
    credentials_trial_begin();
    CHECK_EQ_I(credentials_trial_set_wifi("BoxDeTest", "mot-de-passe-de-test"), ESP_OK, "essai en cours");
    CHECK_EQ_I(credentials_init(), ESP_OK, "redemarrage brutal (relecture de la memoire)");
    CHECK(!credentials_trial_active(), "aucun essai n'est repris au demarrage");
    credentials_get(&c);
    CHECK(strcmp(c.wifi_ssid, SSID_TEST) == 0, "les acces precedents sont ceux qui redemarrent");
    CHECK(strcmp(c.dexcom_password, MDP_DEXCOM) == 0, "mot de passe Dexcom precedent conserve");

    section("Essai reussi et confirme : les nouveaux acces remplacent les anciens");
    CHECK_EQ_I(credentials_trial_commit(), ESP_ERR_INVALID_STATE, "rien a confirmer sans essai");
    credentials_trial_begin();
    CHECK_EQ_I(credentials_trial_set_wifi("BoxVoisine", "mot-de-passe-voisin"), ESP_OK, "nouveau Wi-Fi");
    CHECK_EQ_I(credentials_trial_set_dexcom("claire@exemple.fr", "MotDePasseClaire", "jp"), ESP_OK,
               "nouveau compte");
    CHECK_EQ_I(credentials_trial_commit(), ESP_OK, "confirmation");
    CHECK(!credentials_trial_active(), "plus d'essai en cours");
    CHECK(fake_nvs_write_count() > ecritures, "cette fois, la memoire permanente a ete ecrite");
    CHECK_EQ_I(credentials_init(), ESP_OK, "redemarrage");
    credentials_get(&c);
    CHECK(strcmp(c.wifi_ssid, "BoxVoisine") == 0, "nouveau reseau conserve apres redemarrage");
    CHECK(strcmp(c.dexcom_user, "claire@exemple.fr") == 0, "nouveau compte conserve");
    CHECK(strcmp(c.dexcom_region, "jp") == 0, "nouvelle region conservee");
    CHECK(strcmp(c.wifi_password, MDP_WIFI) != 0, "l'ancien mot de passe Wi-Fi a bien ete remplace");
    credentials_trial_cancel();
    CHECK(!credentials_trial_active(), "annuler apres confirmation ne fait rien");
    credentials_get(&c);
    CHECK(strcmp(c.wifi_ssid, "BoxVoisine") == 0, "les acces confirmes ne sont pas defaits");

    section("Essai : saisies invalides refusees sans toucher aux acces en place");
    credentials_trial_begin();
    CHECK_EQ_I(credentials_trial_set_wifi("", MDP_WIFI), ESP_ERR_INVALID_ARG, "nom de reseau vide");
    remplir(buf, CRED_PASS_MAX, 'z');
    CHECK_EQ_I(credentials_trial_set_wifi("BoxX", buf), ESP_ERR_INVALID_ARG, "mot de passe trop long");
    CHECK_EQ_I(credentials_trial_set_dexcom("", MDP_DEXCOM, "ous"), ESP_ERR_INVALID_ARG, "compte vide");
    CHECK_EQ_I(credentials_trial_set_dexcom(USER_TEST, MDP_DEXCOM, "xx"), ESP_ERR_INVALID_ARG,
               "region inconnue");
    credentials_get(&c);
    CHECK(strcmp(c.wifi_ssid, "BoxVoisine") == 0, "les acces en place sont intacts apres ces refus");
    credentials_trial_cancel();

    section("Effacement a la demande de l'utilisateur");
    fake_nvs_reset();
    CHECK_EQ_I(credentials_init(), ESP_OK, "memoire vierge");
    CHECK_EQ_I(credentials_set_wifi(SSID_TEST, MDP_WIFI), ESP_OK, "Wi-Fi enregistre");
    CHECK_EQ_I(credentials_set_dexcom(USER_TEST, MDP_DEXCOM, "us"), ESP_OK, "Dexcom enregistre");
    CHECK_EQ_I(credentials_clear(), ESP_OK, "effacement");
    CHECK(!credentials_has_wifi() && !credentials_has_dexcom(), "plus rien en memoire vive");
    CHECK(!credentials_configured(), "l'assistant s'ouvrira au prochain demarrage");
    /* A configuration NVS explicitly empty prevents the recovery of old
     * identifiers that would be embedded in a personal firmware. */
    brut = fake_nvs_blob("ident", "v1", &taille);
    CHECK(brut != NULL && taille == sizeof(credentials_t),
          "un bloc NVS vide est conserve comme marqueur, meme apres effacement");
    CHECK(!contient(brut, taille, MDP_WIFI), "mot de passe Wi-Fi ancien absent du marqueur");
    CHECK(!contient(brut, taille, MDP_DEXCOM), "mot de passe Dexcom ancien absent du marqueur");
    credentials_get(&c);
    CHECK(strcmp(c.dexcom_region, "ous") == 0, "region revenue au defaut");
    CHECK_EQ_I(credentials_init(), ESP_OK, "redemarrage apres effacement");
    CHECK(!credentials_configured(), "toujours vierge apres redemarrage");
    credentials_masked_wifi(buf, sizeof(buf));
    CHECK(strcmp(buf, "-") == 0, "plus de nom de reseau a afficher");
    credentials_masked_password(buf, sizeof(buf), true);
    CHECK(strcmp(buf, "-") == 0, "plus de mot de passe a afficher");

    section("Effacement sur erreur NVS : aucun faux succes");
    fake_nvs_reset();
    CHECK_EQ_I(credentials_init(), ESP_OK, "memoire vierge pour les pannes");
    CHECK_EQ_I(credentials_set_wifi(SSID_TEST, MDP_WIFI), ESP_OK, "Wi-Fi initial");
    CHECK_EQ_I(credentials_set_dexcom(USER_TEST, MDP_DEXCOM, "ous"), ESP_OK,
               "Dexcom initial");

    fake_nvs_fail_next_open("ident");
    CHECK_EQ_I(credentials_clear(), ESP_FAIL, "echec NVS ident : erreur signalee");
    CHECK(credentials_configured(), "configuration active conservee apres refus NVS");
    CHECK_EQ_I(credentials_init(), ESP_OK, "redemarrage apres echec NVS");
    CHECK(credentials_configured(), "acces permanents conserves apres l'echec");

    /* An error in the dexcom auxiliary space should not hide the fact
     * that the secrets have been removed from NVS ident. */
    fake_nvs_fail_next_open("dexcom");
    CHECK_EQ_I(credentials_clear(), ESP_OK, "identifiants effaces meme si nettoyage du verrou refuse");
    CHECK(!credentials_configured(), "aucun compte en memoire");
    brut = fake_nvs_blob("ident", "v1", &taille);
    CHECK(brut != NULL && taille == sizeof(credentials_t),
          "bloc vide enregistre avant nettoyage du verrou");
    CHECK_EQ_I(credentials_init(), ESP_OK, "redemarrage apres nettoyage partiel");
    CHECK(!credentials_configured(), "aucun retour des identifiants retires");

    section("Nettoyage du verrou dexcom apres effacement confirme");
    CHECK_EQ_I(credentials_set_wifi(SSID_TEST, MDP_WIFI), ESP_OK, "nouveau reseau");
    CHECK_EQ_I(credentials_set_dexcom(USER_TEST, MDP_DEXCOM, "ous"), ESP_OK, "nouveau compte");
    nvs_handle_t guard_handle = 0;
    CHECK_EQ_I(nvs_open("dexcom", NVS_READWRITE, &guard_handle), ESP_OK,
               "espace du verrou dexcom accessible");
    const uint8_t tag[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK_EQ_I(nvs_set_blob(guard_handle, "cred", tag, sizeof(tag)), ESP_OK,
               "empreinte de test ecrite");
    CHECK_EQ_I(nvs_commit(guard_handle), ESP_OK, "empreinte de test confirmee");
    nvs_close(guard_handle);
    CHECK(fake_nvs_blob("dexcom", "cred", &taille) != NULL, "empreinte de test presente");
    CHECK_EQ_I(credentials_clear(), ESP_OK, "effacement des identifiants et du verrou");
    CHECK(fake_nvs_blob("dexcom", "cred", &taille) == NULL,
          "verrou de l'ancien compte retire de l'espace correct");
    CHECK_EQ_I(credentials_init(), ESP_OK, "redemarrage");
    CHECK(!credentials_configured(), "configuration toujours vierge");

    return test_report("test_credentials");
}
