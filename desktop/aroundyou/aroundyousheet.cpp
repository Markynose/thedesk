/****************************************
 *
 *   theDesk - Around you: the sheet, the bar's shapes, USB drives
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "aroundyousheet.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDateTime>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <aroundyou.h>
#include <barmanager.h>
#include <hudmanager.h>
#include <keygrab.h>
#include <cmath>
#include <m3motion.h>
#include <m3scheme.h>
#include <memory>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>
#include <tpopover.h>
#include <transparentdialog.h>

namespace {
    constexpr int Margin = 24;

    QFont flex(double px, double wght, double wdth = 100) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        font.setVariableAxis(QFont::Tag("wdth"), float(wdth));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        return font;
    }

    QPointer<tPopover> openSheet;

    // The bar shows what's in use, not everything nearby
    QList<AroundYouDevice> connectedDevices() {
        QList<AroundYouDevice> devices = AroundYou::instance()->devices();
        devices.removeIf([](const AroundYouDevice& device) {
            return !device.connected || !device.inBar;
        });
        return devices;
    }
} // namespace

// ---- USB drives, from UDisks2 -----------------------------------------------

using InterfaceMap = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;
Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjects)

namespace {
    const QString UDisks = QStringLiteral("org.freedesktop.UDisks2");

    QString fromBytes(const QVariant& value) {
        QByteArray bytes = value.toByteArray();
        if (bytes.isEmpty() && value.canConvert<QDBusArgument>()) {
            const QDBusArgument argument = value.value<QDBusArgument>();
            argument >> bytes;
        }
        while (bytes.endsWith('\0')) bytes.chop(1);
        return QString::fromUtf8(bytes);
    }

    QStringList mountPoints(const QVariant& value) {
        QStringList points;
        if (!value.canConvert<QDBusArgument>()) return points;
        const QDBusArgument argument = value.value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd()) {
            QByteArray bytes;
            argument >> bytes;
            while (bytes.endsWith('\0')) bytes.chop(1);
            points.append(QString::fromUtf8(bytes));
        }
        argument.endArray();
        return points;
    }

    QSet<QString> knownDrives;
    bool drivesReady = false;

    void refreshDrives() {
        QDBusMessage call = QDBusMessage::createMethodCall(UDisks, QStringLiteral("/org/freedesktop/UDisks2"), QStringLiteral("org.freedesktop.DBus.ObjectManager"), QStringLiteral("GetManagedObjects"));
        auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), AroundYou::instance());
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, AroundYou::instance(), [](QDBusPendingCallWatcher* watcher) {
            watcher->deleteLater();
            QDBusReply<ManagedObjects> reply = *watcher;
            if (!reply.isValid()) return;
            const ManagedObjects objects = reply.value();

            QSet<QString> seen;
            for (auto object = objects.cbegin(); object != objects.cend(); object++) {
                const InterfaceMap& interfaces = object.value();
                if (!interfaces.contains(QStringLiteral("org.freedesktop.UDisks2.Filesystem")) || !interfaces.contains(QStringLiteral("org.freedesktop.UDisks2.Block"))) continue;
                const QVariantMap block = interfaces.value(QStringLiteral("org.freedesktop.UDisks2.Block"));
                if (block.value(QStringLiteral("HintIgnore")).toBool() || block.value(QStringLiteral("HintSystem")).toBool()) continue;
                const QString drivePath = block.value(QStringLiteral("Drive")).value<QDBusObjectPath>().path();
                const InterfaceMap driveObject = objects.value(QDBusObjectPath(drivePath));
                const QVariantMap drive = driveObject.value(QStringLiteral("org.freedesktop.UDisks2.Drive"));
                const bool removable = drive.value(QStringLiteral("Removable")).toBool() || drive.value(QStringLiteral("ConnectionBus")).toString() == QStringLiteral("usb");
                if (!removable) continue;

                const QString blockPath = object.key().path();
                const QString serial = drive.value(QStringLiteral("Serial")).toString();
                const QString id = QStringLiteral("drive:") + (serial.isEmpty() ? blockPath : serial + QLatin1Char(':') + block.value(QStringLiteral("IdUUID")).toString());
                seen.insert(id);

                QString name = block.value(QStringLiteral("IdLabel")).toString();
                if (name.isEmpty()) name = drive.value(QStringLiteral("Model")).toString().trimmed();
                if (name.isEmpty()) name = AroundYou::tr("USB drive");
                const QStringList points = mountPoints(interfaces.value(QStringLiteral("org.freedesktop.UDisks2.Filesystem")).value(QStringLiteral("MountPoints")));
                const qulonglong size = block.value(QStringLiteral("Size")).toULongLong();

                AroundYouDevice device;
                device.id = id;
                device.symbol = QStringLiteral("usb");
                device.name = name;
                device.state = points.isEmpty() ? QLocale().formattedDataSize(qint64(size)) : AroundYou::tr("Open at %1").arg(points.first());
                device.activate = [blockPath, points] {
                    if (!points.isEmpty()) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(points.first()));
                        return;
                    }
                    // Mount it, then open it
                    QDBusMessage mount = QDBusMessage::createMethodCall(UDisks, blockPath, QStringLiteral("org.freedesktop.UDisks2.Filesystem"), QStringLiteral("Mount"));
                    mount.setArguments({QVariantMap()});
                    auto* w = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(mount), AroundYou::instance());
                    QObject::connect(w, &QDBusPendingCallWatcher::finished, AroundYou::instance(), [](QDBusPendingCallWatcher* w) {
                        w->deleteLater();
                        QDBusReply<QString> path = *w;
                        if (path.isValid()) QDesktopServices::openUrl(QUrl::fromLocalFile(path.value()));
                    });
                };
                const QString drivePathCopy = drivePath;
                device.actionSymbol = QStringLiteral("eject");
                device.action = [blockPath, drivePathCopy, points] {
                    // Unmount, then eject (or power off) the drive
                    auto ejectDrive = [drivePathCopy] {
                        QDBusMessage eject = QDBusMessage::createMethodCall(UDisks, drivePathCopy, QStringLiteral("org.freedesktop.UDisks2.Drive"), QStringLiteral("Eject"));
                        eject.setArguments({QVariantMap()});
                        QDBusConnection::systemBus().asyncCall(eject);
                    };
                    if (points.isEmpty()) return ejectDrive();
                    QDBusMessage unmount = QDBusMessage::createMethodCall(UDisks, blockPath, QStringLiteral("org.freedesktop.UDisks2.Filesystem"), QStringLiteral("Unmount"));
                    unmount.setArguments({QVariantMap()});
                    auto* w = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(unmount), AroundYou::instance());
                    QObject::connect(w, &QDBusPendingCallWatcher::finished, AroundYou::instance(), [ejectDrive](QDBusPendingCallWatcher* w) {
                        w->deleteLater();
                        if (!w->isError()) {
                            ejectDrive();
                            StateManager::hudManager()->showHud({
                                {QStringLiteral("icon"), QStringLiteral("media-eject")},
                                {QStringLiteral("title"), AroundYou::tr("USB drive")},
                                {QStringLiteral("text"), AroundYou::tr("Safe to remove")}
                            });
                        }
                    });
                };
                // In the sheet: onto this computer mounts it, away ejects it
                device.connected = !points.isEmpty();
                device.connect = [blockPath] {
                    QDBusMessage mount = QDBusMessage::createMethodCall(UDisks, blockPath, QStringLiteral("org.freedesktop.UDisks2.Filesystem"), QStringLiteral("Mount"));
                    mount.setArguments({QVariantMap()});
                    QDBusConnection::systemBus().asyncCall(mount);
                };
                device.disconnect = device.action;
                AroundYou::instance()->setDevice(device, drivesReady && !knownDrives.contains(id));
            }
            for (const QString& id : std::as_const(knownDrives)) {
                if (!seen.contains(id)) AroundYou::instance()->removeDevice(id);
            }
            knownDrives = seen;
            drivesReady = true;
        });
    }

    void watchDrives() {
        qDBusRegisterMetaType<InterfaceMap>();
        qDBusRegisterMetaType<ManagedObjects>();
        QDBusConnection bus = QDBusConnection::systemBus();
        auto* refresh = new QTimer(AroundYou::instance());
        refresh->setSingleShot(true);
        refresh->setInterval(300);
        QObject::connect(refresh, &QTimer::timeout, AroundYou::instance(), &refreshDrives);
        auto* relay = new QObject(AroundYou::instance());
        // Anything added, removed or changed (a mount): look again
        for (const QString& signal : {QStringLiteral("InterfacesAdded"), QStringLiteral("InterfacesRemoved")}) {
            bus.connect(UDisks, QStringLiteral("/org/freedesktop/UDisks2"), QStringLiteral("org.freedesktop.DBus.ObjectManager"), signal, refresh, SLOT(start()));
        }
        bus.connect(UDisks, QString(), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), refresh, SLOT(start()));
        Q_UNUSED(relay)
        refreshDrives();
    }
} // namespace

// ---- Stand-in devices for testing (TD_AROUNDYOU_DEMO) -----------------------

namespace {
    // Pretend devices that connect and let go without touching real hardware
    void publishDemo(const QString& id, bool connected) {
        struct Demo {
                const char *id, *symbol, *name, *on, *off;
        };
        static const Demo demos[] = {
            {"demo:tv",         "tv",           "TV",         "Mirroring",              "Off"   },
            {"demo:headphones", "headphones",   "Headphones", "Connected \u00b7 80% battery", "Paired"},
            {"demo:usb",        "usb",          "USB drive",  "Open \u00b7 12 GB free",  "16 GB" },
            {"demo:phone",      "phone_iphone", "iPhone",     "Connected",              "Nearby"}
        };
        for (const Demo& demo : demos) {
            if (id != QLatin1String(demo.id)) continue;
            AroundYouDevice device;
            device.id = id;
            device.symbol = QString::fromLatin1(demo.symbol);
            device.name = QString::fromLatin1(demo.name);
            device.connected = connected;
            device.state = QString::fromUtf8(connected ? demo.on : demo.off);
            // Answers a moment later, like real hardware
            auto later = [id](bool on) {
                QTimer::singleShot(900, AroundYou::instance(), [id, on] {
                    publishDemo(id, on);
                });
            };
            if (id == QLatin1String("demo:tv")) {
                device.choices = {
                    {QStringLiteral("Extend"), [later] { later(true); }},
                    {QStringLiteral("Mirror"), [later] { later(true); }},
                    {QStringLiteral("Only TV"), [later] { later(true); }}
                };
            } else {
                device.connect = [later] {
                    later(true);
                };
            }
            device.disconnect = [later] {
                later(false);
            };
            AroundYou::instance()->setDevice(device);
        }
    }

    void addDemoDevices() {
        publishDemo(QStringLiteral("demo:tv"), false);
        publishDemo(QStringLiteral("demo:headphones"), false);
        publishDemo(QStringLiteral("demo:usb"), true);
        publishDemo(QStringLiteral("demo:phone"), false);
    }
} // namespace

// ---- The sheet: the devices orbiting this computer ------------------------
//
// This computer is the shape in the middle. What's in use sits on the inner
// ring, joined to it; what's only nearby floats on the outer ring. Dragging a
// device onto the computer connects it (or asks how to use it, for a
// display); dragging it away stretches the link until it lets go.

namespace {
    constexpr double RingIn = 110;
    constexpr double RingOut = 176;
    constexpr double DeviceSize = 64;
    constexpr double CentreSize = 112;
    constexpr double OrbitTop = 96;     // the header above the outer ring's top device
    constexpr double ChoiceHeight = 40; // the connected button group

    // A spring stepped by the sheet's own clock (every device has several)
    struct Spring {
            double x = 0, v = 0, target = 0;
            M3Motion::Spring spec = M3Motion::spring(M3Motion::SpatialDefault);

            void step(double dt) {
                const double k = spec.stiffness, c = 2 * spec.dampingRatio * std::sqrt(k);
                const int n = qMax(1, int(std::ceil(dt / 0.002)));
                const double h = dt / n;
                for (int i = 0; i < n; i++) {
                    v += (-k * (x - target) - c * v) * h;
                    x += v * h;
                }
            }

            void snap(double value) {
                x = target = value;
                v = 0;
            }
    };

    struct Body {
            Spring x, y, scale, lift, neck;
            std::shared_ptr<m3shapes::Morph> morph; // its shape to a circle, when lifted
            AroundYou::Look look;
            bool fresh = true; // springs out of the centre on the first tick
            // The link has just snapped: each half pulls back into its shape
            // as a drop (0 just broken, 1 gone)
            Spring snap;
            bool snapping = false;
    };

    struct Pending {
            bool connected;
            QString state;
            qint64 until;
    };

    const m3shapes::RoundedPolygon& shapeNamed(const char* name) {
        for (const auto& named : MaterialShapes::all()) {
            if (qstrcmp(named.name, name) == 0) return named.shape();
        }
        return MaterialShapes::circle();
    }

    bool thisIsLaptop() {
        // SMBIOS chassis types: portable, laptop, notebook, sub notebook,
        // hand held, tablet, convertible, detachable
        QFile file(QStringLiteral("/sys/class/dmi/id/chassis_type"));
        if (!file.open(QFile::ReadOnly)) return false;
        static const QList<int> portable = {8, 9, 10, 11, 14, 30, 31, 32};
        return portable.contains(file.readAll().trimmed().toInt());
    }

    QColor container(const M3::Scheme& s, int role) {
        return role == 0 ? s.primaryContainer : role == 1 ? s.secondaryContainer : s.tertiaryContainer;
    }

    QColor onContainer(const M3::Scheme& s, int role) {
        return role == 0 ? s.onPrimaryContainer : role == 1 ? s.onSecondaryContainer : s.onTertiaryContainer;
    }
} // namespace

struct AroundYouSheetPrivate {
        QString highlight;
        QList<AroundYouDevice> devices;
        QHash<QString, Body> bodies;
        QHash<QString, Pending> pending; // connecting/disconnecting, until the device says so
        bool laptop = false;

        QTimer* ticker = nullptr;
        QElapsedTimer clock;
        double time = 0; // seconds open, for the drift

        Spring centreScale, halo, choicesShown;

        int focused = -1;
        bool keyboard = false;
        int hovered = -1;

        int pressed = -1;
        QString pressedId;
        QHash<QString, double> angles; // each device's place on its ring
        QString layoutKey;
        QPointF pressPos, grabOffset;
        bool dragging = false;
        bool breaking = false;
        bool docking = false;

        QString choosingFor;
        QList<AroundYouChoice> choices;
        int choiceFocused = 0;
        int choiceHovered = -1;
        int choicePressed = -1;
};

AroundYouSheet::AroundYouSheet(QString highlight, QWidget* parent) :
    QWidget(parent) {
    d = new AroundYouSheetPrivate();
    d->highlight = highlight;
    d->laptop = thisIsLaptop();
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setMinimumHeight(preferredHeight());
    this->setAccessibleName(tr("Around you"));

    d->centreScale.spec = M3Motion::spring(M3Motion::SpatialFast);
    d->centreScale.snap(0);
    d->centreScale.target = 1;
    d->halo.spec = M3Motion::spring(M3Motion::EffectsDefault);
    d->choicesShown.spec = M3Motion::spring(M3Motion::EffectsDefault);

    devicesChanged();
    for (int i = 0; i < d->devices.size(); i++) {
        if (d->devices.at(i).id == highlight) d->focused = i;
    }
    connect(AroundYou::instance(), &AroundYou::devicesChanged, this, &AroundYouSheet::devicesChanged);

    d->ticker = new QTimer(this);
    d->ticker->setInterval(16);
    connect(d->ticker, &QTimer::timeout, this, &AroundYouSheet::tick);
    d->clock.start();
    d->ticker->start();
}

AroundYouSheet::~AroundYouSheet() {
    delete d;
}

int AroundYouSheet::preferredHeight() const {
    return int(choicesTop() + ChoiceHeight + 24);
}

QPointF AroundYouSheet::centre() const {
    return QPointF(this->width() / 2.0, OrbitTop + RingOut + DeviceSize / 2);
}

double AroundYouSheet::choicesTop() const {
    // Under the outer ring's bottom device and its name
    return OrbitTop + 2 * RingOut + DeviceSize + 52;
}

bool AroundYouSheet::isConnected(int index) const {
    const AroundYouDevice& device = d->devices.at(index);
    if (device.id == d->choosingFor) return true;
    if (d->pending.contains(device.id)) return d->pending.value(device.id).connected;
    return device.connected;
}

bool AroundYouSheet::canMove(int index) const {
    const AroundYouDevice& device = d->devices.at(index);
    if (isConnected(index)) return bool(device.disconnect);
    return bool(device.connect) || !device.choices.isEmpty();
}

QList<int> AroundYouSheet::order() const {
    // Inner ring, then outer, each by id: the layout and the keyboard order
    QList<int> inner, outer;
    for (int i = 0; i < d->devices.size(); i++) (isConnected(i) ? inner : outer).append(i);
    auto byId = [this](int a, int b) {
        return d->devices.at(a).id < d->devices.at(b).id;
    };
    std::sort(inner.begin(), inner.end(), byId);
    std::sort(outer.begin(), outer.end(), byId);
    return inner + outer;
}

QPointF AroundYouSheet::home(int index) const {
    const double angle = qDegreesToRadians(d->angles.value(d->devices.at(index).id, -90));
    return centre() + QPointF(std::cos(angle), std::sin(angle)) * (isConnected(index) ? RingIn : RingOut);
}

QString AroundYouSheet::stateOf(int index) const {
    const AroundYouDevice& device = d->devices.at(index);
    if (device.id == d->choosingFor) return tr("How should it be used?");
    if (d->pending.contains(device.id)) return d->pending.value(device.id).state;
    return device.state;
}

QRectF AroundYouSheet::labelRect(int index, QPointF at, bool outward) const {
    // The name and what it's doing: under the shape, or (on the inner ring)
    // pointing away from the computer, clear of the shape whichever way
    const AroundYouDevice& device = d->devices.at(index);
    const double width = qMin(160.0, qMax(QFontMetricsF(flex(14, 560)).horizontalAdvance(device.name), QFontMetricsF(flex(12, 420)).horizontalAdvance(stateOf(index))));
    constexpr double height = 38;
    QPointF middle = at + QPointF(0, DeviceSize / 2 + 4 + height / 2);
    if (outward) {
        const double length = QLineF(centre(), at).length();
        const QPointF u = length > 1 ? (at - centre()) / length : QPointF(0, 1);
        middle = at + u * (DeviceSize / 2 + 8 + std::abs(u.x()) * width / 2 + std::abs(u.y()) * height / 2);
    }
    return QRectF(middle.x() - width / 2, middle.y() - height / 2, width, height);
}

void AroundYouSheet::relayout() {
    // Where each device sits on its ring: evenly spread, the rings turned so
    // that no name or shape covers another (or the computer, or the header)
    QString key;
    for (int i = 0; i < d->devices.size(); i++) key += d->devices.at(i).id + (isConnected(i) ? QStringLiteral("+") : QStringLiteral("-")) + stateOf(i) + QLatin1Char('\n');
    key += QString::number(this->width());
    if (key == d->layoutKey) return;
    d->layoutKey = key;

    QList<int> inner, outer;
    for (int i : order()) (isConnected(i) ? inner : outer).append(i);
    const QPointF c = centre();
    const QRectF computer(c.x() - CentreSize / 2, c.y() - CentreSize / 2, CentreSize, CentreSize);
    const QRectF header(0, 0, this->width(), 80);
    auto overlap = [](const QRectF& a, const QRectF& b) {
        const QRectF both = a.intersected(b);
        return both.isEmpty() ? 0.0 : both.width() * both.height();
    };

    double bestCost = -1, bestInner = -150, bestOuter = -30;
    const double innerStep = 360.0 / qMax(1, int(inner.size())), outerStep = 360.0 / qMax(1, int(outer.size()));
    for (double innerTurn = 0; innerTurn < innerStep; innerTurn += 10) {
        for (double outerTurn = 0; outerTurn < outerStep; outerTurn += 5) {
            QList<QRectF> rects;
            double cost = 0;
            auto place = [&](const QList<int>& ring, double start, double step, double radius, bool outward) {
                for (int k = 0; k < ring.size(); k++) {
                    const double angle = qDegreesToRadians(start + k * step);
                    const QPointF at = c + QPointF(std::cos(angle), std::sin(angle)) * radius;
                    // With a little room around each
                    const QRectF shape = QRectF(at.x() - DeviceSize / 2, at.y() - DeviceSize / 2, DeviceSize, DeviceSize).adjusted(-8, -8, 8, 8);
                    const QRectF label = labelRect(ring.at(k), at, outward).adjusted(-6, -2, 6, 2);
                    for (const QRectF& other : std::as_const(rects)) cost += overlap(shape, other) + overlap(label, other);
                    cost += overlap(label, computer) + overlap(label, header) + overlap(shape, header);
                    rects.append(shape);
                    rects.append(label);
                }
            };
            place(inner, -150 + innerTurn, innerStep, RingIn, true);
            place(outer, -30 + outerTurn, outerStep, RingOut, false);
            // Ties: stay near the usual places
            cost += (innerTurn + outerTurn) * 0.01;
            if (bestCost < 0 || cost < bestCost) {
                bestCost = cost;
                bestInner = -150 + innerTurn;
                bestOuter = -30 + outerTurn;
            }
        }
    }
    d->angles.clear();
    for (int k = 0; k < inner.size(); k++) d->angles.insert(d->devices.at(inner.at(k)).id, bestInner + k * innerStep);
    for (int k = 0; k < outer.size(); k++) d->angles.insert(d->devices.at(outer.at(k)).id, bestOuter + k * outerStep);
}

void AroundYouSheet::devicesChanged() {
    const QString focusedId = d->focused >= 0 && d->focused < d->devices.size() ? d->devices.at(d->focused).id : QString();
    d->devices = AroundYou::instance()->devices();

    QSet<QString> ids;
    for (const AroundYouDevice& device : std::as_const(d->devices)) {
        ids.insert(device.id);
        // Done connecting/disconnecting
        if (d->pending.contains(device.id) && d->pending.value(device.id).connected == device.connected) d->pending.remove(device.id);
        if (d->bodies.contains(device.id)) continue;
        // New: springs out of this computer
        Body body;
        body.look = AroundYou::lookFor(device.id);
        body.morph = std::make_shared<m3shapes::Morph>(shapeNamed(body.look.shape), MaterialShapes::circle());
        body.x.snap(centre().x());
        body.y.snap(centre().y());
        body.scale.snap(0);
        body.scale.target = 1;
        body.lift.spec = M3Motion::spring(M3Motion::SpatialFast);
        body.neck.spec = M3Motion::spring(M3Motion::SpatialFast);
        body.neck.snap(0);
        d->bodies.insert(device.id, body);
    }
    for (const QString& id : d->bodies.keys()) {
        if (!ids.contains(id)) d->bodies.remove(id);
    }
    for (const QString& id : d->pending.keys()) {
        if (!ids.contains(id)) d->pending.remove(id);
    }
    if (!d->choosingFor.isEmpty() && !ids.contains(d->choosingFor)) cancelChoices();

    d->focused = -1;
    for (int i = 0; i < d->devices.size(); i++) {
        if (d->devices.at(i).id == focusedId) d->focused = i;
    }
    // Still holding the same device (a battery level changed under it)
    const int wasPressed = d->pressed;
    d->pressed = -1;
    for (int i = 0; i < d->devices.size(); i++) {
        if (wasPressed >= 0 && d->devices.at(i).id == d->pressedId) d->pressed = i;
    }
    if (d->pressed < 0) d->dragging = false;
    d->hovered = -1;
}

void AroundYouSheet::tick() {
    const double dt = qMin(0.05, d->clock.restart() / 1000.0);
    d->time += dt;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QString& id : d->pending.keys()) {
        // It didn't happen: back where it really is
        if (d->pending.value(id).until < now) d->pending.remove(id);
    }

    relayout();
    for (int i = 0; i < d->devices.size(); i++) {
        Body& body = d->bodies[d->devices.at(i).id];
        if (body.fresh) {
            body.fresh = false;
            body.x.snap(centre().x());
            body.y.snap(centre().y());
        }
        const bool held = d->dragging && i == d->pressed;
        if (!held) {
            const QPointF h = home(i);
            body.x.target = h.x();
            body.y.target = h.y();
            body.neck.target = isConnected(i) ? 1 : 0;
        }
        body.x.step(dt);
        body.y.step(dt);
        body.scale.step(dt);
        body.lift.step(dt);
        body.neck.step(dt);
        if (body.snapping) {
            body.snap.step(dt);
            if (body.snap.x > 0.99 && std::abs(body.snap.v) < 0.05) body.snapping = false;
        }
    }
    d->centreScale.step(dt);
    d->halo.step(dt);
    d->choicesShown.target = d->choosingFor.isEmpty() ? 0 : 1;
    d->choicesShown.step(dt);
    this->update();
}

int AroundYouSheet::deviceAt(QPointF pos) const {
    int found = -1;
    double best = DeviceSize / 2 + 8;
    for (int i = 0; i < d->devices.size(); i++) {
        const Body& body = d->bodies.value(d->devices.at(i).id);
        const double distance = QLineF(pos, QPointF(body.x.x, body.y.x)).length();
        if (distance < best) {
            best = distance;
            found = i;
        }
    }
    return found;
}

QList<QRectF> AroundYouSheet::choiceRects() const {
    QList<QRectF> rects;
    if (d->choices.isEmpty()) return rects;
    const QFontMetrics metrics(flex(14, 560));
    QList<double> widths;
    double total = 0;
    for (const AroundYouChoice& choice : std::as_const(d->choices)) {
        widths.append(qMax(96.0, metrics.horizontalAdvance(choice.label) + 48.0));
        total += widths.last();
    }
    total += 2 * (d->choices.size() - 1);
    double x = this->width() / 2.0 - total / 2;
    for (double width : std::as_const(widths)) {
        rects.append(QRectF(x, choicesTop(), width, ChoiceHeight));
        x += width + 2;
    }
    return rects;
}

int AroundYouSheet::choiceAt(QPointF pos) const {
    if (d->choosingFor.isEmpty()) return -1;
    const QList<QRectF> rects = choiceRects();
    for (int i = 0; i < rects.size(); i++) {
        if (rects.at(i).contains(pos)) return i;
    }
    return -1;
}

void AroundYouSheet::connectDevice(int index) {
    const AroundYouDevice device = d->devices.at(index);
    d->centreScale.v += 1.6; // a bump as they join
    if (!device.choices.isEmpty()) {
        // How to use it, under the orbit
        d->choosingFor = device.id;
        d->choices = device.choices;
        d->choiceFocused = 0;
        return;
    }
    if (!device.connect) return;
    d->pending.insert(device.id, {true, tr("Connecting…"), QDateTime::currentMSecsSinceEpoch() + 20000});
    device.connect();
}

void AroundYouSheet::snapLink(const QString& id) {
    // The link breaks: it vanishes at once and two drops take its place; both
    // shapes wobble
    Body& body = d->bodies[id];
    if (body.neck.x < 0.3) return;
    body.neck.snap(0);
    body.snap.spec = M3Motion::spring(M3Motion::SpatialSlow);
    body.snap.snap(0);
    body.snap.target = 1;
    body.snapping = true;
    body.scale.v += 2.2;
    d->centreScale.v -= 1.2;
}

void AroundYouSheet::disconnectDevice(int index) {
    const AroundYouDevice device = d->devices.at(index);
    if (!device.disconnect) return;
    snapLink(device.id);
    d->pending.insert(device.id, {false, tr("Letting go…"), QDateTime::currentMSecsSinceEpoch() + 20000});
    device.disconnect();
}

void AroundYouSheet::pickChoice(int choice) {
    if (choice < 0 || choice >= d->choices.size()) return;
    const std::function<void()> pick = d->choices.at(choice).pick;
    d->pending.insert(d->choosingFor, {true, tr("Connecting…"), QDateTime::currentMSecsSinceEpoch() + 20000});
    d->choosingFor.clear();
    d->choices.clear();
    // A display changes the screens under the sheet: close it first
    emit done();
    QTimer::singleShot(250, AroundYou::instance(), [pick] {
        if (pick) pick();
    });
}

void AroundYouSheet::cancelChoices() {
    d->choosingFor.clear();
    d->choices.clear();
    d->choiceHovered = -1;
    d->choicePressed = -1;
}

void AroundYouSheet::paintDrops(QPainter* painter, const M3::Scheme& s, int index) const {
    // The two halves of a link that has just broken, each pulling back into
    // its own shape: a tapering drop from the shape's edge, its tip round
    const Body& body = d->bodies.value(d->devices.at(index).id);
    const QPointF c = centre(), to(body.x.x, body.y.x);
    const double length = QLineF(c, to).length();
    if (length < 1) return;
    const QPointF u = (to - c) / length;
    const double r1 = CentreSize / 2 * qMax(0.0, d->centreScale.x) * 0.9;
    const double r2 = DeviceSize / 2 * qMax(0.0, body.scale.x) * 0.9;
    const double gap = qMax(0.0, length - r1 - r2);
    const double left = 1 - qBound(0.0, body.snap.x, 1.2);
    if (left <= 0.01) return;
    // Long and thin at the break, short and round as it goes back in
    const double reach = qMin(gap / 2, 36.0) * left;

    auto drop = [&](QPointF edge, QPointF outward, double half, QColor colour) {
        const QPointF n(-outward.y(), outward.x());
        const QPointF tip = edge + outward * reach;
        const double tipRadius = qMax(1.0, half * 0.5);
        QPainterPath path;
        // Flares into the shape (hidden under it), narrows to the tip
        path.moveTo(edge - outward * 6 + n * half * 1.8);
        path.cubicTo(edge + n * half, tip - outward * reach * 0.45 + n * tipRadius, tip + n * tipRadius);
        path.lineTo(tip - n * tipRadius);
        path.cubicTo(tip - outward * reach * 0.45 - n * tipRadius, edge - n * half, edge - outward * 6 - n * half * 1.8);
        path.closeSubpath();
        path.addEllipse(tip, tipRadius, tipRadius);
        path.setFillRule(Qt::WindingFill);
        painter->setPen(Qt::NoPen);
        painter->setBrush(colour);
        painter->drawPath(path);
    };
    drop(c + u * r1, u, 4 + 6 * left, s.primary);
    drop(to - u * r2, -u, 3.5 + 5 * left, container(s, body.look.role));
}

void AroundYouSheet::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QPointF c = centre();
    const bool expressive = M3Motion::expressive();

    // Header
    p.setFont(flex(24, 500));
    p.setPen(s.onSurface);
    p.drawText(QRectF(Margin, 20, this->width() - 2 * Margin, 32), Qt::AlignCenter, tr("Around you"));
    p.setFont(flex(14, 420));
    p.setPen(s.onSurfaceVariant);
    p.drawText(QRectF(Margin, 54, this->width() - 2 * Margin, 20), Qt::AlignCenter,
        d->devices.isEmpty() ? tr("Nothing around you right now") : tr("Drag a device onto your computer to connect it, or away to let it go"));

    // The rings
    QColor ring = s.outlineVariant;
    ring.setAlphaF(0.55f);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ring, 1));
    p.drawEllipse(c, RingIn, RingIn);
    QPen dashed(ring, 1, Qt::CustomDashLine);
    dashed.setDashPattern({4, 6});
    p.setPen(dashed);
    p.drawEllipse(c, RingOut, RingOut);

    const double cr = CentreSize / 2 * qMax(0.0, d->centreScale.x);
    const double breakAt = (RingIn + RingOut) / 2 + 10;

    // The links to what's connected
    for (int i = 0; i < d->devices.size(); i++) {
        const Body& body = d->bodies.value(d->devices.at(i).id);
        const double w = qBound(0.0, body.neck.x, 1.3);
        if (body.snapping) paintDrops(&p, s, i);
        if (w < 0.02) continue;
        const QPointF to(body.x.x, body.y.x);
        const double length = QLineF(c, to).length();
        if (length < 1) continue;
        const double dr = DeviceSize / 2 * qMax(0.0, body.scale.x);
        // Thins as it stretches
        const double stretch = qBound(0.0, (length - RingIn) / (breakAt - RingIn), 1.0);
        // A metaball bridge: it leaves each shape along its edge and is thin
        // in the middle; it spreads less as it stretches, and as it lets go
        const double r1 = cr * 0.9, r2 = dr * 0.9;
        const double v = 0.42 * qMin(w, 1.1) * (1 - 0.55 * stretch);
        const double spread = std::acos(qBound(-1.0, (r1 - r2) / length, 1.0));
        const double towards = std::atan2(to.y() - c.y(), to.x() - c.x());
        auto along = [](double angle) {
            return QPointF(std::cos(angle), std::sin(angle));
        };
        const double a1 = towards + spread * v, a2 = towards - spread * v;
        const double a3 = towards + M_PI - (M_PI - spread) * v, a4 = towards - M_PI + (M_PI - spread) * v;
        const QPointF p1 = c + along(a1) * r1, p2 = c + along(a2) * r1, p3 = to + along(a3) * r2, p4 = to + along(a4) * r2;
        const double handle = qMin(v * 2.4, QLineF(p1, p3).length() / (r1 + r2)) * qMin(1.0, length * 2 / (r1 + r2));
        QPainterPath neck;
        neck.moveTo(p1);
        neck.cubicTo(p1 + along(a1 - M_PI / 2) * r1 * handle, p3 + along(a3 + M_PI / 2) * r2 * handle, p3);
        neck.lineTo(p4);
        neck.cubicTo(p4 + along(a4 - M_PI / 2) * r2 * handle, p2 + along(a2 + M_PI / 2) * r1 * handle, p2);
        neck.closeSubpath();
        const QPointF a = c, b = to;
        QLinearGradient gradient(a, b);
        gradient.setColorAt(0, s.primary);
        gradient.setColorAt(1, container(s, body.look.role));
        p.setPen(Qt::NoPen);
        p.setBrush(gradient);
        p.drawPath(neck);
    }

    // This computer
    if (d->halo.x > 0.01) {
        QColor halo = s.primary;
        halo.setAlphaF(float(0.24 * qBound(0.0, d->halo.x, 1.0)));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        const double r = cr + 14 + (expressive ? 5 * std::sin(d->time * 6) : 0);
        p.drawEllipse(c, r, r);
    }
    if (cr > 1) {
        QTransform transform;
        transform.translate(c.x(), c.y());
        transform.rotate(expressive ? d->time * 4 : 0);
        p.setPen(Qt::NoPen);
        p.setBrush(s.primary);
        p.drawPath(transform.map(m3shapes::toPath(MaterialShapes::cookie12Sided(), QRectF(-cr, -cr, 2 * cr, 2 * cr))));
        QFont symbol(QStringLiteral("Material Symbols Rounded"));
        symbol.setPixelSize(qMax(1, int(cr * 0.9)));
        symbol.setVariableAxis(QFont::Tag("FILL"), 1);
        symbol.setVariableAxis(QFont::Tag("wght"), 500);
        symbol.setVariableAxis(QFont::Tag("opsz"), 48);
        p.setFont(symbol);
        p.setPen(s.onPrimary);
        p.drawText(QRectF(c.x() - cr, c.y() - cr, 2 * cr, 2 * cr), Qt::AlignCenter, d->laptop ? QStringLiteral("laptop") : QStringLiteral("computer"));
    }

    // The devices, the held one on top
    QList<int> paintOrder;
    for (int i = 0; i < d->devices.size(); i++) {
        if (!(d->dragging && i == d->pressed)) paintOrder.append(i);
    }
    if (d->dragging && d->pressed >= 0) paintOrder.append(d->pressed);
    for (int i : std::as_const(paintOrder)) {
        const AroundYouDevice& device = d->devices.at(i);
        const Body& body = d->bodies.value(device.id);
        const double lift = qBound(0.0, body.lift.x, 1.0);
        const double size = DeviceSize * qMax(0.0, body.scale.x) * (1 + 0.12 * body.lift.x);
        if (size < 1) continue;
        const double reveal = qBound(0.0, body.scale.x, 1.0);
        const QPointF at(body.x.x, body.y.x);
        QColor fill = container(s, body.look.role);
        QColor ink = onContainer(s, body.look.role);

        if (lift > 0.02) {
            // Lifted: a soft shadow, and the dragged state layer
            QColor shadow = s.shadow;
            shadow.setAlphaF(float(0.35 * lift));
            QRadialGradient gradient(at + QPointF(0, 8), size * 0.75);
            gradient.setColorAt(0.5, shadow);
            gradient.setColorAt(1, Qt::transparent);
            p.setPen(Qt::NoPen);
            p.setBrush(gradient);
            p.drawEllipse(at + QPointF(0, 8), size * 0.75, size * 0.75);
            fill = M3::layer(fill, ink, 0.16 * lift);
        } else if (i == d->hovered) {
            fill = M3::layer(fill, ink, 0.08);
        }
        fill.setAlphaF(float(reveal));
        ink.setAlphaF(float(reveal));

        QTransform transform;
        transform.translate(at.x(), at.y());
        transform.rotate(body.look.turn + (expressive ? d->time * 6 * (1 - lift) : 0));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(transform.map(m3shapes::toPath(*body.morph, float(lift), QRectF(-size / 2, -size / 2, size, size))));

        QFont symbol(QStringLiteral("Material Symbols Rounded"));
        symbol.setPixelSize(qMax(1, int(size * 0.46)));
        symbol.setVariableAxis(QFont::Tag("FILL"), 1);
        symbol.setVariableAxis(QFont::Tag("wght"), 500);
        symbol.setVariableAxis(QFont::Tag("opsz"), 40);
        p.setFont(symbol);
        p.setPen(ink);
        p.drawText(QRectF(at.x() - size, at.y() - size, 2 * size, 2 * size), Qt::AlignCenter, device.symbol);

        // Picked out (the snackbar's Open) or focused from the keyboard
        if ((d->keyboard && i == d->focused && this->hasFocus()) || (!d->keyboard && device.id == d->highlight)) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(s.secondary, 2));
            p.drawEllipse(at, size / 2 + 6, size / 2 + 6);
        }

        // Name and what it's doing
        const QString state = stateOf(i);
        const QRectF label = labelRect(i, at, isConnected(i) && !(d->dragging && i == d->pressed));
        QColor on = s.onSurface, onVariant = s.onSurfaceVariant;
        // Out of the way while it's held (it would sit over the link)
        const double named = reveal * (1 - lift);
        on.setAlphaF(float(named));
        onVariant.setAlphaF(float(named));
        p.setFont(flex(14, 560));
        p.setPen(on);
        p.drawText(QRectF(label.center().x() - 80, label.top(), 160, 20), Qt::AlignCenter, p.fontMetrics().elidedText(device.name, Qt::ElideRight, 160));
        p.setFont(flex(12, 420));
        p.setPen(onVariant);
        p.drawText(QRectF(label.center().x() - 80, label.top() + 19, 160, 18), Qt::AlignCenter, p.fontMetrics().elidedText(state, Qt::ElideRight, 160));
    }

    // How to use it: a connected button group
    const double shown = qBound(0.0, d->choicesShown.x, 1.0);
    if (shown > 0.01 && !d->choices.isEmpty()) {
        p.setOpacity(shown);
        const QList<QRectF> rects = choiceRects();
        for (int i = 0; i < rects.size(); i++) {
            const QRectF r = rects.at(i);
            const bool first = i == 0, last = i == rects.size() - 1;
            const bool pressed = i == d->choicePressed;
            // Inner corners round out when pressed
            const double outer = r.height() / 2, inner = pressed ? r.height() / 2 : 8;
            const double left = first ? outer : inner, right = last ? outer : inner;
            QPainterPath path;
            path.moveTo(r.left() + left, r.top());
            path.lineTo(r.right() - right, r.top());
            path.arcTo(QRectF(r.right() - 2 * right, r.top(), 2 * right, 2 * right), 90, -90);
            path.lineTo(r.right(), r.bottom() - right);
            path.arcTo(QRectF(r.right() - 2 * right, r.bottom() - 2 * right, 2 * right, 2 * right), 0, -90);
            path.lineTo(r.left() + left, r.bottom());
            path.arcTo(QRectF(r.left(), r.bottom() - 2 * left, 2 * left, 2 * left), 270, -90);
            path.lineTo(r.left(), r.top() + left);
            path.arcTo(QRectF(r.left(), r.top(), 2 * left, 2 * left), 180, -90);
            path.closeSubpath();
            QColor background = s.secondaryContainer;
            const double state = pressed ? 0.10 : i == d->choiceHovered ? 0.08 : 0;
            background = M3::layer(background, s.onSecondaryContainer, state);
            p.setPen(Qt::NoPen);
            p.setBrush(background);
            p.drawPath(path);
            if (d->keyboard && i == d->choiceFocused) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(s.secondary, 2));
                p.drawPath(path);
            }
            p.setFont(flex(14, 560));
            p.setPen(s.onSecondaryContainer);
            p.drawText(r, Qt::AlignCenter, d->choices.at(i).label);
        }
        p.setOpacity(1);
    }
}

void AroundYouSheet::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    if (d->pressed >= 0) {
        if (!d->dragging && (pos - d->pressPos).manhattanLength() > 6) d->dragging = true;
        if (!d->dragging) return;
        Body& body = d->bodies[d->devices.at(d->pressed).id];
        const QPointF c = centre();
        QPointF want = pos + d->grabOffset;
        const bool movable = canMove(d->pressed);
        const bool connected = isConnected(d->pressed);
        if (!movable) {
            // It can't go anywhere: it only gives a little
            const QPointF h = home(d->pressed);
            want = h + (want - h) * 0.25;
        }
        const double distance = QLineF(want, c).length();
        d->docking = movable && !connected && distance < RingIn + 40;
        const bool wasBreaking = d->breaking;
        d->breaking = movable && connected && distance > (RingIn + RingOut) / 2 + 10;
        if (d->breaking && !wasBreaking) snapLink(d->pressedId);
        // Pulled back in before letting go: it joins up again
        if (!d->breaking && wasBreaking) d->bodies[d->pressedId].snapping = false;
        if (d->docking) {
            // Close enough: this computer pulls it in
            const QPointF u = distance > 1 ? (want - c) / distance : QPointF(0, -1);
            const double pull = qBound(0.0, (RingIn + 40 - distance) / (RingIn + 40), 1.0) * 0.45;
            want = want * (1 - pull) + (c + u * RingIn) * pull;
        }
        d->halo.target = d->docking ? 1 : 0;
        d->centreScale.target = d->docking ? 1.06 : 1;
        body.neck.target = connected && !d->breaking ? 1 : 0;
        body.x.target = want.x();
        body.y.target = want.y();
        return;
    }

    const int device = deviceAt(pos);
    const int choice = choiceAt(pos);
    if (device != d->hovered || choice != d->choiceHovered) {
        d->hovered = device;
        d->choiceHovered = choice;
        this->setCursor(device >= 0 || choice >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void AroundYouSheet::mousePressEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    d->keyboard = false;
    d->choicePressed = choiceAt(pos);
    if (d->choicePressed >= 0) return;
    d->pressed = deviceAt(pos);
    if (d->pressed < 0) {
        // Somewhere else: not choosing any more
        if (!d->choosingFor.isEmpty()) cancelChoices();
        return;
    }
    d->pressedId = d->devices.at(d->pressed).id;
    Body& body = d->bodies[d->pressedId];
    d->pressPos = pos;
    d->grabOffset = QPointF(body.x.x, body.y.x) - pos;
    d->dragging = false;
    d->docking = false;
    d->breaking = false;
    // Lifted: rounds into a circle; held tight to the pointer
    body.lift.target = 1;
    body.x.spec = body.y.spec = M3Motion::spring(M3Motion::SpatialFast, false);
}

void AroundYouSheet::mouseReleaseEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    if (d->choicePressed >= 0) {
        const int choice = choiceAt(pos);
        const int pressed = d->choicePressed;
        d->choicePressed = -1;
        if (choice == pressed) pickChoice(choice);
        return;
    }
    if (d->pressed < 0) return;
    const int index = d->pressed;
    const QString id = d->devices.at(index).id;
    Body& body = d->bodies[id];
    body.lift.target = 0;
    body.x.spec = body.y.spec = M3Motion::spring(M3Motion::SpatialFast);
    d->halo.target = 0;
    d->centreScale.target = 1;
    const bool dragged = d->dragging;
    d->pressed = -1;
    d->dragging = false;

    if (!dragged) {
        // A click: open it, after the sheet has gone (what it opens may
        // take the screen)
        if (deviceAt(pos) != index) return;
        emit done();
        QTimer::singleShot(200, AroundYou::instance(), [id] {
            AroundYou::instance()->activate(id);
        });
        return;
    }
    if (d->docking) connectDevice(index);
    else if (d->breaking) disconnectDevice(index);
    d->docking = d->breaking = false;
}

void AroundYouSheet::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    d->hovered = -1;
    d->choiceHovered = -1;
}

void AroundYouSheet::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (!d->choosingFor.isEmpty()) cancelChoices();
        else emit done();
        return;
    }

    if (!d->choosingFor.isEmpty()) {
        // The button group has the keyboard
        d->keyboard = true;
        switch (event->key()) {
            case Qt::Key_Left:
            case Qt::Key_Up:
                d->choiceFocused = qMax(0, d->choiceFocused - 1);
                return;
            case Qt::Key_Right:
            case Qt::Key_Down:
            case Qt::Key_Tab:
                d->choiceFocused = qMin(int(d->choices.size()) - 1, d->choiceFocused + 1);
                return;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Space:
                pickChoice(d->choiceFocused);
                return;
            default:
                return QWidget::keyPressEvent(event);
        }
    }

    if (d->devices.isEmpty()) return QWidget::keyPressEvent(event);
    const QList<int> keys = order();
    int at = keys.indexOf(d->focused);
    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Up:
        case Qt::Key_Backtab:
            d->focused = keys.at(at < 0 ? 0 : (at - 1 + keys.size()) % keys.size());
            break;
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_Tab:
            d->focused = keys.at(at < 0 ? 0 : (at + 1) % keys.size());
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space: {
            if (d->focused < 0) d->focused = keys.first();
            // Nearby: connect it; in use: open it
            if (!isConnected(d->focused) && canMove(d->focused)) {
                connectDevice(d->focused);
                break;
            }
            const QString id = d->devices.at(d->focused).id;
            emit done();
            QTimer::singleShot(200, AroundYou::instance(), [id] {
                AroundYou::instance()->activate(id);
            });
            return;
        }
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            if (d->focused >= 0 && isConnected(d->focused) && canMove(d->focused)) disconnectDevice(d->focused);
            break;
        default:
            return QWidget::keyPressEvent(event);
    }
    d->keyboard = true;
}

bool AroundYouSheet::focusNextPrevChild(bool next) {
    // Tab moves between the devices, not out of the sheet
    Q_UNUSED(next)
    return false;
}

void AroundYouSheet::open(QString highlight) {
    if (openSheet) return;
    auto* dialog = new TransparentDialog();
    dialog->showFullScreen();
    QPointer<TransparentDialog> dialogPointer = dialog;
    QTimer::singleShot(200, dialog, [dialogPointer, highlight] {
        if (!dialogPointer) return;
        auto* sheet = new AroundYouSheet(highlight);
        auto* popover = new tPopover(sheet);
        openSheet = popover;
        popover->setPopoverSide(tPopover::Bottom);
        popover->setPopoverWidth(qMin(sheet->preferredHeight(), dialogPointer->height() - 48));
        QObject::connect(sheet, &AroundYouSheet::done, popover, &tPopover::dismiss);
        QObject::connect(popover, &tPopover::dismissed, popover, [popover, sheet, dialogPointer] {
            popover->deleteLater();
            sheet->deleteLater();
            if (dialogPointer) dialogPointer->deleteLater();
        });
        popover->show(dialogPointer);
        sheet->setFocus();
    });
}

void AroundYouSheet::setUp() {
    QObject::connect(AroundYou::instance(), &AroundYou::sheetRequested, AroundYou::instance(), &AroundYouSheet::open);

    // Super+A: Around you
    auto* grab = new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::Key_A), QStringLiteral("aroundYou"));
    QObject::connect(grab, &KeyGrab::activated, AroundYou::instance(), [] {
        AroundYouSheet::open(QString());
    });

    // The shapes in the bar, while there's anything to show
    auto* chunk = new AroundYouChunk();
    auto update = [chunk] {
        const bool any = !connectedDevices().isEmpty();
        const bool registered = StateManager::barManager()->isChunkRegistered(chunk);
        if (any && !registered) StateManager::barManager()->addChunk(chunk);
        if (!any && registered) StateManager::barManager()->removeChunk(chunk);
        chunk->updateGeometry();
        chunk->update();
    };
    QObject::connect(AroundYou::instance(), &AroundYou::devicesChanged, chunk, update);
    update();

    if (qEnvironmentVariableIsSet("TD_AROUNDYOU_DEMO")) addDemoDevices();

    // USB drives, if UDisks2 is there
    if (QDBusConnection::systemBus().interface()->isServiceRegistered(UDisks) || QDBusConnection::systemBus().interface()->activatableServiceNames().value().contains(UDisks)) {
        watchDrives();
    }
}

// ---- The bar's shapes -------------------------------------------------------

struct AroundYouChunkPrivate {
        int hover = 0;
};

namespace {
    constexpr int ChunkShape = 20;
    constexpr int ChunkSpacing = 4;
    constexpr int ChunkMax = 4;
} // namespace

AroundYouChunk::AroundYouChunk() :
    Chunk() {
    d = new AroundYouChunkPrivate();
    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(tr("Around you"));
    connect(this, &Chunk::clicked, this, [] {
        AroundYouSheet::open(QString());
    });
}

AroundYouChunk::~AroundYouChunk() {
    delete d;
}

QString AroundYouChunk::name() {
    return QStringLiteral("aroundyou");
}

int AroundYouChunk::expandedHeight() {
    return ChunkShape + 18;
}

int AroundYouChunk::statusBarHeight() {
    return ChunkShape + 6;
}

QSize AroundYouChunk::sizeHint() const {
    const int count = qMin(ChunkMax, int(connectedDevices().size()));
    return QSize(qMax(1, count) * (ChunkShape + ChunkSpacing) + 6, ChunkShape + 6);
}

void AroundYouChunk::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter p(this);
    const QList<AroundYouDevice> devices = connectedDevices();
    double x = 3 + ChunkShape / 2.0;
    for (int i = 0; i < qMin(ChunkMax, int(devices.size())); i++) {
        AroundYou::paintShape(&p, devices.at(i).id, devices.at(i).symbol, QPointF(x, this->height() / 2.0), ChunkShape);
        x += ChunkShape + ChunkSpacing;
    }
}
