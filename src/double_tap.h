#ifndef DTP_DOUBLE_TAP_H
#define DTP_DOUBLE_TAP_H
#include <stdbool.h>
#include <stdint.h>

enum { DTP_PAIR_WINDOW_MS = 400, DTP_PRESS_LIMIT_MS = 350,
       DTP_PAIR_RADIUS_PX = 48, DTP_MOVEMENT_RADIUS_PX = 24 };

struct dtp_point { int32_t x, y; };
struct dtp_sample {
    uintptr_t context; /* Identity only; never dereferenced. */
    long long ms;
    int state; /* Native Qt: started=1, updated=2, finished=3, cancelled=4. */
    bool held, native_double;
    struct dtp_point point;
};
struct dtp_recognizer {
    uintptr_t context;
    long long last_ms, down_ms, first_down_ms;
    struct dtp_point down, first;
    bool active, pending;
};
enum dtp_result { DTP_NONE, DTP_PAIR, DTP_NATIVE_DOUBLE };

void dtp_reset(struct dtp_recognizer *recognizer);
enum dtp_result dtp_observe(struct dtp_recognizer *recognizer,
                            const struct dtp_sample *sample);
#endif
