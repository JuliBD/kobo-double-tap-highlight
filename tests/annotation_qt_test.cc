/* ARM Qt 5.2.1 adapter smoke test. Nickel operations are explicit test doubles. */
#include "annotation_qt_test.h"
#include "../src/annotations.h"
#include "../src/toggle.h"
#include "../src/japanese.h"
#include "../src/rendering.h"
#include <QApplication>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QResizeEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <cassert>
#include <cstdio>

struct TestBookmark {
    QString id, content, start, end;
    QByteArray text, note;
    int startChild, endChild, startOffset, endOffset;
    bool highlight, noteType, deleted;
};
static unsigned saves, deletes, reports;
static bool save_ok = true;
static unsigned delete_mutation, delete_attempts;
static unsigned highlight_attempts, visual_cleanups;
static bool rendered_visual;
static QByteArray manager_text_override;
extern "C" void nh_log(const char *, ...) {}
extern "C" void dtp_append_report(const char *, const char *) { ++reports; }
extern "C" void dtp_append_recent_report(const char *, const char *) { ++reports; }

static const QString &id_getter(const void *p) { return static_cast<const TestBookmark *>(p)->id; }
static const QString &content_getter(const void *p) { return static_cast<const TestBookmark *>(p)->content; }
static const QString &start_getter(const void *p) { return static_cast<const TestBookmark *>(p)->start; }
static const QString &end_getter(const void *p) { return static_cast<const TestBookmark *>(p)->end; }
static const QByteArray &text_getter(const void *p) { return static_cast<const TestBookmark *>(p)->text; }
static const QByteArray &note_getter(const void *p) { return static_cast<const TestBookmark *>(p)->note; }
static int start_child(const void *p) { return static_cast<const TestBookmark *>(p)->startChild; }
static int end_child(const void *p) { return static_cast<const TestBookmark *>(p)->endChild; }
static int start_offset(const void *p) { return static_cast<const TestBookmark *>(p)->startOffset; }
static int end_offset(const void *p) { return static_cast<const TestBookmark *>(p)->endOffset; }
static bool is_highlight(const void *p) { return static_cast<const TestBookmark *>(p)->highlight; }
static bool is_note(const void *p) { return static_cast<const TestBookmark *>(p)->noteType; }
static bool save(void *p, const QString &) {
    ++saves;
    if (save_ok) static_cast<TestBookmark *>(p)->id = QStringLiteral("12345678-1234-4234-8234-123456789abc");
    return save_ok;
}
static void erase(void *p, const void *) { ++deletes; static_cast<TestBookmark *>(p)->deleted = true; }
static QString current_id(const void *p) { return static_cast<const SelectionMenuController *>(p)->id; }
static QWidget *factory(void *, QWidget *parent, const QString &label) { return new MenuTextItem(label, parent); }
static void add_item(void *v, QWidget *item) { static_cast<SelectionMenuView *>(v)->layout->addWidget(item); }
static void setup(void *p) {
    SelectionMenuController *menu = static_cast<SelectionMenuController *>(p);
    while (menu->view->layout->count()) {
        QLayoutItem *item = menu->view->layout->takeAt(0);
        delete item->widget(); delete item;
    }
    if (!menu->id.isEmpty()) dtp_annotation_bookmark_id(menu->bookmark);
    QString text = menu->id.isEmpty() ? QStringLiteral("Highlight") : QStringLiteral("Remove Highlight");
    QWidget *item = static_cast<QWidget *>(dtp_annotation_create_item(menu, menu->view, &text));
    add_item(menu->view, item);
}

SelectionMenuController::SelectionMenuController(SelectionMenuView *v, TestBookmark *b) : view(v), bookmark(b) {
    QObject::connect(this, &SelectionMenuController::highlight, this, [this]() {
        ++highlight_attempts;
        rendered_visual = true; // Nickel mutates the page before the save callback.
        QString database = QStringLiteral("test-only");
        if (bookmark->id.isEmpty()) bookmark->id = QStringLiteral("12345678-1234-4234-8234-123456789abc");
        if (!manager_text_override.isEmpty()) bookmark->text = manager_text_override;
        dtp_annotation_save(bookmark, &database);
        if (dtpr_add) {
            const QString style(QStringLiteral("test-style"));
            dtp_render_add(bookmark, bookmark, bookmark->startOffset, bookmark->endOffset, 0, false, &style);
        }
        if (selection_owner && dtpt_clear) dtp_toggle_clear(selection_owner);
    });
}
void SelectionMenuController::removeHighlight() {
    ++delete_attempts;
    switch (delete_mutation) {
    case 1: bookmark->note = "new note"; break;
    case 2: bookmark->endOffset++; break;
    case 3: bookmark->text += " changed"; break;
    case 4: bookmark->id = QStringLiteral("ffffffff-ffff-4fff-8fff-ffffffffffff"); break;
    case 5: bookmark->noteType = true; break;
    case 6: bookmark->content += " changed"; break;
    case 7: bookmark->start += " changed"; break;
    case 8: bookmark->endChild++; break;
    default: break;
    }
    dtp_annotation_delete(bookmark, NULL);
    if (delete_mutation == 9) dtp_annotation_delete(bookmark, NULL);
}

static MenuTextItem *action(SelectionMenuView &view, const char *label) {
    const QList<MenuTextItem *> items = view.findChildren<MenuTextItem *>();
    for (int i = 0; i < items.size(); ++i)
        if (items.at(i)->text() == QLatin1String(label)) return items.at(i);
    return NULL;
}
static void tap(MenuTextItem *item) {
    assert(item);
    assert(QMetaObject::invokeMethod(item, "tapped", Qt::DirectConnection, Q_ARG(bool, false)));
}

#include "rendering_qt_cases.inc"
#include "toggle_qt_cases.inc"
#include "annotation_failure_cases.inc"

int main(int argc, char **argv) {
    std::fputs("annotation smoke: application start\n", stderr);
    QApplication app(argc, argv);
    std::fputs("annotation smoke: application ready\n", stderr);
    const QString registry = QStringLiteral("build/native-test/registry");
    QFile::remove(registry);
    dtpa_setup = reinterpret_cast<void *>(setup);
    dtpa_create_item = reinterpret_cast<void *>(factory);
    dtpa_add_item = reinterpret_cast<void *>(add_item);
    dtpa_current_id = reinterpret_cast<void *>(current_id);
    dtpa_get_id = reinterpret_cast<void *>(id_getter);
    dtpa_save = reinterpret_cast<void *>(save);
    dtpa_delete = reinterpret_cast<void *>(erase);
    dtpa_is_highlight = reinterpret_cast<void *>(is_highlight);
    dtpa_is_note = reinterpret_cast<void *>(is_note);
    dtpa_annotation_raw = reinterpret_cast<void *>(note_getter);
    dtpa_text_raw = reinterpret_cast<void *>(text_getter);
    dtpa_content_id = reinterpret_cast<void *>(content_getter);
    dtpa_start_path = reinterpret_cast<void *>(start_getter);
    dtpa_end_path = reinterpret_cast<void *>(end_getter);
    dtpa_start_child = reinterpret_cast<void *>(start_child);
    dtpa_end_child = reinterpret_cast<void *>(end_child);
    dtpa_start_offset = reinterpret_cast<void *>(start_offset);
    dtpa_end_offset = reinterpret_cast<void *>(end_offset);
    dtp_annotations_enable(true);
    dtp_annotation_test_menu_enable(true); // Historical explicit-action backend fixtures.
    std::fputs("annotation smoke: adapter ready\n", stderr);

    TestBookmark bookmark = {QString(),QStringLiteral("test-book"),QStringLiteral("p1"),QStringLiteral("p1"),
                             QByteArray("testword"),QByteArray(),0,0,0,8,true,false,false};
    SelectionMenuView view;
    SelectionMenuController menu(&view, &bookmark);
    dtp_annotation_setup(&menu);
    std::fputs("annotation smoke: save menu ready\n", stderr);
    tap(action(view,"Prototype: save selection"));
    std::fputs("annotation smoke: save dispatched\n", stderr);
    assert(saves == 1);
    tap(action(view,"Prototype: save selection"));
    assert(saves == 1); /* One dispatch per menu, even if tapped twice. */
    QFile file(registry);
    assert(file.open(QIODevice::ReadOnly));
    QByteArray data = file.readAll(); file.close();
    assert(data.contains(bookmark.id.toLatin1()));
    assert(!data.contains("testword") && !data.contains("test-book"));
    /* Independent Python/SQLite fingerprint vector, not a round-trip-only test. */
    assert(data.startsWith("DTP-TEST-HIGHLIGHTS-2\n"));
    assert(data.contains("99381cf0b05db0751a5f5c00fd10a08af023990e5892226f9444095207c7479f"));
    menu.id = bookmark.id;
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: save selection"));
    tap(action(view,"Prototype: remove test highlight"));
    assert(deletes == 1 && bookmark.deleted);
    tap(action(view,"Prototype: remove test highlight"));
    assert(deletes == 1);

    bookmark.note = QByteArray("private note");
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    bookmark.note.clear(); bookmark.noteType = true;
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    bookmark.noteType = false;
    bookmark.endOffset = 40;
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    bookmark.endOffset = 8;

    /* Raw getters are QByteArray references, including multibyte UTF-8. */
    bookmark.text = QByteArray::fromHex("e697a5e69cace8aa9e");
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    bookmark.text = QByteArray("testword");
    bookmark.text += QByteArray(" expanded");
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    bookmark.text = QByteArray("testword");
    const QString owned_id = bookmark.id;
    bookmark.id = menu.id = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    bookmark.id = menu.id = owned_id;
    dtp_annotation_setup(&menu);
    bookmark.note = QByteArray("added after opening the menu");
    tap(action(view,"Prototype: remove test highlight"));
    assert(deletes == 1); /* Fresh native-call guard catches stale UI state. */
    bookmark.note.clear();
    dtp_annotation_setup(&menu);
    bookmark.endOffset = 20;
    tap(action(view,"Prototype: remove test highlight"));
    assert(deletes == 1);
    bookmark.endOffset = 8;

    file.setFileName(registry);
    assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QByteArray legacy = data;
    legacy.replace("DTP-TEST-HIGHLIGHTS-2", "DTP-TEST-HIGHLIGHTS-1");
    file.write(legacy); file.close();
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("malformed registry\n"); file.close();
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));

    /* A failed native save is returned unchanged and never registered. */
    save_ok = false;
    SelectionMenuView failed_view;
    SelectionMenuController failed_menu(&failed_view, &bookmark);
    dtp_annotation_setup(&failed_menu);
    tap(action(failed_view,"Prototype: save selection"));
    assert(saves == 2);
    assert(file.open(QIODevice::ReadOnly));
    assert(file.readAll() == "malformed registry\n"); file.close();
    save_ok = true;

    /* QObject lifetime guard; the menu item can outlive its controller. */
    SelectionMenuView orphan_view;
    SelectionMenuController *orphan = new SelectionMenuController(&orphan_view, &bookmark);
    dtp_annotation_setup(orphan);
    MenuTextItem *save_item = action(orphan_view,"Prototype: save selection");
    assert(save_item); delete orphan; tap(save_item); assert(saves == 2);

    /* Ordinary native operations remain pass-through, even on note records. */
    bookmark.note = QByteArray("native user operation");
    dtp_annotation_delete(&bookmark,NULL);
    assert(deletes == 2);

    /* Independent UTF-8 fingerprint vector through the real custom save scope. */
    TestBookmark utf8 = bookmark;
    utf8.id.clear(); utf8.note.clear();
    utf8.text = QByteArray::fromHex("e697a5e69cace8aa9e");
    SelectionMenuView utf8_view;
    SelectionMenuController utf8_menu(&utf8_view, &utf8);
    dtp_annotation_setup(&utf8_menu);
    tap(action(utf8_view,"Prototype: save selection"));
    assert(file.open(QIODevice::ReadOnly));
    data = file.readAll(); file.close();
    assert(data.contains("a7cf4d7480a93336252886d377d9cf5b9f0eace2a6bd0a3f19c6fa42f8986273"));
    utf8_menu.id = utf8.id;
    dtp_annotation_setup(&utf8_menu);
    assert(action(utf8_view,"Prototype: remove test highlight"));
    dtp_annotations_enable(false);
    dtp_annotation_setup(&menu);
    assert(!action(view,"Prototype: remove test highlight"));
    QFile::remove(registry);
    dtp_annotation_test_menu_enable(false); // Actual device menu behavior.
    run_toggle_cases();
    dtp_annotation_test_menu_enable(true);
    run_annotation_failure_cases();
    run_rendering_cases();
    run_old_highlight_cases();
    run_longpress_cases();
    std::puts("PASS: ARM Qt 5.2.1 menu signals, native save/delete dispatch, persistent registry, note/range/text/ownership guards, stale UI guards, QObject lifetime, ordinary pass-through");
    return 0;
}
#include "annotation-qt-test.moc"
