/****************************************
 *
 *   theDesk - onboarding: you (your picture's shape)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef ONBOARDINGYOU_H
#define ONBOARDINGYOU_H

#include "onboardingpage.h"

struct OnboardingYouPrivate;

/**
 * "This is you": your picture and name as the login and lock screens show
 * them, and a row of MaterialShapes to pick the shape your picture wears
 * there (UserCard/shapes; the greeter gets it through ThemeSync).
 */
class OnboardingYou : public OnboardingPage {
        Q_OBJECT
    public:
        explicit OnboardingYou(QWidget* parent = nullptr);
        ~OnboardingYou() override;

        QString name() override;
        QString displayName() override;
        void pageActivated() override;

    private:
        OnboardingYouPrivate* d;
};

#endif // ONBOARDINGYOU_H
