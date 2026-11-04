#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../src/double_tap.h"

static struct dtp_recognizer r;
static enum dtp_result feed(long long ms, int state, int x, int y) {
    struct dtp_sample s = {.context = 1, .ms = ms, .state = state, .point = {x, y}};
    return dtp_observe(&r, &s);
}
static enum dtp_result tap(long long ms, int x, int y) {
    assert(feed(ms, 1, x, y) == DTP_NONE);
    return feed(ms + 30, 3, x, y);
}
int main(void) {
    /* Timing observed in the real 0.2 report: first pair too slow,
     * next two pairs qualify if their coordinates are nearby. */
    assert(feed(110268, 1, 100, 100) == DTP_NONE);
    assert(feed(110412, 3, 100, 100) == DTP_NONE);
    assert(feed(110818, 1, 100, 100) == DTP_NONE);
    assert(feed(110819, 3, 100, 100) == DTP_NONE);
    assert(feed(112067, 1, 100, 100) == DTP_NONE);
    assert(feed(112235, 3, 100, 100) == DTP_NONE);
    assert(feed(112305, 1, 100, 100) == DTP_NONE);
    assert(feed(112307, 3, 100, 100) == DTP_PAIR);
    assert(feed(113996, 1, 100, 100) == DTP_NONE);
    assert(feed(114152, 3, 100, 100) == DTP_NONE);
    assert(feed(114218, 1, 100, 100) == DTP_NONE);
    assert(feed(114219, 3, 100, 100) == DTP_PAIR);

    dtp_reset(&r);
    assert(tap(1000, 0, 0) == DTP_NONE);
    assert(tap(1400, 48, 0) == DTP_PAIR); /* inclusive limits */
    assert(feed(1431, 3, 48, 0) == DTP_NONE); /* no duplicate finish */
    assert(tap(1500, 48, 0) == DTP_NONE); /* triple tap doesn't overlap */
    assert(tap(1700, 48, 0) == DTP_PAIR);
    dtp_reset(&r);
    assert(tap(1000, 0, 0) == DTP_NONE);
    assert(tap(1401, 0, 0) == DTP_NONE);
    dtp_reset(&r);
    assert(tap(1000, 0, 0) == DTP_NONE);
    assert(tap(1200, 49, 0) == DTP_NONE);
    dtp_reset(&r);
    assert(tap(1000, 0, 0) == DTP_NONE);
    assert(tap(1200, 48, 48) == DTP_NONE); /* radial, not square */
    dtp_reset(&r);
    tap(1000, INT_MIN, INT_MAX);
    assert(tap(1200, INT_MAX, INT_MIN) == DTP_NONE); /* no overflow */

    dtp_reset(&r);
    tap(1000, 100, 100);
    feed(1200, 1, 100, 100);
    feed(1220, 2, 200, 100); /* swipe/movement cancels */
    assert(feed(1250, 3, 100, 100) == DTP_NONE);
    assert(tap(1300, 100, 100) == DTP_NONE);
    dtp_reset(&r);
    feed(1000, 1, 100, 100);
    assert(feed(1400, 3, 100, 100) == DTP_NONE); /* held too long */
    assert(tap(1450, 100, 100) == DTP_NONE);
    feed(1500, 4, 100, 100);
    assert(tap(1550, 100, 100) == DTP_NONE); /* cancelled history */
    assert(feed(-1, 1, 100, 100) == DTP_NONE);
    assert(tap(1600, 100, 100) == DTP_NONE);
    assert(tap(1000, 100, 100) == DTP_NONE); /* clock rollback */

    struct dtp_sample s = {.context = 2, .ms = 1200, .state = 1,
                           .point = {100, 100}};
    assert(dtp_observe(&r, &s) == DTP_NONE);
    s.state = 3; s.ms = 1230;
    assert(dtp_observe(&r, &s) == DTP_NONE); /* new context */
    s.ms = 1250; s.held = true;
    assert(dtp_observe(&r, &s) == DTP_NONE);
    s.held = false; s.native_double = true;
    assert(dtp_observe(&r, &s) == DTP_NATIVE_DOUBLE);
    assert(tap(1300, 100, 100) == DTP_NONE);
    puts("PASS: real timing replay, timing/distance bounds, disjoint pairs, long press, movement, cancellation, clock errors, context changes, native double");
    return 0;
}
