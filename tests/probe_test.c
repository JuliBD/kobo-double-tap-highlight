/* Tests the actual hook entry point with simulated native gesture accessors. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define DTP_DEVICE_VERSION_FILE "build/probe-device-version"
#define DTP_DISABLE_FILE "build/probe-disable"
#include "../src/probe.c"

struct tap { int state; bool doubled, held, pressed; };
static unsigned forwarded, reported;
static void *last_reader, *last_tap;
static char last_kind[128];
static long long clock_ms = 1000;

/* The C hook tests do not invoke the Qt annotation adapter. */
void *dtpa_count_words, *dtpa_inline_results;
void dtp_dictionary_popup_enable(bool tested) { (void)tested; }
void *dtpa_setup, *dtpa_create_item, *dtpa_add_item, *dtpa_current_id;
void *dtpa_get_id, *dtpa_save, *dtpa_delete;
void *dtpa_is_highlight, *dtpa_is_note, *dtpa_annotation_raw, *dtpa_text_raw;
void *dtpa_content_id, *dtpa_start_path, *dtpa_end_path;
void *dtpa_start_child, *dtpa_end_child, *dtpa_start_offset, *dtpa_end_offset;
static bool annotations_enabled, toggle_enabled;
void dtp_annotations_enable(bool enable) { annotations_enabled=enable; }
void dtp_annotation_setup(void *p) { (void)p; }
void *dtp_annotation_create_item(void *p, void *v, const void *t) { (void)p; (void)v; (void)t; return NULL; }
const void *dtp_annotation_bookmark_id(const void *p) { return p; }
bool dtp_annotation_save(void *p, const void *d) { (void)p; (void)d; return false; }
void dtp_annotation_delete(void *p, const void *d) { (void)p; (void)d; }
void *dtpj_evaluate, *dtpj_selected_text;
void *dtpj_web_view, *dtpj_page, *dtpj_frame, *dtpj_remove_visual;
void *dtpr_add, *dtpr_partial, *dtpr_part_id, *dtpr_a2, *dtpr_close_footer;
void *dtpr_repaint;
void *dtpr_force_full;
void *dtps_usb_allowed;
bool dtp_sudachi_usb_allowed(void *p) { (void)p; return true; }
void dtp_render_force_full(void *p) { (void)p; }
void dtp_render_add(void *e, const void *b, int s, int f, int p, bool r, const void *c) { (void)e; (void)b; (void)s; (void)f; (void)p; (void)r; (void)c; }
void dtp_render_a2(void *p, bool a, bool f) { (void)p; (void)a; (void)f; }
void dtp_render_close_footer(void *p) { (void)p; }
void dtp_render_repaint(void *p) { (void)p; }
#define DTP_TEST_POINTER(n) void *n;
DTP_TOGGLE_POINTERS(DTP_TEST_POINTER)
#undef DTP_TEST_POINTER
void *dtpl_is_held;
bool dtp_longpress_start(void *c, void *t) { (void)c; (void)t; return false; }
bool dtp_uninstall_cleanup(void) { return true; }
static bool toggle_consume;
static unsigned toggle_calls;
void dtp_toggle_enable(bool enable) { toggle_enabled=enable; }
bool dtp_toggle_process(void *r, void *t, const struct dtp_sample *s) {
    (void)r; (void)t; (void)s; ++toggle_calls; return toggle_consume;
}
void dtp_toggle_allow(void *t, bool a) { (void)t; (void)a; }
void dtp_toggle_set_current(void *m, const void *i) { (void)m; (void)i; }
void dtp_toggle_clear(void *p) { (void)p; }
void dtp_toggle_page(void *r) { (void)r; }
void dtp_toggle_close(void *r) { (void)r; }
void dtp_toggle_volume(void *r, const void *v, const void *b) { (void)r; (void)v; (void)b; }

long long dtp_clock_ms(void) { return clock_ms; }

void nh_log(const char *fmt, ...) { (void)fmt; }
void dtp_append_report(const char *path, const char *kind) {
    (void)path;
    ++reported;
    snprintf(last_kind, sizeof(last_kind), "%s", kind);
}
static void forward(void *reader, void *tap) {
    ++forwarded;
    last_reader = reader;
    last_tap = tap;
}
static int state(const void *p) { return ((const struct tap *)p)->state; }
static bool doubled(const void *p) { return ((const struct tap *)p)->doubled; }
static bool held(const void *p) { return ((const struct tap *)p)->held; }
static bool pressed(const void *p) { return ((const struct tap *)p)->pressed; }
static void position(struct dtp_point *out, const void *p) {
    (void)p;
    out->x = 100;
    out->y = 100;
}

int main(void) {
    int reader;
    struct tap t = {1, true, false, false};
    original_process_tap = forward;
    gesture_state = state;
    tap_is_double = doubled;
    tap_is_held = held;
    tap_is_pressed = pressed;
    tap_position = position;
    assert(is_tested_configuration("fake,4.1.15,4.38.23852,4.1.15,4.1.15," DTP_MODEL "\r\n"));
    assert(!is_tested_configuration("fake,4.1.15,4.38.23684,4.1.15,4.1.15," DTP_MODEL));
    assert(!is_tested_configuration("fake,4.1.15,4.38.23852,4.1.15,4.1.15,other"));
    assert(!is_tested_configuration("fake,4.1.15,4.38.23852,4.1.15,4.1.15," DTP_MODEL ",extra"));
    assert(!is_tested_configuration(""));

    /* Untested or unreadable metadata must not block initialization. */
    remove(DTP_DEVICE_VERSION_FILE); remove(DTP_DISABLE_FILE);
    const char *configurations[] = {
        "fake,4.1.15,4.38.23852,4.1.15,4.1.15," DTP_MODEL,
        "fake,4.1.15,4.38.23684,4.1.15,4.1.15," DTP_MODEL,
        "fake,4.1.15,4.38.23852,4.1.15,4.1.15,other",
        "unrecognized metadata"
    };
    for (unsigned i=0; i<sizeof(configurations)/sizeof(configurations[0]); ++i) {
        FILE *fixture=fopen(DTP_DEVICE_VERSION_FILE,"w"); assert(fixture);
        assert(fputs(configurations[i],fixture)>=0); assert(fclose(fixture)==0);
        assert(probe_init()==0 && enabled && annotations_enabled && toggle_enabled);
        assert(strstr(last_kind,i==0 ? "tested-configuration=yes" : "tested-configuration=no"));
    }
    remove(DTP_DEVICE_VERSION_FILE);
    assert(probe_init()==0 && enabled && annotations_enabled && toggle_enabled);
    assert(strstr(last_kind,"tested-configuration=no"));
    tap_is_held=NULL;
    assert(probe_init()==0 && !enabled && !annotations_enabled && !toggle_enabled);
    tap_is_held=held;
    FILE *disable=fopen(DTP_DISABLE_FILE,"w"); assert(disable); assert(fclose(disable)==0);
    assert(probe_init()==0 && !enabled && !annotations_enabled && !toggle_enabled);
    assert(remove(DTP_DISABLE_FILE)==0);
    reported=0; enabled=false;

    dtp_process_tap(&reader, &t);
    assert(forwarded == 1 && reported == 0); /* disabled */
    enabled = true;
    for (int s = 0; s <= 4; ++s) {
        t.state = s;
        dtp_process_tap(&reader, &t);
    }
    assert(forwarded == 6 && reported == 5);
    assert(strstr(last_kind, "t_ms=1000 state=4 double=1 held=0 pressed=0"));
    assert(last_reader == &reader && last_tap == &t);
    t.state = 3;
    t.doubled = false;
    dtp_process_tap(&reader, &t);
    assert(strstr(last_kind, "state=3 double=0 held=0 pressed=0"));
    t.doubled = true; t.held = true;
    dtp_process_tap(&reader, &t);
    assert(strstr(last_kind, "double=1 held=1 pressed=0"));
    t.held = false; t.pressed = true;
    dtp_process_tap(&reader, &t);
    assert(strstr(last_kind, "double=1 held=0 pressed=1"));
    clock_ms = -1;
    t.held = true;
    dtp_process_tap(&reader, &t);
    assert(strstr(last_kind, "t_ms=-1 state=3 double=1 held=1 pressed=1"));
    dtp_process_tap(&reader, NULL);
    assert(last_tap == NULL && reported == 9);
    t.pressed = false;
    for (int i = 0; i < 100; ++i)
        dtp_process_tap(&reader, &t);
    assert(reported == 96 && forwarded == 111);
    enabled = false;
    dtp_process_tap(&reader, &t);
    assert(reported == 96 && forwarded == 112);
    enabled = true;
    toggle_consume = true;
    const unsigned before_toggle_calls = toggle_calls;
    dtp_process_tap(&reader, &t);
    assert(toggle_calls == before_toggle_calls + 1 && forwarded == 112 && reported == 96);
    /* Behavior keeps running after the bounded diagnostic budget is exhausted. */
    puts("PASS: untested/missing metadata allowed, required-symbol and disable guards, all gesture states, overlapping flags, clock failure, forwarding, null tap, log budget");
    return 0;
}
