#ifndef DTP_JAPANESE_H
#define DTP_JAPANESE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern void *dtpj_evaluate, *dtpj_selected_text;
extern void *dtpj_web_view, *dtpj_page, *dtpj_frame, *dtpj_remove_visual;
/* expected_bytes points to a live QByteArray; no text is logged or persisted. */
bool dtp_japanese_expand(void *book, void *expected_bytes);
bool dtp_remove_failed_visual(void *book, const void *id_qstring);
#ifdef __cplusplus
}
#endif
#endif
