#ifndef DTP_ANNOTATIONS_H
#define DTP_ANNOTATIONS_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
extern void *dtpa_setup, *dtpa_create_item, *dtpa_add_item, *dtpa_current_id;
extern void *dtpa_get_id, *dtpa_save, *dtpa_delete;
extern void *dtpa_is_highlight, *dtpa_is_note, *dtpa_annotation_raw, *dtpa_text_raw;
extern void *dtpa_content_id, *dtpa_start_path, *dtpa_end_path;
extern void *dtpa_start_child, *dtpa_end_child, *dtpa_start_offset, *dtpa_end_offset;
extern void *dtpa_count_words, *dtpa_inline_results;
void dtp_dictionary_popup_enable(bool tested);
int dtp_dictionary_count_words(const void *text, const void *language, const void *locale);
void dtp_annotations_enable(bool enable);
void dtp_annotation_setup(void *controller);
void *dtp_annotation_create_item(void *controller, void *parent, const void *label);
const void *dtp_annotation_bookmark_id(const void *bookmark);
bool dtp_annotation_save(void *bookmark, const void *database);
void dtp_annotation_delete(void *bookmark, const void *device);
bool dtp_annotation_has_current(void *controller);
bool dtp_annotation_auto_action(void *controller, const char *expected_utf8, void *book);
bool dtp_annotations_ready(void);
#ifdef DTP_TEST_MANUAL_MENU
void dtp_annotation_test_menu_enable(bool enable);
#endif
#ifdef __cplusplus
}
#endif
#endif
