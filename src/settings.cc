#include "settings.h"
#include "annotations.h"
#include "toggle.h"
#include "japanese.h"
#include <QApplication>
#include <QFile>
#include <QPointer>
#include <QThread>
#include <QWidget>

#ifndef DTP_SETTINGS_FILE
#define DTP_SETTINGS_FILE "/mnt/onboard/.kobo/doubletap.conf"
#endif

extern "C" { void *dtpl_is_held; }

namespace {
bool read_setting(const QByteArray &key) {
    QFile file(QLatin1String(DTP_SETTINGS_FILE));
    if (!file.open(QIODevice::ReadOnly) || file.size()>4096) return true;
    bool value=true;
    while (!file.atEnd()) {
        const QByteArray line=file.readLine().trimmed();
        const int equal=line.indexOf('=');
        if (equal<0 || line.left(equal).trimmed()!=key) continue;
        const QByteArray setting=line.mid(equal+1).trimmed();
        if (setting=="0" || setting=="1") value=setting=="1";
    }
    return value;
}
}
extern "C" bool dtp_settings_longpress(void) { return read_setting("japanese_long_press_words"); }
extern "C" bool dtp_settings_rendering(void) { return read_setting("highlight_rendering_fix"); }

extern "C" __attribute__((visibility("default"))) bool dtp_longpress_start(void *controller, void *tap) {
    // Verified native ABI: bool startSelection(TapGesture*). Always forward once.
    if (!dtpt_start) return false;
    const bool eligible=controller && tap && qApp &&
        QThread::currentThread()==qApp->thread() && dtp_annotations_ready() &&
        !dtp_toggle_busy() && dtpl_is_held && dtpt_watched && dtp_settings_longpress() &&
        reinterpret_cast<bool (*)(const void *)>(dtpl_is_held)(tap);
    QPointer<QObject> selection;
    QPointer<QWidget> book;
    if (eligible) {
        selection=reinterpret_cast<QObject *>(controller);
        QObject *watched=reinterpret_cast<QObject *(*)(const void *)>(dtpt_watched)(static_cast<char *>(tap)+8);
        if (watched && watched->isWidgetType() && watched->inherits("KepubBookReader")) {
            QWidget *widget=static_cast<QWidget *>(watched);
            QWidget *reader=widget->parentWidget();
            while (reader && !reader->inherits("ReadingView")) reader=reader->parentWidget();
            if (reader && selection->parent()==reader && widget->isVisible() && reader->isVisible() &&
                !QApplication::activeModalWidget() && !QApplication::activePopupWidget()) book=widget;
        }
    }
    const bool selected=reinterpret_cast<bool (*)(void *,void *)>(dtpt_start)(controller,tap);
    if (selected && selection && book) {
        QByteArray expected;
        // Reuse the bounded DOM/native-text validation; never save automatically.
        dtp_japanese_expand(book.data(),&expected);
    }
    return selected;
}
