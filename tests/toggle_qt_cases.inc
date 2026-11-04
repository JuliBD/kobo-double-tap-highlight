/* Uses real ARM Qt objects and production adapters; native operations are stubs. */
#include <QEvent>
#include <QKeyEvent>
#include <QTouchEvent>
#include "../src/japanese_backend.h"
#include "../src/settings.h"
static QPointer<QObject> watched_widget;
static void *live_tap;
static bool allow_value, selectable = true;
static unsigned starts, completions, cleanups, page_calls;
static TestBookmark *active_bookmark;
static unsigned japanese_mode;
extern int dtp_test_sudachi_failure;
static void *visual_getter(const void *p) { return const_cast<void *>(p); }
static void visual_remove(void *, const QString &id) {
    assert(id == QStringLiteral("12345678-1234-4234-8234-123456789abc"));
    rendered_visual = false; ++visual_cleanups;
}
static QString native_selected_text(void *) { return QString::fromUtf8(active_bookmark->text); }
static QVariant native_japanese_evaluate(void *, QString script) {
#if DTP_USE_SUDACHI
    assert(!script.contains(QStringLiteral("function TinySegmenter")));
#else
    assert(script.contains(QStringLiteral("function TinySegmenter")));
#endif
    if (japanese_mode == 1) return QStringLiteral("{\"status\":\"reject\"}");
    if (japanese_mode == 3) return QStringLiteral("invalid JSON");
    if (japanese_mode == 4) { delete watched_widget.data(); return QStringLiteral("{\"status\":\"expanded\",\"word\":\"高校\"}"); }
    if (script.endsWith(QStringLiteral("})({\"mode\":\"context\"})"))) {
        const QString initial=QString::fromUtf8(active_bookmark->text);
        QJsonObject context;
        context.insert(QStringLiteral("status"),QStringLiteral("context"));
        context.insert(QStringLiteral("text"),QStringLiteral("今日は高校に通う。"));
        context.insert(QStringLiteral("start"),3); context.insert(QStringLiteral("end"),3+initial.size());
        context.insert(QStringLiteral("initial"),initial);
        return QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact));
    }
    if (japanese_mode != 2) active_bookmark->text = QStringLiteral("高校").toUtf8();
    return QStringLiteral("{\"status\":\"expanded\",\"word\":\"高校\"}");
}
static QObject *watched(const void *mixin) {
    assert(mixin == static_cast<char *>(live_tap) + 8);
    return watched_widget.data();
}
static void allow_native(void *tap_pointer, bool allow) {
    assert(tap_pointer == live_tap); allow_value = allow;
}
static void set_current(void *p, const void *id) {
    static_cast<SelectionMenuController *>(p)->id = *static_cast<const QString *>(id);
}
static bool selection_mode(const void *p) { return static_cast<const SelectionMenuController *>(p)->mode; }
static bool menu_visible(void *) { return false; }
static bool only_images(const void *) { return false; }
static bool selection_start(void *p, void *t) {
    assert(t == live_tap); ++starts;
    dtp_render_a2(p,true,true);
    static_cast<SelectionMenuController *>(p)->mode = true;
    return selectable;
}
static void selection_complete(void *p) {
    ++completions;
    SelectionMenuController *selection = static_cast<SelectionMenuController *>(p);
    if (selection->native_menu) {
        delete selection->native_menu->view;
        delete selection->native_menu;
        selection->native_menu = NULL;
    }
    SelectionMenuView *view = new SelectionMenuView;
    view->setParent(static_cast<QWidget *>(selection->parent()));
    SelectionMenuController *menu = new SelectionMenuController(view, selection->bookmark);
    menu->setParent(selection->parent());
    menu->selection_owner = selection;
    selection->native_menu = menu;
    dtp_toggle_set_current(menu, &selection->bookmark->id);
}
static void selection_clear(void *p) {
    dtp_render_repaint(p); // Native clearSelection emits the forced-refresh signal.
    ++cleanups; static_cast<SelectionMenuController *>(p)->mode = false;
}
static void selection_off(void *p) {
    dtp_render_a2(p,false,false);
    dtp_render_close_footer(p);
    SelectionMenuController *selection = static_cast<SelectionMenuController *>(p);
    if (selection->native_menu) {
        delete selection->native_menu->view;
        delete selection->native_menu;
        selection->native_menu = NULL;
    }
    selection->mode = false;
}
static void page_native(void *) { ++page_calls; }
static void volume_native(void *, const void *, const void *) { ++page_calls; }
static dtp_sample begin_pair(ReadingView &reader, void *tap_pointer) {
    dtp_toggle_allow(tap_pointer, false);
    assert(allow_value);
    dtp_sample sample = {0, 0, 0, false, false, {0, 0}};
    sample.context = reinterpret_cast<uintptr_t>(&reader);
    sample.ms = 1000; sample.state = 1;
    assert(!dtp_toggle_process(&reader, tap_pointer, &sample));
    sample.ms = 1200; sample.state = 3; sample.native_double = true;
    return sample;
}
static void run_toggle_cases() {
    dtp_annotations_enable(true);
    initialize_rendering();
    QFile::remove(QStringLiteral("build/native-test/registry"));
    dtpt_allow = reinterpret_cast<void *>(allow_native);
    dtpt_watched = reinterpret_cast<void *>(watched);
    dtpt_set_current = reinterpret_cast<void *>(set_current);
    dtpt_start = reinterpret_cast<void *>(selection_start);
    dtpt_process_selection = reinterpret_cast<void *>(selection_complete);
    dtpt_mode = reinterpret_cast<void *>(selection_mode);
    dtpt_menu_visible = reinterpret_cast<void *>(menu_visible);
    dtpt_images = reinterpret_cast<void *>(only_images);
    dtpt_clear = reinterpret_cast<void *>(selection_clear);
    dtpt_mode_off = reinterpret_cast<void *>(selection_off);
    dtpt_page = dtpt_close = reinterpret_cast<void *>(page_native);
    dtpt_volume = reinterpret_cast<void *>(volume_native);
    dtpj_selected_text = reinterpret_cast<void *>(native_selected_text);
    dtpj_evaluate = reinterpret_cast<void *>(native_japanese_evaluate);
    dtpj_web_view = dtpj_page = dtpj_frame = reinterpret_cast<void *>(visual_getter);
    dtpj_remove_visual = reinterpret_cast<void *>(visual_remove);
    dtp_toggle_enable(true);

    ReadingView reader;
    KepubBookReader book(&reader);
    reader.resize(600, 800); book.resize(600, 800);
    SelectionMenuView ordinary_view;
    TestBookmark bookmark = {QString(),QStringLiteral("test-book"),QStringLiteral("p1"),QStringLiteral("p1"),
                             QByteArray("testword"),QByteArray(),0,0,0,8,true,false,false};
    active_bookmark = &bookmark;
    SelectionController selection(&ordinary_view, &bookmark);
    dtp_annotation_setup(&selection);
    assert(action(ordinary_view, "Highlight"));
    assert(!action(ordinary_view, "Prototype: save selection"));
    assert(!action(ordinary_view, "Prototype: remove test highlight"));
    selection.setParent(&reader);
    reader.show(); book.show(); QApplication::processEvents();
    watched_widget = &book;
    char gesture_storage[16] = {};
    live_tap = gesture_storage;
    unsigned saved = saves, removed = deletes;
    QCoreApplication::sendPostedEvents(&reader, QEvent::UpdateRequest);
    unsigned updates_before = reader.update_events;
    dtp_sample sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1 && !bookmark.id.isEmpty());
    {
        SelectionMenuView saved_view;
        SelectionMenuController saved_menu(&saved_view, &bookmark);
        saved_menu.id = bookmark.id;
        dtp_annotation_setup(&saved_menu);
        assert(action(saved_view, "Remove Highlight"));
        assert(!action(saved_view, "Prototype: save selection"));
        assert(!action(saved_view, "Prototype: remove test highlight"));
    }
    assert(starts == 1 && completions == 1 && cleanups == 1 && !selection.mode);
    assert(a2_calls == 0 && footer_calls == 0);
    assert(forced_refresh_calls == 0);
    assert(reader.update_events == updates_before); // No forced early paint.
    // Repeated completion callbacks from the old gesture must not erase the repaint.
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1 && starts == 1);
    // Qt may deliver the triggering raw touch after the native double completion.
    // Use a real event on a plain receiver to exercise the application filter.
    QObject touch_receiver;
    QTouchEvent triggering_touch(QEvent::TouchBegin);
    QCoreApplication::sendEvent(&touch_receiver, &triggering_touch);
    QCoreApplication::postEvent(&reader, new QEvent(QEvent::UpdateRequest));
    QCoreApplication::sendPostedEvents(&reader, QEvent::UpdateRequest);
    assert(reader.update_events == updates_before + 1 && reader.update_partial);
    assert(!reader.testAttribute(static_cast<Qt::WidgetAttribute>(130)));
    QCoreApplication::postEvent(&reader, new QEvent(QEvent::UpdateRequest));
    QCoreApplication::sendPostedEvents(&reader, QEvent::UpdateRequest);
    assert(reader.update_events == updates_before + 2 && !reader.update_partial); // One-shot.
    // Ordinary selection/controller calls outside an automatic toggle retain UI behavior.
    dtp_render_a2(&selection,true,true); dtp_render_a2(&selection,false,false); dtp_render_close_footer(&selection);
    assert(a2_calls == 2 && footer_calls == 1);
    dtp_render_repaint(&selection);
    assert(forced_refresh_calls == 1); // Same controller outside our operation stays native.
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(deletes == removed + 1 && bookmark.deleted && !selection.mode);
    assert(forced_refresh_calls == 1); // Automatic removal also skips forced refresh.
    // A native full-refresh request after the toggle cancels pending partial delivery.
    dtp_render_force_full(&reader);
    QCoreApplication::postEvent(&reader, new QEvent(QEvent::UpdateRequest));
    QCoreApplication::sendPostedEvents(&reader, QEvent::UpdateRequest);
    assert(!reader.update_partial);

    /* Notes are protected; older note-free ranges can be removed. */
    bookmark.note = QByteArray("protected note");
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(deletes == removed + 1);
    // New user input cancels pending treatment before its repaint.
    QKeyEvent new_input(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&reader, &new_input);
    QCoreApplication::postEvent(&reader, new QEvent(QEvent::UpdateRequest));
    QCoreApplication::sendPostedEvents(&reader, QEvent::UpdateRequest);
    assert(!reader.update_partial);
    bookmark.note.clear(); bookmark.endOffset = 99;
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(deletes == removed + 2);
    // A genuinely new recognized gesture still cancels the pending hint.
    dtp_sample new_start = sample;
    new_start.state = 1; new_start.native_double = false;
    assert(!dtp_toggle_process(&reader, live_tap, &new_start));
    QCoreApplication::postEvent(&reader, new QEvent(QEvent::UpdateRequest));
    QCoreApplication::sendPostedEvents(&reader, QEvent::UpdateRequest);
    assert(!reader.update_partial);
    bookmark.endOffset = 8;
    bookmark.id = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(deletes == removed + 3);

    /* Reject a multiword selection before any native database save is called. */
    bookmark.id.clear(); bookmark.text = QByteArray("two words");
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1);
    bookmark.id.clear(); bookmark.text = QByteArray("testword");

    const QByteArray invalid_words[] = {QByteArray("line\nbreak"), QByteArray("\tword"), QByteArray(),
                                      QByteArray("..."), QByteArray(513, 'x'), QByteArray::fromHex("ff")};
    for (unsigned i = 0; i < sizeof(invalid_words) / sizeof(invalid_words[0]); ++i) {
        bookmark.id.clear(); bookmark.text = invalid_words[i];
        sample = begin_pair(reader, live_tap);
        assert(dtp_toggle_process(&reader, live_tap, &sample));
        assert(saves == saved + 1);
    }
    bookmark.id.clear(); bookmark.text = QByteArray("testword");

    /* Normal singles pass through, and lifecycle changes cancel pending pairs. */
    sample = begin_pair(reader, live_tap);
    sample.native_double = false;
    assert(!dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1);
    sample = begin_pair(reader, live_tap);
    dtp_toggle_page(&reader);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1 && page_calls == 1);
    sample = begin_pair(reader, live_tap);
    dtp_toggle_volume(&reader, NULL, NULL);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1 && page_calls == 2);
    sample = begin_pair(reader, live_tap);
    // Send a real typed resize below; a bare QEvent would be invalid for QWidget.
    QResizeEvent rotation(QSize(800,600), QSize(600,800));
    QApplication::sendEvent(&reader, &rotation);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1);
    sample = begin_pair(reader, live_tap);
    sample.held = true; sample.native_double = false; sample.state = 2;
    assert(!dtp_toggle_process(&reader, live_tap, &sample));
    sample.held = false; sample.native_double = true; sample.state = 3;
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1);
    selectable = false;
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == saved + 1 && !selection.mode);
    selectable = true;

    /* An existing multiword passage is removed in its entirety. */
    bookmark.id.clear(); bookmark.text = QByteArray("two words");
    SelectionMenuView manual_view;
    SelectionMenuController manual_menu(&manual_view, &bookmark);
    dtp_annotation_setup(&manual_menu);
    dtp_annotation_test_menu_enable(true);
    dtp_annotation_setup(&manual_menu);
    tap(action(manual_view, "Prototype: save selection"));
    dtp_annotation_test_menu_enable(false);
    assert(saves == saved + 2);
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(deletes == removed + 4);

    /* Japanese expansion must agree with native selected text and manager text. */
    const unsigned japanese_saved = saves, japanese_removed = deletes;
    bookmark.id.clear(); bookmark.text = QStringLiteral("高").toUtf8();
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == japanese_saved + 1 && bookmark.text == QStringLiteral("高校").toUtf8());
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(deletes == japanese_removed + 1);
    for (japanese_mode = 1; japanese_mode <= 3; ++japanese_mode) {
        bookmark.id.clear(); bookmark.text = QStringLiteral("高").toUtf8();
        sample = begin_pair(reader, live_tap);
        assert(dtp_toggle_process(&reader, live_tap, &sample));
        assert(saves == japanese_saved + 1 && !selection.mode);
    }
    japanese_mode = 0;
#if DTP_USE_SUDACHI
    dtp_test_sudachi_failure = 1;
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == japanese_saved + 1); // No implicit Tiny fallback on dictionary failure.
    dtp_test_sudachi_failure = 0;
#endif
    dtpj_evaluate = NULL;
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(saves == japanese_saved + 1);
    dtpj_evaluate = reinterpret_cast<void *>(native_japanese_evaluate);

    /* An older single-character highlight is removable without re-tokenizing. */
    SelectionMenuView character_view;
    SelectionMenuController character_menu(&character_view, &bookmark);
    bookmark.id.clear(); bookmark.text = QStringLiteral("高").toUtf8();
    dtp_annotation_setup(&character_menu);
    dtp_annotation_test_menu_enable(true);
    dtp_annotation_setup(&character_menu);
    tap(action(character_view, "Prototype: save selection"));
    dtp_annotation_test_menu_enable(false);
    character_menu.id = bookmark.id;
    const QByteArray expected_japanese = QStringLiteral("高校").toUtf8();
    assert(dtp_annotation_auto_action(&character_menu, NULL, &book));
    assert(deletes == japanese_removed + 2);
    /* Also reject a native save that reports less text than the planned token. */
    character_menu.id.clear(); bookmark.id.clear();
    const unsigned before_mismatch_save = saves;
    assert(!dtp_annotation_auto_action(&character_menu, expected_japanese.constData(), &book));
    assert(saves == before_mismatch_save);

    // Native existing-highlight resolution must precede tokenization. The
    // segmenter deliberately rejects here, but a whole old passage still deletes.
    const unsigned old_passage_removed=deletes;
    bookmark.id=QStringLiteral("cccccccc-cccc-4ccc-8ccc-cccccccccccc");
    bookmark.text=QString(80,QChar(0x9ad8)).toUtf8()+QStringLiteral("。昔の文章").toUtf8();
    japanese_mode=1;
    sample=begin_pair(reader,live_tap);
    assert(dtp_toggle_process(&reader,live_tap,&sample));
    assert(deletes==old_passage_removed+1);
    japanese_mode=0;

    /* The native evaluator can close the reader; never call a getter afterward. */
    KepubBookReader *closing_book = new KepubBookReader(&reader);
    closing_book->resize(600,800); closing_book->show();
    watched_widget = closing_book;
    bookmark.id.clear(); bookmark.text = QStringLiteral("高").toUtf8();
    japanese_mode = 4;
    sample = begin_pair(reader, live_tap);
    assert(dtp_toggle_process(&reader, live_tap, &sample));
    assert(!watched_widget && saves == before_mismatch_save);
    japanese_mode = 0; watched_widget = &book;

    /* Other widgets and disabled/partially resolved integration retain policy. */
    watched_widget = &ordinary_view;
    dtp_toggle_allow(live_tap, false); assert(!allow_value);
    dtp_toggle_allow(live_tap, true); assert(allow_value);
    watched_widget = &book;
    selection.mode = true;
    dtp_toggle_allow(live_tap, false); assert(!allow_value);
    selection.mode = false;
    dtp_toggle_enable(false);
    dtp_toggle_allow(live_tap, false); assert(!allow_value);
    dtpt_start = NULL;
    dtp_toggle_enable(true);
    dtp_toggle_allow(live_tap, false); assert(!allow_value);
    dtp_toggle_enable(false);
    QFile::remove(QStringLiteral("build/native-test/registry"));
    std::puts("PASS: ARM Qt double-tap save/remove, Japanese expansion/native-text agreement, malformed/missing evaluator and closed-reader guards, older passage removal, word/note/stale-range guards, single/held pass-through, lifecycle cancellation, disabled fallback");
}

static bool held_longpress=true;
static bool held_getter(const void *) { return held_longpress; }
static void run_longpress_cases() {
    dtp_annotations_enable(true);
    initialize_rendering();
    const QString config(QStringLiteral("build/native-test/settings"));
    QFile::remove(config);
    assert(dtp_settings_longpress());
    ReadingView reader; reader.resize(600,800); reader.show();
    KepubBookReader book(&reader); book.resize(600,800); book.show();
    TestBookmark bookmark={QString(),QStringLiteral("book"),QStringLiteral("p1"),QStringLiteral("p1"),
        QStringLiteral("高").toUtf8(),QByteArray(),0,0,0,1,true,false,false};
    SelectionMenuView view;
    SelectionController selection(&view,&bookmark); selection.setParent(&reader);
    char tap[16]={}; live_tap=tap; active_bookmark=&bookmark; watched_widget=&book;
    dtpl_is_held=reinterpret_cast<void *>(held_getter);
    dtpt_start=reinterpret_cast<void *>(selection_start);
    dtpt_watched=reinterpret_cast<void *>(watched);
    dtpj_evaluate=reinterpret_cast<void *>(native_japanese_evaluate);
    dtpj_selected_text=reinterpret_cast<void *>(native_selected_text);
    japanese_mode=0;
    const unsigned before_saves=saves,before_deletes=deletes;
    assert(dtp_longpress_start(&selection,tap));
    assert(bookmark.text==QStringLiteral("高校").toUtf8());
    assert(saves==before_saves && deletes==before_deletes); // Never auto-highlight.
    QFile file(config); assert(file.open(QIODevice::WriteOnly));
    file.write("japanese_long_press_words=0\n");file.close();
    assert(!dtp_settings_longpress());
    bookmark.text=QStringLiteral("高").toUtf8();
    assert(dtp_longpress_start(&selection,tap));
    assert(bookmark.text==QStringLiteral("高").toUtf8());
    QFile::remove(config); assert(dtp_settings_longpress());
    held_longpress=false;
    assert(dtp_longpress_start(&selection,tap));
    assert(bookmark.text==QStringLiteral("高").toUtf8());
    held_longpress=true;
    bookmark.text="English";
    assert(dtp_longpress_start(&selection,tap));
    assert(bookmark.text=="English");
    watched_widget=&view; bookmark.text=QStringLiteral("高").toUtf8();
    assert(dtp_longpress_start(&selection,tap));
    assert(bookmark.text==QStringLiteral("高").toUtf8());
    watched_widget=&book; japanese_mode=1;
    assert(dtp_longpress_start(&selection,tap));
    assert(bookmark.text==QStringLiteral("高").toUtf8());
    japanese_mode=0;
    bookmark.text=QStringLiteral("高校").toUtf8();
    const QString database(QStringLiteral("test-only"));
    assert(dtp_annotation_save(&bookmark,&database));
    assert(dtp_annotation_owned(&bookmark)); // Manual words retain bounded rendering on reopen.
    selectable=false;
    assert(!dtp_longpress_start(&selection,tap));
    selectable=true; bookmark.text=QStringLiteral("高").toUtf8();
    dtp_annotations_enable(false);
    assert(dtp_longpress_start(&selection,tap));
    assert(bookmark.text==QStringLiteral("高").toUtf8());
    dtp_annotations_enable(true);
    std::puts("PASS: Japanese long-press default On, persisted Off, English/non-held/non-KEPUB/disabled/failure pass-through; no auto-save/delete");
}
