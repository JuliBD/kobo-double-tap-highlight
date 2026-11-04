#ifndef DTP_ANNOTATION_QT_TEST_H
#define DTP_ANNOTATION_QT_TEST_H
#include <QLabel>
#include <QVBoxLayout>
#include <QEvent>
struct TestBookmark;
class SelectionMenuController;

class ReadingView : public QWidget {
    Q_OBJECT
public:
    unsigned update_events = 0;
    bool update_partial = false;
    bool event(QEvent *event) override {
        if (event->type() != QEvent::UpdateRequest) return QWidget::event(event);
        ++update_events;
        update_partial = testAttribute(static_cast<Qt::WidgetAttribute>(130));
        return true;
    }
};
class KepubBookReader : public QWidget {
    Q_OBJECT
public:
    explicit KepubBookReader(QWidget *parent) : QWidget(parent) {}
};

class MenuTextItem : public QLabel {
    Q_OBJECT
public:
    MenuTextItem(const QString &text, QWidget *parent) : QLabel(text, parent) {}
signals:
    void tapped(bool);
};

class SelectionMenuView : public QWidget {
    Q_OBJECT
public:
    QVBoxLayout *layout;
    SelectionMenuView() : layout(new QVBoxLayout(this)) {}
};

class SelectionMenuController : public QObject {
    Q_OBJECT
public:
    SelectionMenuView *view;
    TestBookmark *bookmark;
    QString id;
    bool mode = false;
    SelectionMenuController *native_menu = NULL;
    void *selection_owner = NULL;
    SelectionMenuController(SelectionMenuView *v, TestBookmark *b);
signals:
    void highlight();
public slots:
    void removeHighlight();
};
class SelectionController : public SelectionMenuController {
    Q_OBJECT
public:
    SelectionController(SelectionMenuView *v, TestBookmark *b) : SelectionMenuController(v, b) {}
};
#endif
