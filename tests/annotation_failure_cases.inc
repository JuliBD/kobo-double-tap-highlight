static void run_annotation_failure_cases() {
    dtp_annotations_enable(true);
    const QString registry(QStringLiteral("build/native-test/registry"));
    QWidget book;
    TestBookmark bookmark = {QString(),QStringLiteral("test-book"),QStringLiteral("p1"),QStringLiteral("p1"),
                             QByteArray("testword"),QByteArray(),0,0,0,8,true,false,false};
    SelectionMenuView view;
    SelectionMenuController menu(&view, &bookmark);
    QFile file(registry);
    // The old 4096-entry metadata limit must no longer affect native saves.
    assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("DTP-TEST-HIGHLIGHTS-2\n");
    for (unsigned i=0;i<5000;++i) {
        const QString id=QStringLiteral("10000000-0000-4000-8000-%1").arg(i,12,16,QLatin1Char('0'));
        file.write(id.toLatin1()+'\t'+QByteArray(64,'a')+'\n');
    }
    file.close();
    assert(file.open(QIODevice::ReadOnly));const QByteArray original=file.readAll();file.close();
    const unsigned initial_saves=saves;
    assert(dtp_annotation_auto_action(&menu,"testword",&book));
    assert(saves==initial_saves+1);
    assert(file.open(QIODevice::ReadOnly));assert(file.readAll()==original);file.close();
    QFile::remove(registry);
    bookmark.id.clear();menu.id.clear();
    assert(dtp_annotation_auto_action(&menu,"testword",&book));
    assert(saves==initial_saves+2 && !QFile::exists(registry));
    bookmark.id.clear();menu.id.clear();
    unsigned attempts=highlight_attempts;
    unsigned before_saves=saves;
    rendered_visual=false;
    /* Sentence punctuation, whitespace, and long Japanese tokens never dispatch. */
    const QByteArray invalid_words[] = {QStringLiteral("これは文章です。").toUtf8(), QByteArray("two words"),
                                      QString(25,QChar(0x9ad8)).toUtf8(), QByteArray("word!")};
    for (unsigned i = 0; i < sizeof(invalid_words)/sizeof(invalid_words[0]); ++i) {
        assert(!dtp_annotation_auto_action(&menu,invalid_words[i].constData(),&book));
        assert(highlight_attempts == attempts && !rendered_visual);
    }

    /* Unexpected native text drift is blocked and its already-created visual undone. */
    const unsigned cleaned = visual_cleanups;
    manager_text_override = QByteArray("unexpected sentence.");
    bookmark.text = QByteArray("testword");
    assert(!dtp_annotation_auto_action(&menu,"testword",&book));
    assert(saves == before_saves && !rendered_visual && visual_cleanups == cleaned + 1);
    manager_text_override.clear(); bookmark.id.clear(); bookmark.text = QByteArray("testword");

    /* A real native save failure also cleans the visual, without a DB deletion. */
    const unsigned before_deletes = deletes;
    save_ok = false;
    assert(!dtp_annotation_auto_action(&menu,"testword",&book));
    assert(saves == before_saves + 1 && !rendered_visual && visual_cleanups == cleaned + 2 && deletes == before_deletes);
    save_ok = true;
    QFile::remove(registry);
    std::puts("PASS: native save works with 5000 legacy entries and without a metadata file, sentence rejection before native display change, native text/save-failure visual rollback without annotation deletion");
}

// Deletion must validate a fresh menu snapshot again at the manager boundary.
static void run_old_highlight_cases() {
    dtp_annotations_enable(true);
    dtp_annotation_test_menu_enable(false);
    QFile::remove(QStringLiteral("build/native-test/registry"));
    QWidget book;
    const TestBookmark original = {QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"),
        QStringLiteral("old-book"),QStringLiteral("p1"),QStringLiteral("p3"),
        QStringLiteral("これは以前に保存した長い文章です。これは単語ではありません。").toUtf8(),
        QByteArray(),0,2,3,20,true,false,false};
    TestBookmark bookmark=original;
    SelectionMenuView view;
    SelectionMenuController menu(&view,&bookmark);
    menu.id=bookmark.id;
    unsigned before=deletes;
    assert(dtp_annotation_auto_action(&menu,NULL,&book));
    assert(deletes==before+1 && bookmark.deleted); // No registry entry; whole Japanese passage.
    for (unsigned mutation=1; mutation<=8; ++mutation) {
        bookmark=original; menu.id=bookmark.id; delete_mutation=mutation;
        before=deletes;
        assert(!dtp_annotation_auto_action(&menu,NULL,&book));
        assert(deletes==before && !bookmark.deleted);
    }
    delete_mutation=0;
    bookmark=original; bookmark.note="keep this note"; menu.id=bookmark.id;
    before=delete_attempts;
    assert(!dtp_annotation_auto_action(&menu,NULL,&book));
    assert(delete_attempts==before); // Refuse before invoking native removal.
    bookmark=original; bookmark.noteType=true;
    assert(!dtp_annotation_auto_action(&menu,NULL,&book));
    assert(delete_attempts==before);
    bookmark=original; menu.id=bookmark.id; delete_mutation=9;
    before=deletes;
    assert(!dtp_annotation_auto_action(&menu,NULL,&book));
    assert(deletes==before+1); // Never forward a second manager deletion.
    delete_mutation=0;
    std::puts("PASS: old Japanese multiword highlight removal without registry; notes protected; ID/text/book/path/range mutations blocked; one deletion maximum");
}
