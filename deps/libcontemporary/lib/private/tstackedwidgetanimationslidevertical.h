#ifndef TSTACKEDWIDGETANIMATIONSLIDEVERTICAL_H
#define TSTACKEDWIDGETANIMATIONSLIDEVERTICAL_H

#include "tstackedwidgetanimationslide.h"

class tStackedWidgetAnimationSlideVertical : public tStackedWidgetAnimationSlide {
        Q_OBJECT
    public:
        explicit tStackedWidgetAnimationSlideVertical(int oldIndex, int newIndex, tStackedWidget* parent);

    signals:

    private:
        // tStackedWidgetAnimationSlide interface
    protected:
        QRect oldWidgetRect(double progress);
        QRect newWidgetRect(double progress);
};

#endif // TSTACKEDWIDGETANIMATIONSLIDEVERTICAL_H
