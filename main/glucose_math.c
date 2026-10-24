/*
 * glucose_math.c: Pure glucose statistics and conversion helpers.
 * Design note: Side-effect-free calculations make boundary cases easier to test.
 */

/*
 * Provides pure, host-testable calculations for reading age, freshness, cadence and delta.
 * Collects testable calculations of freshness, rate and variation.
 */
#include "glucose_math.h"

#include <stdlib.h>
#include <string.h>

/* Return an unknown age when time is not synchronized or no measurement is available. */
int64_t gm_age_s(const glucose_point_t *pts, int count, int64_t now_ms, bool time_ok)
{
    if (count <= 0 || !time_ok) {
        return -1;
    }
    int64_t age_ms = now_ms - pts[0].wt_ms;
    return age_ms > 0 ? age_ms / 1000 : 0;
}

/* Distinguish current, late and stale readings without altering their glucose values. */
gm_freshness_t gm_freshness(int64_t age_s)
{
    if (age_s < 0) {
        return GM_FRESH_NONE;
    }
    if (age_s >= (int64_t)GLY_AGE_STALE_MIN * 60) {
        return GM_FRESH_STALE;
    }
    if (age_s >= (int64_t)GLY_AGE_FRESH_MAX_MIN * 60) {
        return GM_FRESH_LATE;
    }
    return GM_FRESH_OK;
}

static int cmp_int32(const void *a, const void *b)
{
    int32_t x = *(const int32_t *)a;
    int32_t y = *(const int32_t *)b;
    return (x > y) - (x < y);
}

/* Estimate the observed sampling interval from reasonable timestamp gaps. */
int32_t gm_cadence_s(const glucose_point_t *pts, int count)
{
    int32_t intervals[16];
    int n = 0;
    for (int i = 0; i + 1 < count && i < 16; i++) {
        int64_t dt = (pts[i].wt_ms - pts[i + 1].wt_ms) / 1000;
        /* only the intervals of one measurement to the next (4 to 6 min) are representative */
        if (dt >= 240 && dt <= 360) {
            intervals[n++] = (int32_t)dt;
        }
    }
    if (n == 0) {
        return GLY_CADENCE_DEFAULT_S;
    }
    qsort(intervals, (size_t)n, sizeof(int32_t), cmp_int32);
    return intervals[n / 2];
}

/* Only compare readings separated by a meaningful elapsed interval. */
bool gm_delta_between(const glucose_point_t *newer, const glucose_point_t *older, int *delta)
{
    int64_t dt = (newer->wt_ms - older->wt_ms) / 1000;
    if (dt < GLY_DELTA_MIN_S || dt > GLY_DELTA_MAX_S) {
        return false;
    }
    *delta = newer->mgdl - older->mgdl;
    return true;
}

/* Never show a reassuring delta calculated from stale values. */
bool gm_delta_current(const glucose_point_t *pts, int count, int64_t age_s, int *delta)
{
    if (count < 2 || age_s < 0 || age_s >= (int64_t)GLY_AGE_STALE_MIN * 60) {
        return false;
    }
    return gm_delta_between(&pts[0], &pts[1], delta);
}

int64_t gm_next_expected_ms(const glucose_point_t *pts, int count)
{
    if (count <= 0) {
        return 0;
    }
    return pts[0].wt_ms + (int64_t)gm_cadence_s(pts, count) * 1000;
}
