/* Covers glucose arithmetic and its boundary values.
 * FR: Teste les calculs glycémiques et leurs limites.
 * Host tests use simulated dependencies, never personal readings.
 */
/*
 * Tests du calcul glycemique reel (main/glucose_math.c et main/app_state.c compiles tels quels) :
 * age et fraicheur, cadence, variation entre mesures, prochaine mesure, noms de tendance, historique.
 */
/* EN: Verify reading age, cadence and delta using controlled sample timestamps.
 * FR: Teste les calculs d'ancienneté et de variation des mesures.
 */
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_state.h"
#include "glucose_math.h"
#include "mini_test.h"

#define MIN_MS (60LL * 1000)

/* Serie de `n` mesures espacees de `step_s`, la plus recente en tete (comme dans le firmware). */
static void serie(glucose_point_t *p, int n, int64_t newest_ms, int step_s, int16_t first, int16_t delta)
{
    for (int i = 0; i < n; i++) {
        p[i].wt_ms = newest_ms - (int64_t)i * step_s * 1000;
        p[i].mgdl = (int16_t)(first - i * delta);
        p[i].trend = TREND_FLAT;
    }
}

int main(void)
{
    glucose_point_t p[16];
    const int64_t now = 1760000000LL * 1000;

    section("Age de la derniere mesure");
    serie(p, 5, now - 2 * MIN_MS, 300, 120, 2);
    CHECK_EQ_I(gm_age_s(p, 5, now, true), 120, "mesure vieille de 2 min -> 120 s");
    CHECK_EQ_I(gm_age_s(p, 0, now, true), -1, "aucune mesure -> age inconnu (-1)");
    CHECK_EQ_I(gm_age_s(p, 5, now, false), -1, "heure non synchronisee -> age inconnu (-1)");
    serie(p, 2, now + 30 * 1000, 300, 120, 0);
    CHECK_EQ_I(gm_age_s(p, 2, now, true), 0, "mesure dans le futur -> age ramene a 0");

    section("Fraicheur (A JOUR < 6 min, RETARD 6-10 min, DONNEES ANCIENNES >= 10 min)");
    CHECK_EQ_I(gm_freshness(-1), GM_FRESH_NONE, "age inconnu -> aucune donnee");
    CHECK_EQ_I(gm_freshness(0), GM_FRESH_OK, "0 s -> A JOUR");
    CHECK_EQ_I(gm_freshness(359), GM_FRESH_OK, "5 min 59 s -> A JOUR");
    CHECK_EQ_I(gm_freshness(360), GM_FRESH_LATE, "6 min 00 s -> RETARD");
    CHECK_EQ_I(gm_freshness(599), GM_FRESH_LATE, "9 min 59 s -> RETARD");
    CHECK_EQ_I(gm_freshness(600), GM_FRESH_STALE, "10 min 00 s -> DONNEES ANCIENNES");
    CHECK_EQ_I(gm_freshness(86400), GM_FRESH_STALE, "24 h -> DONNEES ANCIENNES");

    section("Cadence reelle");
    CHECK_EQ_I(gm_cadence_s(p, 0), GLY_CADENCE_DEFAULT_S, "aucune mesure -> cadence par defaut 300 s");
    CHECK_EQ_I(gm_cadence_s(p, 1), GLY_CADENCE_DEFAULT_S, "une seule mesure -> cadence par defaut");
    serie(p, 6, now, 300, 100, 0);
    CHECK_EQ_I(gm_cadence_s(p, 6), 300, "mesures toutes les 5 min -> 300 s");
    serie(p, 6, now, 310, 100, 0);
    CHECK_EQ_I(gm_cadence_s(p, 6), 310, "mesures toutes les 5 min 10 s -> 310 s");
    /* Intervalles hors plage 4-6 min : ignores, retour a la valeur par defaut */
    p[0].wt_ms = now; p[1].wt_ms = now - 3600LL * 1000; p[2].wt_ms = now - 7200LL * 1000;
    CHECK_EQ_I(gm_cadence_s(p, 3), GLY_CADENCE_DEFAULT_S, "trous d'une heure -> cadence par defaut");

    section("Variation entre deux mesures (fenetre 2,5 a 7,5 min)");
    int d = 0;
    serie(p, 2, now, 300, 120, 7);   /* 120 puis 113 */
    CHECK(gm_delta_between(&p[0], &p[1], &d), "mesures a 5 min : variation calculable");
    CHECK_EQ_I(d, 7, "120 apres 113 -> +7 mg/dL");
    serie(p, 2, now, 300, 100, -5);  /* 100 puis 105 */
    (void)gm_delta_between(&p[0], &p[1], &d);
    CHECK_EQ_I(d, -5, "100 apres 105 -> -5 mg/dL");
    serie(p, 2, now, 120, 120, 7);
    CHECK(!gm_delta_between(&p[0], &p[1], &d), "mesures a 2 min : trop rapprochees, refusee");
    serie(p, 2, now, 600, 120, 7);
    CHECK(!gm_delta_between(&p[0], &p[1], &d), "mesures a 10 min : trou, refusee");
    serie(p, 2, now, GLY_DELTA_MIN_S, 120, 7);
    CHECK(gm_delta_between(&p[0], &p[1], &d), "2 min 30 exactement : acceptee (borne basse)");
    serie(p, 2, now, GLY_DELTA_MAX_S, 120, 7);
    CHECK(gm_delta_between(&p[0], &p[1], &d), "7 min 30 exactement : acceptee (borne haute)");

    section("Variation affichable");
    serie(p, 4, now - 60 * 1000, 300, 150, 3);
    CHECK(gm_delta_current(p, 4, 60, &d), "donnee fraiche : variation affichee");
    CHECK_EQ_I(d, 3, "+3 mg/dL");
    CHECK(!gm_delta_current(p, 1, 60, &d), "une seule mesure : pas de variation");
    CHECK(!gm_delta_current(p, 4, 600, &d), "donnees anciennes (10 min) : variation masquee");
    CHECK(!gm_delta_current(p, 4, -1, &d), "age inconnu : variation masquee");

    section("Prochaine mesure attendue");
    serie(p, 6, now, 300, 100, 0);
    CHECK_EQ_I(gm_next_expected_ms(p, 6), now + 300LL * 1000, "derniere mesure + cadence");
    CHECK_EQ_I(gm_next_expected_ms(p, 0), 0, "aucune mesure -> inconnue");

    section("Noms de tendance Dexcom");
    CHECK(strcmp(trend_name(TREND_FLAT), "Flat") == 0, "TREND_FLAT -> \"Flat\"");
    CHECK(strcmp(trend_name(TREND_DOUBLE_UP), "DoubleUp") == 0, "TREND_DOUBLE_UP -> \"DoubleUp\"");
    CHECK(strcmp(trend_name(TREND_FORTY_FIVE_DOWN), "FortyFiveDown") == 0, "TREND_FORTY_FIVE_DOWN -> \"FortyFiveDown\"");
    CHECK(strcmp(trend_name(TREND_NOT_COMPUTABLE), "NotComputable") == 0, "TREND_NOT_COMPUTABLE -> \"NotComputable\"");
    CHECK(trend_name(250) != NULL, "code inconnu : pas de plantage");

    section("Historique partage (288 mesures maximum)");
    app_state_init();
    glucose_point_t big[GLY_HISTORY_MAX + 10];
    for (int i = 0; i < GLY_HISTORY_MAX + 10; i++) {
        big[i].wt_ms = now - (int64_t)i * 300 * 1000;
        big[i].mgdl = (int16_t)(100 + (i % 50));
        big[i].trend = TREND_FLAT;
    }
    app_history_set(big, GLY_HISTORY_MAX + 10);
    glucose_point_t out[GLY_HISTORY_MAX + 10];
    uint32_t seq = 0;
    int n = app_history_get(out, GLY_HISTORY_MAX + 10, &seq);
    CHECK_EQ_I(n, GLY_HISTORY_MAX, "historique borne a 288 mesures (24 h)");
    CHECK_EQ_I(out[0].wt_ms, now, "la mesure la plus recente reste en tete");
    uint32_t seq1 = app_history_seq();
    app_history_set(big, 10);
    CHECK(app_history_seq() != seq1, "le numero de sequence change a chaque mise a jour");

    return test_report("test_glucose_math");
}
