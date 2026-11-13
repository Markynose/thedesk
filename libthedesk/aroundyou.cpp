/****************************************
 *
 *   theDesk - the devices around you
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "aroundyou.h"

#include "hudmanager.h"
#include "statemanager.h"
#include <QCryptographicHash>
#include <QPainter>
#include <QPainterPath>
#include <m3scheme.h>
#include <shapes/materialshapes.h>
#include <shapes/shapepath.h>

struct AroundYouPrivate {
        QList<AroundYouDevice> devices;
};

namespace {
    const m3shapes::RoundedPolygon& shapeNamed(const char* name) {
        for (const auto& named : MaterialShapes::all()) {
            if (qstrcmp(named.name, name) == 0) return named.shape();
        }
        return MaterialShapes::circle();
    }

    const QString ActionPrefix = QStringLiteral("aroundyou:");
} // namespace

AroundYou* AroundYou::instance() {
    static AroundYou* instance = new AroundYou();
    return instance;
}

AroundYou::AroundYou() :
    QObject(nullptr) {
    d = new AroundYouPrivate();
    // A snackbar's Open: the sheet, with that device picked out
    connect(StateManager::hudManager(), &HudManager::actionTriggered, this, [this](QString actionId) {
        if (actionId.startsWith(ActionPrefix)) showSheet(actionId.mid(ActionPrefix.length()));
    });
}

void AroundYou::setDevice(const AroundYouDevice& device, bool announce) {
    bool found = false;
    for (AroundYouDevice& existing : d->devices) {
        if (existing.id != device.id) continue;
        existing = device;
        found = true;
    }
    if (!found) d->devices.append(device);
    emit devicesChanged();

    if (announce) {
        StateManager::hudManager()->showHud({
            {QStringLiteral("icon"),     device.symbol                          },
            {QStringLiteral("title"),    device.name                            },
            {QStringLiteral("text"),     tr("%1 connected").arg(device.name)},
            {QStringLiteral("action"),   tr("Open")                             },
            {QStringLiteral("actionId"), ActionPrefix + device.id               },
            {QStringLiteral("timeout"),  5000                                   }
        });
    }
}

void AroundYou::removeDevice(const QString& id) {
    const qsizetype before = d->devices.size();
    d->devices.removeIf([&id](const AroundYouDevice& device) {
        return device.id == id;
    });
    if (d->devices.size() != before) emit devicesChanged();
}

QList<AroundYouDevice> AroundYou::devices() const {
    return d->devices;
}

void AroundYou::activate(const QString& id) {
    for (const AroundYouDevice& device : std::as_const(d->devices)) {
        if (device.id == id && device.activate) device.activate();
    }
}

void AroundYou::triggerAction(const QString& id) {
    for (const AroundYouDevice& device : std::as_const(d->devices)) {
        if (device.id == id && device.action) device.action();
    }
}

void AroundYou::showSheet(const QString& highlight) {
    emit sheetRequested(highlight);
}

AroundYou::Look AroundYou::lookFor(const QString& id) {
    // Shapes that read well small and differ from each other
    static const char* shapes[] = {"Cookie9Sided", "Clover4Leaf", "SoftBurst", "Cookie6Sided", "Sunny", "Gem", "Pill", "Flower", "Cookie12Sided", "Clover8Leaf", "Pentagon", "Ghostish", "Cookie7Sided", "Puffy"};
    const QByteArray hash = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha1);
    return {shapes[uchar(hash[0]) % std::size(shapes)], uchar(hash[1]) % 3, double(uchar(hash[2]) % 8) * 45};
}

void AroundYou::paintShape(QPainter* painter, const QString& id, const QString& symbol, QPointF centre, double size, double opacity, double spin) {
    if (size < 1) return;
    const M3::Scheme s = M3::currentScheme();
    const Look look = lookFor(id);
    QColor fill = look.role == 0 ? s.primaryContainer : look.role == 1 ? s.secondaryContainer : s.tertiaryContainer;
    QColor ink = look.role == 0 ? s.onPrimaryContainer : look.role == 1 ? s.onSecondaryContainer : s.onTertiaryContainer;
    fill.setAlphaF(float(opacity));
    ink.setAlphaF(float(opacity));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath path = m3shapes::toPath(shapeNamed(look.shape), QRectF(-size / 2, -size / 2, size, size));
    QTransform transform;
    transform.translate(centre.x(), centre.y());
    transform.rotate(look.turn + spin);
    painter->fillPath(transform.map(path), fill);

    QFont font(QStringLiteral("Material Symbols Rounded"));
    const double px = size * 0.5;
    font.setPixelSize(qMax(1, qRound(px)));
    font.setVariableAxis(QFont::Tag("FILL"), 1);
    font.setVariableAxis(QFont::Tag("wght"), 500);
    font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
    painter->setFont(font);
    painter->setPen(ink);
    painter->drawText(QRectF(centre.x() - px, centre.y() - px, 2 * px, 2 * px), Qt::AlignCenter, symbol);
    painter->restore();
}
