#ifndef DTP_TOGGLE_H
#define DTP_TOGGLE_H
#include "double_tap.h"
#ifdef __cplusplus
extern "C" {
#endif
#define DTP_TOGGLE_POINTERS(X) \
    X(dtpt_allow) X(dtpt_set_current) X(dtpt_start) X(dtpt_process_selection) \
    X(dtpt_mode) X(dtpt_menu_visible) X(dtpt_clear) X(dtpt_mode_off) \
    X(dtpt_page) X(dtpt_close) X(dtpt_volume) X(dtpt_images) X(dtpt_watched)
#define DTP_DECLARE_POINTER(n) extern void *n;
DTP_TOGGLE_POINTERS(DTP_DECLARE_POINTER)
#undef DTP_DECLARE_POINTER
void dtp_toggle_enable(bool enable);
bool dtp_toggle_process(void *reader, void *tap, const struct dtp_sample *sample);
bool dtp_toggle_busy(void);
bool dtp_toggle_active_controller(const void *controller);
void dtp_toggle_trace_refresh(const void *reader);
void dtp_toggle_allow(void *tap, bool allow);
void dtp_toggle_set_current(void *menu, const void *id);
void dtp_toggle_clear(void *controller);
void dtp_toggle_page(void *reader);
void dtp_toggle_close(void *reader);
void dtp_toggle_volume(void *reader, const void *volume, const void *bookmark);
#ifdef __cplusplus
}
#endif
#endif
