#ifndef DTP_SUDACHI_H
#define DTP_SUDACHI_H
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
int dtp_sudachi_select(const unsigned char *text, size_t length, unsigned start, unsigned end, unsigned *bounds);
const char *dtp_sudachi_backend(void);
extern void *dtps_usb_allowed;
void dtp_sudachi_release(void);
bool dtp_sudachi_usb_allowed(void *manager);
#ifdef __cplusplus
}
#endif
#endif
