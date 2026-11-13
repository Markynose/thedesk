/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2022 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#ifndef ONBOARDINGNETWORK_H
#define ONBOARDINGNETWORK_H

#include <onboardingpage.h>

class QLabel;

namespace Ui {
    class OnboardingNetwork;
}

class OnboardingNetwork : public OnboardingPage {
        Q_OBJECT

    public:
        explicit OnboardingNetwork(QWidget* parent = nullptr);
        ~OnboardingNetwork();

    private:
        Ui::OnboardingNetwork* ui;

        void updateConnectivity();

        // OnboardingPage interface
    public:
        QString name();
        QString displayName();
        void pageActivated() override;

    private:
        bool autoAdvanced = false;
        int stepMoves = 0; // steps taken in onboarding, by anyone
        bool countingMoves = false;
        QWidget* connectedBadge = nullptr;
        QLabel* connectedName = nullptr;

    private slots:
        void on_titleLabel_backButtonClicked();
        void on_nextButton_clicked();
        void on_disconnectButton_clicked();
};

#endif // ONBOARDINGNETWORK_H
