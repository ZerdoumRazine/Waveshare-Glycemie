/* Mini cadre d'assertions : aucune dependance, code de sortie 1 si un controle echoue. */
#pragma once
#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failed;
static const char *g_section = "";

static void section(const char *name) { g_section = name; printf("\n-- %s\n", name); }

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        g_checks++;                                                           \
        if (cond) {                                                           \
            printf("   OK    "); printf(__VA_ARGS__); printf("\n");           \
        } else {                                                              \
            g_failed++;                                                       \
            printf("   ECHEC "); printf(__VA_ARGS__);                         \
            printf("   (%s ligne %d)\n", g_section, __LINE__);                \
        }                                                                     \
    } while (0)

#define CHECK_EQ_I(got, want, ...)                                            \
    do {                                                                      \
        long long _g = (long long)(got), _w = (long long)(want);              \
        g_checks++;                                                           \
        if (_g == _w) {                                                       \
            printf("   OK    "); printf(__VA_ARGS__); printf("\n");           \
        } else {                                                              \
            g_failed++;                                                       \
            printf("   ECHEC "); printf(__VA_ARGS__);                         \
            printf("   attendu %lld, obtenu %lld (%s ligne %d)\n",            \
                   _w, _g, g_section, __LINE__);                              \
        }                                                                     \
    } while (0)

static int test_report(const char *suite)
{
    printf("\n%s : %d controles, %d echec(s)\n", suite, g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
