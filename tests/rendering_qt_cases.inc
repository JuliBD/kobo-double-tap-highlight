static unsigned render_original_calls, render_partial_calls, a2_calls, footer_calls;
static unsigned forced_refresh_calls;
static unsigned direct_full_refresh_calls;
static void native_force_full(void *) { ++direct_full_refresh_calls; }
static int rendered_start, rendered_end;
static QString rendered_id, rendered_base;
static void native_render_original(void *, const void *, int, int, int, bool, const void *) { ++render_original_calls; }
static void native_render_partial(void *, int start, int end, const QString &id, const QString &base, bool, const QString &) {
    ++render_partial_calls; rendered_start = start; rendered_end = end; rendered_id = id; rendered_base = base;
}
static QString native_part_id(int part, const QString &id) { return id + QStringLiteral(".-.") + QString::number(part); }
static void native_a2(void *, bool, bool) { ++a2_calls; }
static void native_footer(void *) { ++footer_calls; }
static void native_forced_refresh_signal(void *) { ++forced_refresh_calls; }
static void initialize_rendering() {
    dtpr_add = reinterpret_cast<void *>(native_render_original);
    dtpr_partial = reinterpret_cast<void *>(native_render_partial);
    dtpr_part_id = reinterpret_cast<void *>(native_part_id);
    dtpr_a2 = reinterpret_cast<void *>(native_a2);
    dtpr_close_footer = reinterpret_cast<void *>(native_footer);
    dtpr_repaint = reinterpret_cast<void *>(native_forced_refresh_signal);
    dtpr_force_full = reinterpret_cast<void *>(native_force_full);
}
class RefreshProbe : public QWidget {
public:
    unsigned updates = 0;
    bool saw_partial = false;
    QPointer<QWidget> check_child, delete_child;
    bool event(QEvent *event) override {
        if (event->type() != QEvent::UpdateRequest) return QWidget::event(event);
        ++updates;
        saw_partial = testAttribute(static_cast<Qt::WidgetAttribute>(130));
        if (check_child) assert(check_child->testAttribute(static_cast<Qt::WidgetAttribute>(130)) == saw_partial);
        if (delete_child) delete delete_child.data();
        return true;
    }
};
static void run_partial_delivery_cases() {
    const Qt::WidgetAttribute partial = static_cast<Qt::WidgetAttribute>(130);
    const Qt::WidgetAttribute full = static_cast<Qt::WidgetAttribute>(131);
    RefreshProbe window, other;
    QWidget reader(&window), book(&reader), child(&book);
    window.show(); reader.show(); book.show(); child.show(); other.show();
    QCoreApplication::sendPostedEvents(NULL, QEvent::UpdateRequest);
    window.updates = other.updates = 0;
    window.check_child = &child;
    QEvent update(QEvent::UpdateRequest);
    QCoreApplication::postEvent(&other, new QEvent(QEvent::UpdateRequest));
    assert(dtp_render_deliver_partial(&reader, &book, &update));
    assert(window.updates == 1 && window.saw_partial && other.updates == 0);
    assert(!window.testAttribute(partial) && !reader.testAttribute(partial) &&
           !book.testAttribute(partial) && !child.testAttribute(partial));
    QCoreApplication::sendPostedEvents(&other, QEvent::UpdateRequest);
    assert(other.updates == 1 && !other.saw_partial);

    // An existing full hint leaves event delivery and all settings to the caller.
    child.setAttribute(full, true);
    assert(!dtp_render_deliver_partial(&reader, &book, &update) && window.updates == 1);
    assert(child.testAttribute(full) && !window.testAttribute(partial));
    child.setAttribute(full, false);
    QCoreApplication::sendEvent(&window, &update);
    assert(window.updates == 2 && !window.saw_partial);

    // Existing partial hints survive; destroyed widgets are not restored via stale pointers.
    child.setAttribute(partial, true);
    window.check_child.clear();
    QWidget *temporary = new QWidget(&book);
    window.delete_child = temporary;
    assert(dtp_render_deliver_partial(&reader, &book, &update));
    assert(!window.delete_child && child.testAttribute(partial) && !window.testAttribute(partial));
    QEvent wrong(QEvent::User);
    assert(!dtp_render_deliver_partial(NULL, &book, &update) && !dtp_render_deliver_partial(&reader, &other, &update));
    assert(!dtp_render_deliver_partial(&reader, &book, &wrong));
    std::puts("PASS: scoped partial update delivers only the supplied event, restores hints, respects full hints and tolerates widget destruction");
}
static void run_rendering_cases() {
    dtp_annotations_enable(true); initialize_rendering();
    QFile::remove(QStringLiteral("build/native-test/settings"));
    assert(dtp_settings_rendering());
    run_partial_delivery_cases();
    const unsigned full_before=direct_full_refresh_calls;
    dtp_render_force_full(NULL);
    assert(direct_full_refresh_calls==full_before+1);
    TestBookmark bookmark={QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"),
        QStringLiteral("test-book"),QStringLiteral("p1"),QStringLiteral("p1"),
        QStringLiteral("私").toUtf8(),QByteArray(),0,0,0,1,true,false,false};
    const TestBookmark unchanged=bookmark;
    render_original_calls=render_partial_calls=0;
    int element=0;
    const QString style(QStringLiteral("test-style"));
    dtp_render_add(&element,&bookmark,0,1,0,false,&style);
    assert(render_partial_calls==1 && render_original_calls==0 && rendered_start==0 && rendered_end==1);
    assert(rendered_id==bookmark.id && rendered_base==bookmark.id);
    dtp_render_add(&element,&bookmark,0,1,2,false,&style);
    assert(render_partial_calls==2 && rendered_id==bookmark.id+QStringLiteral(".-.2") && rendered_base==bookmark.id);
    dtp_render_add(&element,&bookmark,1,2,0,false,&style);
    dtp_render_add(&element,&bookmark,0,0,0,false,&style);
    dtp_render_add(&element,&bookmark,0,-1,0,false,&style);
    assert(render_original_calls==3);
    // Old highlights, passages and notes use the supplied native range too.
    bookmark.text="an old multiword passage";bookmark.note="attached note";bookmark.noteType=true;
    dtp_render_add(&element,&bookmark,0,5,0,true,&style);
    assert(render_partial_calls==3 && rendered_end==5);
    assert(bookmark.text=="an old multiword passage" && bookmark.note=="attached note" && bookmark.noteType);
    assert(bookmark.start==unchanged.start && bookmark.end==unchanged.end && bookmark.endOffset==unchanged.endOffset);
    QFile config(QStringLiteral("build/native-test/settings"));
    assert(config.open(QIODevice::WriteOnly));
    config.write("japanese_long_press_words=1\nhighlight_rendering_fix=0\n");config.close();
    assert(dtp_settings_longpress() && !dtp_settings_rendering());
    dtp_render_add(&element,&bookmark,0,1,0,false,&style);
    assert(render_original_calls==4 && render_partial_calls==3);
    assert(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
    config.write("japanese_long_press_words=0\nhighlight_rendering_fix=1\n");config.close();
    assert(!dtp_settings_longpress() && dtp_settings_rendering());
    dtp_render_add(&element,&bookmark,0,1,0,false,&style);
    assert(render_partial_calls==4);
    bookmark.id.clear(); // Unsaved selections are not persistent highlights.
    dtp_render_add(&element,&bookmark,0,1,0,false,&style);
    assert(render_original_calls==5 && render_partial_calls==4);
    dtp_annotations_enable(false);
    dtp_render_add(&element,&bookmark,0,1,0,false,&style);
    assert(render_original_calls==6);
    QFile::remove(config.fileName());
    std::puts("PASS: default-On rendering for old/new/long/note highlights without metadata; Off/native fallback, independent preferences and multipart/sentinel behavior; no annotation changes");
}
