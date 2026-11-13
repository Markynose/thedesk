/****************************************
 *
 *   theDesk - Material 3 Expressive Gateway button
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef GATEWAYLOGO_H
#define GATEWAYLOGO_H

#include <QWidget>

struct GatewayLogoPrivate;

/**
 * The Gateway button's mark: a MaterialShapes Sunny in TertiaryContainer with
 * a heavy, wide Roboto Flex "d". On hover the shape spins an eighth of a turn
 * and the d dips thin and springs back (expressive springs); hover a while and
 * the d flips into a p and back. As the Gateway opens the shape rounds into a
 * circle and the d squeezes narrow and tall, following the open gesture.
 * Lays itself over a button's icon, or at the leading edge of the Gateway's
 * search bar, where opening the sheet shows the round, squeezed form.
 */
class GatewayLogo : public QWidget {
        Q_OBJECT
    public:
        enum class Placement {
            ButtonIcon, // over a button's icon, reacting to the button's hover
            Leading     // inside a field's leading edge (the Gateway search bar)
        };

        explicit GatewayLogo(QWidget* host, Placement placement = Placement::ButtonIcon, int size = 32);
        ~GatewayLogo() override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;
        bool event(QEvent* event) override;

    private:
        GatewayLogoPrivate* d;
        void place();
        void hoverChanged(bool hovered);
};

#endif // GATEWAYLOGO_H
