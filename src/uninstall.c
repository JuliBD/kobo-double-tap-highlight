#include "uninstall.h"
#include <errno.h>
#include <unistd.h>

#ifndef DTP_UNINSTALL_MENU
#define DTP_UNINSTALL_MENU "/mnt/onboard/.adds/nm/doubletap"
#endif
#ifndef DTP_UNINSTALL_SCRIPT
#define DTP_UNINSTALL_SCRIPT "/mnt/onboard/.adds/doubletap/toggle-japanese-longpress.sh"
#endif

/* Runs from NickelHook before Qt initialization. Remove only our integration
 * files; retain preferences, diagnostics, annotations and other NM entries. */
bool dtp_uninstall_cleanup(void) {
    const char *paths[] = {DTP_UNINSTALL_MENU, DTP_UNINSTALL_SCRIPT};
    bool ok = true;
    for (unsigned i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i)
        if (unlink(paths[i]) && errno != ENOENT) ok = false;
    return ok;
}
