/****************************************
 *
 *   theDesk - onboarding: a few tips
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ONBOARDINGTIPS_H
#define ONBOARDINGTIPS_H

#include "onboardingpage.h"

struct OnboardingTipsPrivate;

// "A few tips": the keys and gestures worth knowing, springing in one by one
class OnboardingTips : public OnboardingPage {
        Q_OBJECT
    public:
        explicit OnboardingTips(QWidget* parent = nullptr);
        ~OnboardingTips() override;

        QString name() override;
        QString displayName() override;
        void pageActivated() override;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        OnboardingTipsPrivate* d;
};

struct OnboardingShortcutsPrivate;

// "Handy shortcuts": the rest of the keys, after the tips
class OnboardingShortcuts : public OnboardingPage {
        Q_OBJECT
    public:
        explicit OnboardingShortcuts(QWidget* parent = nullptr);
        ~OnboardingShortcuts() override;

        QString name() override;
        QString displayName() override;
        void pageActivated() override;

    private:
        OnboardingShortcutsPrivate* d;
};

#endif // ONBOARDINGTIPS_H
