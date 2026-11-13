#ifndef ASSISTANTPANEL_H
#define ASSISTANTPANEL_H

#include <QDialog>
#include <QKeyEvent>
#include <QHash>
#include <functional>

// Pebble, the assistant: a sheet in the middle of the screen (Super+Space)
// with a thread of answers, cards and follow-up chips. Esc or clicking away
// while it works puts it into the bar until it's done.
class AssistantPanel : public QDialog {
        Q_OBJECT
    public:
        static AssistantPanel* instance();
        static void setUp();

        void open();
        void close();
        void openWithSelection();

    private slots:
        void batteryChanged();

    public:

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void changeEvent(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        AssistantPanel();
        struct Private;
        Private* d;

        void send();
        void say(const QString& raw, bool isGreeting);
        void greet();
        void startIntro();
        void introNext();
        void introBack();
        void finishIntro();
        QRectF introCard() const;
        void paintIntro(QPainter& p);
        void paintFocus(QPainter& p);
        bool focusKey(QKeyEvent* event);
        void sendText(QString text);
        void toBar();
        void tick();
        void animateFor(int ms);
        void redraw();
        void scrollToEnd();
        QRectF sheetRect() const;
        double paintThread(QPainter& p, const QRectF& view, double scroll);
        using HitFn = std::function<void(const QRectF&, std::function<void(QPointF)>)>;
        using HoverFn = std::function<bool(const QRectF&)>;
        double paintLiving(QPainter& p, const QString& raw, const QList<qint64>& wordBorn, QPointF at, double width, double px, const HitFn& hit, const HoverFn& hovered, const QHash<QString, double>& countFrom = {});
};

#endif // ASSISTANTPANEL_H
