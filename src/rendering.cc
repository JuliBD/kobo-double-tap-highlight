/* Correct the pinned native zero-offset renderer without changing stored ranges. */
#include "rendering.h"
#include "annotations.h"
#include "settings.h"
#include "toggle.h"
#include "report.h"
#include <QString>
#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <QThread>
#include <QWidget>

extern "C" { void *dtpr_add, *dtpr_partial, *dtpr_part_id, *dtpr_a2, *dtpr_close_footer; }
extern "C" { void *dtpr_repaint; }
extern "C" { void *dtpr_force_full; }

namespace {
// Verified pinned Qt attributes; use QWidget's API, never private field offsets.
const Qt::WidgetAttribute partial_update = static_cast<Qt::WidgetAttribute>(130);
const Qt::WidgetAttribute full_update = static_cast<Qt::WidgetAttribute>(131);
struct PartialScope {
    QList<QPointer<QWidget> > changed;
    ~PartialScope() {
        for (int i = 0; i < changed.size(); ++i)
            if (changed.at(i)) changed.at(i)->setAttribute(partial_update, false);
    }
    void add(QWidget *widget) {
        if (!widget->testAttribute(partial_update)) {
            changed.append(QPointer<QWidget>(widget));
            widget->setAttribute(partial_update, true);
        }
    }
};
}

extern "C" bool dtp_render_deliver_partial(void *reader_pointer, void *book_pointer, void *event_pointer) {
    if (!dtpr_force_full || !qApp || QThread::currentThread() != qApp->thread() || !reader_pointer || !book_pointer || !event_pointer) return false;
    QEvent *event = static_cast<QEvent *>(event_pointer);
    if (event->type() != QEvent::UpdateRequest) return false;
    QWidget *reader = static_cast<QWidget *>(reader_pointer);
    QWidget *book = static_cast<QWidget *>(book_pointer);
    if (!reader->isVisible() || !book->isVisible() || !reader->isAncestorOf(book) ||
        QApplication::activePopupWidget() || QApplication::activeModalWidget()) return false;
    QWidget *window = reader->window();
    QList<QWidget *> targets = book->findChildren<QWidget *>();
    targets.append(book); targets.append(reader); targets.append(window);
    // Respect an explicit full-refresh hint anywhere in the affected widget tree.
    for (int i = 0; i < targets.size(); ++i)
        if (targets.at(i)->testAttribute(full_update)) return false;
    PartialScope scope;
    for (int i = 0; i < targets.size(); ++i) scope.add(targets.at(i));
    // The caller intercepts this exact native event and guards re-entry. Deliver
    // it once, with no early flush or processing of other pending events.
    QCoreApplication::sendEvent(window, event);
    return true;
}

extern "C" __attribute__((visibility("default"))) void dtp_render_force_full(void *reader) {
    dtp_toggle_trace_refresh(reader);
    // Diagnostic only: preserve every request, including outside a toggle.
    reinterpret_cast<void (*)(void *)>(dtpr_force_full)(reader);
}

static void report(const char *message) {
    static unsigned records;
    if (records++ < 64) dtp_append_report("/mnt/onboard/.kobo/doubletap-render.log", message);
}

extern "C" __attribute__((visibility("default"))) void dtp_render_add(
        void *element, const void *bookmark, int start, int end, int part, bool reverse, const void *style) {
    if (start == 0 && end > 0 && dtpr_partial && dtpr_part_id && bookmark && dtpa_get_id &&
        dtp_annotations_ready() && dtp_settings_rendering()) {
        const QString &id = reinterpret_cast<const QString &(*)(const void *)>(dtpa_get_id)(bookmark);
        if (id.isEmpty()) {
            reinterpret_cast<void (*)(void *, const void *, int, int, int, bool, const void *)>(dtpr_add)(
                element, bookmark, start, end, part, reverse, style);
            return;
        }
        const QString span_id = part ? reinterpret_cast<QString (*)(int, const QString &)>(dtpr_part_id)(part, id) : id;
        // The native add helper chooses its whole-element overload when start==0,
        // ignoring a positive finite end. Invoke its existing partial overload.
        reinterpret_cast<void (*)(void *, int, int, const QString &, const QString &, bool, const QString &)>(dtpr_partial)(
            element, start, end, span_id, id, reverse, *static_cast<const QString *>(style));
        report("zero-offset highlight rendered through bounded native range");
        return;
    }
    reinterpret_cast<void (*)(void *, const void *, int, int, int, bool, const void *)>(dtpr_add)(
        element, bookmark, start, end, part, reverse, style);
}

extern "C" __attribute__((visibility("default"))) void dtp_render_a2(void *controller, bool active, bool force) {
    if (dtp_toggle_active_controller(controller)) { report("automatic selection skipped long-press display-mode transition"); return; }
    reinterpret_cast<void (*)(void *, bool, bool)>(dtpr_a2)(controller, active, force);
}

extern "C" __attribute__((visibility("default"))) void dtp_render_close_footer(void *controller) {
    if (dtp_toggle_active_controller(controller)) { report("automatic selection skipped footer-menu close signal"); return; }
    reinterpret_cast<void (*)(void *)>(dtpr_close_footer)(controller);
}

extern "C" __attribute__((visibility("default"))) void dtp_render_repaint(void *controller) {
    // Despite its name, ReadingView connects this signal to forceFullUpdate().
    // WebKit's normal dirty-region updates still paint the changed word.
    if (dtp_toggle_active_controller(controller)) {
        report("automatic selection skipped repaint signal wired to forceFullUpdate"); return;
    }
    reinterpret_cast<void (*)(void *)>(dtpr_repaint)(controller);
}
