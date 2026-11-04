/* UI adapter tests use a tokenizer double; the real ARM engine is tested separately. */
#include "../src/sudachi.h"
int dtp_test_sudachi_failure;
extern "C" int dtp_sudachi_select(const unsigned char *, size_t, unsigned, unsigned, unsigned *bounds) {
    if (dtp_test_sudachi_failure) return 2;
    bounds[0]=3; bounds[1]=5; return 0;
}
