/* Tests sensor dates, consumables and stock updates.
 * Host tests use simulated dependencies, never personal readings.
 */
/*
 * Consumables tests on the real firmware code (main/sensor_session.c, main/consumables.c, main/stock.c
 * compiles tels quels) : echeances, rappels 24 h / 2 h, anti-repetition, persistance NVS After starting again,
 * stock never negative and decrement only once by change.
 *
 * La NVS is simulated in memory (tests/hote/shims) : a "restart" consist of recalling the functions
 * initialization without deleting the NVS, exactly as at the start of the map.
 */
/* Verify sensor/catheter dates, memories and stock after simulated restaurt.
 */
#include <stdio.h>
#include <string.h>

#include "consumables.h"
#include "mini_test.h"
#include "nvs.h"
#include "sensor_session.h"
#include "stock.h"

void test_set_time_valid(int ok);

#define H (3600LL)
#define J (24LL * 3600)

static const int64_t POSE = 1789747393LL; /* example: any date of installation */

static void demarrage(void)
{
    sensor_session_init();
    stock_init();
    consumables_init();
}

int main(void)
{
    sensor_status_t s;
    catheter_status_t c;
    bool done = false;

    fake_nvs_reset();
    test_set_time_valid(1);
    demarrage();

    section("Capteur G7 : echeance a pose + 10 jours exactement");
    sensor_session_get(POSE + 3 * J, &s);
    CHECK(!s.known, "aucun capteur enregistre au depart");
    sensor_session_start(POSE, POSE);
    sensor_session_get(POSE + 3 * J, &s);
    CHECK(s.known, "capteur enregistre");
    CHECK_EQ_I(s.start_s, POSE, "heure de pose conservee");
    CHECK_EQ_I(s.due_s, POSE + 10 * J, "echeance = pose + 10 jours (la grace ne prolonge pas)");
    CHECK_EQ_I(s.day, 4, "3 jours ecoules -> jour 4 sur 10");
    CHECK(!s.expired, "avant l'echeance : pas a changer");
    CHECK_EQ_I(s.remaining_s, 7 * J, "7 jours restants");
    CHECK_EQ_I(s.progress_pct, 30, "avancement 30 pour cent");

    sensor_session_get(POSE + 10 * J, &s);
    CHECK(s.expired, "a l'echeance exacte : A CHANGER");
    CHECK_EQ_I(s.day, 10, "jour borne a 10");
    CHECK_EQ_I(s.progress_pct, 100, "avancement borne a 100 pour cent");
    sensor_session_get(POSE + 10 * J - 1, &s);
    CHECK(!s.expired, "une seconde avant l'echeance : pas encore");
    sensor_session_get(POSE + 12 * J, &s);
    CHECK(s.remaining_s < 0, "apres l'echeance : temps restant negatif");

    section("Capteur G7 : rappels 24 h et 2 h avant l'echeance");
    CHECK_EQ_I(sensor_session_current_reminder(POSE + 1 * J, &done), -1, "jour 2 : aucun rappel");
    CHECK_EQ_I(sensor_session_current_reminder(POSE + 10 * J - 25 * H, &done), -1, "25 h avant : aucun rappel");
    CHECK_EQ_I(sensor_session_current_reminder(POSE + 10 * J - 24 * H, &done), SENSOR_REMIND_24H,
               "24 h avant : rappel 24 h");
    CHECK(!done, "rappel 24 h pas encore signale");
    CHECK_EQ_I(sensor_session_current_reminder(POSE + 10 * J - 3 * H, &done), SENSOR_REMIND_24H,
               "3 h avant : toujours le rappel 24 h");
    CHECK_EQ_I(sensor_session_current_reminder(POSE + 10 * J - 2 * H, &done), SENSOR_REMIND_2H,
               "2 h avant : rappel 2 h");
    CHECK_EQ_I(sensor_session_current_reminder(POSE + 10 * J + 1, &done), -1,
               "apres l'echeance : plus de rappel (l'etat A CHANGER prend le relais)");

    section("Capteur G7 : anti-repetition");
    sensor_session_current_reminder(POSE + 10 * J - 24 * H, &done);
    CHECK(!done, "avant signalement : a presenter");
    sensor_session_mark_reminder_done(SENSOR_REMIND_24H);
    sensor_session_current_reminder(POSE + 10 * J - 24 * H, &done);
    CHECK(done, "apres signalement : ne sera plus rejoue");
    sensor_session_current_reminder(POSE + 10 * J - 2 * H, &done);
    CHECK(!done, "le rappel 2 h reste a presenter");
    sensor_session_mark_reminder_done(SENSOR_REMIND_2H);
    sensor_session_current_reminder(POSE + 10 * J - 2 * H, &done);
    CHECK(done, "rappel 2 h signale a son tour");

    section("Capteur G7 : un redemarrage ne perd ni la date ni les rappels deja signales");
    const uint8_t masque_avant = sensor_session_reminder_mask();
    demarrage(); /* rereading of the NVS, as after a card reboot */
    sensor_session_get(POSE + 3 * J, &s);
    CHECK(s.known, "capteur toujours connu apres redemarrage");
    CHECK_EQ_I(s.start_s, POSE, "heure de pose identique a la seconde pres");
    CHECK_EQ_I(sensor_session_reminder_mask(), masque_avant, "rappels deja signales conserves");
    sensor_session_current_reminder(POSE + 10 * J - 24 * H, &done);
    CHECK(done, "aucun rappel rejoue apres redemarrage");

    section("Capteur G7 : pose enregistree tardivement, rappels deja passes");
    sensor_session_start(POSE, POSE + 10 * J - 3 * H); /* records 3 hours before maturity */
    sensor_session_current_reminder(POSE + 10 * J - 3 * H, &done);
    CHECK(done, "le rappel 24 h deja depasse est marque fait : pas de rafale de rappels perimes");
    sensor_session_current_reminder(POSE + 10 * J - 2 * H, &done);
    CHECK(!done, "le rappel 2 h a venir reste a presenter");

    section("Catheter : intervalle 72 h par defaut, rappel unique 2 h avant");
    CHECK_EQ_I(catheter_interval_h(), 72, "intervalle par defaut 72 h");
    consumable_restore(CONSUMABLE_CATHETER, POSE, 0);
    catheter_get(POSE + 24 * H, &c);
    CHECK(c.known, "changement enregistre");
    CHECK_EQ_I(c.due_s, POSE + 72 * H, "echeance = changement + 72 h");
    CHECK_EQ_I(c.day, 2, "24 h ecoulees -> jour 2 sur 3");
    CHECK(!c.overdue, "pas encore depasse");
    CHECK_EQ_I(catheter_current_reminder(POSE + 24 * H, &done), -1, "24 h apres : aucun rappel");
    CHECK_EQ_I(catheter_current_reminder(POSE + 70 * H, &done), CATHETER_REMIND_2H, "2 h avant : rappel");
    CHECK(!done, "rappel pas encore signale");
    catheter_mark_reminder_done(CATHETER_REMIND_2H);
    catheter_current_reminder(POSE + 70 * H, &done);
    CHECK(done, "rappel signale une seule fois");
    catheter_get(POSE + 72 * H, &c);
    CHECK(c.overdue, "a l'echeance : A CHANGER");
    CHECK_EQ_I(catheter_current_reminder(POSE + 73 * H, &done), -1, "apres l'echeance : plus de rappel");

    section("Catheter : intervalle 48 h");
    catheter_set_interval_h(48);
    CHECK_EQ_I(catheter_interval_h(), 48, "intervalle passe a 48 h");
    catheter_get(POSE + 10 * H, &c);
    CHECK_EQ_I(c.due_s, POSE + 48 * H, "echeance recalculee a changement + 48 h");
    CHECK_EQ_I(c.days_total, 2, "2 jours au total");
    catheter_set_interval_h(36);
    CHECK_EQ_I(catheter_interval_h(), 48, "une valeur non prevue (36 h) est refusee");
    catheter_set_interval_h(72);

    section("Catheter : persistance apres redemarrage");
    demarrage();
    catheter_get(POSE + 24 * H, &c);
    CHECK(c.known, "changement toujours connu");
    CHECK_EQ_I(c.start_s, POSE, "date de changement conservee");
    catheter_current_reminder(POSE + 70 * H, &done);
    CHECK(done, "rappel deja signale non rejoue");

    section("Stock : jamais negatif, une seule decrementation par changement");
    stock_set(STOCK_SENSOR, 2);
    CHECK_EQ_I(stock_get(STOCK_SENSOR), 2, "stock initial 2");
    CHECK(stock_consume(STOCK_SENSOR, POSE), "premier changement : une unite consommee");
    CHECK_EQ_I(stock_get(STOCK_SENSOR), 1, "stock a 1");
    CHECK(!stock_consume(STOCK_SENSOR, POSE), "meme changement rejoue : aucune consommation");
    CHECK_EQ_I(stock_get(STOCK_SENSOR), 1, "stock inchange");
    CHECK(stock_consume(STOCK_SENSOR, POSE + 10 * J), "changement suivant : une unite consommee");
    CHECK_EQ_I(stock_get(STOCK_SENSOR), 0, "stock a 0");
    stock_consume(STOCK_SENSOR, POSE + 20 * J);
    CHECK_EQ_I(stock_get(STOCK_SENSOR), 0, "stock jamais negatif");
    stock_set(STOCK_CATHETER, STOCK_UNKNOWN);
    stock_consume(STOCK_CATHETER, POSE);
    CHECK_EQ_I(stock_get(STOCK_CATHETER), STOCK_UNKNOWN, "stock non renseigne : reste non renseigne (jamais 0)");
    stock_set(STOCK_CATHETER, 150);
    CHECK_EQ_I(stock_get(STOCK_CATHETER), STOCK_MAX, "valeur bornee a 99");

    section("Stock : persistance apres redemarrage");
    stock_set(STOCK_SENSOR, 5);
    demarrage();
    CHECK_EQ_I(stock_get(STOCK_SENSOR), 5, "quantite conservee");
    CHECK(!stock_consume(STOCK_SENSOR, POSE + 20 * J), "le dernier changement decompte reste memorise");

    section("Nouveau consommable refuse si l'heure n'est pas synchronisee");
    test_set_time_valid(0);
    CHECK_EQ_I(consumable_start_now(CONSUMABLE_SENSOR, "test"), ESP_ERR_INVALID_STATE,
               "sans heure fiable : enregistrement refuse");
    sensor_session_get(POSE + 3 * J, &s);
    CHECK_EQ_I(s.start_s, POSE, "la date de pose precedente n'est pas ecrasee");
    test_set_time_valid(1);

    section("Restauration console : date exacte, aucune ligne d'historique ajoutee");
    consumable_restore(CONSUMABLE_SENSOR, POSE, 0x03);
    sensor_session_get(POSE, &s);
    CHECK_EQ_I(s.start_s, POSE, "date restauree a la seconde");
    CHECK_EQ_I(sensor_session_reminder_mask(), 0x03, "masque de rappels restaure");
    consumable_restore(CONSUMABLE_SENSOR, 0, 0);
    sensor_session_get(POSE, &s);
    CHECK(!s.known, "depart 0 : plus aucun capteur enregistre");

    return test_report("test_consommables");
}
