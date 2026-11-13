#ifndef TSTACKEDWIDGETANIMATIONFADE_H
#define TSTACKEDWIDGETANIMATIONFADE_H

#include "tstackedwidgetanimation.h"

class tStackedWidget;
struct tStackedWidgetAnimationFadePrivate;
class tStackedWidgetAnimationFade : public tStackedWidgetAnimation {
        Q_OBJECT
    public:
        explicit tStackedWidgetAnimationFade(int oldIndex, int newIndex, tStackedWidget* parent);
        ~tStackedWidgetAnimationFade();

        void startAnimation();
        void stopAnimation();

    signals:

    private:
        tStackedWidgetAnimationFadePrivate* d;
};

#endif // TSTACKEDWIDGETANIMATIONFADE_H
