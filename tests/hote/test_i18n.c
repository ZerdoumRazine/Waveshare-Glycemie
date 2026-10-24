/* Verifies translation IDs and supported languages.
 * Host tests use simulated dependencies, never personal readings.
 */
/*
 * Translation table tests (main/i18n.c compile as is).
 *
 * Real danger is not a spelling error: it is an absent format specifier or
 * different from one language to another. "Day %d / %d" translated to "Day %d" would read an argument
 * non-existent on the map. These tests therefore compare, for each text, the exact sequence of
 * specifiers of all languages with that of French.
 */
/* Check translation coverage and language-specific formatting.
 */
#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "mini_test.h"

/* Extract the sequence of specifiers in a format: "Day %d / %d" -> "dd", "%%" ignores. */
static void specs(const char *s, char *out, size_t size)
{
    size_t n = 0;
    for (const char *p = s; *p != '\0' && n + 1 < size; p++) {
        if (*p != '%') {
            continue;
        }
        p++;
        if (*p == '%' || *p == '\0') {
            continue; /* "%%" : a literary percentage, not an argument */
        }
        while (*p != '\0' && strchr("0123456789.+- #'", *p) != NULL) {
            p++;
        }
        while (*p == 'l' || *p == 'h' || *p == 'z' || *p == 'j') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        out[n++] = *p;
    }
    out[n] = '\0';
}

int main(void)
{
    section("Les six langues sont declarees");
    CHECK_EQ_I(LANG_COUNT, 6, "6 langues (francais, anglais, espagnol, allemand, italien, portugais)");
    for (int l = 0; l < LANG_COUNT; l++) {
        CHECK(lang_name((lang_t)l) != NULL && lang_name((lang_t)l)[0] != '\0', "nom de la langue %d renseigne", l);
        CHECK(lang_code((lang_t)l) != NULL && strlen(lang_code((lang_t)l)) == 2, "code ISO a 2 lettres (langue %d)", l);
    }
    CHECK(strcmp(lang_name(LANG_FR), "Fran\xc3\xa7" "ais") == 0, "LANG_FR se nomme \"Francais\"");
    CHECK(strcmp(lang_code(LANG_EN), "en") == 0, "LANG_EN a le code \"en\"");
    CHECK(lang_name((lang_t)99) != NULL, "langue hors plage : repli, pas de plantage");

    section("Aucun texte vide, dans aucune langue");
    int vides = 0;
    int premier_vide = -1;
    for (int id = 0; id < S_COUNT; id++) {
        for (int l = 0; l < LANG_COUNT; l++) {
            i18n_set_lang((lang_t)l);
            const char *t = tr((str_id_t)id);
            if (t == NULL || t[0] == '\0') {
                if (premier_vide < 0) {
                    premier_vide = id;
                }
                vides++;
            }
        }
    }
    CHECK_EQ_I(vides, 0, "les %d textes sont traduits dans les 6 langues (1er manquant : id %d)",
               S_COUNT, premier_vide);

    section("Specificateurs de format identiques dans toutes les langues");
    int divergences = 0;
    int premier_ecart = -1;
    for (int id = 0; id < S_COUNT; id++) {
        i18n_set_lang(LANG_FR);
        char ref[16];
        specs(tr((str_id_t)id), ref, sizeof(ref));
        for (int l = 1; l < LANG_COUNT; l++) {
            i18n_set_lang((lang_t)l);
            char autre[16];
            specs(tr((str_id_t)id), autre, sizeof(autre));
            if (strcmp(ref, autre) != 0) {
                if (premier_ecart < 0) {
                    premier_ecart = id;
                    printf("        id %d : francais \"%s\" (%s) vs langue %d \"%s\" (%s)\n", id,
                           (i18n_set_lang(LANG_FR), tr((str_id_t)id)), ref, l,
                           (i18n_set_lang((lang_t)l), tr((str_id_t)id)), autre);
                }
                divergences++;
            }
        }
    }
    CHECK_EQ_I(divergences, 0, "aucun %%d ou %%s perdu en traduction (1er ecart : id %d)", premier_ecart);

    section("Quelques traductions attendues");
    i18n_set_lang(LANG_EN);
    CHECK(strcmp(tr(S_NAV_HOME), "HOME") == 0, "S_NAV_HOME en anglais -> \"HOME\"");
    CHECK(strcmp(tr(S_FRESH_OK), "UP TO DATE") == 0, "S_FRESH_OK en anglais");
    i18n_set_lang(LANG_DE);
    CHECK(strcmp(tr(S_NAV_SETTINGS), "EINSTELLUNG") == 0, "S_NAV_SETTINGS en allemand");
    i18n_set_lang(LANG_ES);
    CHECK(strcmp(tr(S_TREND_FLAT), "Estable") == 0, "S_TREND_FLAT en espagnol");

    section("Le texte francais reste accessible pour les journaux");
    for (int l = 0; l < LANG_COUNT; l++) {
        i18n_set_lang((lang_t)l);
        CHECK(strcmp(tr_fr(S_ALERT_LOW), "GLYC\xc3\x89MIE BASSE") == 0,
              "tr_fr() reste en francais quelle que soit la langue (%s)", lang_code((lang_t)l));
    }
    CHECK(tr_fr((str_id_t)-1)[0] == '\0', "tr_fr() hors plage : chaine vide");
    CHECK(tr((str_id_t)S_COUNT)[0] == '\0', "tr() hors plage : chaine vide");

    section("Dates longues");
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = 126; /* 2026 */
    t.tm_mon = 9;    /* octobre */
    t.tm_mday = 15;
    t.tm_wday = 4;   /* jeudi */
    for (int l = 0; l < LANG_COUNT; l++) {
        i18n_set_lang((lang_t)l);
        char buf[64] = "";
        i18n_format_date_long(&t, buf, sizeof(buf));
        CHECK(strlen(buf) > 6 && strstr(buf, "15") != NULL, "date longue en %s : \"%s\"",
              lang_code((lang_t)l), buf);
        CHECK(!(buf[0] >= 'a' && buf[0] <= 'z'), "date longue en %s : majuscule initiale", lang_code((lang_t)l));
    }
    i18n_set_lang(LANG_FR);
    t.tm_mday = 1;
    char buf[64];
    i18n_format_date_long(&t, buf, sizeof(buf));
    CHECK(strstr(buf, "1er") != NULL, "francais : le premier du mois s'ecrit \"1er\" (%s)", buf);
    i18n_set_lang(LANG_EN);
    i18n_format_date_long(&t, buf, sizeof(buf));
    CHECK(strstr(buf, "1er") == NULL, "anglais : pas de \"1er\" (%s)", buf);

    section("Noms de jours");
    for (int l = 0; l < LANG_COUNT; l++) {
        i18n_set_lang((lang_t)l);
        for (int j = 0; j < 7; j++) {
            if (i18n_day_name(j) == NULL || i18n_day_name(j)[0] == '\0') {
                CHECK(false, "jour %d manquant en %s", j, lang_code((lang_t)l));
            }
        }
    }
    CHECK(i18n_day_name(-1) != NULL && i18n_day_name(99) != NULL, "jour hors plage : repli, pas de plantage");

    i18n_set_lang(LANG_FR);
    return test_report("test_i18n");
}
