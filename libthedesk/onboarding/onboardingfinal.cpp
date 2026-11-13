/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
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
#include "onboardingfinal.h"
#include "ui_onboardingfinal.h"
#include "onboardinghello.h"
#include <QTimer>

#include <statemanager.h>
#include <onboardingmanager.h>

OnboardingFinal::OnboardingFinal(QWidget* parent) :
    OnboardingPage(parent),
    ui(new Ui::OnboardingFinal) {
    ui->setupUi(this);
    ui->label->setProperty("m3OnboardingTitle", 44); // a big expressive title (OnboardingText)

    // M3E: the welcome's shapes again (no greeting), springing in around the
    // edges; the old picture goes
    auto* hello = new OnboardingHello(this);
    hello->lower();
    ui->label_3->setVisible(false);
    QFont body = ui->label_2->font();
    body.setPixelSize(16); // BodyLarge
    ui->label_2->setFont(body);
    ui->finishButton->setProperty("m3ButtonSize", QStringLiteral("m"));
    ui->finishButton->setProperty("m3ButtonStyle", QStringLiteral("filled"));
    this->layout()->setSpacing(16);
}

void OnboardingFinal::resizeEvent(QResizeEvent* event) {
    OnboardingPage::resizeEvent(event);
    if (auto* hello = this->findChild<OnboardingHello*>()) hello->setGeometry(this->rect());
}

OnboardingFinal::~OnboardingFinal() {
    delete ui;
}

void OnboardingFinal::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}

QString OnboardingFinal::name() {
    return "OnboardingFinal";
}

QString OnboardingFinal::displayName() {
    return tr("Done");
}

void OnboardingFinal::on_finishButton_clicked() {
    // A burst of MaterialShapes from the button, then on to the desktop
    QWidget* window = this->window();
    OnboardingCelebration::burst(window, ui->finishButton->mapTo(window, ui->finishButton->rect().center()));
    ui->finishButton->setEnabled(false);
    QTimer::singleShot(900, this, [] {
        StateManager::onboardingManager()->nextStep();
    });
}
