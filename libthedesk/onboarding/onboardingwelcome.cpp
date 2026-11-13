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
#include "onboardingwelcome.h"
#include "ui_onboardingwelcome.h"

#include <statemanager.h>
#include <onboardingmanager.h>
#include <localemanager.h>
#include <tsettings.h>
#include <Wm/desktopwm.h>
#include <QSvgRenderer>
#include <QPainter>
#include <QDBusInterface>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "onboardinghello.h"
#include "onboardingintro.h"
#include "onboardingtext.h"

struct OnboardingWelcomePrivate {
    tSettings settings;
    QSvgRenderer backgroundRenderer;
    QDBusInterface* hostnamed;
    OnboardingHello* hello;
    OnboardingIntro* intro = nullptr; // the opening, in the greeting's place
    QWidget* chips = nullptr;
    bool introStarted = false;
};

OnboardingWelcome::OnboardingWelcome(QWidget* parent) :
    OnboardingPage(parent),
    ui(new Ui::OnboardingWelcome) {
    ui->setupUi(this);

    d = new OnboardingWelcomePrivate();
    d->backgroundRenderer.load(d->settings.value("Onboarding/welcomeGraphic").toString());

    ui->emergencyButton->setVisible(false);

    connect(DesktopWm::instance(), &DesktopWm::currentKeyboardLayoutChanged, this, [ = ] {
        ui->keyboardButton->setText(DesktopWm::currentKeyboardLayout());
    });
    ui->keyboardButton->setText(DesktopWm::currentKeyboardLayout());

    d->hostnamed = new QDBusInterface("org.freedesktop.hostname1", "/org/freedesktop/hostname1", "org.freedesktop.hostname1", QDBusConnection::systemBus(), this);

    if (StateManager::onboardingManager()->isSystemOnboarding()) {
        ui->titleLabel->setText(tr("Welcome to %1!").arg(d->hostnamed->property("OperatingSystemPrettyName").toString()));
        ui->descriptionLabel->setText(tr("You've made it! We'll quickly go through some important setup like setting up accounts and connecting to the Internet, and then you'll be up and running."));
    } else {
        ui->titleLabel->setText(tr("Welcome to %1!").arg(QApplication::applicationName()));
        ui->descriptionLabel->setText(tr("Just a bit of important setup to do before you get started. This won't take long!"));
    }

    // In the manner of Pixel's "Hello": the picture (shapes and the greeting)
    // behind everything, the words and buttons over it
    d->hello = new OnboardingHello(this);
    d->hello->lower();
    delete this->layout(); // the widgets stay; they get a new layout
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(48, 36, 48, 36);
    column->setSpacing(10);
    column->addSpacing(d->hello->greetingSize().height() + 20);
    ui->titleLabel->setProperty("m3NoHeader", true); // Roboto Flex, springing in (OnboardingText)
    ui->descriptionLabel->setMaximumWidth(440);
    column->addWidget(ui->titleLabel);
    column->addWidget(ui->descriptionLabel);
    column->addStretch();
    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(8);
    // The chips: a standard M3E button group (the pressed one widens)
    auto* chips = new QWidget(this);
    chips->setProperty("m3ButtonGroup", QStringLiteral("standard"));
    auto* chipsLayout = new QHBoxLayout(chips);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(8);
    for (QToolButton* button : {ui->languageButton, ui->keyboardButton, ui->emergencyButton}) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
        button->setParent(chips);
        chipsLayout->addWidget(button);
    }
    bottom->addWidget(chips);
    d->chips = chips;
    bottom->addStretch();
    ui->nextButton->setText(tr("Get started"));
    ui->nextButton->setProperty("m3ButtonSize", QStringLiteral("m"));
    ui->nextButton->setProperty("m3ButtonStyle", QStringLiteral("filled"));
    bottom->addWidget(ui->nextButton);
    column->addLayout(bottom);
    ui->label->setVisible(false); // "OTHER OPTIONS"

    // The opening (Onboarding/intro, on by default) plays here first: theDesk
    // written, scribbled away and dropped back into the greeting's place.
    // The greeting, the words and the buttons wait for it
    if (!d->settings.keysInGroup(QStringLiteral("Onboarding")).contains(QStringLiteral("intro")) || d->settings.value(QStringLiteral("Onboarding/intro")).toBool()) {
        d->intro = new OnboardingIntro(this);
        d->hello->holdGreeting();
        this->setProperty("m3HoldText", true);
        d->chips->setVisible(false);
        ui->nextButton->setVisible(false);
        connect(d->intro, &OnboardingIntro::finished, this, [this] {
            d->intro = nullptr;
            d->hello->releaseGreeting(QStringLiteral("theDesk"));
            this->setProperty("m3HoldText", false);
            d->chips->setVisible(true);
            ui->nextButton->setVisible(true);
            OnboardingText::animatePage(this, 250);
        });
    }
}

void OnboardingWelcome::showEvent(QShowEvent* event) {
    OnboardingPage::showEvent(event);
    if (d->intro && !d->introStarted) {
        d->introStarted = true;
        d->intro->setGeometry(this->rect());
        d->intro->raise();
        // Resting exactly where (and as) the greeting draws its words
        d->intro->setPlacement(QPointF(44, 30 + d->hello->greetingSize().height() * 0.78), 104);
        d->intro->start(900); // the card has opened and the shapes come in
    }
}

void OnboardingWelcome::resizeEvent(QResizeEvent* event) {
    OnboardingPage::resizeEvent(event);
    d->hello->setGeometry(this->rect());
    d->hello->setGreetingRect(QRectF(44, 30, this->width() - 88, d->hello->greetingSize().height()));
    if (d->intro) d->intro->setGeometry(this->rect());
}

OnboardingWelcome::~OnboardingWelcome() {
    delete d;
    delete ui;
}

void OnboardingWelcome::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        ui->nextButton->setText(tr("Get started"));
    }
}


QString OnboardingWelcome::name() {
    return "OnboardingWelcome";
}

QString OnboardingWelcome::displayName() {
    return tr("Welcome");
}

void OnboardingWelcome::on_nextButton_clicked() {
    StateManager::onboardingManager()->nextStep();
}

void OnboardingWelcome::on_languageButton_clicked() {
    LocaleManager* manager = StateManager::localeManager();
    bool ok;
    QLocale locale = manager->showLocaleSelector(this->window(), &ok);
    if (ok) {
        if (manager->locales().contains(locale)) manager->removeLocale(locale);
        manager->prependLocale(locale);
    }
}

void OnboardingWelcome::on_keyboardButton_clicked() {
    bool ok;
    QString newLayout = StateManager::localeManager()->showKeyboardLayoutSelector(this->window(), &ok);
    if (!ok) return;

    QStringList layouts = d->settings.delimitedList("Input/keyboard.layouts");
    if (!layouts.contains(newLayout)) {
        layouts.append(newLayout);
        layouts.removeAll("");
        d->settings.setDelimitedList("Input/keyboard.layouts", layouts);
    }

    DesktopWm::setCurrentKeyboardLayout(newLayout);
}


void OnboardingWelcome::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event) // the card and OnboardingHello draw the picture
}
