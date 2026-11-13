/****************************************
 *
 *   theDesk - Material 3 Expressive user card
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3usercard.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTime>
#include <m3motion.h>
#include <m3scheme.h>
#include <memory>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <unistd.h>

#include <Wm/desktopwm.h>
#include <tsettings.h>

namespace {
    // UserCard/shapes in theDesk.conf: one MaterialShapes name (Compose
    // spelling, e.g. Cookie4Sided) keeps the picture in that shape; several
    // take turns, a different one each time a card is shown. Unset: shapes
    // that frame a face well.
    const QList<const m3shapes::RoundedPolygon*>& pictureShapes() {
        static const QList<const m3shapes::RoundedPolygon*> shapes = [] {
            QStringList names;
            tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk"));
            if (settings.keysInGroup(QStringLiteral("UserCard")).contains(QStringLiteral("shapes"))) {
                names = settings.value(QStringLiteral("UserCard/shapes")).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            }
            if (names.isEmpty()) names = {QStringLiteral("Cookie9Sided"), QStringLiteral("Sunny"), QStringLiteral("Clover8Leaf"), QStringLiteral("Cookie12Sided"), QStringLiteral("SoftBurst"), QStringLiteral("Gem"), QStringLiteral("Cookie6Sided"), QStringLiteral("PuffyDiamond")};
            QList<const m3shapes::RoundedPolygon*> list;
            for (const QString& name : names) {
                for (const auto& named : MaterialShapes::all()) {
                    if (name.trimmed().compare(QLatin1String(named.name), Qt::CaseInsensitive) == 0) list.append(&named.shape());
                }
            }
            if (list.isEmpty()) list.append(&MaterialShapes::cookie9Sided());
            return list;
        }();
        return shapes;
    }

    const m3shapes::RoundedPolygon& shapeAt(int index) {
        const auto& shapes = pictureShapes();
        const int count = int(shapes.size());
        return *shapes[((index % count) + count) % count];
    }

    int nextShapeIndex() {
        // Different shape every time a card is shown, across the process
        static int counter = int(QTime::currentTime().msecsSinceStartOfDay() / 1000) % qMax(1, int(pictureShapes().size()));
        return counter++;
    }

    struct Metrics {
            double picture;
            double name;
            double greeting;
            double gap;
    };

    Metrics metricsFor(M3UserCard::CardSize size) {
        switch (size) {
            case M3UserCard::Compact:
                return {72, 36, 14, 16};
            case M3UserCard::Hero:
                return {128, 72, 18, 24};
            case M3UserCard::Large:
            default:
                return {96, 52, 16, 20};
        }
    }
} // namespace

struct M3UserCardPrivate {
        M3UserCard::CardSize size = M3UserCard::Large;
        Qt::Alignment alignment = Qt::AlignLeft;
        QString name;
        QString nameSuffix;
        QString greeting;
        bool greetingShown = true;
        QImage picture;

        int shapeIndex = 0;
        std::unique_ptr<m3shapes::Morph> morph;
        M3SpringAnimation* morphSpring = nullptr;   // 0 -> 1: previous shape -> this one
        M3SpringAnimation* nameSpring = nullptr;    // 0 -> 1: name weight/width
        M3SpringAnimation* greetingFade = nullptr;  // 0 -> 1: greeting opacity
};

M3UserCard::M3UserCard(QWidget* parent) :
    QWidget(parent) {
    d = new M3UserCardPrivate();
    d->morphSpring = new M3SpringAnimation(this);
    d->morphSpring->setToken(M3Motion::SpatialSlow);
    d->nameSpring = new M3SpringAnimation(this);
    d->nameSpring->setToken(M3Motion::SpatialDefault);
    d->greetingFade = new M3SpringAnimation(this);
    d->greetingFade->setToken(M3Motion::EffectsSlow);
    for (auto* spring : {d->morphSpring, d->nameSpring, d->greetingFade}) {
        spring->snapTo(1.0);
        connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    }

    d->shapeIndex = nextShapeIndex();
    setUser(QString());
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

M3UserCard::~M3UserCard() {
    delete d;
}

QString M3UserCard::currentUserPicture() {
    // README screenshots: a stand-in user, no picture
    if (qEnvironmentVariableIsSet("TD_DISPLAY_NAME")) return QString();
    const QStringList candidates = {
        QDir::home().absoluteFilePath(QStringLiteral(".face")),
        QDir::home().absoluteFilePath(QStringLiteral(".face.icon")),
        QStringLiteral("/var/lib/AccountsService/icons/") + QString::fromLocal8Bit(qgetenv("USER")),
    };
    for (const QString& path : candidates) {
        if (QFile::exists(path) && !QImage(path).isNull()) return path;
    }
    return QString();
}

QString M3UserCard::greetingForNow() {
    int hour = QTime::currentTime().hour();
    if (hour >= 5 && hour < 12) return tr("Good morning,");
    if (hour >= 12 && hour < 17) return tr("Good afternoon,");
    if (hour >= 17 && hour < 22) return tr("Good evening,");
    return tr("Good night,");
}

void M3UserCard::setCardSize(CardSize size) {
    d->size = size;
    this->updateGeometry();
    this->update();
}

void M3UserCard::setUser(QString displayName, QString picturePath) {
    QString ownName = qEnvironmentVariableIsSet("TD_DISPLAY_NAME") ? qEnvironmentVariable("TD_DISPLAY_NAME") : DesktopWm::userDisplayName();
    d->name = (displayName.isEmpty() ? ownName : displayName) + d->nameSuffix;
    if (picturePath.isEmpty() && displayName.isEmpty()) picturePath = currentUserPicture();
    d->picture = picturePath.isEmpty() ? QImage() : QImage(picturePath);
    this->updateGeometry();
    this->update();
}

void M3UserCard::setNameSuffix(QString suffix) {
    // Shares the name's springing type; strip the old suffix first
    if (!d->nameSuffix.isEmpty() && d->name.endsWith(d->nameSuffix)) d->name.chop(d->nameSuffix.size());
    d->nameSuffix = suffix;
    d->name += suffix;
    this->updateGeometry();
    this->update();
}

void M3UserCard::setGreeting(QString greeting) {
    d->greeting = greeting;
    this->update();
}

void M3UserCard::setGreetingShown(bool shown) {
    d->greetingShown = shown;
    this->updateGeometry();
    this->update();
}

void M3UserCard::setAlignment(Qt::Alignment alignment) {
    d->alignment = alignment;
    this->update();
}

const m3shapes::RoundedPolygon& M3UserCard::pictureShape() const {
    return shapeAt(d->shapeIndex);
}

void M3UserCard::playEntrance() {
    int previous = d->shapeIndex;
    d->shapeIndex = nextShapeIndex();
    if (pictureShapes().size() > 1) {
        if (d->shapeIndex == previous) d->shapeIndex++;
        d->morph = std::make_unique<m3shapes::Morph>(shapeAt(previous), shapeAt(d->shapeIndex));
    } else {
        // One shape: it blooms out of a circle each time the card appears
        d->morph = std::make_unique<m3shapes::Morph>(MaterialShapes::circle(), shapeAt(0));
    }
    d->morphSpring->snapTo(0.0);
    d->morphSpring->animateTo(1.0);
    d->nameSpring->snapTo(0.0);
    d->nameSpring->animateTo(1.0);
    d->greetingFade->snapTo(0.0);
    d->greetingFade->animateTo(1.0);
}

void M3UserCard::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    playEntrance();
}

QSize M3UserCard::sizeHint() const {
    Metrics m = metricsFor(d->size);
    QFont nameFont = this->font();
    nameFont.setPixelSize(int(m.name));
    nameFont.setVariableAxis(QFont::Tag("wght"), 1000);
    nameFont.setVariableAxis(QFont::Tag("wdth"), 151);
    int nameWidth = QFontMetrics(nameFont).horizontalAdvance(d->name);
    int width = int(m.picture + m.gap) + nameWidth;
    if (d->alignment & Qt::AlignHCenter) width = qMax(int(m.picture), nameWidth);
    const double greetingLine = d->greetingShown ? m.greeting * 1.5 : 0;
    int height = (d->alignment & Qt::AlignHCenter) ? int(m.picture + m.gap + greetingLine + m.name * 1.2) : int(qMax(m.picture, greetingLine + m.name * 1.2));
    return QSize(width + 8, height + 8);
}

QSize M3UserCard::minimumSizeHint() const {
    Metrics m = metricsFor(d->size);
    return QSize(int(m.picture) + 8, sizeHint().height());
}

QRectF M3UserCard::pictureRect() const {
    const Metrics m = metricsFor(d->size);
    return (d->alignment & Qt::AlignHCenter) ? QRectF((this->width() - m.picture) / 2, 4, m.picture, m.picture)
                                             : QRectF(4, (this->height() - m.picture) / 2, m.picture, m.picture);
}

void M3UserCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Colours: the M3 scheme behind this palette
    const QPalette& pal = this->palette();
    M3::Scheme s = M3::currentScheme();
    if (s.primary.rgb() != pal.color(QPalette::Highlight).rgb()) {
        s = M3::Scheme::generate(pal.color(QPalette::Highlight), pal.color(QPalette::Window).lightnessF() < 0.5);
    }

    const Metrics m = metricsFor(d->size);
    const bool centred = d->alignment & Qt::AlignHCenter;
    const double greetingLine = d->greetingShown ? m.greeting * 1.5 : 0;
    const double textHeight = greetingLine + m.name * 1.2;

    // ---- Picture, clipped to a morphing M3E shape
    QRectF pictureRect = this->pictureRect();
    double morph = d->morphSpring->currentValue().toDouble();
    QPainterPath shape = d->morph ? m3shapes::toPath(*d->morph, float(morph), pictureRect)
                                  : m3shapes::toPath(shapeAt(d->shapeIndex), pictureRect);
    if (!d->picture.isNull()) {
        painter.save();
        painter.setClipPath(shape);
        QImage scaled = d->picture.scaled(pictureRect.size().toSize() * this->devicePixelRatioF(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(this->devicePixelRatioF());
        painter.drawImage(pictureRect, scaled);
        painter.restore();
    } else {
        painter.fillPath(shape, s.primaryContainer);
        QFont initialFont = this->font();
        initialFont.setPixelSize(int(m.picture * 0.45));
        initialFont.setVariableAxis(QFont::Tag("wght"), 900);
        painter.setFont(initialFont);
        painter.setPen(s.onPrimaryContainer);
        painter.drawText(pictureRect, Qt::AlignCenter, d->name.left(1).toUpper());
    }

    // ---- Greeting and name
    QRectF textRect = centred ? QRectF(0, pictureRect.bottom() + m.gap, this->width(), textHeight)
                              : QRectF(pictureRect.right() + m.gap, (this->height() - textHeight) / 2, this->width() - pictureRect.right() - m.gap, textHeight);
    Qt::Alignment hAlign = centred ? Qt::AlignHCenter : Qt::AlignLeft;

    QFont greetingFont = this->font();
    greetingFont.setPixelSize(int(m.greeting));
    painter.setFont(greetingFont);
    QColor greetingColour = s.onSurfaceVariant;
    greetingColour.setAlphaF(qBound(0.0, d->greetingFade->currentValue().toDouble(), 1.0));
    painter.setPen(greetingColour);
    QString greeting = d->greeting.isEmpty() ? greetingForNow() : d->greeting;
    if (d->greetingShown) painter.drawText(QRectF(textRect.left(), textRect.top(), textRect.width(), greetingLine), hAlign | Qt::AlignVCenter, greeting);

    // Name: weight and width spring from regular to heaviest and widest; if
    // it would not fit, the width axis gives way first, then the size
    double t = d->nameSpring->currentValue().toDouble();
    double weight = qBound(100.0, 400 + 600 * t, 1000.0);
    double width = qBound(25.0, 100 + 51 * t, 151.0);
    QFont nameFont = this->font();
    nameFont.setPixelSize(int(m.name));
    nameFont.setVariableAxis(QFont::Tag("opsz"), qBound(8.0, m.name, 144.0));
    nameFont.setVariableAxis(QFont::Tag("wght"), float(weight));
    for (int attempt = 0; attempt < 12; attempt++) {
        nameFont.setVariableAxis(QFont::Tag("wdth"), float(width));
        if (QFontMetricsF(nameFont).horizontalAdvance(d->name) <= textRect.width() || width <= 25) break;
        width = qMax(25.0, width - 15);
        if (width <= 25) nameFont.setPixelSize(qMax(12, nameFont.pixelSize() - 4));
    }
    painter.setFont(nameFont);
    painter.setPen(s.primary);
    painter.drawText(QRectF(textRect.left(), textRect.top() + greetingLine, textRect.width(), m.name * 1.2), hAlign | Qt::AlignVCenter, d->name);
}
