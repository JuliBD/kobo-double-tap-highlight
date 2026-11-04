#include "double_tap.h"
#include <string.h>

void dtp_reset(struct dtp_recognizer *r) { memset(r, 0, sizeof(*r)); }

static bool nearby(struct dtp_point a, struct dtp_point b, int radius) {
    int64_t dx = (int64_t)a.x - b.x, dy = (int64_t)a.y - b.y;
    /* Bound differences before squaring; extreme coordinates cannot overflow. */
    if (dx < -radius || dx > radius || dy < -radius || dy > radius)
        return false;
    return dx * dx + dy * dy <= (int64_t)radius * radius;
}

enum dtp_result dtp_observe(struct dtp_recognizer *r, const struct dtp_sample *s) {
    if (!s->context || s->ms < 0 || s->state == 4 || s->held) {
        dtp_reset(r);
        return DTP_NONE;
    }
    if (s->context != r->context || s->ms < r->last_ms)
        dtp_reset(r);
    r->context = s->context;
    r->last_ms = s->ms;
    if (s->native_double) {
        dtp_reset(r);
        return s->state == 3 ? DTP_NATIVE_DOUBLE : DTP_NONE;
    }
    if (s->state == 1) {
        /* A new start without a finish cancels the incomplete gesture. */
        if (r->active)
            r->pending = false;
        if (r->pending && s->ms - r->first_down_ms > DTP_PAIR_WINDOW_MS)
            r->pending = false;
        r->active = true;
        r->down_ms = s->ms;
        r->down = s->point;
        return DTP_NONE;
    }
    if (!r->active)
        return DTP_NONE;
    if (s->ms - r->down_ms > DTP_PRESS_LIMIT_MS ||
        !nearby(s->point, r->down, DTP_MOVEMENT_RADIUS_PX)) {
        dtp_reset(r);
        return DTP_NONE;
    }
    if (s->state != 3)
        return DTP_NONE;
    r->active = false;
    if (r->pending && r->down_ms - r->first_down_ms <= DTP_PAIR_WINDOW_MS &&
        nearby(r->first, r->down, DTP_PAIR_RADIUS_PX)) {
        dtp_reset(r);
        return DTP_PAIR;
    }
    r->pending = true;
    r->first_down_ms = r->down_ms;
    r->first = r->down;
    return DTP_NONE;
}
