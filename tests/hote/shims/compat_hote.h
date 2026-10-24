/* Adapts firmware code for a desktop C compiler.
 * Test and configuration interface only; no live patient data is included.
 */
/*
 * Compatibility of the hot compiler for PC tests.
 * Inclus de force par tests/run_tests.py (option /FI with MSVC-included with gcc/clang).
 */
/* Portable replacements for ESP-IDF platform conveniences during tests.
 */
#pragma once

#ifdef _MSC_VER
#include <time.h>
/* MSVC fournit localtime_s / gmtime_s, with inverse arguments in relation to POSIX.
 * Parameters should not be called "tm": the token appears in "struct tm *". */
#define localtime_r(_t, _out) (localtime_s((_out), (_t)) == 0 ? (_out) : (struct tm *)0)
#define gmtime_r(_t, _out)    (gmtime_s((_out), (_t)) == 0 ? (_out) : (struct tm *)0)
#endif

/* strlcpy : extension BSD fournie par ESP-IDF, absent from PC C librarians. */
#include <stddef.h>
size_t strlcpy(char *dst, const char *src, size_t size);
