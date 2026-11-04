#include "report.h"
#include <stdio.h>
#include <string.h>

void dtp_append_report(const char *path, const char *kind) {
    FILE *file = fopen(path, "ab+");
    if (!file)
        return;
    if (!fseek(file, 0, SEEK_END)) {
        long size = ftell(file);
        size_t len = strlen(kind);
        if (size >= 0 && len <= 128 && size + (long)len + 1 <= 8192) {
            fwrite(kind, 1, len, file);
            fputc('\n', file);
        }
    }
    fclose(file);
}

void dtp_append_recent_report(const char *path, const char *kind) {
    const size_t len = strlen(kind);
    if (len > 256) return;
    FILE *file = fopen(path, "ab+");
    if (!file) return;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return; }
    const long size = ftell(file);
    if (size < 0) { fclose(file); return; }
    if (size + (long)len + 1 > 8192) {
        fclose(file);
        char previous[512];
        const int count = snprintf(previous, sizeof(previous), "%s.previous", path);
        if (count < 0 || (size_t)count >= sizeof(previous)) return;
        remove(previous);
        if (rename(path, previous)) return;
        file = fopen(path, "ab");
        if (!file) return;
    }
    fwrite(kind, 1, len, file);
    fputc('\n', file);
    fclose(file);
}
