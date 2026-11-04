#ifndef DTP_SETTINGS_H
#define DTP_SETTINGS_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool dtp_settings_longpress(void);
extern void *dtpl_is_held;
bool dtp_longpress_start(void *controller, void *tap);
#ifdef __cplusplus
}
#endif
#endif
