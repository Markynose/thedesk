/****************************************
 *
 *   theDesk - onboarding: the Pixel-style page chrome
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ONBOARDINGCHROME_H
#define ONBOARDINGCHROME_H

#include <QWidget>

class OnboardingPage;
struct OnboardingHeroPrivate;
struct OnboardingProgressPrivate;

/**
 * Every step's picture, top-left above its headline, in the manner of
 * Pixel's setup: a MaterialShape in a container colour with a Material
 * Symbol in it. One hero for all the steps, over the pages: as the pages
 * slide, it morphs from one step's shape into the next's (spinning a little,
 * the symbol swapping halfway), so moving through setup is one animation.
 */
class OnboardingHero : public QWidget {
        Q_OBJECT
    public:
        explicit OnboardingHero(QWidget* parent = nullptr);
        ~OnboardingHero() override;

        static constexpr int size = 72;

        // The look for a step (by OnboardingPage::name()); hidden steps
        // (the welcome, the end) have their own pictures
        void showStep(const QString& pageName);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        OnboardingHeroPrivate* d;
};

/**
 * A thin M3E wavy progress line across the top of the card: how far through
 * setup you are, springing along as you go, its waves drifting.
 */
class OnboardingProgress : public QWidget {
        Q_OBJECT
    public:
        explicit OnboardingProgress(QWidget* parent = nullptr);
        ~OnboardingProgress() override;

        void setProgress(double progress); // 0..1

    protected:
        void paintEvent(QPaintEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        OnboardingProgressPrivate* d;
};

namespace OnboardingChrome {
    // Room above a page's headline for the hero, and its inset
    constexpr int margin = 24;
    constexpr int heroSpace = OnboardingHero::size + 16;

    // Restyles a step page (not the welcome or the end) the Pixel way: room
    // for the hero, a 32 px headline flush with it (no back arrow), body text
    // in BodyLarge, no rules, and a bottom bar with a text Back button on the
    // left and a filled Next on the right (the page's own nextButton)
    void stylePage(OnboardingPage* page);

    // Whether a step has the hero (the welcome and the end draw their own)
    bool hasHero(const QString& pageName);

    // A step's own shape and symbol (the hero's, and the step list's), and
    // its container colour role: 0 Primary, 1 Tertiary, 2 Secondary
    struct StepLook {
            const char* shape;
            const char* symbol;
            int role;
    };
    StepLook stepLook(const QString& pageName);
} // namespace OnboardingChrome

#endif // ONBOARDINGCHROME_H
