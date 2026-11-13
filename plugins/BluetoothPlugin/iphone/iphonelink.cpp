/****************************************
 *
 *   theDesk - an iPhone's notifications, over Bluetooth Low Energy
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "iphonelink.h"

#include "callcard.h"

#include <BluezQt/Adapter>
#include <BluezQt/Device>
#include <BluezQt/GattCharacteristicRemote>
#include <BluezQt/GattServiceRemote>
#include <BluezQt/PendingCall>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QtEndian>
#include <tlogger.h>
#include <tnotification.h>
#include <tsettings.h>

namespace {
    // Apple Notification Center Service
    const QString AncsService = QStringLiteral("7905f431-b5ce-4e99-a40f-4b1e122d00d0");
    const QString NotificationSource = QStringLiteral("9fbf120d-6301-42d9-8c58-25e699a21dbd");
    const QString ControlPoint = QStringLiteral("69d1d8f3-45e1-49a8-9821-9bbdfdaad9d9");
    const QString DataSource = QStringLiteral("22eac6e9-24d6-4bb5-be44-b36ace7c7bfb");

    enum Event : quint8 {
        Added = 0,
        Modified = 1,
        Removed = 2
    };

    enum Flag : quint8 {
        Silent = 1,
        Important = 2,
        PreExisting = 4,
        PositiveAction = 8,
        NegativeAction = 16
    };

    enum Category : quint8 {
        IncomingCall = 1,
        MissedCall = 2
    };

    enum Command : quint8 {
        GetNotificationAttributes = 0,
        GetAppAttributes = 1,
        PerformNotificationAction = 2
    };

    enum Attribute : quint8 {
        AppIdentifier = 0,
        Title = 1,
        Subtitle = 2,
        Message = 3,
        PositiveActionLabel = 6,
        NegativeActionLabel = 7
    };

    const QString AdvertisementPath = QStringLiteral("/org/thedesk/iphonelink/advertisement");
    constexpr int SetupSeconds = 180;
} // namespace

// ---- What this computer advertises ------------------------------------------

// org.bluez.LEAdvertisement1: connectable, asking for the phone's
// notifications (a solicitation, as Apple asks of accessories)
class IPhoneLinkAdvertisement : public QObject {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.bluez.LEAdvertisement1")
        Q_PROPERTY(QString Type READ type)
        Q_PROPERTY(QStringList SolicitUUIDs READ solicitUuids)
        Q_PROPERTY(QString LocalName READ localName)
        Q_PROPERTY(bool Discoverable READ discoverable)

    public:
        explicit IPhoneLinkAdvertisement(QObject* parent) :
            QObject(parent) {}

        QString type() const {
            return QStringLiteral("peripheral");
        }

        QStringList solicitUuids() const {
            return {AncsService};
        }

        QString localName() const {
            return name;
        }

        bool discoverable() const {
            return setup;
        }

        QString name;
        bool setup = false;

    public slots:
        Q_SCRIPTABLE void Release() {}
};

// ---- One phone's notifications ---------------------------------------------

class AncsClient : public QObject {
        Q_OBJECT
    public:
        AncsClient(BluezQt::GattCharacteristicRemotePtr source, BluezQt::GattCharacteristicRemotePtr control, BluezQt::GattCharacteristicRemotePtr data, QObject* parent) :
            QObject(parent), source(source), control(control), data(data) {
            connect(data.data(), &BluezQt::GattCharacteristicRemote::valueChanged, this, &AncsClient::dataArrived);
            connect(source.data(), &BluezQt::GattCharacteristicRemote::valueChanged, this, &AncsClient::notificationEvent);
            timeout = new QTimer(this);
            timeout->setSingleShot(true);
            timeout->setInterval(5000);
            connect(timeout, &QTimer::timeout, this, [this] {
                tWarn("IPhoneLink") << "No answer from the phone; moving on";
                finishRequest();
            });

            // Data Source first: answers come there. Subscribing may ask the
            // phone to pair (the characteristics need an encrypted link)
            auto* call = data->startNotify();
            connect(call, &BluezQt::PendingCall::finished, this, [this, call] {
                if (call->error()) {
                    tWarn("IPhoneLink") << "Couldn't subscribe to Data Source:" << call->errorText();
                    return;
                }
                auto* sourceCall = this->source->startNotify();
                connect(sourceCall, &BluezQt::PendingCall::finished, this, [sourceCall] {
                    if (sourceCall->error()) tWarn("IPhoneLink") << "Couldn't subscribe to Notification Source:" << sourceCall->errorText();
                    else tInfo("IPhoneLink") << "Receiving the phone's notifications";
                });
            });
        }

        ~AncsClient() override {
            // The phone's gone: so are its notifications and calls
            for (const QPointer<tNotification>& notification : std::as_const(notifications)) {
                if (notification) notification->dismiss();
            }
            for (const QPointer<CallCard>& call : std::as_const(calls)) {
                if (call) call->end();
            }
        }

        QString phoneName; // what the phone calls itself (its name in its settings)

    private:
        struct Request {
                Command command;
                quint32 uid = 0;
                QString appId;
                QList<quint8> attributes;
                quint8 flags = 0;
                quint8 category = 0;
        };

        struct Pending {
                quint32 uid;
                quint8 flags, category;
                QHash<quint8, QString> values;
        };

        BluezQt::GattCharacteristicRemotePtr source, control, data;
        QQueue<Request> requests;
        bool busy = false;
        QByteArray buffer;
        QTimer* timeout;
        QHash<quint32, QPointer<tNotification>> notifications;
        QHash<quint32, QPointer<CallCard>> calls;
        QHash<QString, QString> appNames;
        QList<Pending> waitingForName;

        void notificationEvent(const QByteArray& value) {
            if (value.size() < 8) return;
            const quint8 event = quint8(value.at(0)), flags = quint8(value.at(1)), category = quint8(value.at(2));
            const quint32 uid = qFromLittleEndian<quint32>(value.constData() + 4);
            if (event == Removed) {
                // Dealt with on the phone (a call: answered, declined or over)
                if (QPointer<tNotification> notification = notifications.take(uid)) notification->dismiss();
                if (QPointer<CallCard> call = calls.take(uid)) call->end();
                return;
            }
            // What was already on the phone when it connected stays there
            if (flags & PreExisting) return;
            Request request;
            request.command = GetNotificationAttributes;
            request.uid = uid;
            request.flags = flags;
            request.category = category;
            request.attributes = {AppIdentifier, Title, Subtitle, Message, PositiveActionLabel, NegativeActionLabel};
            requests.enqueue(request);
            sendNext();
        }

        void sendNext() {
            if (busy || requests.isEmpty()) return;
            const Request& request = requests.head();
            QByteArray command;
            command.append(char(request.command));
            if (request.command == GetNotificationAttributes) {
                char uid[4];
                qToLittleEndian<quint32>(request.uid, uid);
                command.append(uid, 4);
                for (quint8 attribute : request.attributes) {
                    command.append(char(attribute));
                    // Title, subtitle and message need a maximum length
                    const quint16 max = attribute == Message ? 300 : attribute == Title || attribute == Subtitle ? 80 : 0;
                    if (max) {
                        char length[2];
                        qToLittleEndian<quint16>(max, length);
                        command.append(length, 2);
                    }
                }
            } else {
                command.append(request.appId.toUtf8());
                command.append('\0');
                command.append(char(0)); // the app's display name
            }
            busy = true;
            buffer.clear();
            timeout->start();
            auto* call = control->writeValue(command, {});
            connect(call, &BluezQt::PendingCall::finished, this, [this, call] {
                if (call->error()) {
                    // Gone already, or an app the phone won't name
                    tDebug("IPhoneLink") << "Control point:" << call->errorText();
                    finishRequest();
                }
            });
        }

        void finishRequest() {
            timeout->stop();
            if (busy && !requests.isEmpty()) {
                const Request request = requests.dequeue();
                if (request.command == GetAppAttributes) {
                    // No name came: use the identifier
                    if (!appNames.contains(request.appId)) appNames.insert(request.appId, request.appId);
                    releaseWaiting(request.appId);
                }
            }
            busy = false;
            buffer.clear();
            sendNext();
        }

        void dataArrived(const QByteArray& value) {
            // An answer, maybe in several pieces
            if (!busy || requests.isEmpty()) return;
            buffer.append(value);
            const Request& request = requests.head();
            QHash<quint8, QString> values;
            int at = 1;
            if (request.command == GetNotificationAttributes) {
                at = 5;
            } else {
                const int end = buffer.indexOf('\0', 1);
                if (end < 0) return;
                at = end + 1;
            }
            for (int i = 0; i < (request.command == GetNotificationAttributes ? request.attributes.size() : 1); i++) {
                if (buffer.size() < at + 3) return; // more to come
                const quint8 id = quint8(buffer.at(at));
                const quint16 length = qFromLittleEndian<quint16>(buffer.constData() + at + 1);
                if (buffer.size() < at + 3 + length) return;
                values.insert(id, QString::fromUtf8(buffer.mid(at + 3, length)));
                at += 3 + length;
            }

            if (request.command == GetNotificationAttributes) {
                Pending pending{request.uid, request.flags, request.category, values};
                const QString appId = values.value(AppIdentifier);
                if (appNames.contains(appId) || appId.isEmpty()) {
                    show(pending);
                } else {
                    waitingForName.append(pending);
                    bool asked = false;
                    for (const Request& queued : std::as_const(requests)) {
                        if (queued.command == GetAppAttributes && queued.appId == appId) asked = true;
                    }
                    if (!asked) {
                        Request nameRequest;
                        nameRequest.command = GetAppAttributes;
                        nameRequest.appId = appId;
                        requests.enqueue(nameRequest);
                    }
                }
            } else {
                const QString name = values.value(0);
                appNames.insert(request.appId, name.isEmpty() ? request.appId : name);
                releaseWaiting(request.appId);
            }
            finishRequest();
        }

        void releaseWaiting(const QString& appId) {
            for (int i = 0; i < waitingForName.size(); i++) {
                if (waitingForName.at(i).values.value(AppIdentifier) != appId) continue;
                show(waitingForName.takeAt(i));
                i--;
            }
        }

        void show(const Pending& pending) {
            // A call rings as a card of its own (it needs a compositor)
            if (pending.category == IncomingCall && CallCard::available()) {
                if (calls.value(pending.uid)) return; // already ringing
                const quint32 uid = pending.uid;
                QString caller = pending.values.value(Title);
                if (caller.isEmpty()) caller = tr("Unknown caller");
                calls.insert(uid, new CallCard(caller, phoneName, [this, uid](bool answer) {
                    perform(uid, answer ? 0 : 1);
                }));
                return;
            }
            QPointer<tNotification> notification = notifications.value(pending.uid);
            if (!notification) {
                notification = new tNotification();
                notifications.insert(pending.uid, notification);
                const quint32 uid = pending.uid;
                connect(notification, &tNotification::actionClicked, this, [this, uid](QString key) {
                    perform(uid, key == QStringLiteral("positive") ? 0 : 1);
                });
                connect(notification, &tNotification::dismissed, this, [this, uid] {
                    // Gone from theDesk; it stays on the phone
                    if (QPointer<tNotification> gone = notifications.take(uid)) gone->deleteLater();
                });
            }
            const QString appId = pending.values.value(AppIdentifier);
            const QString title = pending.values.value(Title);
            const QString subtitle = pending.values.value(Subtitle);
            notification->setAppName(appNames.value(appId, tr("iPhone")));
            notification->setAppIcon(QStringLiteral("phone"));
            notification->setSummary(subtitle.isEmpty() ? title : tr("%1 · %2").arg(title, subtitle));
            notification->setText(pending.values.value(Message).toHtmlEscaped());
            notification->clearActions();
            if (pending.flags & PositiveAction) notification->insertAction(QStringLiteral("positive"), pending.values.value(PositiveActionLabel));
            if (pending.flags & NegativeAction) notification->insertAction(QStringLiteral("negative"), pending.values.value(NegativeActionLabel));
            const bool call = pending.category == IncomingCall;
            notification->setUrgency(call || (pending.flags & Important) ? tNotification::Critical : tNotification::Normal);
            if (call) notification->setTimeout(0);
            notification->setSoundOn(!(pending.flags & Silent));
            notification->post(false);
        }

        void perform(quint32 uid, quint8 action) {
            QByteArray command;
            command.append(char(PerformNotificationAction));
            char bytes[4];
            qToLittleEndian<quint32>(uid, bytes);
            command.append(bytes, 4);
            command.append(char(action));
            control->writeValue(command, {});
        }
};

// ---- Linking, and the phones that come and go ----------------------------

struct IPhoneLinkPrivate {
        BluezQt::ManagerPtr manager;
        mutable tSettings settings;
        QHash<QString, QPointer<AncsClient>> clients; // by address
        QDateTime setupUntil;
        QPointer<tNotification> setupNotification;
        IPhoneLinkAdvertisement* advertisement = nullptr;
        QString registeredOn; // the adapter the advertisement is up on
        QSet<QString> pairing; // addresses being asked to pair
        QHash<QString, int> battery;
        bool unregistering = false;

        QStringList linked() const {
            return settings.value(QStringLiteral("Bluetooth/iPhones")).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
        }
};

namespace {
    IPhoneLink* current = nullptr;
}

IPhoneLink::IPhoneLink(BluezQt::ManagerPtr manager, QObject* parent) :
    QObject(parent) {
    d = new IPhoneLinkPrivate();
    d->manager = manager;
    current = this;

    d->advertisement = new IPhoneLinkAdvertisement(this);
    QDBusConnection::systemBus().registerObject(AdvertisementPath, d->advertisement, QDBusConnection::ExportAllProperties | QDBusConnection::ExportScriptableSlots);

    connect(manager.data(), &BluezQt::Manager::deviceAdded, this, &IPhoneLink::watch);
    connect(manager.data(), &BluezQt::Manager::bluetoothOperationalChanged, this, &IPhoneLink::updateAdvertising);
    connect(manager.data(), &BluezQt::Manager::usableAdapterChanged, this, &IPhoneLink::updateAdvertising);
    for (const BluezQt::DevicePtr& device : manager->devices()) watch(device);

    // Only phones that really paired count as linked (an attempt that
    // failed left its address behind)
    QStringList linked;
    for (const QString& address : d->linked()) {
        BluezQt::DevicePtr device = manager->deviceForAddress(address);
        if (device && device->isPaired()) linked.append(address);
    }
    if (linked != d->linked()) d->settings.setValue(QStringLiteral("Bluetooth/iPhones"), linked.join(QLatin1Char(',')));
    updateAdvertising();
}

IPhoneLink::~IPhoneLink() {
    if (current == this) current = nullptr;
    advertise(false);
    QDBusConnection::systemBus().unregisterObject(AdvertisementPath);
    delete d;
}

IPhoneLink* IPhoneLink::instance() {
    return current;
}

bool IPhoneLink::isLinked(const QString& address) const {
    return d->linked().contains(address);
}

int IPhoneLink::battery(const QString& address) const {
    return d->battery.value(address, -1);
}

bool IPhoneLink::audioAllowed() {
    tSettings settings;
    return settings.value(QStringLiteral("Bluetooth/iPhoneAudio")).toBool();
}

bool IPhoneLink::isAudioProfile(const QString& uuid) {
    // A2DP (source, sink), headset and hands-free (both ends)
    static const QStringList audio = {QStringLiteral("0000110a"), QStringLiteral("0000110b"), QStringLiteral("00001108"), QStringLiteral("00001112"), QStringLiteral("0000111e"), QStringLiteral("0000111f")};
    return audio.contains(uuid.left(8).toLower());
}

void IPhoneLink::keepSoundOnPhone(BluezQt::DevicePtr device) {
    // Already playing here (it connected as a speaker before): hang that up
    if (audioAllowed() || !device->mediaTransport()) return;
    tInfo("IPhoneLink") << "Keeping the iPhone's sound on the iPhone";
    device->disconnectProfile(QStringLiteral("0000110a-0000-1000-8000-00805f9b34fb"));
}

void IPhoneLink::startSetup() {
    BluezQt::AdapterPtr adapter = d->manager->usableAdapter();
    if (!adapter) return;
    d->setupUntil = QDateTime::currentDateTime().addSecs(SetupSeconds);
    // Visible and pairable for a few minutes
    adapter->setPairable(true);
    adapter->setDiscoverableTimeout(SetupSeconds);
    adapter->setDiscoverable(true);
    QTimer::singleShot(SetupSeconds * 1000 + 500, this, &IPhoneLink::updateAdvertising);

    if (d->setupNotification) d->setupNotification->dismiss();
    auto* notification = new tNotification();
    notification->setAppName(tr("Bluetooth"));
    notification->setAppIcon(QStringLiteral("phone"));
    notification->setSummary(tr("Link your iPhone"));
    notification->setText(tr("On your iPhone, open Settings, then Bluetooth, and tap %1. Confirm the code on both.").arg(QLocale().quoteString(adapter->name())));
    notification->setTimeout(SetupSeconds * 1000);
    notification->post();
    d->setupNotification = notification;

    // Re-register, so the phone sees the name and the discoverable flag
    advertise(false);
    updateAdvertising();
}

void IPhoneLink::watch(BluezQt::DevicePtr device) {
    auto again = [this, device] {
        look(device);
    };
    connect(device.data(), &BluezQt::Device::connectedChanged, this, again);
    connect(device.data(), &BluezQt::Device::servicesResolvedChanged, this, again);
    connect(device.data(), &BluezQt::Device::pairedChanged, this, again);
    connect(device.data(), &BluezQt::Device::gattServiceAdded, this, [this, device](BluezQt::GattServiceRemotePtr service) {
        // Its characteristics follow the service
        connect(service.data(), &BluezQt::GattServiceRemote::gattCharacteristicAdded, this, [this, device] {
            look(device);
        });
        look(device);
    });
    look(device);
}

void IPhoneLink::look(BluezQt::DevicePtr device) {
    const QString address = device->address();
    if (!device->isConnected()) {
        if (QPointer<AncsClient> client = d->clients.take(address)) client->deleteLater();
        updateAdvertising();
        return;
    }
    if (d->clients.value(address)) return;

    BluezQt::GattCharacteristicRemotePtr source, control, data;
    for (const BluezQt::GattServiceRemotePtr& service : device->gattServices()) {
        if (service->uuid().toLower() != AncsService) continue;
        for (const BluezQt::GattCharacteristicRemotePtr& characteristic : service->characteristics()) {
            const QString uuid = characteristic->uuid().toLower();
            if (uuid == NotificationSource) source = characteristic;
            else if (uuid == ControlPoint) control = characteristic;
            else if (uuid == DataSource) data = characteristic;
        }
    }
    if (!source || !control || !data) return;

    // Its notifications only come over a paired (encrypted) link. The phone
    // connected to us, so ask it to pair: both show a code to confirm (the
    // plugin's agent asks here). Only while linking, or a phone linked before
    const bool setup = d->setupUntil.isValid() && d->setupUntil > QDateTime::currentDateTime();
    if (!device->isPaired()) {
        if (!setup && !d->linked().contains(address)) return;
        if (d->pairing.contains(address)) return;
        d->pairing.insert(address);
        tInfo("IPhoneLink") << "An iPhone connected; pairing";
        auto* call = device->pair();
        connect(call, &BluezQt::PendingCall::finished, this, [this, call, device, address] {
            d->pairing.remove(address);
            if (call->error()) tWarn("IPhoneLink") << "Pairing failed:" << call->errorText();
            look(device);
        });
        return;
    }

    tInfo("IPhoneLink") << "An iPhone connected; asking for its notifications";
    auto* client = new AncsClient(source, control, data, this);
    // The phone's own name, if it shares it once paired ("iPhone" if not)
    client->phoneName = device->name();
    connect(device.data(), &BluezQt::Device::nameChanged, client, [client](const QString& name) {
        client->phoneName = name;
    });
    d->clients.insert(address, client);
    // Not trusted: then each profile it opens is asked about (the agent),
    // and sound is turned away, so the phone doesn't take this computer for
    // a speaker. Its notifications don't need trust; it comes back by itself
    if (device->isTrusted() && !audioAllowed()) device->setTrusted(false);
    keepSoundOnPhone(device);
    connect(device.data(), &BluezQt::Device::mediaTransportChanged, client, [this, device] {
        keepSoundOnPhone(device);
    });

    // Its battery, from its own battery service
    for (const BluezQt::GattServiceRemotePtr& service : device->gattServices()) {
        if (service->uuid().toLower() != QStringLiteral("0000180f-0000-1000-8000-00805f9b34fb")) continue;
        for (const BluezQt::GattCharacteristicRemotePtr& level : service->characteristics()) {
            if (level->uuid().toLower() != QStringLiteral("00002a19-0000-1000-8000-00805f9b34fb")) continue;
            auto take = [this, address](const QByteArray& value) {
                if (value.isEmpty()) return;
                d->battery.insert(address, quint8(value.at(0)));
                emit batteryChanged(address);
            };
            connect(level.data(), &BluezQt::GattCharacteristicRemote::valueChanged, client, take);
            auto* read = level->readValue({});
            connect(read, &BluezQt::PendingCall::finished, client, [read, take] {
                if (!read->error()) take(read->value().toByteArray());
            });
            level->startNotify();
        }
    }
    QStringList linked = d->linked();
    if (!linked.contains(address)) {
        linked.append(address);
        d->settings.setValue(QStringLiteral("Bluetooth/iPhones"), linked.join(QLatin1Char(',')));
        emit linkedChanged();
    }
    // Linked: no need to stay visible
    if (d->setupUntil.isValid()) {
        d->setupUntil = QDateTime();
        if (d->setupNotification) d->setupNotification->dismiss();
        if (BluezQt::AdapterPtr adapter = d->manager->usableAdapter()) adapter->setDiscoverable(false);
    }
    updateAdvertising();
}

void IPhoneLink::updateAdvertising() {
    const bool setup = d->setupUntil.isValid() && d->setupUntil > QDateTime::currentDateTime();
    bool away = false;
    for (const QString& address : d->linked()) {
        if (!d->clients.value(address)) away = true;
    }
    // Up while linking, and while a linked phone is away (so it can find
    // this computer and connect when it's back)
    if (d->advertisement->setup != setup) {
        advertise(false);
        d->advertisement->setup = setup;
    }
    advertise(d->manager->isBluetoothOperational() && (setup || away));
}

void IPhoneLink::advertise(bool on) {
    BluezQt::AdapterPtr adapter = d->manager ? d->manager->usableAdapter() : BluezQt::AdapterPtr();
    const QString path = adapter ? adapter->ubi() : QString();
    if (d->unregistering) return; // asks again once that's done
    if (!d->registeredOn.isEmpty() && (!on || d->registeredOn != path)) {
        // Take it down first; registering again before BlueZ has let go of
        // it fails ("Already Exists")
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"), d->registeredOn, QStringLiteral("org.bluez.LEAdvertisingManager1"), QStringLiteral("UnregisterAdvertisement"));
        message.setArguments({QVariant::fromValue(QDBusObjectPath(AdvertisementPath))});
        d->registeredOn.clear();
        d->unregistering = true;
        auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* watcher) {
            watcher->deleteLater();
            d->unregistering = false;
            updateAdvertising();
        });
        return;
    }
    if (!on || path.isEmpty() || d->registeredOn == path) return;

    d->advertisement->name = adapter->name();
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"), path, QStringLiteral("org.bluez.LEAdvertisingManager1"), QStringLiteral("RegisterAdvertisement"));
    message.setArguments({QVariant::fromValue(QDBusObjectPath(AdvertisementPath)), QVariantMap()});
    d->registeredOn = path;
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher* watcher) {
        watcher->deleteLater();
        if (watcher->isError() && watcher->error().name() != QStringLiteral("org.bluez.Error.AlreadyExists")) {
            tWarn("IPhoneLink") << "Couldn't advertise:" << watcher->error().message();
            if (d->registeredOn == path) d->registeredOn.clear();
        } else {
            tDebug("IPhoneLink") << "Advertising" << (d->advertisement->setup ? "for linking" : "for a linked iPhone");
        }
    });
}

#include "iphonelink.moc"
