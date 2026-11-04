#ifndef DTP_RENDERING_H
#define DTP_RENDERING_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern void *dtpr_add, *dtpr_partial, *dtpr_part_id, *dtpr_a2, *dtpr_close_footer;
extern void *dtpr_repaint;
extern void *dtpr_force_full;
void dtp_render_force_full(void *reader);
bool dtp_render_deliver_partial(void *reader, void *book, void *event);
void dtp_render_add(void *element, const void *bookmark, int start, int end, int part, bool reverse, const void *style);
void dtp_render_a2(void *controller, bool active, bool force);
void dtp_render_close_footer(void *controller);
void dtp_render_repaint(void *controller);
#ifdef __cplusplus
}
#endif
#endif
