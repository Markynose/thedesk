#ifndef ASSISTANTPANEL_H
#define ASSISTANTPANEL_H

#include <QDialog>
#include <QKeyEvent>
#include <QHash>
#include <functional>
#include <m3motion.h>

// Pebble, the assistant (Super+Space): a pill near the top of the screen;
// under it the answer as one big line, cards in a carousel and follow-ups. Esc or clicking away
// while it works puts it into the bar until it's done.
class AssistantPanel : public QDialog {
        Q_OBJECT
    public:
        static AssistantPanel* instance();
        static void setUp();

        void open();
        void close();
        void openWithSelection();
        void openAndAsk(const QString& text); // opens and sends it, as if typed
        void openWithScreenshot(const QImage& image);
        // circle to search: as a sheet rising from a pill at the bottom, asked about what was circled
        void openForCircle(const QImage& image, const QString& question);

    signals:
        void sheetClosed();

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
        bool smallTalk(const QString& text); // hi, thanks, gn…: the face answers, no brain
        void react(const QString& kind);
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
        QRectF pillRect() const;
        double stageWidth() const;
        QList<int> turnStarts() const;
        void showTurn(int back);
        double paintStage(QPainter& p, const QRectF& view, const QList<int>& starts, double seesK, int only = -1, bool leaving = false);
        QImage imageOf(const QString& source);
        double glide(const QString& key, double target, M3Motion::Token token = M3Motion::SpatialDefault);
        void markLeaving();
        void sheetGone();
        using HitFn = std::function<void(const QRectF&, std::function<void(QPointF)>)>;
        using HoverFn = std::function<bool(const QRectF&)>;
        double paintLiving(QPainter& p, const QString& raw, const QList<qint64>& wordBorn, QPointF at, double width, double px, const HitFn& hit, const HoverFn& hovered, const QHash<QString, double>& countFrom = {}, bool centred = false);
};

#endif // ASSISTANTPANEL_H
