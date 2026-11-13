#include "tstackedwidgetanimationslidehorizontal.h"
#include "libcontemporary_global.h"

#include "tstackedwidget.h"
#include <QWidget>

tStackedWidgetAnimationSlideHorizontal::tStackedWidgetAnimationSlideHorizontal(int oldIndex, int newIndex, tStackedWidget* parent) :
    tStackedWidgetAnimationSlide{oldIndex, newIndex, parent} {
}

bool tStackedWidgetAnimationSlideHorizontal::moveLeft() {
    return (this->movingForwards() && !QApplication::isRightToLeft()) || (!this->movingForwards() && QApplication::isRightToLeft());
}

QRect tStackedWidgetAnimationSlideHorizontal::oldWidgetRect(double progress) {
    auto geometry = QRect(QPoint(0, 0), this->parentStack()->size());
    const int distance = libContemporaryCommon::isMaterial3() ? 30 : 50; // M3 shared axis: 30 dp
    if (this->moveLeft()) {
        return geometry.translated(-distance * progress, 0);
    } else {
        return geometry.translated(distance * progress, 0);
    }
}

QRect tStackedWidgetAnimationSlideHorizontal::newWidgetRect(double progress) {
    auto geometry = QRect(QPoint(0, 0), this->parentStack()->size());
    const int distance = libContemporaryCommon::isMaterial3() ? 30 : 50; // M3 shared axis: 30 dp
    if (this->moveLeft()) {
        return geometry.translated(distance * (1 - progress), 0);
    } else {
        return geometry.translated(-distance * (1 - progress), 0);
    }
}
