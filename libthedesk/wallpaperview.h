/****************************************
 *
 *   theDesk - the shape wallpaper
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef WALLPAPERVIEW_H
#define WALLPAPERVIEW_H

#include "libthedesk_global.h"
#include <QWidget>

struct WallpaperViewPrivate;

/**
 * theDesk's wallpaper, in the manner of Pixel's shape wallpaper: two picked
 * MaterialShapes (Wallpaper/shapes in theDesk.conf) and a small accent in a
 * grid on a dark tonal ground, shrinking to nothing towards the middle. Its
 * colours are the session's M3 scheme (dark), so it follows the seed.
 *
 * Each desktop pans it one step and jiggles the shapes; the lock wears it
 * dimmed and zoomed in, the greeter darker and greyer; a new seed or shape
 * pair sweeps across, each shape morphing and recolouring.
 *
 * Battery friendly: still frames come from a cached pixmap; it animates only
 * during those transitions. No compositor needed.
 */
class LIBTHEDESK_EXPORT WallpaperView : public QWidget {
        Q_OBJECT
    public:
        enum class Look {
            Desktop,
            Lock,   // dimmed, zoomed in, quiet behind the clock
            Greeter // darker, greyer
        };

        explicit WallpaperView(QWidget* parent = nullptr);
        ~WallpaperView() override;

        // Pans one step per desktop (the launcher's parallax)
        void setDesktop(int index, int count, bool animate = true);
        void setLook(Look look, bool animate = true);
        // Shapes over it shrink away while locked (the lock's clock)
        void setQuietRect(QRectF rect);

        void playIntro(); // the shapes pop in from the middle outwards
        void playPop();   // unlocking: they bounce from the middle outwards

        // The two picked shapes' MaterialShapes names
        static QStringList shapeNames();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:
        WallpaperViewPrivate* d;

        void ensureCells();
        void tick();
        void startAnimating();
        void schemeOrShapesChanged();
        void paintFrame(QPaintDevice* device);
};

#endif // WALLPAPERVIEW_H
