#include "japanese.h"
#include "report.h"
#include "japanese_backend.h"
#include "sudachi.h"
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QVariant>
#include "../build/japanese_bundle.h"

extern "C" { void *dtpj_evaluate, *dtpj_selected_text; }
extern "C" { void *dtpj_web_view, *dtpj_page, *dtpj_frame, *dtpj_remove_visual; }

static void report(const char *message) {
    static unsigned records;
    if (records++ < 64) dtp_append_report("/mnt/onboard/.kobo/doubletap-japanese.log", message);
}

extern "C" bool dtp_japanese_expand(void *book, void *expected_bytes) {
    QByteArray &expected = *static_cast<QByteArray *>(expected_bytes);
    expected.clear();
    if (!book || !dtpj_selected_text) { report("Japanese selection unavailable: native text getter missing"); return false; }
    QPointer<QObject> live_book(reinterpret_cast<QObject *>(book));
    const QString initial = reinterpret_cast<QString (*)(void *)>(dtpj_selected_text)(book);
    bool japanese = false;
    for (int i = 0; i < initial.size(); ++i) {
        const ushort c = initial.at(i).unicode();
        japanese = japanese || (c >= 0x3040 && c <= 0x30ff) ||
                   (c >= 0x3400 && c <= 0x9fff) || (c >= 0xff66 && c <= 0xff9f);
    }
    if (!japanese) { expected = initial.toUtf8(); return true; }
    if (initial.size() > 128 || !dtpj_evaluate) { report("Japanese selection refused: native evaluator missing or oversized selection"); return false; }
    QString script;
#if DTP_USE_SUDACHI
    script = QString::fromUtf8(reinterpret_cast<const char *>(dtp_japanese_context_script), sizeof(dtp_japanese_context_script));
    const QVariant context_value = reinterpret_cast<QVariant (*)(void *, QString)>(dtpj_evaluate)(book, script);
    if (!live_book) return false;
    const QByteArray raw_context = context_value.toString().toUtf8();
    if (raw_context.size() > 65536) return false;
    const QJsonDocument context_document = QJsonDocument::fromJson(raw_context);
    const QJsonObject context = context_document.object();
    const QString text = context.value(QStringLiteral("text")).toString();
    const int start = context.value(QStringLiteral("start")).toInt(-1);
    const int end = context.value(QStringLiteral("end")).toInt(-1);
    if (!context_document.isObject() || context.value(QStringLiteral("status")).toString() != QStringLiteral("context") ||
        text.isEmpty() || text.size() > 4096 || text.contains(QChar(0)) || start < 0 || end <= start || end > text.size() ||
        context.value(QStringLiteral("initial")).toString() != initial || text.mid(start,end-start) != initial) {
        report("Sudachi refused: invalid or stale paragraph context"); return false;
    }
    const QByteArray utf8 = text.toUtf8();
    unsigned bounds[2] = {0,0};
    const int status = dtp_sudachi_select(reinterpret_cast<const unsigned char *>(utf8.constData()),utf8.size(),start,end,bounds);
    if (status || bounds[0] > unsigned(start) || bounds[1] < unsigned(end) || bounds[1] > unsigned(text.size())) {
        report("Sudachi refused: dictionary unavailable, invalid result or no covering token"); return false;
    }
    const QString target = text.mid(bounds[0],bounds[1]-bounds[0]);
    if (target.isEmpty() || target.size() > 24) { report("Sudachi refused: unsafe token length"); return false; }
    QJsonObject request;
    request.insert(QStringLiteral("mode"),QStringLiteral("apply"));
    request.insert(QStringLiteral("text"),text);
    request.insert(QStringLiteral("initialStart"),start); request.insert(QStringLiteral("initialEnd"),end);
    request.insert(QStringLiteral("start"),int(bounds[0])); request.insert(QStringLiteral("end"),int(bounds[1]));
    request.insert(QStringLiteral("word"),target);
    script = QString::fromUtf8(reinterpret_cast<const char *>(dtp_japanese_apply_script),sizeof(dtp_japanese_apply_script));
    script.replace(QStringLiteral("__DTP_APPLY_REQUEST__"),QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
#else
    script = QString::fromUtf8(reinterpret_cast<const char *>(dtp_japanese_script), sizeof(dtp_japanese_script));
#endif
    // Native ABI: QVariant by value, this, QString by value. Always return a
    // nonempty JSON string so the native helper never retries a mutation.
    const QVariant value = reinterpret_cast<QVariant (*)(void *, QString)>(dtpj_evaluate)(book, script);
    if (!live_book) { report("Japanese selection refused: reader closed during evaluation"); return false; }
    const QByteArray result = value.toString().toUtf8();
    if (result.size() > 2048) { report("Japanese selection refused: oversized result"); return false; }
    const QJsonDocument document = QJsonDocument::fromJson(result);
    const QJsonObject object = document.object();
    const QString word = object.value(QStringLiteral("word")).toString();
    if (!document.isObject() || object.value(QStringLiteral("status")).toString() != QStringLiteral("expanded") ||
        word.isEmpty() || word.size() > 24 || word.contains(QChar(0))) {
        report("Japanese selection refused: no safe token/range mapping"); return false;
    }
    expected = word.toUtf8();
    if (expected.size() > 512) { expected.clear(); report("Japanese selection refused: oversized word"); return false; }
    // Confirm Nickel sees the expanded DOM range before its save/menu path runs.
    if (reinterpret_cast<QString (*)(void *)>(dtpj_selected_text)(book).toUtf8() != expected) {
        expected.clear(); report("Japanese selection refused: native text does not match token"); return false;
    }
    report("Japanese word selection expanded and verified");
    return true;
}

extern "C" bool dtp_remove_failed_visual(void *book, const void *id_qstring) {
    if (!book || !dtpj_web_view || !dtpj_page || !dtpj_frame || !dtpj_remove_visual) return false;
    QPointer<QObject> live_book(reinterpret_cast<QObject *>(book));
    void *view = reinterpret_cast<void *(*)(const void *)>(dtpj_web_view)(book);
    if (!view || !live_book) return false;
    void *page = reinterpret_cast<void *(*)(const void *)>(dtpj_page)(view);
    if (!page || !live_book) return false;
    void *frame = reinterpret_cast<void *(*)(const void *)>(dtpj_frame)(page);
    if (!frame || !live_book) return false;
    // Static native visual-removal path; does not call BookmarkManager deletion.
    reinterpret_cast<void (*)(void *, const QString &)>(dtpj_remove_visual)(frame, *static_cast<const QString *>(id_qstring));
    return true;
}
