#include "tstackedwidgetanimationslidevertical.h"

#include "tstackedwidget.h"
#include "tvariantanimation.h"
#include <QWidget>

tStackedWidgetAnimationSlideVertical::tStackedWidgetAnimationSlideVertical(int oldIndex, int newIndex, tStackedWidget* parent) :
    tStackedWidgetAnimationSlide{oldIndex, newIndex, parent} {
}

QRect tStackedWidgetAnimationSlideVertical::oldWidgetRect(double progress) {
    auto geometry = QRect(QPoint(0, 0), this->parentStack()->size());

    if (this->movingForwards()) {
        return geometry.translated(0, -this->parentStack()->height() / 8 * progress);
    } else {
        return geometry.translated(0, this->parentStack()->height() / 8 * progress);
    }
}

QRect tStackedWidgetAnimationSlideVertical::newWidgetRect(double progress) {
    auto geometry = QRect(QPoint(0, 0), this->parentStack()->size());

    if (this->movingForwards()) {
        return geometry.translated(0, this->parentStack()->height() * (1 - progress));
    } else {
        return geometry.translated(0, -this->parentStack()->height() * (1 - progress));
    }
}
