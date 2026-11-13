#include "batteryhealth.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <m3motion.h>
#include <m3scheme.h>

namespace {
    constexpr int CardHeight = 156;
    constexpr int Gap = 12;
    constexpr int Header = 40;
    const QString Service = QStringLiteral("org.freedesktop.UPower");

    QFont flex(double px, int wght) {
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("wght"), float(wght));
        return font;
    }

    // UPower's states
    enum State { Unknown = 0, Charging = 1, Discharging = 2, Empty = 3, Full = 4, PendingCharge = 5, PendingDischarge = 6 };
} // namespace

struct BatteryHealth::Private {
        struct Battery {
                QString path, name, model;
                double percentage = 0, energyFull = 0, energyDesign = 0, rate = 0;
                int state = Unknown, cycles = -1, endThreshold = 0;
        };
        QList<Battery> batteries;
        M3SpringAnimation* fill; // the health bars fill as the page shows
        QTimer wave;             // the draining battery's surface moves
        QTimer refresh;          // readings while the page shows
        QElapsedTimer clock;
};

BatteryHealth::BatteryHealth(QWidget* parent) :
    QWidget(parent) {
    d = new Private();
    d->fill = new M3SpringAnimation(this);
    d->fill->setToken(M3Motion::SpatialSlow);
    d->fill->snapTo(0.0);
    connect(d->fill, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    d->wave.setInterval(33);
    connect(&d->wave, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    d->clock.start();
    d->refresh.setInterval(2000);
    connect(&d->refresh, &QTimer::timeout, this, [this] {
        for (const Private::Battery& battery : std::as_const(d->batteries)) read(battery.path);
    });
    load();
}

BatteryHealth::~BatteryHealth() {
    delete d;
}

void BatteryHealth::load() {
    QDBusInterface upower(Service, QStringLiteral("/org/freedesktop/UPower"), Service, QDBusConnection::systemBus());
    const QDBusReply<QList<QDBusObjectPath>> reply = upower.call(QStringLiteral("EnumerateDevices"));
    for (const QDBusObjectPath& path : reply.value()) {
        QDBusInterface device(Service, path.path(), QStringLiteral("org.freedesktop.UPower.Device"), QDBusConnection::systemBus());
        // Laptop batteries only: not mice, phones or UPSes
        if (device.property("Type").toUInt() != 2 || !device.property("PowerSupply").toBool()) continue;
        Private::Battery battery;
        battery.path = path.path();
        d->batteries.append(battery);
        read(path.path());
    }
    // Names follow the order UPower lists them in (BAT0, BAT1)
    for (int i = 0; i < d->batteries.size(); i++) d->batteries[i].name = d->batteries.size() > 1 ? tr("Battery %1").arg(i + 1) : tr("Battery");
    this->setVisible(!d->batteries.isEmpty());
    this->updateGeometry();
}

void BatteryHealth::read(const QString& path) {
    QDBusInterface device(Service, path, QStringLiteral("org.freedesktop.UPower.Device"), QDBusConnection::systemBus());
    for (Private::Battery& battery : d->batteries) {
        if (battery.path != path) continue;
        battery.model = device.property("Model").toString();
        battery.percentage = device.property("Percentage").toDouble();
        battery.energyFull = device.property("EnergyFull").toDouble();
        battery.energyDesign = device.property("EnergyFullDesign").toDouble();
        battery.rate = device.property("EnergyRate").toDouble();
        battery.state = int(device.property("State").toUInt());
        const QVariant cycles = device.property("ChargeCycles");
        battery.cycles = cycles.isValid() ? cycles.toInt() : -1;
        battery.endThreshold = int(device.property("ChargeEndThreshold").toUInt());
    }
    const bool draining = std::any_of(d->batteries.cbegin(), d->batteries.cend(), [](const Private::Battery& b) {
        return b.state == Discharging;
    });
    if (draining && M3Motion::expressive() && this->isVisible()) d->wave.start();
    else d->wave.stop();
    this->update();
}

QSize BatteryHealth::sizeHint() const {
    const int n = int(d->batteries.size());
    return QSize(400, n == 0 ? 0 : Header + n * CardHeight + (n - 1) * Gap + 8);
}

void BatteryHealth::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    for (const Private::Battery& battery : std::as_const(d->batteries)) read(battery.path);
    d->fill->snapTo(0.0);
    d->fill->animateTo(1.0);
    d->refresh.start();
}

void BatteryHealth::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    d->wave.stop();
    d->refresh.stop();
}

void BatteryHealth::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double time = d->clock.elapsed() / 1000.0;
    const double grow = qMax(0.0, d->fill->currentValue().toDouble());
    const bool calm = !M3Motion::expressive();

    p.setFont(flex(14, 600));
    p.setPen(s.primary);
    p.drawText(QRectF(4, 0, this->width(), Header - 8), Qt::AlignBottom | Qt::AlignLeft, d->batteries.size() > 1 ? tr("Batteries") : tr("Battery"));

    double y = Header;
    for (const Private::Battery& b : std::as_const(d->batteries)) {
        const QRectF card(0, y, this->width(), CardHeight);
        y += CardHeight + Gap;
        p.setPen(Qt::NoPen);
        p.setBrush(s.surfaceContainerHigh);
        p.drawRoundedRect(card, 24, 24);

        // The capsule: filled to the charge; the one draining has a moving surface
        const QRectF capsule(card.left() + 22, card.top() + 22, 52, card.height() - 44);
        const bool draining = b.state == Discharging;
        const bool charging = b.state == Charging;
        QPainterPath shell;
        shell.addRoundedRect(capsule, 16, 16);
        p.setBrush(s.surfaceContainerHighest.darker(115));
        p.drawPath(shell);
        p.save();
        p.setClipPath(shell);
        const double level = capsule.bottom() - capsule.height() * qBound(0.0, b.percentage / 100.0, 1.0);
        QPainterPath water;
        water.moveTo(capsule.left(), capsule.bottom());
        if (draining && !calm) {
            for (double x = capsule.left(); x <= capsule.right() + 1; x += 2) water.lineTo(x, level + 3 * std::sin((x - capsule.left()) / 9.0 + time * 3.2));
        } else {
            water.lineTo(capsule.left(), level);
            water.lineTo(capsule.right(), level);
        }
        water.lineTo(capsule.right(), capsule.bottom());
        water.closeSubpath();
        p.setBrush(b.percentage <= 10 && !charging ? s.error : charging ? s.tertiary : s.primary);
        p.drawPath(water);
        p.restore();
        p.setPen(b.percentage > 55 ? (b.percentage <= 10 && !charging ? s.onError : charging ? s.onTertiary : s.onPrimary) : s.onSurface);
        p.setFont(flex(15, 700));
        p.drawText(capsule.adjusted(0, 0, 0, -capsule.height() * 0.45), Qt::AlignCenter, QStringLiteral("%1%").arg(qRound(b.percentage)));

        // The words
        const double left = capsule.right() + 20;
        const double width = card.right() - 22 - left;
        p.setFont(flex(18, 600));
        p.setPen(s.onSurface);
        p.drawText(QRectF(left, card.top() + 18, width, 26), Qt::AlignLeft | Qt::AlignVCenter, b.name);
        QString status;
        if (draining) status = tr("Draining · %1 W").arg(QLocale().toString(b.rate, 'f', 1));
        else if (charging) status = tr("Charging · %1 W").arg(QLocale().toString(b.rate, 'f', 1));
        else if (b.state == Full) status = tr("Full");
        else status = tr("Resting");
        if (b.endThreshold > 0 && b.endThreshold < 100) status += QStringLiteral(" · ") + tr("charges to %1%").arg(b.endThreshold);
        p.setFont(flex(13, 450));
        p.setPen(s.onSurfaceVariant);
        p.drawText(QRectF(left, card.top() + 44, width, 20), Qt::AlignLeft | Qt::AlignVCenter, status);

        // Health: what it holds now against what it held new
        if (b.energyDesign > 0) {
            const double health = qBound(0.0, b.energyFull / b.energyDesign, 1.0);
            const QString word = health >= 0.8 ? tr("Good") : health >= 0.5 ? tr("Worn") : tr("Replace soon");
            const QColor chip = health >= 0.8 ? s.secondaryContainer : health >= 0.5 ? s.tertiaryContainer : s.errorContainer;
            const QColor chipInk = health >= 0.8 ? s.onSecondaryContainer : health >= 0.5 ? s.onTertiaryContainer : s.onErrorContainer;
            const QColor bar = health >= 0.8 ? s.primary : health >= 0.5 ? s.tertiary : s.error;

            p.setFont(flex(28, 650));
            p.setPen(s.onSurface);
            const QString figure = QStringLiteral("%1%").arg(qRound(health * 100));
            const double figureWidth = QFontMetrics(p.font()).horizontalAdvance(figure);
            p.drawText(QRectF(left, card.top() + 70, figureWidth + 4, 36), Qt::AlignLeft | Qt::AlignVCenter, figure);
            p.setFont(flex(12, 600));
            const double chipWidth = QFontMetrics(p.font()).horizontalAdvance(word) + 20;
            const QRectF chipRect(left + figureWidth + 10, card.top() + 77, chipWidth, 24);
            p.setPen(Qt::NoPen);
            p.setBrush(chip);
            p.drawRoundedRect(chipRect, 12, 12);
            p.setPen(chipInk);
            p.drawText(chipRect, Qt::AlignCenter, word);
            p.setFont(flex(12, 450));
            p.setPen(s.onSurfaceVariant);
            p.drawText(QRectF(chipRect.right() + 10, card.top() + 77, card.right() - 22 - chipRect.right() - 10, 24), Qt::AlignVCenter | Qt::AlignRight,
                tr("health · holds %1 of %2 Wh").arg(QLocale().toString(b.energyFull, 'f', 1), QLocale().toString(b.energyDesign, 'f', 1)));

            // The bar: the track is the battery new, the fill what's left of it
            const QRectF track(left, card.top() + 114, width, 12);
            p.setPen(Qt::NoPen);
            p.setBrush(s.surfaceContainerHighest);
            p.drawRoundedRect(track, 6, 6);
            const double shown = track.width() * health * qMin(grow, 1.05);
            p.setBrush(bar);
            p.drawRoundedRect(QRectF(track.left(), track.top(), qMax(12.0, shown), track.height()), 6, 6);
            if (b.cycles > 0) {
                p.setPen(s.onSurfaceVariant);
                p.drawText(QRectF(left, track.bottom() + 4, width, 18), Qt::AlignRight | Qt::AlignVCenter, tr("%n cycles", nullptr, b.cycles));
            }
        }
    }
}
