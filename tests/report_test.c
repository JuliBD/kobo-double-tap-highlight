#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/report.h"

int main(void) {
    const char *path = "build/report-test.log";
    remove(path);
    dtp_append_report(path, "double");
    FILE *file = fopen(path, "rb");
    assert(file);
    char line[32];
    assert(fgets(line, sizeof(line), file));
    assert(!strcmp(line, "double\n"));
    fclose(file);
    for (int i = 0; i < 2000; ++i)
        dtp_append_report(path, "double");
    file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    assert(ftell(file) <= 8192 && ftell(file) > 8180);
    fclose(file);
    dtp_append_report("build/no-such-directory/report.log", "double");
    remove(path);
    const char *recent = "build/report-recent-test.log";
    const char *previous = "build/report-recent-test.log.previous";
    remove(recent); remove(previous);
    for (int i = 0; i < 4000; ++i) {
        char entry[96];
        snprintf(entry, sizeof(entry), "op=%d timestamp=2026-11-01T16:00:00Z diagnostic", i);
        dtp_append_recent_report(recent, entry);
    }
    const char *logs[] = {recent, previous};
    for (unsigned i = 0; i < 2; ++i) {
        file = fopen(logs[i], "rb");
        assert(file && !fseek(file, 0, SEEK_END));
        assert(ftell(file) > 0 && ftell(file) <= 8192);
        fclose(file);
    }
    file = fopen(recent, "rb");
    char contents[8193] = {0};
    assert(file); fread(contents, 1, 8192, file); fclose(file);
    assert(strstr(contents, "op=3999 ") && !strstr(contents, "op=0 "));
    char oversized[258]; memset(oversized, 'x', 257); oversized[257] = 0;
    dtp_append_recent_report(recent, oversized);
    dtp_append_recent_report("build/no-such-directory/recent.log", "entry");
    remove(recent); remove(previous);
    puts("PASS: report contents, size bound, unavailable destination");
    puts("PASS: recent-report rotation stays bounded and retains newest operations after repeated rotations");
    return 0;
}
