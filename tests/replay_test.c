/* Replay recorded on-device coordinates/timing, including rejected slow taps. */
#include <assert.h>
#include <stdio.h>
#include "../src/double_tap.h"

int main(void) {
    FILE *file = fopen("tests/fixtures/prototype-0.3.log", "r");
    assert(file);
    char line[256];
    struct dtp_recognizer recognizer = {0};
    unsigned count = 0, detections = 0;
    while (fgets(line, sizeof(line), file)) {
        unsigned event, doubled, held, pressed, expected_pair;
        long long ms;
        int state, x, y;
        int fields = sscanf(line,
            "event=%u t_ms=%lld state=%d double=%u held=%u pressed=%u pair=%u x=%d y=%d",
            &event, &ms, &state, &doubled, &held, &pressed, &expected_pair, &x, &y);
        if (fields != 9)
            continue;
        assert(event == ++count);
        struct dtp_sample sample = {
            .context = 1, .ms = ms, .state = state, .held = held != 0,
            .native_double = doubled != 0, .point = {x, y},
        };
        enum dtp_result result = dtp_observe(&recognizer, &sample);
        assert((unsigned)(result == DTP_PAIR) == expected_pair);
        if (result == DTP_PAIR)
            ++detections;
    }
    fclose(file);
    assert(count == 51 && detections == 7);
    puts("PASS: real device replay, 51 callbacks, seven close pairs, spaced taps/long press/distant pair rejected");
    return 0;
}
