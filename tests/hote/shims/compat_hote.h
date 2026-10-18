/*
 * Compatibilite du compilateur hote pour les tests sur PC.
 * Inclus de force par tests/run_tests.py (option /FI avec MSVC, -include avec gcc/clang).
 */
#pragma once

#ifdef _MSC_VER
#include <time.h>
/* MSVC fournit localtime_s / gmtime_s, avec des arguments inverses par rapport a POSIX.
 * Les parametres ne doivent pas s'appeler "tm" : le jeton apparait dans "struct tm *". */
#define localtime_r(_t, _out) (localtime_s((_out), (_t)) == 0 ? (_out) : (struct tm *)0)
#define gmtime_r(_t, _out)    (gmtime_s((_out), (_t)) == 0 ? (_out) : (struct tm *)0)
#endif

/* strlcpy : extension BSD fournie par ESP-IDF, absente des bibliotheques C du PC. */
#include <stddef.h>
size_t strlcpy(char *dst, const char *src, size_t size);
