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
#ifndef SPLASHWIDGET_H
#define SPLASHWIDGET_H

#include <QWidget>

class LivingTypeView;

/**
 * The loading splash: the living-type field (splash/livingtypeview.h) around
 * the M3 Expressive loading indicator. When the desktop is ready it plays an
 * exit (Splash/exit in theDesk.conf: random, vortex, bloom, ripple, drop,
 * sweep or cascade) and then reports done.
 */
class SplashWidget : public QWidget {
        Q_OBJECT
    public:
        explicit SplashWidget(QWidget* parent = nullptr);
        ~SplashWidget();

        void playExit();
        void setRecovery(bool recovery); // the loading screen after a crash

    signals:
        void exitFinished();

    private:
        LivingTypeView* view;
};

#endif // SPLASHWIDGET_H
