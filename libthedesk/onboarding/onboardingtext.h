/****************************************
 *
 *   theDesk - onboarding: expressive, animated text
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ONBOARDINGTEXT_H
#define ONBOARDINGTEXT_H

#include <QObject>

class QLabel;
struct OnboardingTextPrivate;

/**
 * Takes over painting a QLabel to animate it in M3 Expressive's manner.
 *
 *   Title: Roboto Flex, each letter springing in through the width and
 *          weight axes (narrow and light to full), rising a little and
 *          turning from Primary to OnSurface. A click makes the letters
 *          jump in a wave.
 *   Body:  a description; its lines rise and fade in one after another,
 *          then the label paints itself as usual.
 *
 * The object lives as a child of the label.
 */
class OnboardingText : public QObject {
        Q_OBJECT
    public:
        enum Kind {
            Title,
            Body
        };

        // The label's animator, made the first time (a Title's size in px)
        static OnboardingText* forLabel(QLabel* label, Kind kind, int pixelSize = 28);

        // Finds a page's title (the label with the m3OnboardingTitle property,
        // else "titleLabel") and description ("descriptionLabel", else a
        // word-wrapped "label") and plays them in
        static void animatePage(QWidget* page, int delayMs = 0);
        // Hides a page's text until animatePage, so a page sliding in (the
        // stacked widget slides a picture of it) arrives without its words
        static void preparePage(QWidget* page);

        void play(int delayMs = 0);
        void hide(); // invisible until play()

        ~OnboardingText() override;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        OnboardingText(QLabel* label, Kind kind, int pixelSize);
        OnboardingTextPrivate* d;
        void paintTitle();
        bool paintBody();
};

#endif // ONBOARDINGTEXT_H
