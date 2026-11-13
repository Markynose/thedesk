/****************************************
 *
 *   theDesk - onboarding: you (your picture's shape)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "onboardingyou.h"

#include <QBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTime>
#include <QPainterPath>
#include <QPushButton>
#include <Wm/desktopwm.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <m3usercard.h>
#include <memory>
#include <onboardingmanager.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <statemanager.h>
#include <tsettings.h>
#include <ttitlelabel.h>

namespace {
    // Shapes that frame a face well (Cookie4Sided first: theDesk's own)
    const char* const shapeNames[] = {"Cookie4Sided", "Cookie9Sided", "Sunny", "Clover8Leaf", "Cookie12Sided", "SoftBurst", "Gem", "Circle"};
    constexpr int shapeCount = int(std::size(shapeNames));

    const m3shapes::RoundedPolygon& shapeNamed(const char* name) {
        for (const auto& named : MaterialShapes::all()) {
            if (qstrcmp(named.name, name) == 0) return named.shape();
        }
        return MaterialShapes::circle();
    }

    // The picture in its shape, and the name under it as the greeter shows it
    class Preview : public QWidget {
        public:
            explicit Preview(QWidget* parent) :
                QWidget(parent) {
                this->setFixedHeight(lockHeight);
                image = QImage(M3UserCard::currentUserPicture());
                name = DesktopWm::userDisplayName();
                spring = new M3SpringAnimation(this);
                spring->setToken(M3Motion::SpatialDefault);
                spring->setStartValue(1.0);
                QObject::connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
            }

            void setShape(int index, bool animate) {
                if (index == current) return;
                if (animate && current >= 0) {
                    morph = std::make_unique<m3shapes::Morph>(shapeNamed(shapeNames[current]), shapeNamed(shapeNames[index]));
                    spring->snapTo(0.0);
                    spring->animateTo(1.0);
                } else {
                    morph.reset();
                }
                current = index;
                this->update();
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                const M3::Scheme& s = M3::currentScheme();
                const double k = spring->currentValue().toDouble();

                // A small lock screen: where the picture actually shows
                const QRectF lock((this->width() - lockWidth) / 2.0, 0, lockWidth, lockHeight);
                QPainterPath screen;
                screen.addRoundedRect(lock, 28, 28);
                painter.fillPath(screen, s.surfaceContainerLowest);
                painter.save();
                painter.setClipPath(screen);
                QPainterPath ground;
                ground.moveTo(lock.left(), lock.bottom());
                ground.lineTo(lock.left(), lock.bottom() - 70);
                ground.quadTo(lock.center().x(), lock.bottom() - 130, lock.right(), lock.bottom() - 70);
                ground.lineTo(lock.right(), lock.bottom());
                painter.fillPath(ground, s.surfaceContainerHigh);
                painter.restore();
                QFont time(QStringLiteral("Roboto Flex"));
                time.setPixelSize(44);
                time.setVariableAxis(QFont::Tag("wght"), 250);
                time.setVariableAxis(QFont::Tag("wdth"), 60);
                painter.setFont(time);
                painter.setPen(s.onSurface);
                painter.drawText(QRectF(lock.left(), lock.top() + 14, lock.width(), 56), Qt::AlignCenter, QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));

                const QRectF box(lock.center().x() - picture / 2.0, lock.top() + 78, picture, picture);
                QPainterPath shape = morph ? m3shapes::toPath(*morph, float(qBound(0.0, k, 1.0)), box)
                                           : m3shapes::toPath(shapeNamed(shapeNames[qMax(0, current)]), box);
                // A little spin as it changes (the overshoot sways it)
                QTransform spin;
                spin.translate(box.center().x(), box.center().y());
                spin.rotate((1 - k) * 45);
                spin.translate(-box.center().x(), -box.center().y());
                shape = spin.map(shape);
                if (!image.isNull()) {
                    // The picture's middle square, made once at this size
                    // (not stretched, not rescaled every frame)
                    const qreal dpr = this->devicePixelRatioF();
                    if (squared.isNull() || squaredDpr != dpr) {
                        const int side = qMin(image.width(), image.height());
                        const QImage middle = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side);
                        squared = QPixmap::fromImage(middle.scaled(QSize(picture, picture) * dpr, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
                        squared.setDevicePixelRatio(dpr);
                        squaredDpr = dpr;
                    }
                    painter.save();
                    painter.setClipPath(shape);
                    painter.drawPixmap(box.topLeft(), squared);
                    painter.restore();
                } else {
                    painter.fillPath(shape, s.primaryContainer);
                    QFont initial(QStringLiteral("Roboto Flex"));
                    initial.setPixelSize(56);
                    initial.setVariableAxis(QFont::Tag("wght"), 800);
                    painter.setFont(initial);
                    painter.setPen(s.onPrimaryContainer);
                    painter.drawText(box, Qt::AlignCenter, name.left(1).toUpper());
                }
                // The name, heavy and wide like the login screen's
                QFont font(QStringLiteral("Roboto Flex"));
                font.setPixelSize(32);
                font.setVariableAxis(QFont::Tag("wght"), 900);
                font.setVariableAxis(QFont::Tag("wdth"), 140);
                painter.setFont(font);
                painter.setPen(s.primary);
                painter.drawText(QRectF(lock.left(), box.bottom() + 6, lock.width(), 44), Qt::AlignHCenter | Qt::AlignVCenter, name);
            }

        private:
            static constexpr int picture = 112;
            static constexpr int lockWidth = 440;
            static constexpr int lockHeight = 260;
            QImage image;
            QPixmap squared;
            qreal squaredDpr = 0;
            QString name;
            int current = -1;
            std::unique_ptr<m3shapes::Morph> morph;
            M3SpringAnimation* spring;
    };

    // A row of shapes to pick from: the chosen one Primary, the others tonal
    class ShapePicker : public QWidget {
        public:
            std::function<void(int)> picked;

            explicit ShapePicker(QWidget* parent) :
                QWidget(parent) {
                this->setFixedSize(shapeCount * cell + (shapeCount - 1) * spacing, cell);
                this->setMouseTracking(true);
                this->setCursor(Qt::PointingHandCursor);
                pop = new M3SpringAnimation(this);
                pop->setToken(M3Motion::SpatialFast);
                pop->setStartValue(1.0);
                QObject::connect(pop, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
            }

            void setSelected(int index) {
                selected = index;
                this->update();
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                const M3::Scheme& s = M3::currentScheme();
                for (int i = 0; i < shapeCount; i++) {
                    const QRectF cellRect(i * (cell + spacing), 0, cell, cell);
                    double side = cell - 8;
                    if (i == selected) side *= qMax(0.0, pop->currentValue().toDouble());
                    const QRectF box(cellRect.center().x() - side / 2, cellRect.center().y() - side / 2, side, side);
                    QColor fill = i == selected ? s.primary : s.secondaryContainer;
                    if (i == hovered && i != selected) fill = s.surfaceContainerHighest;
                    painter.fillPath(m3shapes::toPath(shapeNamed(shapeNames[i]), box), fill);
                }
            }

            void mouseMoveEvent(QMouseEvent* event) override {
                const int at = indexAt(event->position());
                if (at != hovered) {
                    hovered = at;
                    this->update();
                }
            }

            void leaveEvent(QEvent*) override {
                hovered = -1;
                this->update();
            }

            void mousePressEvent(QMouseEvent* event) override {
                const int at = indexAt(event->position());
                if (at < 0) return;
                selected = at;
                pop->snapTo(0.6);
                pop->animateTo(1.0);
                if (picked) picked(at);
            }

        private:
            int indexAt(QPointF at) const {
                const int i = int(at.x() / (cell + spacing));
                return i >= 0 && i < shapeCount && at.x() - i * (cell + spacing) <= cell ? i : -1;
            }

            static constexpr int cell = 52;
            static constexpr int spacing = 8;
            int selected = -1;
            int hovered = -1;
            M3SpringAnimation* pop;
    };
} // namespace

struct OnboardingYouPrivate {
        tSettings settings{QStringLiteral("theSuite"), QStringLiteral("theDesk")};
        Preview* preview;
        ShapePicker* picker;
        int shown = 0; // the shape on screen
};

OnboardingYou::OnboardingYou(QWidget* parent) :
    OnboardingPage(parent) {
    d = new OnboardingYouPrivate();
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new tTitleLabel(this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setText(tr("This is you"));
    layout->addWidget(title);

    auto* description = new QLabel(tr("Your picture wears a shape when you log in and on the lock screen. Pick the one you like."), this);
    description->setObjectName(QStringLiteral("descriptionLabel"));
    description->setWordWrap(true);
    layout->addWidget(description);

    layout->addSpacing(8);
    d->preview = new Preview(this);
    layout->addWidget(d->preview);
    layout->addSpacing(8);
    d->picker = new ShapePicker(this);
    layout->addWidget(d->picker, 0, Qt::AlignHCenter);
    layout->addStretch();

    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    auto* next = new QPushButton(tr("Next"), this);
    next->setObjectName(QStringLiteral("nextButton"));
    next->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    connect(next, &QPushButton::clicked, this, [this] {
        // No shape chosen yet: the one shown is now theirs (else the login
        // and lock screens would go on cycling theDesk's own)
        if (!d->settings.keysInGroup(QStringLiteral("UserCard")).contains(QStringLiteral("shapes"))) {
            d->settings.setValue(QStringLiteral("UserCard/shapes"), QString::fromLatin1(shapeNames[d->shown]));
        }
        StateManager::onboardingManager()->nextStep();
    });
    bottom->addWidget(next);
    layout->addLayout(bottom);

    // The shape now: UserCard/shapes (one name), else theDesk's own
    int index = 0;
    const QString chosen = d->settings.keysInGroup(QStringLiteral("UserCard")).contains(QStringLiteral("shapes")) ? d->settings.value(QStringLiteral("UserCard/shapes")).toString().section(QLatin1Char(','), 0, 0).trimmed() : QString();
    for (int i = 0; i < shapeCount; i++) {
        if (chosen.compare(QLatin1String(shapeNames[i]), Qt::CaseInsensitive) == 0) index = i;
    }
    d->shown = index;
    d->preview->setShape(index, false);
    d->picker->setSelected(index);
    d->picker->picked = [this](int i) {
        d->shown = i;
        d->preview->setShape(i, true);
        d->settings.setValue(QStringLiteral("UserCard/shapes"), QString::fromLatin1(shapeNames[i]));
    };
}

OnboardingYou::~OnboardingYou() {
    delete d;
}

QString OnboardingYou::name() {
    return QStringLiteral("OnboardingYou");
}

QString OnboardingYou::displayName() {
    return tr("You");
}

void OnboardingYou::pageActivated() {
    d->preview->update();
}
