#ifndef TSTACKEDWIDGETANIMATION_H
#define TSTACKEDWIDGETANIMATION_H

#include <QObject>

class tStackedWidget;
struct tStackedWidgetAnimationPrivate;
class tStackedWidgetAnimation : public QObject {
        Q_OBJECT
    public:
        explicit tStackedWidgetAnimation(int oldIndex, int newIndex, tStackedWidget* parent);
        ~tStackedWidgetAnimation();

        QWidget* oldWidget();
        QWidget* newWidget();
        tStackedWidget* parentStack();
        bool movingForwards();

        virtual void startAnimation() = 0;
        virtual void stopAnimation() = 0;

    signals:
        void done(int newIndex);

    protected:
        void emitDone();

    private:
        tStackedWidgetAnimationPrivate* d;
};

#endif // TSTACKEDWIDGETANIMATION_H
