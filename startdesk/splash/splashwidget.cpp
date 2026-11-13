/****************************************
 *
 *   theDesk - loading splash
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "splashwidget.h"

#include "livingtypeview.h"

#include <QRandomGenerator>
#include <QVBoxLayout>
#include <tsettings.h>

SplashWidget::SplashWidget(QWidget* parent) :
    QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    view = new LivingTypeView(this);
    view->setMode(LivingTypeView::Field);
    layout->addWidget(view);

    // Splash/exit: one exit every time, or a different one each startup
    tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
    QString exit = settings.keysInGroup(QStringLiteral("Splash")).contains(QStringLiteral("exit")) ? settings.value(QStringLiteral("Splash/exit")).toString() : QStringLiteral("random");
    for (int style = LivingTypeView::RandomExit; style < LivingTypeView::ExitStyleCount; style++) {
        if (LivingTypeView::exitStyleName(style) == exit) view->setExitStyle(LivingTypeView::ExitStyle(style));
    }

    // Splash/colours: scheme (the M3 colours), mlm (the MLM pride flag), or
    // rare (the flag now and then, about one startup in fifteen)
    QString colours = settings.keysInGroup(QStringLiteral("Splash")).contains(QStringLiteral("colours")) ? settings.value(QStringLiteral("Splash/colours")).toString() : QStringLiteral("scheme");
    view->setFlagColours(colours == QStringLiteral("mlm") || (colours == QStringLiteral("rare") && QRandomGenerator::global()->bounded(15) == 0));

    connect(view, &LivingTypeView::exitFinished, this, &SplashWidget::exitFinished);
}

SplashWidget::~SplashWidget() = default;

void SplashWidget::playExit() {
    if (!this->isVisible()) {
        emit exitFinished();
        return;
    }
    view->playExit();
}

void SplashWidget::setRecovery(bool recovery) {
    view->setRecovery(recovery);
}
