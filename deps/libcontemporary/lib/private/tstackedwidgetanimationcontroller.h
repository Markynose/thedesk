#ifndef TSTACKEDWIDGETANIMATIONCONTROLLER_H
#define TSTACKEDWIDGETANIMATIONCONTROLLER_H

#include <QObject>

class tStackedWidgetAnimation;
class tStackedWidget;
struct tStackedWidgetAnimationControllerPrivate;
class tStackedWidgetAnimationController : public QObject {
        Q_OBJECT
    public:
        explicit tStackedWidgetAnimationController(tStackedWidget* parentStack, QObject* parent = nullptr);
        ~tStackedWidgetAnimationController();

        void startAnimation(tStackedWidgetAnimation* animation);
        void stopCurrentAnimation();

    signals:
        void done(int newIndex);

    private:
        tStackedWidgetAnimationControllerPrivate* d;
};

#endif // TSTACKEDWIDGETANIMATIONCONTROLLER_H
