/****************************************
 *
 *   theDesk - onboarding: the opening
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ONBOARDINGINTRO_H
#define ONBOARDINGINTRO_H

#include "../libthedesk_global.h"
#include <QWidget>

struct OnboardingIntroPrivate;

/**
 * The opening of first-time setup, all Roboto Flex: "theDesk" writes itself
 * in and rests; its k crouches and streams out of the word along a
 * scribbled path (a loop, a dip, a swoop), one k after another, each turning
 * with the path and its axes (weight, width, slant, grade, counters, stroke
 * contrast, ascenders) alive with the motion; the whole string rides the
 * path off the screen; then the word tumbles back in from the top, squashes
 * as it lands and breathes once, resting in the welcome's greeting (drawn
 * the way the greeting draws it) for the greeting to take over (finished).
 * It plays inside the welcome page, over its shapes.
 *
 * A click or a key skips to the end.
 */
class LIBTHEDESK_EXPORT OnboardingIntro : public QWidget {
        Q_OBJECT
    public:
        explicit OnboardingIntro(QWidget* parent = nullptr);
        ~OnboardingIntro() override;

        // Where the word rests: the greeting's baseline start (in this
        // widget) and its size, so the greeting can take over seamlessly
        void setPlacement(QPointF baselineLeft, double px);

        void start(int delayMs);
        // Elsewhere (System Settings' About): stay, the word resting where
        // it landed, instead of handing over and going; a click plays it again
        void setStays(bool stays);
        // Straight to the word at rest, without playing
        void showResting();
        // Play it again from the start
        void replay();

    signals:
        void finished(); // landed: the greeting takes over (this goes)

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        OnboardingIntroPrivate* d;
        void tick();
        void skip();
};

#endif // ONBOARDINGINTRO_H
