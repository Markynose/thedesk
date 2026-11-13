/****************************************
 *
 *   theDesk - loading screen settings
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef SPLASHSETTINGS_H
#define SPLASHSETTINGS_H

#include <QWidget>

struct SplashSettingsPrivate;

/**
 * The loading screen's settings in the Theme pane: a live preview (the real
 * loading screen over a small living horizon, its exit opening onto it), the
 * exit (Splash/exit), the colours (Splash/colours: the scheme, the MLM pride
 * flag, or the flag now and then) and the seasonal phrases (Splash/seasonal).
 * All in theDesk.conf; they take effect from the next start.
 */
class SplashSettings : public QWidget {
        Q_OBJECT
    public:
        explicit SplashSettings(QWidget* parent = nullptr);
        ~SplashSettings() override;

    protected:
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        SplashSettingsPrivate* d;
        void load();
        void playPreview();
};

#endif // SPLASHSETTINGS_H
