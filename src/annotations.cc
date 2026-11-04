/* Native menu integration for an explicitly selected test range. */
#include "annotations.h"
#include "report.h"
#include "japanese.h"
#include "settings.h"
#include <NickelHook.h>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QMap>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QStringList>
#include <QThread>
#include <QVariant>
#include <QWidget>

#ifndef DTP_ANNOTATION_REGISTRY
#define DTP_ANNOTATION_REGISTRY "/mnt/onboard/.kobo/doubletap-test-highlights"
#endif
#ifndef DTP_ANNOTATION_REPORT
#define DTP_ANNOTATION_REPORT "/mnt/onboard/.kobo/doubletap-annotations.log"
#endif

extern "C" {
void *dtpa_setup, *dtpa_create_item, *dtpa_add_item, *dtpa_current_id;
void *dtpa_get_id, *dtpa_save, *dtpa_delete;
void *dtpa_is_highlight, *dtpa_is_note, *dtpa_annotation_raw, *dtpa_text_raw;
void *dtpa_content_id, *dtpa_start_path, *dtpa_end_path;
void *dtpa_start_child, *dtpa_end_child, *dtpa_start_offset, *dtpa_end_offset;
}

namespace {
typedef const QString &(*StringGetter)(const void *);
typedef const QByteArray &(*BytesGetter)(const void *);
typedef int (*IntGetter)(const void *);
typedef bool (*BoolGetter)(const void *);
typedef QWidget *(*ItemFactory)(void *, QWidget *, const QString &);
typedef QMap<QString, QByteArray> Registry;

bool annotation_enabled;
#ifdef DTP_TEST_MANUAL_MENU
bool test_manual_menu;
#endif
bool manual_menu_enabled() {
#ifdef DTP_TEST_MANUAL_MENU
    return test_manual_menu;
#else
    return false;
#endif
}
unsigned report_records;
struct BuildScope { void *controller; bool injected; QString id; QByteArray hash; bool plain; };
struct SaveScope { QString tracked_id; bool automatic; QString candidate_id; QByteArray candidate_hash, expected_word; QStringList failed_ids; SaveScope() : automatic(false) {} };
struct DeleteScope { QString id; unsigned forwarded, blocked; bool automatic; QByteArray expected_word, expected_hash; };
static __thread BuildScope *building;
static __thread SaveScope *saving;
static __thread DeleteScope *deleting;

template <typename T> class ScopeGuard {
    T *&target;
    T *previous;
public:
    ScopeGuard(T *&slot, T *value) : target(slot), previous(slot) { target = value; }
    ~ScopeGuard() { target = previous; }
};

void report(const char *message) {
    nh_log("%s", message);
    if (report_records++ < 64)
        dtp_append_report(DTP_ANNOTATION_REPORT, message);
}

bool valid_id(const QString &id) {
    if (id.size() != 36) return false;
    for (int i = 0; i < id.size(); ++i) {
        const ushort c = id.at(i).unicode();
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

Registry read_registry() {
    Registry result;
    QFile file(QLatin1String(DTP_ANNOTATION_REGISTRY));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 512 * 1024) return result;
    if (file.readLine().trimmed() != "DTP-TEST-HIGHLIGHTS-2") return result;
    while (!file.atEnd()) {
        const QList<QByteArray> fields = file.readLine(256).trimmed().split('\t');
        if (fields.size() != 2) return Registry();
        const QString id = QString::fromLatin1(fields.at(0));
        const QByteArray hash = fields.at(1);
        if (!valid_id(id) || hash.size() != 64 || result.contains(id)) return Registry();
        for (int i = 0; i < hash.size(); ++i) {
            const char c = hash.at(i);
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return Registry();
        }
        result.insert(id, hash);
        if (result.size() > DTP_REGISTRY_LIMIT) return Registry();
    }
    return result;
}

bool remember(const QString &id, const QByteArray &hash) {
    if (!valid_id(id) || hash.size() != 64) return false;
    Registry entries = read_registry();
    if (entries.size() >= DTP_REGISTRY_LIMIT && !entries.contains(id)) return false;
    entries.insert(id, hash);
    QSaveFile file(QLatin1String(DTP_ANNOTATION_REGISTRY));
    if (!file.open(QIODevice::WriteOnly)) return false;
    QByteArray data("DTP-TEST-HIGHLIGHTS-2\n");
    for (Registry::const_iterator it = entries.constBegin(); it != entries.constEnd(); ++it)
        data += it.key().toLatin1() + '\t' + it.value() + '\n';
    return file.write(data) == data.size() && file.commit();
}

void hash_part(QCryptographicHash &hash, const QByteArray &part) {
    hash.addData(QByteArray::number(part.size()) + ':' + part);
}

QByteArray fingerprint(const void *bookmark) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    void *strings[] = {dtpa_content_id, dtpa_start_path, dtpa_end_path};
    for (unsigned i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        const QString &value = reinterpret_cast<StringGetter>(strings[i])(bookmark);
        if (value.size() > 8192) return QByteArray();
        hash_part(hash, value.toUtf8());
    }
    const QByteArray &text = reinterpret_cast<BytesGetter>(dtpa_text_raw)(bookmark);
    if (text.size() > 32768) return QByteArray();
    hash_part(hash, text);
    void *integers[] = {dtpa_start_child, dtpa_start_offset, dtpa_end_child, dtpa_end_offset};
    for (unsigned i = 0; i < sizeof(integers) / sizeof(integers[0]); ++i)
        hash_part(hash, QByteArray::number(reinterpret_cast<IntGetter>(integers[i])(bookmark)));
    return hash.result().toHex();
}

bool plain_highlight(const void *bookmark) {
    return reinterpret_cast<BoolGetter>(dtpa_is_highlight)(bookmark) &&
           !reinterpret_cast<BoolGetter>(dtpa_is_note)(bookmark) &&
           reinterpret_cast<BytesGetter>(dtpa_annotation_raw)(bookmark).isEmpty();
}

bool word_text(const QByteArray &raw) {
    if (raw.isEmpty() || raw.size() > 512) return false;
    const QString text = QString::fromUtf8(raw.constData(), raw.size());
    if (text.toUtf8() != raw || text.size() > 128) return false;
    bool letter = false, japanese = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        const ushort u = c.unicode();
        japanese = japanese || (u >= 0x3040 && u <= 0x30ff) || (u >= 0x3400 && u <= 0x9fff) || (u >= 0xff66 && u <= 0xff9f);
        if (!c.isLetterOrNumber() && !c.isMark() && u != '\'' && u != '-' && u != 0x2019) return false;
        letter = letter || c.isLetterOrNumber();
    }
    return letter && (!japanese || text.size() <= 24);
}

bool single_word(const void *bookmark) { return word_text(reinterpret_cast<BytesGetter>(dtpa_text_raw)(bookmark)); }

QString current_id(const void *controller) {
    return reinterpret_cast<QString (*)(const void *)>(dtpa_current_id)(controller);
}

bool gui_thread() {
    return QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread();
}

void add_action(void *controller, QWidget *view, bool remove, const QString &expected_id) {
    QWidget *item = reinterpret_cast<ItemFactory>(dtpa_create_item)(controller, view,
        remove ? QStringLiteral("Prototype: remove test highlight") : QStringLiteral("Prototype: save selection"));
    if (!item) return;
    QPushButton *adapter = new QPushButton(item);
    adapter->hide();
    if (!QObject::connect(item, SIGNAL(tapped(bool)), adapter, SIGNAL(pressed()))) {
        item->deleteLater(); adapter->deleteLater(); report("annotation action connection failed"); return;
    }
    QPointer<QObject> menu(reinterpret_cast<QObject *>(controller));
    QObject::connect(adapter, &QPushButton::pressed, adapter, [menu, expected_id, remove]() {
        if (!annotation_enabled || !menu || !gui_thread()) return;
        const QString id = current_id(menu.data());
        if (remove) {
            if (menu->property("dtp_delete_dispatched").toBool()) return;
            if (id != expected_id || !menu->property("dtp_plain").toBool() ||
                menu->property("dtp_id").toString() != id ||
                read_registry().value(id) != menu->property("dtp_hash").toByteArray()) {
                report("annotation remove refused: stale or unowned selection"); return;
            }
            menu->setProperty("dtp_delete_dispatched", true);
            DeleteScope scope = {id, 0, 0, false, QByteArray(), menu->property("dtp_hash").toByteArray()};
            ScopeGuard<DeleteScope> guard(deleting, &scope);
            const bool invoked = QMetaObject::invokeMethod(menu.data(), "removeHighlight", Qt::DirectConnection);
            report(!invoked ? "annotation remove method unavailable" :
                   scope.blocked ? "annotation remove blocked: protected or changed record; reopen book if view changed" :
                   scope.forwarded ? "annotation remove dispatched through native manager" :
                   "annotation remove invoked; no native deletion observed");
        } else {
            if (menu->property("dtp_save_dispatched").toBool()) return;
            if (!id.isEmpty()) { report("annotation save refused: selection already has an annotation"); return; }
            menu->setProperty("dtp_save_dispatched", true);
            SaveScope scope;
            ScopeGuard<SaveScope> guard(saving, &scope);
            const bool invoked = QMetaObject::invokeMethod(menu.data(), "highlight", Qt::DirectConnection);
            report(!invoked ? "annotation highlight signal unavailable" :
                   scope.tracked_id.isEmpty() ? "annotation save invoked; no plain saved record tracked" :
                   "annotation save completed and test record registered");
        }
    });
    reinterpret_cast<void (*)(void *, QWidget *)>(dtpa_add_item)(view, item);
    report(remove ? "annotation menu: test-remove action added" : "annotation menu: test-save action added");
}
}

extern "C" void dtp_annotations_enable(bool enable) {
    annotation_enabled = enable && dtpa_setup && dtpa_create_item && dtpa_add_item && dtpa_current_id &&
        dtpa_get_id && dtpa_save && dtpa_delete && dtpa_is_highlight && dtpa_is_note &&
        dtpa_annotation_raw && dtpa_text_raw && dtpa_content_id && dtpa_start_path && dtpa_end_path &&
        dtpa_start_child && dtpa_end_child && dtpa_start_offset && dtpa_end_offset;
    report_records = 0;
    report(annotation_enabled ? "annotation prototype enabled" : "annotation prototype inactive");
}

#ifdef DTP_TEST_MANUAL_MENU
// Historical backend fixtures only. No switch or manual actions in device builds.
extern "C" void dtp_annotation_test_menu_enable(bool enable) { test_manual_menu = enable; }
#endif

extern "C" bool dtp_annotations_ready(void) { return annotation_enabled; }
extern "C" bool dtp_annotation_owned(const void *bookmark) {
    if (!annotation_enabled || !bookmark || !plain_highlight(bookmark)) return false;
    const QString &id = reinterpret_cast<StringGetter>(dtpa_get_id)(bookmark);
    const QByteArray expected = read_registry().value(id);
    return !expected.isEmpty() && expected == fingerprint(bookmark);
}

extern "C" bool dtp_annotation_has_current(void *controller) {
    return annotation_enabled && controller && gui_thread() && !current_id(controller).isEmpty();
}

extern "C" bool dtp_annotation_auto_action(void *controller, const char *expected_utf8, void *book) {
    if (!annotation_enabled || !controller || !gui_thread()) return false;
    QPointer<QObject> menu(reinterpret_cast<QObject *>(controller));
    dtp_annotation_setup(controller);
    if (!menu) return false;
    const QString id = current_id(controller);
    if (id.isEmpty()) {
        // Nickel changes its page markup before it reaches BookmarkManager::save.
        // Refuse known failures before invoking that mutating native signal.
        if (!expected_utf8 || !word_text(QByteArray(expected_utf8)) || read_registry().size() >= DTP_REGISTRY_LIMIT) {
            report("double-tap save refused before display change: invalid word or full registry"); return false;
        }
        if (!book || !dtpj_web_view || !dtpj_page || !dtpj_frame || !dtpj_remove_visual) {
            report("double-tap save refused before display change: visual rollback unavailable"); return false;
        }
        QPointer<QObject> live_book(reinterpret_cast<QObject *>(book));
        SaveScope scope;
        scope.automatic = true;
        if (expected_utf8) scope.expected_word = QByteArray(expected_utf8);
        ScopeGuard<SaveScope> guard(saving, &scope);
        const bool invoked = QMetaObject::invokeMethod(menu.data(), "highlight", Qt::DirectConnection);
        for (int i = 0; i < scope.failed_ids.size(); ++i) {
            if (scope.failed_ids.at(i) == scope.tracked_id) continue;
            const bool cleared = dtp_remove_failed_visual(live_book.data(), &scope.failed_ids.at(i));
            report(cleared ? "rejected native highlight visual cleanup dispatched" : "rejected native highlight: reopen book to clear display");
        }
        const bool ok = invoked && !scope.tracked_id.isEmpty();
        report(ok ? "double-tap highlight saved and registered" : "double-tap save refused or no synchronous save observed");
        return ok;
    }
    // Native hit testing supplies the existing highlight ID. Deletion is not
    // restricted to our registry: the user may remove any note-free passage.
    const QByteArray snapshot = menu->property("dtp_hash").toByteArray();
    if (!valid_id(id) || !menu->property("dtp_plain").toBool() ||
        menu->property("dtp_id").toString() != id || snapshot.size() != 64) {
        report("double-tap remove refused: invalid or annotated selection"); return false;
    }
    DeleteScope scope = {id, 0, 0, true, QByteArray(), snapshot};
    ScopeGuard<DeleteScope> guard(deleting, &scope);
    const bool invoked = QMetaObject::invokeMethod(menu.data(), "removeHighlight", Qt::DirectConnection);
    const bool ok = invoked && scope.forwarded && !scope.blocked;
    report(ok ? "double-tap highlight removed through native manager" : "double-tap remove refused or no synchronous deletion observed");
    return ok;
}

extern "C" __attribute__((visibility("default"))) void dtp_annotation_setup(void *controller) {
    if (!annotation_enabled || !gui_thread()) {
        reinterpret_cast<void (*)(void *)>(dtpa_setup)(controller); return;
    }
    QObject *object = reinterpret_cast<QObject *>(controller);
    if (!object->inherits("SelectionMenuController")) {
        reinterpret_cast<void (*)(void *)>(dtpa_setup)(controller); return;
    }
    BuildScope scope = {controller, false, QString(), QByteArray(), false};
    ScopeGuard<BuildScope> guard(building, &scope);
    object->setProperty("dtp_plain", false);
    object->setProperty("dtp_word", false);
    object->setProperty("dtp_text", QByteArray());
    object->setProperty("dtp_id", QString());
    object->setProperty("dtp_hash", QByteArray());
    object->setProperty("dtp_save_dispatched", false);
    object->setProperty("dtp_delete_dispatched", false);
    reinterpret_cast<void (*)(void *)>(dtpa_setup)(controller);
    const QString id = current_id(controller);
    if (manual_menu_enabled() && !id.isEmpty() && !scope.injected) {
        report(scope.id != id ? "annotation remove withheld: bookmark not observed during menu build" :
               !scope.plain ? "annotation remove withheld: selection is not a note-free highlight" :
               !read_registry().contains(id) ? "annotation remove withheld: record not registered with current fingerprint format" :
               read_registry().value(id) != scope.hash ? "annotation remove withheld: saved range fingerprint differs" :
               "annotation remove withheld: native option or method not recognized");
    }
}

extern "C" __attribute__((visibility("default"))) void *dtp_annotation_create_item(
        void *controller, void *parent, const void *label) {
    QWidget *view = static_cast<QWidget *>(parent);
    const QString &text = *static_cast<const QString *>(label);
    QWidget *item = reinterpret_cast<ItemFactory>(dtpa_create_item)(controller, view, text);
    if (!manual_menu_enabled() || !annotation_enabled || !building || building->controller != controller || building->injected ||
        !view || !view->inherits("SelectionMenuView")) return item;
    const QString id = current_id(controller);
    const bool save = id.isEmpty() && text == QCoreApplication::translate("SelectionMenuController", "Highlight");
    const bool remove = !id.isEmpty() && id == building->id && building->plain &&
        building->hash.size() == 64 && read_registry().value(id) == building->hash &&
        text == QCoreApplication::translate("SelectionMenuController", "Remove Highlight");
    QObject *menu = reinterpret_cast<QObject *>(controller);
    if ((save && menu->metaObject()->indexOfSignal("highlight()") < 0) ||
        (remove && menu->metaObject()->indexOfSlot("removeHighlight()") < 0)) return item;
    if (save || remove) {
        building->injected = true;
        add_action(controller, view, remove, id);
    }
    return item;
}

extern "C" __attribute__((visibility("default"))) const void *dtp_annotation_bookmark_id(const void *bookmark) {
    const QString &id = reinterpret_cast<StringGetter>(dtpa_get_id)(bookmark);
    if (annotation_enabled && building && !id.isEmpty()) {
        building->id = id;
        building->plain = plain_highlight(bookmark);
        building->hash = fingerprint(bookmark);
        QObject *menu = reinterpret_cast<QObject *>(building->controller);
        menu->setProperty("dtp_id", id);
        menu->setProperty("dtp_plain", building->plain);
        menu->setProperty("dtp_word", single_word(bookmark));
        menu->setProperty("dtp_text", reinterpret_cast<BytesGetter>(dtpa_text_raw)(bookmark));
        menu->setProperty("dtp_hash", building->hash);
    }
    return &id;
}

extern "C" __attribute__((visibility("default"))) bool dtp_annotation_save(void *bookmark, const void *database) {
    if (annotation_enabled && saving && saving->automatic) {
        const QString id = reinterpret_cast<StringGetter>(dtpa_get_id)(bookmark);
        const QByteArray hash = fingerprint(bookmark);
        if (!plain_highlight(bookmark) || !single_word(bookmark) || hash.isEmpty() || !valid_id(id) ||
            (!saving->expected_word.isEmpty() && reinterpret_cast<BytesGetter>(dtpa_text_raw)(bookmark) != saving->expected_word) ||
            (read_registry().size() >= DTP_REGISTRY_LIMIT && !read_registry().contains(id)) ||
            (!saving->candidate_id.isEmpty() && (saving->candidate_id != id || saving->candidate_hash != hash))) {
            if (valid_id(id) && saving->failed_ids.size() < 8 && !saving->failed_ids.contains(id)) saving->failed_ids.append(id);
            report("double-tap native save blocked: invalid word, batch change, or full registry"); return false;
        }
        saving->candidate_id = id;
        saving->candidate_hash = hash;
    }
    const bool ok = reinterpret_cast<bool (*)(void *, const QString &)>(dtpa_save)(
        bookmark, *static_cast<const QString *>(database));
    if (!ok && annotation_enabled && saving && saving->automatic) {
        const QString id = reinterpret_cast<StringGetter>(dtpa_get_id)(bookmark);
        if (valid_id(id) && saving->failed_ids.size() < 8 && !saving->failed_ids.contains(id)) saving->failed_ids.append(id);
    }
    if (annotation_enabled && saving && ok && plain_highlight(bookmark)) {
        const QString &id = reinterpret_cast<StringGetter>(dtpa_get_id)(bookmark);
        if ((saving->tracked_id.isEmpty() || saving->tracked_id == id) && remember(id, fingerprint(bookmark)))
            saving->tracked_id = id;
    }
    // Manually saved Japanese words need the same zero-offset rendering fix
    // as quick highlights. Register only successful, note-free bounded words;
    // this changes the local fingerprint registry, never the saved annotation.
    if (annotation_enabled && !saving && ok && gui_thread() &&
        dtp_settings_longpress() && plain_highlight(bookmark) && single_word(bookmark)) {
        const QString text=QString::fromUtf8(reinterpret_cast<BytesGetter>(dtpa_text_raw)(bookmark));
        bool japanese=false;
        for (int i=0; i<text.size(); ++i) {
            const ushort c=text.at(i).unicode();
            japanese=japanese || (c>=0x3040 && c<=0x30ff) || (c>=0x3400 && c<=0x9fff) || (c>=0xff66 && c<=0xff9f);
        }
        if (japanese) remember(reinterpret_cast<StringGetter>(dtpa_get_id)(bookmark),fingerprint(bookmark));
    }
    return ok;
}

extern "C" __attribute__((visibility("default"))) void dtp_annotation_delete(void *bookmark, const void *device) {
    if (annotation_enabled && saving && saving->automatic) {
        report("double-tap native deletion blocked during save"); return;
    }
    if (annotation_enabled && deleting) {
        const QString &id = reinterpret_cast<StringGetter>(dtpa_get_id)(bookmark);
        const QByteArray expected = deleting->automatic ? deleting->expected_hash : read_registry().value(id);
        if (deleting->forwarded || id != deleting->id || expected.isEmpty() || !plain_highlight(bookmark) ||
            (!deleting->expected_word.isEmpty() && reinterpret_cast<BytesGetter>(dtpa_text_raw)(bookmark) != deleting->expected_word) ||
            expected != fingerprint(bookmark)) {
            ++deleting->blocked;
            return;
        }
        ++deleting->forwarded;
    }
    reinterpret_cast<void (*)(void *, const void *)>(dtpa_delete)(bookmark, device);
}
