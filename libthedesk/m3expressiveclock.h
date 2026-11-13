/****************************************
 *
 *   theDesk - Material 3 Expressive clock
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3EXPRESSIVECLOCK_H
#define M3EXPRESSIVECLOCK_H

#include "libthedesk_global.h"
#include <QWidget>

struct M3ExpressiveClockPrivate;

/**
 * Hours and minutes in expressive Roboto Flex. When a digit changes it dips
 * in weight and springs back (M3 Expressive spatial spring), like the Pixel
 * lock screen clock.
 *
 * Line: one heavy, wide line in WindowText.
 * Stack: heavy, wide hours in Primary over light, condensed, tall-figure
 * minutes in Tertiary, stretched to the width of the hours.
 */
class LIBTHEDESK_EXPORT M3ExpressiveClock : public QWidget {
        Q_OBJECT
    public:
        explicit M3ExpressiveClock(QWidget* parent = nullptr);
        ~M3ExpressiveClock() override;

        enum class Style {
            Line,
            Stack
        };

        void setPixelSize(int px);
        void setStyle(Style style);
        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:
        M3ExpressiveClockPrivate* d;
        void tick();
        void paintLine(QPainter* painter);
        void paintStack(QPainter* painter);
};

#endif // M3EXPRESSIVECLOCK_H
