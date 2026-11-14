/****************************************
 *
 *   theDesk - a volume slider in the sound panel
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef VOLUMECOLUMN_H
#define VOLUMECOLUMN_H

#include <QAbstractSlider>

class QMenu;
class M3SpringAnimation;

/**
 * One column of Android's volume dialog: the level above, a vertical M3E
 * slider with its symbol inside the active track, the name below (with a
 * menu for apps). The master column is wider, and its symbol mutes.
 */
class VolumeColumn : public QAbstractSlider {
        Q_OBJECT
    public:
        explicit VolumeColumn(bool master, QWidget* parent = nullptr);

        void setSymbol(const QString& symbol);
        void setName(const QString& name);
        void setSubtitle(const QString& subtitle);
        void setShownLevel(int percent); // the real level, may be above 100
        void setMuted(bool muted);
        void setMenu(QMenu* menu);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void iconClicked();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void sliderChange(SliderChange change) override;

    private:
        bool master;
        QString symbol, name, subtitle;
        int shown = -1;
        bool muted = false;
        bool iconPressed = false;
        QMenu* menu = nullptr;
        M3SpringAnimation* level;

        QRectF track() const;
        QRectF nameRect() const;
        QRectF iconRect() const;
        int valueAt(double y) const;
        void openMenu();
};

#endif // VOLUMECOLUMN_H
