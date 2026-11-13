/****************************************
 *
 *   theDesk - live horizon view
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef HORIZONVIEW_H
#define HORIZONVIEW_H

#include "libthedesk_global.h"
#include <QIcon>
#include <QWidget>

#include "horizonscene.h"

class QPainter;

struct HorizonViewPrivate;

/**
 * The "living planet": a horizon scene that follows the time of day, with
 * the planet's apex positioned per desktop (it slides when the desktop
 * changes) and a rim whose glow follows the battery level.
 *
 * Battery friendly: it repaints once a minute while idle and animates only
 * during transitions (desktop switch, intro, charger plugged in).
 */
class LIBTHEDESK_EXPORT HorizonView : public QWidget {
        Q_OBJECT
    public:
        explicit HorizonView(QWidget* parent = nullptr);
        ~HorizonView();

        // Fixed scene instead of following the clock (-1 = follow the clock)
        void setFixedScene(int scene);

        // Position the apex for desktop `index` of `count`
        void setDesktop(int index, int count, bool animate = true);

        // Rim glow, 0..1; charging plays a single flare when it switches on
        void setBattery(double level, bool charging);

        // Planet rises into view
        void playIntro();

        // Lock screen: dim to night (sunset) / back to the time of day with
        // light spreading from the apex (sunrise)
        void playSunset();
        void playSunrise();
        bool isNight() const;

        // Serif caption above the horizon (empty title hides it)
        void setCaption(QString title, QString subtitle = QString());

        // Darken the top of the view so text drawn over it stays readable
        void setTopShade(bool shade);
        void setQuietRect(QRectF rect); // shapes over it fade back (the lock screen's clock)

        double batteryLevel() const;

        // Notifications as moons rising from behind the planet (max 5;
        // the oldest sets when another arrives)
        void addMoon(QString title, QString subtitle, QIcon icon = QIcon());
        void clearMoons();
        int moonCount() const;

        // Focus tide: light the rim up to `progress` (0..1); -1 turns it off
        void setTide(double progress);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private slots:
        void pollBattery();

    private:
        HorizonViewPrivate* d;

        void ensureRendered();
        void paintScene(QPaintDevice* device);
        bool paintMoons(QPainter& painter, const HorizonScene::Geometry& geometry, qint64 now, bool labels);
        void startAnimating();
};

#endif // HORIZONVIEW_H
