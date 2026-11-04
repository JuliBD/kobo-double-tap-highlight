/* Reader observer and native KEPUB double-tap toggle for the pinned firmware. */
#include <NickelHook.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "report.h"
#include "probe_clock.h"
#include "double_tap.h"
#include "annotations.h"
#include "toggle.h"
#include "settings.h"
#include "uninstall.h"
#include "japanese.h"
#include "rendering.h"
#include "japanese_backend.h"
#if DTP_USE_SUDACHI
#include "sudachi.h"
#endif

#define DTP_VERSION "4.38.23852"
#define DTP_MODEL "00000000-0000-0000-0000-000000000388"
#define DTP_REPORT "/mnt/onboard/.kobo/doubletap-prototype.log"
#define DTP_EVENT_LIMIT 96

static void (*original_process_tap)(void *, void *);
static bool (*tap_is_double)(const void *);
static bool (*tap_is_held)(const void *);
static bool (*tap_is_pressed)(const void *);
static int (*gesture_state)(const void *);
/* The inspected ARM accessor uses r0=8-byte output, r1=this (QPoint sret). */
static void (*tap_position)(struct dtp_point *, const void *);
static bool enabled;
static unsigned records;
static struct dtp_recognizer recognizer;

/* A serial number is present in field zero; never log the input line. */
static bool supported_device(const char *line) {
    const char *field = line;
    unsigned index = 0;
    bool firmware = false, model = false;
    while (*field) {
        size_t len = strcspn(field, ",\r\n");
        if (index == 2)
            firmware = len == strlen(DTP_VERSION) && !strncmp(field, DTP_VERSION, len);
        if (index == 5)
            model = len == strlen(DTP_MODEL) && !strncmp(field, DTP_MODEL, len);
        if (field[len] != ',')
            break;
        field += len + 1;
        ++index;
    }
    return index == 5 && firmware && model;
}

/* Exported for NickelHook's dlsym. Opaque pointers; no private object layouts. */
__attribute__((visibility("default")))
void dtp_process_tap(void *reader, void *tap) {
    if (enabled && tap) {
        char line[128];
        const int state = gesture_state(tap);
        const bool held = tap_is_held(tap);
        const bool pressed = tap_is_pressed(tap);
        const bool doubled = tap_is_double(tap);
        struct dtp_sample sample = {
            .context = (uintptr_t)reader, .ms = dtp_clock_ms(), .state = state,
            .held = held, .native_double = doubled,
        };
        tap_position(&sample.point, tap);
        enum dtp_result result = dtp_observe(&recognizer, &sample);
        const bool consumed = dtp_toggle_process(reader, tap, &sample);
        if (records < DTP_EVENT_LIMIT) {
        ++records;
        /* Flags are independent: pressed may coexist with double or held.
         * Observe every state; filtering to finished hid useful transitions. */
        snprintf(line, sizeof(line),
                 "event=%u t_ms=%lld state=%d double=%u held=%u pressed=%u pair=%u x=%d y=%d",
                 records, sample.ms, state, (unsigned)doubled,
                 (unsigned)held, (unsigned)pressed, result == DTP_PAIR,
                 (int)sample.point.x, (int)sample.point.y);
        nh_log("%s", line);
        dtp_append_report(DTP_REPORT, line);
        }
        if (consumed) return;
    }
    /* Unconsumed taps, including unsupported/disabled states, forward once. */
    if (original_process_tap)
        original_process_tap(reader, tap);
}

static int probe_init(void) {
    char line[512];
    FILE *file;
    enabled = false;
    dtp_annotations_enable(false);
    dtp_toggle_enable(false);
    if (!original_process_tap || !tap_is_double || !tap_is_held ||
        !tap_is_pressed || !gesture_state || !tap_position) {
        nh_log("probe inactive: required symbols unavailable");
        return 0;
    }
    if (!access("/mnt/onboard/doubletap_disable", F_OK)) {
        nh_log("probe disabled by flag (restart required after changing flag)");
        return 0;
    }
    file = fopen("/mnt/onboard/.kobo/version", "r");
    if (!file) {
        nh_log("probe inactive: cannot read device version");
        return 0;
    }
    const bool read_ok = fgets(line, sizeof(line), file) != NULL;
    fclose(file);
    if (!read_ok || !supported_device(line)) {
        nh_log("probe inactive: unsupported device or firmware");
        return 0;
    }
    enabled = true;
    dtp_annotations_enable(true);
    dtp_toggle_enable(true);
    records = 0;
    dtp_reset(&recognizer);
    nh_log("native gesture observer enabled; at most %u callbacks", DTP_EVENT_LIMIT);
    dtp_append_report(DTP_REPORT,
                      "session-start version=" NH_VERSION " firmware=" DTP_VERSION " budget=96");
    return 0;
}

static struct nh_info info = {
    .name = "KoboDoubleTapProbe",
    .desc = "Native KEPUB double-tap highlight toggle and guarded annotation actions.",
    .uninstall_flag = "/mnt/onboard/doubletap_uninstall",
};

static struct nh_hook hooks[] = {
    {.sym = "_ZN19SelectionController14startSelectionEP10TapGesture", .sym_new = "dtp_longpress_start",
     .lib = "libnickel.so.1.0.0", .out = &dtpt_start, .optional = true},
#if DTP_USE_SUDACHI
    {.sym = "_ZN19PlugWorkflowManager14usbPlugAllowedEv", .sym_new = "dtp_sudachi_usb_allowed",
     .lib = "libnickel.so.1.0.0", .out = &dtps_usb_allowed, .optional = true},
#endif
    {.sym = "_ZN11ReadingView15forceFullUpdateEv", .sym_new = "dtp_render_force_full",
     .lib = "libnickel.so.1.0.0", .out = &dtpr_force_full, .optional = true},
    {.sym = "_ZN19SelectionController7repaintEv", .sym_new = "dtp_render_repaint",
     .lib = "libnickel.so.1.0.0", .out = &dtpr_repaint, .optional = true},
    {.sym = "_ZN19SelectionController14clearSelectionEv", .sym_new = "dtp_toggle_clear",
     .lib = "libnickel.so.1.0.0", .out = &dtpt_clear, .optional = true},
    {.sym = "_ZN19HighlightingManager17addHighlightSpansER11QWebElementRK8BookmarkiiibRK7QString", .sym_new = "dtp_render_add",
     .lib = "libnickel.so.1.0.0", .out = &dtpr_add, .optional = true},
    {.sym = "_ZN19SelectionController8toggleA2Ebb", .sym_new = "dtp_render_a2",
     .lib = "libnickel.so.1.0.0", .out = &dtpr_a2, .optional = true},
    {.sym = "_ZN19SelectionController15closeFooterMenuEv", .sym_new = "dtp_render_close_footer",
     .lib = "libnickel.so.1.0.0", .out = &dtpr_close_footer, .optional = true},
    {.sym = "_ZN11ReadingView10processTapEP10TapGesture",
     .sym_new = "dtp_process_tap", .lib = "libnickel.so.1.0.0",
     .out = nh_symoutptr(original_process_tap), .optional = true},
    {.sym = "_ZN23SelectionMenuController16setupMainOptionsEv", .sym_new = "dtp_annotation_setup",
     .lib = "libnickel.so.1.0.0", .out = &dtpa_setup, .optional = true},
    {.sym = "_ZN23SelectionMenuController18createMenuTextItemEP7QWidgetRK7QString", .sym_new = "dtp_annotation_create_item",
     .lib = "libnickel.so.1.0.0", .out = &dtpa_create_item, .optional = true},
    {.sym = "_ZNK8Bookmark5getIdEv", .sym_new = "dtp_annotation_bookmark_id",
     .lib = "libnickel.so.1.0.0", .out = &dtpa_get_id, .optional = true},
    {.sym = "_ZN15BookmarkManager4saveER8BookmarkRK7QString", .sym_new = "dtp_annotation_save",
     .lib = "libnickel.so.1.0.0", .out = &dtpa_save, .optional = true},
    {.sym = "_ZN15BookmarkManager10setDeletedER8BookmarkRK6Device", .sym_new = "dtp_annotation_delete",
     .lib = "libnickel.so.1.0.0", .out = &dtpa_delete, .optional = true},
    {.sym = "_ZN10TapGesture17setAllowDoubleTapEb", .sym_new = "dtp_toggle_allow",
     .lib = "libnickel.so.1.0.0", .out = &dtpt_allow, .optional = true},
    {.sym = "_ZN23SelectionMenuController18setCurrentBookmarkERK7QString", .sym_new = "dtp_toggle_set_current",
     .lib = "libnickel.so.1.0.0", .out = &dtpt_set_current, .optional = true},
    {.sym = "_ZN11ReadingView12onPageTurnedEv", .sym_new = "dtp_toggle_page",
     .lib = "libnickel.so.1.0.0", .out = &dtpt_page, .optional = true},
    {.sym = "_ZN11ReadingView15readerWillCloseEv", .sym_new = "dtp_toggle_close",
     .lib = "libnickel.so.1.0.0", .out = &dtpt_close, .optional = true},
    {.sym = "_ZN11ReadingView9setVolumeERK6VolumeRK8Bookmark", .sym_new = "dtp_toggle_volume",
     .lib = "libnickel.so.1.0.0", .out = &dtpt_volume, .optional = true},
    {0},
};

static struct nh_dlsym symbols[] = {
    {.name = "_ZN19HighlightingManager16surroundWithSpanER11QWebElementiiRK7QStringS4_bS4_", .out = &dtpr_partial, .optional = true},
    {.name = "_ZN19HighlightingManager14getMultiPartIdEiRK7QString", .out = &dtpr_part_id, .optional = true},
    {.name = "_ZNK10WebkitView7webViewEv", .out = &dtpj_web_view, .optional = true},
    {.name = "_ZNK13CustomWebView4pageEv", .out = &dtpj_page, .optional = true},
    {.name = "_ZNK8QWebPage12currentFrameEv", .out = &dtpj_frame, .optional = true},
    {.name = "_ZN19HighlightingManager15removeHighlightEP9QWebFrameRK7QString", .out = &dtpj_remove_visual, .optional = true},
    {.name = "_ZN10WebkitView32evaluateJavaScriptWithBrokennessE7QString", .out = &dtpj_evaluate, .optional = true},
    {.name = "_ZN10WebkitView12selectedTextEv", .out = &dtpj_selected_text, .optional = true},
    {.name = "_ZNK12GestureMixin7watchedEv", .out = &dtpt_watched, .optional = true},
    {.name = "_ZNK10TapGesture6isHeldEv", .out = &dtpl_is_held, .optional = true},
    {.name = "_ZN19SelectionController16processSelectionEv", .out = &dtpt_process_selection, .optional = true},
    {.name = "_ZNK19SelectionController17isSelectionModeOnEv", .out = &dtpt_mode, .optional = true},
    {.name = "_ZN19SelectionController24selectionMenuViewVisibleEv", .out = &dtpt_menu_visible, .optional = true},
    {.name = "_ZN19SelectionController16selectionModeOffEv", .out = &dtpt_mode_off, .optional = true},
    {.name = "_ZNK11ReadingView13hasOnlyImagesEv", .out = &dtpt_images, .optional = true},
    {.name = "_ZNK10TapGesture8isDoubleEv", .out = nh_symoutptr(tap_is_double), .optional = true},
    {.name = "_ZNK10TapGesture6isHeldEv", .out = nh_symoutptr(tap_is_held), .optional = true},
    {.name = "_ZNK10TapGesture9isPressedEv", .out = nh_symoutptr(tap_is_pressed), .optional = true},
    {.name = "_ZNK8QGesture5stateEv", .out = nh_symoutptr(gesture_state), .optional = true},
    {.name = "_ZNK10TapGesture8positionEv", .out = nh_symoutptr(tap_position), .optional = true},
    {.name = "_ZN17SelectionMenuView11addMenuItemEP12MenuTextItem", .out = &dtpa_add_item, .optional = true},
    {.name = "_ZNK23SelectionMenuController15currentBookmarkEv", .out = &dtpa_current_id, .optional = true},
    {.name = "_ZNK8Bookmark11isHighlightEv", .out = &dtpa_is_highlight, .optional = true},
    {.name = "_ZNK8Bookmark6isNoteEv", .out = &dtpa_is_note, .optional = true},
    {.name = "_ZNK8Bookmark16getAnnotationRawEv", .out = &dtpa_annotation_raw, .optional = true},
    {.name = "_ZNK8Bookmark10getTextRawEv", .out = &dtpa_text_raw, .optional = true},
    {.name = "_ZNK8Bookmark12getContentIdEv", .out = &dtpa_content_id, .optional = true},
    {.name = "_ZNK8Bookmark21getStartContainerPathEv", .out = &dtpa_start_path, .optional = true},
    {.name = "_ZNK8Bookmark19getEndContainerPathEv", .out = &dtpa_end_path, .optional = true},
    {.name = "_ZNK8Bookmark27getStartContainerChildIndexEv", .out = &dtpa_start_child, .optional = true},
    {.name = "_ZNK8Bookmark25getEndContainerChildIndexEv", .out = &dtpa_end_child, .optional = true},
    {.name = "_ZNK8Bookmark14getStartOffsetEv", .out = &dtpa_start_offset, .optional = true},
    {.name = "_ZNK8Bookmark12getEndOffsetEv", .out = &dtpa_end_offset, .optional = true},
    {0},
};

/* GCC 4.9 rejects the framework macro's file-scope C compound literal.
 * Export the same descriptor with a standard aggregate initializer instead. */
__attribute__((visibility("default")))
struct nh NickelHook = {
    .init = probe_init,
    .uninstall = dtp_uninstall_cleanup,
    .info = &info,
    .hook = hooks,
    .dlsym = symbols,
};
