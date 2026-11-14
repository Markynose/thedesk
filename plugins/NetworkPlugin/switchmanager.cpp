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
#include "switchmanager.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDBusConnectionInterface>
#include <QDBusInterface>

#include <statemanager.h>
#include <statuscentermanager.h>
#include <quickswitch.h>
#include <barmanager.h>
#include <icontextchunk.h>
#include <actionquickwidget.h>

#include <tsettings.h>
#include <tswitch.h>

#include <NetworkManagerQt/Device>
#include <NetworkManagerQt/Manager>

#include <QBoxLayout>
#include <QLabel>
#include <QPointer>
#include "statusCenter/popovers/wirelessnetworkselectionpopover.h"

namespace {
    // Wi-Fi's detail, as Android's Internet dialog: the switch over the
    // networks to pick from (made fresh each time it opens)
    class InternetDetail : public QWidget {
        public:
            explicit InternetDetail(QuickSwitch* wifi) :
                wifi(wifi) {
                this->setWindowTitle(SwitchManager::tr("Internet"));
                this->setWindowIcon(QIcon::fromTheme(QStringLiteral("globe")));
                layout = new QVBoxLayout(this);
                layout->setContentsMargins(0, 0, 0, 0);
                auto* row = new QHBoxLayout();
                QLabel* label = new QLabel(SwitchManager::tr("Wi-Fi"), this);
                QFont font = label->font();
                font.setPixelSize(16);
                font.setWeight(QFont::Medium);
                label->setFont(font);
                toggle = new tSwitch(this);
                label->setBuddy(toggle);
                row->addWidget(label);
                row->addStretch();
                row->addWidget(toggle);
                layout->addLayout(row);
                QObject::connect(toggle, &tSwitch::toggled, wifi, &QuickSwitch::setChecked);
                QObject::connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessEnabledChanged, this, [this](bool enabled) {
                    QSignalBlocker blocker(toggle);
                    toggle->setChecked(enabled);
                });
            }

            QSize sizeHint() const override {
                return QSize(460, 440);
            }

        protected:
            void showEvent(QShowEvent* event) override {
                QWidget::showEvent(event);
                {
                    QSignalBlocker blocker(toggle);
                    toggle->setChecked(wifi->isChecked());
                }
                for (const NetworkManager::Device::Ptr& device : NetworkManager::networkInterfaces()) {
                    if (device->type() != NetworkManager::Device::Wifi) continue;
                    selection = new WirelessNetworkSelectionPopover(device->uni(), this);
                    selection->setTitleVisible(false);
                    QObject::connect(selection, &WirelessNetworkSelectionPopover::done, wifi, &QuickSwitch::detailDone);
                    layout->addWidget(selection, 1);
                    break;
                }
            }

            void hideEvent(QHideEvent* event) override {
                QWidget::hideEvent(event);
                if (selection) selection->deleteLater();
            }

        private:
            QuickSwitch* wifi;
            QVBoxLayout* layout;
            tSwitch* toggle;
            QPointer<WirelessNetworkSelectionPopover> selection;
    };
} // namespace

struct SwitchManagerPrivate {
    QDBusServiceWatcher* nmWatcher;
    QDBusInterface* interface;

    QuickSwitch* flightModeSwitch;
    QuickSwitch* wifiSwitch;
    QuickSwitch* cellularSwitch;
    InternetDetail* internetDetail;

    IconTextChunk* flightModeChunk;

    tSettings settings;
    bool block = false;
};

SwitchManager::SwitchManager(QObject* parent) : QObject(parent) {
    d = new SwitchManagerPrivate();

    d->interface = new QDBusInterface("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.DBus.Properties", QDBusConnection::systemBus(), this);

    d->flightModeSwitch = new QuickSwitch("NetworkFlight");
    d->wifiSwitch = new QuickSwitch("NetworkWireless");
    d->cellularSwitch = new QuickSwitch("NetworkCellular");

    d->flightModeChunk = new IconTextChunk("flight-mode");
    d->flightModeChunk->setText(tr("Flight Mode"));
    d->flightModeChunk->setIcon(QIcon::fromTheme("flight-mode"));

    ActionQuickWidget* flightChunkQuickWidget = new ActionQuickWidget(d->flightModeChunk);
    flightChunkQuickWidget->addAction(QIcon::fromTheme("flight-mode"), tr("Switch off Flight Mode"), [ = ] {
        this->setFlightMode(false);
    });
    d->flightModeChunk->setQuickWidget(flightChunkQuickWidget);

    d->flightModeSwitch->setTitle(tr("Flight Mode"));
    connect(d->flightModeSwitch, &QuickSwitch::toggled, this, &SwitchManager::setFlightMode);
    if (d->settings.value("NetworkPlugin/flight.on").toBool()) {
        this->setFlightMode(true);
    }

    d->wifiSwitch->setTitle(tr("Wi-Fi"));
    d->internetDetail = new InternetDetail(d->wifiSwitch);
    d->wifiSwitch->setDetail(d->internetDetail);
    d->wifiSwitch->setDetailPane(QStringLiteral("NetworkManagerPane"));
    d->wifiSwitch->setChecked(NetworkManager::isWirelessEnabled());
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessEnabledChanged, this, [ = ](bool enabled) {
        QSignalBlocker blocker(d->wifiSwitch);
        d->wifiSwitch->setChecked(enabled);
    });
    connect(d->wifiSwitch, &QuickSwitch::toggled, this, [ = ](bool enabled) {
        d->interface->asyncCall("Set", "org.freedesktop.NetworkManager", "WirelessEnabled", QVariant::fromValue(QDBusVariant(enabled)));
    });

    d->cellularSwitch->setTitle(tr("Cellular"));
    d->cellularSwitch->setChecked(NetworkManager::isWwanEnabled());
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wwanEnabledChanged, this, [ = ](bool enabled) {
        QSignalBlocker blocker(d->cellularSwitch);
        d->cellularSwitch->setChecked(enabled);
    });
    connect(d->cellularSwitch, &QuickSwitch::toggled, this, [ = ](bool enabled) {
        d->interface->asyncCall("Set", "org.freedesktop.NetworkManager", "WwanEnabled", QVariant::fromValue(QDBusVariant(enabled)));
    });

    d->nmWatcher = new QDBusServiceWatcher("org.freedesktop.NetworkManager", QDBusConnection::systemBus());
    connect(d->nmWatcher, &QDBusServiceWatcher::serviceRegistered, this, &SwitchManager::networkManagerRunning);
    connect(d->nmWatcher, &QDBusServiceWatcher::serviceUnregistered, this, &SwitchManager::networkManagerGone);

    if (QDBusConnection::systemBus().interface()->registeredServiceNames().value().contains("org.freedesktop.NetworkManager")) networkManagerRunning();

    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceAdded, this, &SwitchManager::updateDevices);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceRemoved, this, &SwitchManager::updateDevices);
}

SwitchManager::~SwitchManager() {
    d->nmWatcher->deleteLater();
    d->flightModeSwitch->deleteLater();
    d->wifiSwitch->deleteLater();
    d->cellularSwitch->deleteLater();
    d->internetDetail->deleteLater();
    delete d;
}

bool SwitchManager::isFlightModeEnabled() {
    return d->flightModeSwitch->isChecked();
}

void SwitchManager::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        d->flightModeSwitch->setTitle(tr("Flight Mode"));
        d->wifiSwitch->setTitle(tr("Wi-Fi"));
        d->cellularSwitch->setTitle(tr("Cellular"));
    }
}

void SwitchManager::networkManagerRunning() {
    StateManager::statusCenterManager()->addSwitch(d->flightModeSwitch);
    updateDevices();
}

void SwitchManager::networkManagerGone() {
    StatusCenterManager* mgr = StateManager::statusCenterManager();
    mgr->removeSwitch(d->flightModeSwitch);
    mgr->removeSwitch(d->wifiSwitch);
    mgr->removeSwitch(d->cellularSwitch);
}

void SwitchManager::updateDevices() {
    StatusCenterManager* mgr = StateManager::statusCenterManager();
    bool wifi = false;
    bool cellular = false;
    for (NetworkManager::Device::Ptr device : NetworkManager::networkInterfaces()) {
        if (device->type() == NetworkManager::Device::Wifi) wifi = true;
        if (device->type() == NetworkManager::Device::Modem) cellular = true;
    }

    if (wifi) {
        mgr->addSwitch(d->wifiSwitch);
    } else {
        mgr->removeSwitch(d->wifiSwitch);
    }

    if (cellular) {
        mgr->addSwitch(d->cellularSwitch);
    } else {
        mgr->removeSwitch(d->cellularSwitch);
    }
}

void SwitchManager::setFlightMode(bool flightOn) {
    QSignalBlocker blocker(d->flightModeSwitch);
    d->flightModeSwitch->setChecked(flightOn);

    if (flightOn) {
        d->settings.setValue("NetworkPlugin/flight.wifi", NetworkManager::isWirelessEnabled());
        d->settings.setValue("NetworkPlugin/flight.cellular", NetworkManager::isWwanEnabled());
        d->settings.setValue("NetworkPlugin/flight.on", true);

        d->interface->asyncCall("Set", "org.freedesktop.NetworkManager", "WirelessEnabled", QVariant::fromValue(QDBusVariant(false)));
        d->interface->asyncCall("Set", "org.freedesktop.NetworkManager", "WwanEnabled", QVariant::fromValue(QDBusVariant(false)));
        if (!StateManager::instance()->barManager()->isChunkRegistered(d->flightModeChunk)) StateManager::instance()->barManager()->addChunk(d->flightModeChunk);
    } else {
        d->interface->asyncCall("Set", "org.freedesktop.NetworkManager", "WirelessEnabled", QVariant::fromValue(QDBusVariant(d->settings.value("NetworkPlugin/flight.wifi").toBool())));
        d->interface->asyncCall("Set", "org.freedesktop.NetworkManager", "WwanEnabled", QVariant::fromValue(QDBusVariant(d->settings.value("NetworkPlugin/flight.cellular").toBool())));

        d->settings.setValue("NetworkPlugin/flight.on", false);
        if (StateManager::instance()->barManager()->isChunkRegistered(d->flightModeChunk)) StateManager::instance()->barManager()->removeChunk(d->flightModeChunk);
    }
}
