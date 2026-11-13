/****************************************
 *
 *   theDesk - End Session tile (Material 3 Expressive)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ENDSESSIONBUTTON_H
#define ENDSESSIONBUTTON_H

#include <QCommandLinkButton>

struct EndSessionButtonPrivate;

/**
 * A power-menu tile in the manner of Android 16's: hero tiles (power off,
 * restart) are tall Primary / SecondaryContainer blocks with the symbol and a
 * big label; the rest are SurfaceContainerHighest tiles with the symbol in a
 * tonal circle. Corners squish on press and hover adds a state layer, both
 * on springs. The description becomes the tooltip.
 */
class EndSessionButton : public QCommandLinkButton {
        Q_OBJECT
    public:
        enum class Role {
            Primary,   // power off
            Secondary, // restart
            Tile       // everything else
        };

        explicit EndSessionButton(QWidget* parent = nullptr);
        ~EndSessionButton();

        void setRole(Role role);
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        bool event(QEvent* event) override;

    private:
        EndSessionButtonPrivate* d;
};

#endif // ENDSESSIONBUTTON_H
