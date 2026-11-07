/* Native KEPUB double-tap arbitration. No stored or synthetic gesture objects. */
#include "toggle.h"
#include "annotations.h"
#include "japanese.h"
#include "rendering.h"
#include "report.h"
#include <NickelHook.h>
#include <QApplication>
#include <QByteArray>
#include <QEvent>
#include <QElapsedTimer>
#include <QDateTime>
#include <QPaintEvent>
#include <QPointer>
#include <QThread>
#include <QWidget>
#include <cstdio>

extern "C" {
#define DTP_DEFINE_POINTER(n) void *n;
DTP_TOGGLE_POINTERS(DTP_DEFINE_POINTER)
#undef DTP_DEFINE_POINTER
}

namespace {
bool enabled;
unsigned reports;
void report(const char *message) {
    nh_log("%s", message);
    if (reports++ < 96) dtp_append_report("/mnt/onboard/.kobo/doubletap-toggle.log", message);
}
bool gui() {
    return qApp && QThread::currentThread() == qApp->thread();
}
QObject *selection_for(QObject *reader) {
    const QObjectList children = reader->children();
    QObject *found = NULL;
    for (int i = 0; i < children.size(); ++i) {
        if (!children.at(i)->inherits("SelectionController")) continue;
        if (found) return NULL;
        found = children.at(i);
    }
    return found;
}
bool unobstructed(QWidget *reader, QWidget *book, QObject *selection) {
    if (!reader || !book || !selection || !reader->isVisible() || !book->isVisible() ||
        !reader->isEnabled() || !book->isEnabled() || QApplication::activeModalWidget() ||
        QApplication::activePopupWidget()) return false;
    if (QApplication::activeWindow() && QApplication::activeWindow() != reader->window()) return false;
    return !reinterpret_cast<bool (*)(const void *)>(dtpt_mode)(selection) &&
           !reinterpret_cast<bool (*)(void *)>(dtpt_menu_visible)(selection) &&
           !reinterpret_cast<bool (*)(const void *)>(dtpt_images)(reader);
}
class Context : public QObject {
public:
    QPointer<QWidget> reader, book;
    QPointer<QObject> selection, menu;
    bool armed, busy, cleared, full_requested;
    long long started;
    QElapsedTimer trace_clock;
    QElapsedTimer pending_clock;
    bool pending_refresh = false, delivering_update = false;
    unsigned trace_count = 0;
    unsigned long long operation = 0;
    void trace(const char *message) {
        if (!trace_clock.isValid() || trace_clock.elapsed() > 1500 || trace_count++ >= 18) return;
        const QByteArray utc = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz'Z'")).toLatin1();
        char line[256];
        std::snprintf(line, sizeof(line), "utc=%s pid=%lld op=%llu t=%lld busy=%d %.144s",
                      utc.constData(), QCoreApplication::applicationPid(), operation, trace_clock.elapsed(), busy, message);
        dtp_append_recent_report("/mnt/onboard/.kobo/doubletap-refresh-069.log", line);
    }
    static unsigned flags(QWidget *widget) {
        // Pinned Qt custom attributes: heuristic, partial, full, A2, expand-to-screen.
        const int attributes[] = {129,130,131,136,148};
        unsigned bits = 0;
        for (unsigned i = 0; widget && i < 5; ++i)
            if (widget->testAttribute(static_cast<Qt::WidgetAttribute>(attributes[i]))) bits |= 1u << i;
        return bits;
    }
    Context() : QObject(qApp), armed(false), busy(false), cleared(false), full_requested(false), started(0) {
        qApp->installEventFilter(this);
    }
    void cancel_refresh(const char *reason) {
        if (pending_refresh) trace(reason);
        pending_refresh = false;
    }
    void cancel() { armed = false; } // Gesture cleanup must not discard a completed toggle's repaint.
    void cancel_all(const char *reason) { cancel(); cancel_refresh(reason); }
    bool eventFilter(QObject *object, QEvent *event) override {
        const QEvent::Type incoming = event->type();
        // Qt can deliver the triggering TouchBegin after the native recognizer
        // has already completed our double tap. New native gesture starts cancel
        // pending treatment in dtp_toggle_process; raw touch delivery alone must
        // not invalidate the completed gesture's scheduled repaint.
        if (pending_refresh && incoming == QEvent::TouchBegin)
            trace("raw TouchBegin retained pending repaint; native start owns cancellation");
        if (pending_refresh && (pending_clock.elapsed() > 250 || incoming == QEvent::MouseButtonPress ||
            incoming == QEvent::KeyPress || incoming == QEvent::Wheel ||
            incoming == QEvent::ApplicationDeactivate || incoming == QEvent::WindowDeactivate ||
            ((object == reader || object == book) && (incoming == QEvent::Hide || incoming == QEvent::Resize || incoming == QEvent::Close))))
        {
            char reason[96];
            std::snprintf(reason, sizeof(reason), "pending repaint cancelled: event=%d age=%lld", int(incoming), pending_clock.elapsed());
            cancel_refresh(reason);
        }
        if (pending_refresh && incoming == QEvent::UpdateRequest) {
            char gate[96];
            std::snprintf(gate, sizeof(gate), "update gate window=%d busy=%d reader=%d book=%d selection=%d",
                          reader && object == reader->window(), busy, !reader.isNull(), !book.isNull(), !selection.isNull());
            trace(gate);
        }
        if (pending_refresh && !busy && !delivering_update && reader && book && selection &&
            incoming == QEvent::UpdateRequest && object == reader->window()) {
            pending_refresh = false; // At most one scheduled update per toggle.
            if (unobstructed(reader, book, selection)) {
                delivering_update = true;
                const bool delivered = dtp_render_deliver_partial(reader.data(), book.data(), event);
                delivering_update = false;
                trace(delivered ? "scheduled highlight repaint delivered with partial hint" : "scheduled repaint retained native hints");
                if (delivered) return true; // Re-delivery already ran the native handler once.
            } else trace("scheduled repaint blocked by native selection/window state");
        }
        if (trace_clock.isValid() && trace_clock.elapsed() <= 1500 && object->isWidgetType() && book && reader) {
            QWidget *widget = static_cast<QWidget *>(object);
            const QEvent::Type type = event->type();
            // Popups can be separate windows: include all nearby show/hide events.
            if (type == QEvent::Show || type == QEvent::Hide || widget == reader ||
                widget == reader->window() || widget == book || book->isAncestorOf(widget)) {
                if (type == QEvent::Paint || type == QEvent::UpdateRequest || type == QEvent::Show || type == QEvent::Hide) {
                    QRect dirty;
                    qint64 area = 0;
                    int regions = 0;
                    if (type == QEvent::Paint) {
                        QPaintEvent *paint = static_cast<QPaintEvent *>(event);
                        dirty = paint->rect();
                        const QVector<QRect> rects = paint->region().rects();
                        regions = rects.size();
                        for (int i = 0; i < rects.size(); ++i) area += qint64(rects.at(i).width()) * rects.at(i).height();
                    }
                    char line[144];
                    std::snprintf(line, sizeof(line), "event=%d %.20s rect=%dx%d area=%lld regions=%d flags=%x reader=%x book=%x",
                                  int(type), widget->metaObject()->className(), dirty.width(), dirty.height(), area, regions,
                                  flags(widget), flags(reader), flags(book));
                    trace(line);
                }
            }
        }
        if (!busy && armed) {
            const QEvent::Type type = event->type();
            if (type == QEvent::ApplicationDeactivate || type == QEvent::WindowDeactivate ||
                ((object == reader || object == book) &&
                 (type == QEvent::Hide || type == QEvent::Resize || type == QEvent::FocusOut)) ||
                (type == QEvent::Show && object->isWidgetType() && object != reader && object != book))
                cancel();
        }
        return false;
    }
};
Context *context;
Context &state() {
    if (!context) context = new Context;
    return *context;
}
void cancel_reader(void *reader) {
    if (context && context->reader.data() == reader) context->cancel_all("pending repaint cancelled: page/book lifecycle");
}
}

extern "C" void dtp_toggle_enable(bool enable) {
    enabled = enable && dtp_annotations_ready() && dtpr_add && dtpr_partial && dtpr_part_id && dtpr_a2 && dtpr_close_footer && dtpr_repaint;
#define DTP_REQUIRE_POINTER(n) enabled = enabled && n;
    DTP_TOGGLE_POINTERS(DTP_REQUIRE_POINTER)
#undef DTP_REQUIRE_POINTER
    reports = 0;
    if (context) context->cancel_all("pending repaint cancelled: enable/context reset");
    report(enabled ? "session-start native KEPUB double-tap toggle enabled" : "native double-tap toggle inactive");
}

extern "C" bool dtp_toggle_busy(void) { return enabled && context && context->busy && gui(); }

extern "C" bool dtp_toggle_active_controller(const void *controller) {
    return enabled && context && context->busy && context->selection.data() == controller && gui();
}

extern "C" void dtp_toggle_trace_refresh(const void *reader) {
    if (context && context->reader.data() == reader && gui()) {
        context->cancel_refresh("pending repaint cancelled: native full-refresh request");
        if (context->busy) context->full_requested = true;
        context->trace("native forceFullUpdate requested (forwarded)");
    }
}

extern "C" __attribute__((visibility("default"))) void dtp_toggle_clear(void *controller) {
    if (dtp_toggle_active_controller(controller)) context->cleared = true;
    reinterpret_cast<void (*)(void *)>(dtpt_clear)(controller);
}

namespace {
bool allow_double_tap_for_reader(void *tap, bool native) {
    if (!enabled || !gui() || !tap) return native;
    // Native TapGesture's public GestureMixin base starts at +8 on this ABI.
    // Verified by its constructor and TapGestureRecognizer::create/reset calls.
    QObject *watched = reinterpret_cast<QObject *(*)(const void *)>(dtpt_watched)(static_cast<char *>(tap) + 8);
    if (!watched || !watched->isWidgetType()) return native;
    // The pinned constructor chain places QWidget/QObject at offset zero.
    QWidget *widget = static_cast<QWidget *>(watched);
    for (unsigned depth = 0; widget && !widget->inherits("KepubBookReader") && depth < 32; ++depth)
        widget = widget->parentWidget();
    if (!widget || !widget->inherits("KepubBookReader")) return native;
    QWidget *reader = widget;
    for (unsigned depth = 0; reader && !reader->inherits("ReadingView") && depth < 32; ++depth)
        reader = reader->parentWidget();
    if (!reader || !reader->inherits("ReadingView") || !widget->inherits("KepubBookReader")) return native;
    QObject *selection = selection_for(reader);
    if (!unobstructed(reader, widget, selection)) return native;
    Context &s = state();
    if (s.busy) return native;
    if (s.reader != reader || s.book != widget || s.selection != selection) {
        s.cancel_all("pending repaint cancelled: reader context changed"); report("native double-tap recognition enabled for KEPUB text reader");
    }
    s.reader = reader; s.book = widget; s.selection = selection;
    return true; // Native recognizer owns the first-tap timer and movement checks.
}
}

extern "C" __attribute__((visibility("default"))) void dtp_toggle_allow(void *tap, bool allow) {
    reinterpret_cast<void (*)(void *, bool)>(dtpt_allow)(tap, allow_double_tap_for_reader(tap, allow));
}

extern "C" __attribute__((visibility("default"))) void dtp_toggle_set_current(void *menu, const void *id) {
    reinterpret_cast<void (*)(void *, const void *)>(dtpt_set_current)(menu, id);
    if (enabled && context && context->busy && gui()) {
        QObject *object = reinterpret_cast<QObject *>(menu);
        if (object->inherits("SelectionMenuController") && object->parent() == context->reader)
            context->menu = object;
    }
}

extern "C" bool dtp_toggle_process(void *reader, void *tap, const struct dtp_sample *sample) {
    if (!enabled || !context || !gui() || !sample || !tap) return false;
    Context &s = *context;
    if (s.busy || s.reader.data() != reader) {
        if (s.reader.data() != reader) s.cancel_all("pending repaint cancelled: different gesture reader");
        else s.cancel();
        return false;
    }
    if (!unobstructed(s.reader, s.book, s.selection) || sample->held || sample->state == 4 || sample->ms < 0) {
        s.cancel_all("pending repaint cancelled: gesture/context guard"); return false;
    }
    if (sample->state == 1 && !sample->native_double) {
        s.cancel_refresh("pending repaint cancelled: new gesture start");
        s.armed = true; s.started = sample->ms;
    }
    if (sample->state != 3) return false;
    const bool armed = s.armed && sample->ms >= s.started && sample->ms - s.started <= 1000;
    s.cancel();
    if (!sample->native_double) return false; // The delayed single tap follows Nickel unchanged.
    if (!armed) { report("double-tap ignored: context changed or no matching start"); return true; }
    s.busy = true; s.cleared = false; s.menu.clear();
    s.full_requested = false;
    ++s.operation;
    s.trace_clock.start(); s.trace_count = 0; s.trace("double-tap begin");
    QPointer<QObject> selection = s.selection;
    const bool selected = reinterpret_cast<bool (*)(void *, void *)>(dtpt_start)(selection.data(), tap);
    if (selected && selection) {
        // Resolve the native highlight under the original tap before tokenizing.
        // This lets a tap anywhere in an old passage remove the whole passage,
        // including Japanese passages that are unsuitable for word expansion.
        reinterpret_cast<void (*)(void *)>(dtpt_process_selection)(selection.data());
        if (selection && s.menu && dtp_annotation_has_current(s.menu.data())) {
            dtp_annotation_auto_action(s.menu.data(), NULL, s.book.data());
        } else if (selection && s.book) {
            const QByteArray initial = dtpj_selected_text ? reinterpret_cast<QString (*)(void *)>(dtpj_selected_text)(s.book.data()).toUtf8() : QByteArray();
            QByteArray expected;
            const bool ready = dtp_japanese_expand(s.book.data(), &expected);
            if (ready && selection && s.book) {
                if (expected != initial) {
                    s.menu.clear();
                    reinterpret_cast<void (*)(void *)>(dtpt_process_selection)(selection.data());
                }
                if (selection && s.menu)
                    dtp_annotation_auto_action(s.menu.data(), expected.constData(), s.book.data());
                else report("double-tap ignored: native selection menu unavailable");
            } else report("double-tap ignored: word selection could not be verified");
        }
    } else report("double-tap ignored: no native text selection");
    if (selection) {
        if (!s.cleared) reinterpret_cast<void (*)(void *)>(dtpt_clear)(selection.data());
        if (selection) reinterpret_cast<void (*)(void *)>(dtpt_mode_off)(selection.data());
    }
    s.menu.clear(); s.busy = false; s.cancel();
    if (!s.full_requested && s.reader && s.book && selection && unobstructed(s.reader, s.book, selection)) {
        s.pending_clock.start(); s.pending_refresh = true;
        s.trace("partial hint armed for next scheduled repaint");
    }
    s.trace("double-tap end");
    return true;
}

extern "C" __attribute__((visibility("default"))) void dtp_toggle_page(void *reader) {
    cancel_reader(reader); reinterpret_cast<void (*)(void *)>(dtpt_page)(reader);
}
extern "C" __attribute__((visibility("default"))) void dtp_toggle_close(void *reader) {
    cancel_reader(reader); reinterpret_cast<void (*)(void *)>(dtpt_close)(reader);
}
extern "C" __attribute__((visibility("default"))) void dtp_toggle_volume(void *reader, const void *volume, const void *bookmark) {
    cancel_reader(reader);
    reinterpret_cast<void (*)(void *, const void *, const void *)>(dtpt_volume)(reader, volume, bookmark);
}
