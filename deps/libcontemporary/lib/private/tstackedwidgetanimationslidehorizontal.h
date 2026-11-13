#ifndef TSTACKEDWIDGETANIMATIONSLIDEHORIZONTAL_H
#define TSTACKEDWIDGETANIMATIONSLIDEHORIZONTAL_H

#include "tstackedwidgetanimationslide.h"

class tStackedWidgetAnimationSlideHorizontal : public tStackedWidgetAnimationSlide {
        Q_OBJECT
    public:
        explicit tStackedWidgetAnimationSlideHorizontal(int oldIndex, int newIndex, tStackedWidget* parent);

    signals:

    private:
        bool moveLeft();

        // tStackedWidgetAnimationSlide interface
    protected:
        QRect oldWidgetRect(double progress);
        QRect newWidgetRect(double progress);
};

#endif // TSTACKEDWIDGETANIMATIONSLIDEHORIZONTAL_H
