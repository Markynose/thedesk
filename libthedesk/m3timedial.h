#ifndef M3TIMEDIAL_H
#define M3TIMEDIAL_H

#include "libthedesk_global.h"
#include <QTime>
#include <QWidget>

// M3 time picker (TimePickerTokens): the hour and minute boxes over a clock
// dial. Picking an hour moves on to the minutes, as on Android. 24-hour
// locales get an inner ring for 12-23; 12-hour ones an AM/PM toggle.
class LIBTHEDESK_EXPORT M3TimeDial : public QWidget {
        Q_OBJECT
    public:
        explicit M3TimeDial(QWidget* parent = nullptr);
        ~M3TimeDial();

        QTime time() const;
        void setTime(QTime time);

        QSize sizeHint() const override;

    signals:
        void timeChanged(QTime time);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        struct Private;
        Private* d;

        void pick(QPointF at, bool settle);
        void setMinutesMode(bool minutes);
        void aimHandle();
};

#endif // M3TIMEDIAL_H
