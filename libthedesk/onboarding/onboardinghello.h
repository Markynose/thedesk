/****************************************
 *
 *   theDesk - onboarding: the "Hello" welcome
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ONBOARDINGHELLO_H
#define ONBOARDINGHELLO_H

#include <QPoint>
#include <QRectF>
#include <QWidget>

struct OnboardingHelloPrivate;

/**
 * The welcome page's picture, in the manner of Pixel's "Hello" setup screen:
 * MaterialShapes around the edges springing in, and a big Roboto Flex
 * greeting that cycles through languages, each letter springing through the
 * font's width and weight axes.
 *
 * Easter eggs (M3 Expressive, all springs):
 *   - click a shape: it squashes and morphs into the next MaterialShape
 *   - click the greeting: the letters jump in a wave; five quick clicks and
 *     it says "M3 Expressive" for a moment
 *   - the Konami code (up up down down left right left right B A): a shape
 *     party, everything spinning and morphing in the scheme's colours
 */
class OnboardingHello : public QWidget {
        Q_OBJECT
    public:
        explicit OnboardingHello(QWidget* parent = nullptr);
        ~OnboardingHello() override;

        void setGreetingRect(QRectF rect); // where the greeting goes, in this widget
        QSize greetingSize() const;        // the room it wants

        // The opening plays in the greeting's place: hold the greeting, then
        // take over from the opening's resting word (it squeezes away and the
        // greetings start)
        void holdGreeting();
        void releaseGreeting(const QString& from);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        OnboardingHelloPrivate* d;
        void tick();
};

/**
 * A burst of small MaterialShapes from a point, falling and fading (the
 * onboarding's "Done"). Lives in its own overlay on top of `window` and
 * deletes itself.
 */
namespace OnboardingCelebration {
    void burst(QWidget* window, QPoint origin);
} // namespace OnboardingCelebration

#endif // ONBOARDINGHELLO_H
