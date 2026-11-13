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
#include "onboardingnetwork.h"
#include "ui_onboardingnetwork.h"

#include <statemanager.h>
#include <onboardingmanager.h>

#include "wiredonboardingsetup.h"
#include "wirelessonboardingsetup.h"

#include <NetworkManagerQt/Manager>
#include <QBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <cmath>
#include <m3motion.h>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>

namespace {
    // "Connected": a check in a Sunny shape, springing in
    class ConnectedBadge : public QWidget {
        public:
            explicit ConnectedBadge(QWidget* parent) :
                QWidget(parent) {
                this->setFixedSize(96, 96);
                spring = new M3SpringAnimation(this);
                spring->setSpring({0.5, 600}); // bouncy
                spring->setStartValue(0.0);
                QObject::connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
            }

            void play() {
                spring->snapTo(0.0);
                spring->animateTo(1.0);
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                const double k = qMax(0.0, spring->currentValue().toDouble());
                if (k <= 0.01) return;
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                const M3::Scheme& s = M3::currentScheme();
                const double side = 88 * k;
                const QPointF c = QRectF(this->rect()).center();
                QTransform spin;
                spin.translate(c.x(), c.y());
                spin.rotate((1 - k) * -120);
                painter.fillPath(spin.map(m3shapes::toPath(MaterialShapes::sunny(), QRectF(-side / 2, -side / 2, side, side))), s.primary);
                QFont symbol(QStringLiteral("Material Symbols Rounded"));
                symbol.setPixelSize(qMax(1, int(44 * k)));
                symbol.setVariableAxis(QFont::Tag("wght"), 600);
                painter.setFont(symbol);
                painter.setPen(s.onPrimary);
                painter.drawText(QRectF(this->rect()), Qt::AlignCenter, QStringLiteral("check"));
            }

        private:
            M3SpringAnimation* spring;
    };
} // namespace

OnboardingNetwork::OnboardingNetwork(QWidget* parent) :
    OnboardingPage(parent),
    ui(new Ui::OnboardingNetwork) {
    ui->setupUi(this);

    ui->titleLabel->setBackButtonShown(true);
    ui->disconnectButton->setProperty("type", "destructive");
    // Connected: a check springing in (the step's hero already shows Wi-Fi),
    // "You're connected!" in Roboto Flex, and which network
    ui->networkConnectedIcon->setVisible(false);
    if (auto* box = qobject_cast<QBoxLayout*>(ui->networkConnectedIcon->parentWidget()->layout())) {
        connectedBadge = new ConnectedBadge(ui->networkConnectedIcon->parentWidget());
        box->insertWidget(box->indexOf(ui->networkConnectedIcon), connectedBadge, 0, Qt::AlignHCenter);
        connectedName = new QLabel(ui->networkConnectedIcon->parentWidget());
        connectedName->setAlignment(Qt::AlignHCenter);
        box->insertWidget(box->indexOf(ui->label_2) + 1, connectedName);
    }
    {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(24);
        font.setVariableAxis(QFont::Tag("wght"), 600);
        ui->label_2->setFont(font);
    }
    ui->stackedWidget->setCurrentAnimation(tStackedWidget::Fade);

    connect(NetworkManager::notifier(), &NetworkManager::Notifier::connectivityChanged, this, &OnboardingNetwork::updateConnectivity);
    updateConnectivity();

    //Find out what network devices are available
    for (const NetworkManager::Device::Ptr& device : NetworkManager::networkInterfaces()) {
        QWidget* setupPane = nullptr;
        switch (device->type()) {
            case NetworkManager::Device::Ethernet:
                setupPane = new WiredOnboardingSetup(device->uni());
                break;
            case NetworkManager::Device::Wifi:
                setupPane = new WirelessOnboardingSetup(device->uni());
                break;
            default:
                break;
        }

        if (setupPane) {
            ui->panesLayout->addWidget(setupPane);
        }
    }
}

OnboardingNetwork::~OnboardingNetwork() {
    delete ui;
}

void OnboardingNetwork::updateConnectivity() {
    switch (NetworkManager::connectivity()) {
        case NetworkManager::UnknownConnectivity:
        case NetworkManager::NoConnectivity:
            ui->skipInfoPrompt->setVisible(true);
            ui->nextButton->setText(tr("Continue without Internet"));
            ui->stackedWidget->setCurrentWidget(ui->networkSelectionPage);
            break;
        case NetworkManager::Limited:
        case NetworkManager::Full:
            if (connectedName) {
                NetworkManager::ActiveConnection::Ptr primary = NetworkManager::primaryConnection();
                connectedName->setText(primary ? tr("Connected to %1").arg(primary->id()) : QString());
            }
            ui->skipInfoPrompt->setVisible(false);
            ui->nextButton->setText(tr("Next"));
            ui->stackedWidget->setCurrentWidget(ui->networkConnectionCompletePage);
            break;
        case NetworkManager::Portal:
            //TODO: something cool, maybe pop open a browser or something
            ui->skipInfoPrompt->setVisible(false);
            ui->nextButton->setText(tr("Next"));
            ui->stackedWidget->setCurrentWidget(ui->networkConnectionCompletePage);
            break;
    }
}

QString OnboardingNetwork::name() {
    return QStringLiteral("OnboardingNetwork");
}

QString OnboardingNetwork::displayName() {
    return tr("Network");
}

void OnboardingNetwork::pageActivated() {
    const bool connected = NetworkManager::connectivity() == NetworkManager::Full || NetworkManager::connectivity() == NetworkManager::Limited;
    if (connected && connectedBadge) static_cast<ConnectedBadge*>(connectedBadge)->play();

    // Already online the first time here: a moment to see it, then on. Only
    // that first visit (Back still comes here; it doesn't move on by itself
    // again), and not if the user has moved on or back in the meantime
    if (autoAdvanced) return;
    autoAdvanced = true;
    if (!connected) return;
    if (!countingMoves) {
        countingMoves = true;
        for (auto signal : {&OnboardingManager::nextStep, &OnboardingManager::previousStep}) {
            connect(StateManager::onboardingManager(), signal, this, [this] {
                stepMoves++;
            });
        }
    }
    const int movesThen = stepMoves;
    QTimer::singleShot(1800, this, [this, movesThen] {
        const bool still = NetworkManager::connectivity() == NetworkManager::Full || NetworkManager::connectivity() == NetworkManager::Limited;
        if (still && stepMoves == movesThen && this->isVisible()) StateManager::onboardingManager()->nextStep();
    });
}

void OnboardingNetwork::on_titleLabel_backButtonClicked() {
    StateManager::onboardingManager()->previousStep();
}

void OnboardingNetwork::on_nextButton_clicked() {
    StateManager::onboardingManager()->nextStep();
}


void OnboardingNetwork::on_disconnectButton_clicked() {
    //Disconnect from all active connections
    for (NetworkManager::ActiveConnection::Ptr connection : NetworkManager::activeConnections()) {
        NetworkManager::deactivateConnection(connection->path());
    }
}

