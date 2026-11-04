#define _POSIX_C_SOURCE 200809L
#define DTP_UNINSTALL_MENU "nm/doubletap"
#define DTP_UNINSTALL_SCRIPT "doubletap/toggle-japanese-longpress.sh"
#include "../src/uninstall.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void create(const char *name) {
    FILE *file = fopen(name, "w");
    assert(file);
    assert(fputs("preserved fixture", file) >= 0);
    assert(fclose(file) == 0);
}

int main(void) {
    char directory[] = "/tmp/dtp-uninstall-XXXXXX";
    assert(mkdtemp(directory));
    assert(chdir(directory) == 0);
    assert(mkdir("nm",0700) == 0);
    assert(mkdir("doubletap",0700) == 0);
    create("nm/config");
    create("doubletap.conf");
    create("annotations-fixture");
    create(DTP_UNINSTALL_MENU);
    create(DTP_UNINSTALL_SCRIPT);
    assert(dtp_uninstall_cleanup());
    assert(access(DTP_UNINSTALL_MENU,F_OK) != 0);
    assert(access(DTP_UNINSTALL_SCRIPT,F_OK) != 0);
    assert(access("nm/config",F_OK) == 0);
    assert(access("doubletap.conf",F_OK) == 0);
    assert(access("annotations-fixture",F_OK) == 0);
    assert(dtp_uninstall_cleanup()); /* Absent optional integration is OK. */
    assert(mkdir(DTP_UNINSTALL_MENU,0700) == 0);
    create(DTP_UNINSTALL_SCRIPT);
    assert(!dtp_uninstall_cleanup()); /* Report failure, never recursively delete. */
    assert(access(DTP_UNINSTALL_MENU,F_OK) == 0);
    assert(access(DTP_UNINSTALL_SCRIPT,F_OK) != 0);
    assert(rmdir(DTP_UNINSTALL_MENU) == 0);
    assert(unlink("nm/config") == 0);
    assert(unlink("doubletap.conf") == 0);
    assert(unlink("annotations-fixture") == 0);
    assert(rmdir("nm") == 0);
    assert(rmdir("doubletap") == 0);
    assert(chdir("/") == 0);
    assert(rmdir(directory) == 0);
    puts("PASS: uninstall removes only plugin NM files; settings/annotations/other entries survive; idempotence and failure reporting");
}
