#include "probe_clock.h"
#include <time.h>

long long dtp_clock_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return -1;
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
