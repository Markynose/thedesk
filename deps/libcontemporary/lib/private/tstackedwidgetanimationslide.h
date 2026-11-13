#ifndef TSTACKEDWIDGETANIMATIONSLIDE_H
#define TSTACKEDWIDGETANIMATIONSLIDE_H

#include "tstackedwidgetanimation.h"

struct tStackedWidgetAnimationSlidePrivate;
class tStackedWidgetAnimationSlide : public tStackedWidgetAnimation {
        Q_OBJECT
    public:
        explicit tStackedWidgetAnimationSlide(int oldIndex, int newIndex, tStackedWidget* parent);
        ~tStackedWidgetAnimationSlide();

        void startAnimation();
        void stopAnimation();

    signals:

    protected:
        virtual QRect oldWidgetRect(double progress) = 0;
        virtual QRect newWidgetRect(double progress) = 0;

    private:
        tStackedWidgetAnimationSlidePrivate* d;
};

#endif // TSTACKEDWIDGETANIMATIONSLIDE_H
