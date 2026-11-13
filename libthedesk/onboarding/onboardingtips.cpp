/****************************************
 *
 *   theDesk - onboarding: a few tips
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "onboardingtips.h"
#include <QKeyEvent>
#include <QApplication>
#include <shapes/shapepath.h>
#include <shapes/materialshapes.h>
#include <keygrab.h>
#include <QHash>
#include <QMouseEvent>

#include <QBoxLayout>
#include <QElapsedTimer>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <cmath>
#include <m3scheme.h>
#include <onboardingmanager.h>
#include <statemanager.h>
#include <ttitlelabel.h>

namespace {
    // Step response of an M3 spring (0 at t <= 0; t in seconds)
    double spring(double t, double damping, double stiffness) {
        if (t <= 0) return 0;
        const double w = std::sqrt(stiffness);
        if (damping >= 1) return 1 - std::exp(-w * t) * (1 + w * t);
        const double wd = w * std::sqrt(1 - damping * damping);
        return 1 - std::exp(-damping * w * t) * (std::cos(wd * t) + damping * w / wd * std::sin(wd * t));
    }

    struct Tip {
            QString symbol; // a Material Symbols ligature
            QString headline;
            QString supporting;
            QString grab = QString(); // the shortcut it teaches (KeyGrab setting name): "Try it"
    };

    // The tips, each in an M3 list row (a tonal circle with a symbol, a
    // headline, supporting text), springing up into place one after another.
    // Tips that teach a shortcut can be tried: press it (or "Try it", which
    // does it) and the row ticks off; the next one to try is picked out
    class TipsList : public QWidget {
        public:
            explicit TipsList(QList<Tip> tips, QWidget* parent) :
                QWidget(parent), tips(tips) {
                this->setFixedHeight(rowsPerColumn() * row);
                this->setMouseTracking(true);
                frame.setInterval(16);
                QObject::connect(&frame, &QTimer::timeout, this, [this] {
                    const qint64 now = clock.elapsed();
                    if (demoAt >= 0 && now - demoAt > DemoLength) {
                        const QString grab = demoGrab;
                        demoAt = -1;
                        demoGrab.clear();
                        emit KeyGrab::hub()->activated(grab); // ticks it off
                    }
                    if (now - playAt > 1600 && now - lastDone > 1000 && demoAt < 0) frame.stop();
                    this->update();
                });
                QObject::connect(KeyGrab::hub(), &KeyGrabHub::activated, this, [this](QString name) {
                    for (const Tip& tip : std::as_const(this->tips)) {
                        if (tip.grab == name && !doneAt.contains(name)) {
                            doneAt.insert(name, clock.elapsed());
                            lastDone = clock.elapsed();
                            frame.start();
                        }
                    }
                });
                clock.start();
                playAt = 1000000000; // hidden until played
            }

            void play() {
                playAt = clock.elapsed() + 120;
                frame.start();
            }

            // Tried: the real thing when the shell is running (KeyGrab
            // reports it back), else a small preview here, then the tick
            void tryIt(const QString& grab) {
                if (grab.isEmpty() || demoAt >= 0) return;
                if (KeyGrab::trigger(grab)) return;
                demoGrab = grab;
                demoAt = clock.elapsed();
                frame.start();
            }

            bool teaches(const QString& grab) const {
                for (const Tip& tip : tips) {
                    if (tip.grab == grab) return true;
                }
                return false;
            }

        protected:
            int nextToTry() const {
                for (int i = 0; i < tips.size(); i++) {
                    if (!tips.at(i).grab.isEmpty() && !doneAt.contains(tips.at(i).grab)) return i;
                }
                return -1;
            }

            QRectF rowRect(int i) const {
                const double columnWidth = (this->width() - columnGap) / 2.0;
                const int column = i / rowsPerColumn(), line = i % rowsPerColumn();
                return QRectF(column * (columnWidth + columnGap), line * row, columnWidth, row);
            }

            QRectF chipRect(int i) const {
                const QRectF r = rowRect(i);
                return QRectF(r.right() - 84, r.top() + 32 - 16, 76, 32);
            }

            void paintEvent(QPaintEvent*) override {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                const M3::Scheme& s = M3::currentScheme();
                const QColor containers[] = {s.primaryContainer, s.tertiaryContainer, s.secondaryContainer};
                const QColor onContainers[] = {s.onPrimaryContainer, s.onTertiaryContainer, s.onSecondaryContainer};
                const int next = nextToTry();
                for (int i = 0; i < tips.size(); i++) {
                    const double t = (clock.elapsed() - playAt) / 1000.0 - i * 0.07;
                    const double k = spring(t, 0.7, 500);
                    const double alpha = qBound(0.0, spring(t, 1.0, 800), 1.0);
                    if (alpha <= 0.01) continue;
                    const Tip& tip = tips.at(i);
                    const bool done = doneAt.contains(tip.grab);
                    const double dk = done ? spring((clock.elapsed() - doneAt.value(tip.grab)) / 1000.0, 0.45, 220) : 0;
                    const QRectF r = rowRect(i);
                    painter.save();
                    painter.setOpacity(alpha);
                    painter.translate(r.left(), r.top() + (1 - k) * 20);

                    // The next one to try: a container to set it apart
                    if (i == next) {
                        QPainterPath box;
                        box.addRoundedRect(QRectF(0, 2, r.width(), row - 6), 24, 24);
                        painter.fillPath(box, s.surfaceContainerHigh);
                    }

                    // The symbol in its tonal circle; a burst with a tick once done
                    // Inset, so the picked-out row's container rounds around it
                    const QRectF circle(inset, 12, 40, 40);
                    if (done) {
                        const double grow = 6 * qBound(0.0, dk, 1.2);
                        QPainterPath burst = m3shapes::toPath(MaterialShapes::softBurst(), circle.adjusted(-grow, -grow, grow, grow));
                        painter.fillPath(burst, s.tertiaryContainer);
                    } else {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(containers[i % 3]);
                        painter.drawEllipse(circle);
                    }
                    QFont symbol(QStringLiteral("Material Symbols Rounded"));
                    symbol.setPixelSize(22);
                    symbol.setVariableAxis(QFont::Tag("FILL"), 1);
                    painter.setFont(symbol);
                    painter.setPen(done ? s.onTertiaryContainer : onContainers[i % 3]);
                    painter.drawText(circle, Qt::AlignCenter, done ? QStringLiteral("check") : tip.symbol);

                    // Headline and supporting text (leaving room for Try it)
                    const double x = inset + 56;
                    const double textWidth = (i == next ? chipRect(i).left() - r.left() - 12 : r.width() - 8) - x;
                    QFont headline(QStringLiteral("Roboto Flex"));
                    headline.setPixelSize(16);
                    headline.setVariableAxis(QFont::Tag("wght"), 600);
                    painter.setFont(headline);
                    painter.setPen(s.onSurface);
                    painter.drawText(QRectF(x, 10, textWidth, 22), Qt::AlignLeft | Qt::AlignBottom, tip.headline);
                    QFont supporting(QStringLiteral("Roboto Flex"));
                    supporting.setPixelSize(14);
                    painter.setFont(supporting);
                    painter.setPen(done ? s.tertiary : s.onSurfaceVariant);
                    painter.drawText(QRectF(x, 34, textWidth, row - 36), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, done ? OnboardingTips::tr("Done, nice") : tip.supporting);

                    if (i == next) {
                        // Try it: a small filled button that does the shortcut
                        const QRectF chip = chipRect(i).translated(-r.left(), -r.top());
                        QPainterPath button;
                        button.addRoundedRect(chip, 16, 16);
                        painter.fillPath(button, s.primary);
                        if (hoverChip) {
                            QColor layer = s.onPrimary;
                            layer.setAlphaF(0.08f);
                            painter.fillPath(button, layer);
                        }
                        QFont label(QStringLiteral("Roboto Flex"));
                        label.setPixelSize(13);
                        label.setVariableAxis(QFont::Tag("wght"), 650);
                        painter.setFont(label);
                        painter.setPen(s.onPrimary);
                        painter.drawText(chip, Qt::AlignCenter, OnboardingTips::tr("Try it"));
                    }
                    painter.restore();
                }
                if (demoAt >= 0) paintDemo(painter, s);
            }

            // A small version of what the shortcut opens, over the tips:
            // in on a spring, a moment, out
            void paintDemo(QPainter& painter, const M3::Scheme& s) {
                const double t = (clock.elapsed() - demoAt) / 1000.0;
                const double length = DemoLength / 1000.0;
                const double in = qBound(0.0, spring(t, 0.7, 260), 1.15);
                const double out = t > length - 0.35 ? qBound(0.0, (t - (length - 0.35)) / 0.35, 1.0) : 0;
                const double show = qMax(0.0, in - out * 1.0);
                QColor scrim = s.surfaceContainer;
                scrim.setAlphaF(float(0.75 * qBound(0.0, show, 1.0)));
                painter.fillRect(this->rect(), scrim);
                auto rounded = [&](QRectF box, double radius, QColor colour) {
                    QPainterPath path;
                    path.addRoundedRect(box, radius, radius);
                    painter.fillPath(path, colour);
                };
                auto line = [&](QRectF box) {
                    QColor c = s.onSurfaceVariant;
                    c.setAlphaF(0.45f);
                    rounded(box, box.height() / 2, c);
                };
                const QRectF area = this->rect();
                painter.save();
                painter.setOpacity(qBound(0.0, 1 - out, 1.0));
                if (demoGrab == QLatin1String("gatewayOpen")) {
                    // The Gateway: a sheet from the left, search and apps
                    QRectF sheet(area.left() + (show - 1) * 260, area.top() + 4, 240, area.height() - 8);
                    rounded(sheet, 24, s.surfaceContainerHigh);
                    rounded(QRectF(sheet.left() + 12, sheet.top() + 12, sheet.width() - 24, 40), 20, s.surfaceContainerHighest);
                    for (int i = 0; i < 5; i++) {
                        rounded(QRectF(sheet.left() + 18, sheet.top() + 70 + i * 40, 26, 26), 9, i % 2 ? s.tertiaryContainer : s.primaryContainer);
                        line(QRectF(sheet.left() + 54, sheet.top() + 79 + i * 40, 90 + (i * 31) % 60, 8));
                    }
                } else if (demoGrab == QLatin1String("statusCenter")) {
                    // The Status Center: from the top, quick tiles
                    QRectF sheet(area.left() + 20, area.top() + (show - 1) * (area.height() + 20), area.width() - 40, area.height() - 20);
                    rounded(sheet, 28, s.surfaceContainerHigh);
                    for (int i = 0; i < 6; i++) {
                        QRectF tile(sheet.left() + 18 + (i % 3) * ((sheet.width() - 48) / 3 + 6), sheet.top() + 18 + (i / 3) * 64, (sheet.width() - 48) / 3, 56);
                        rounded(tile, 20, i == 0 || i == 4 ? s.primary : s.surfaceContainerHighest);
                    }
                    line(QRectF(sheet.left() + 22, sheet.top() + 160, sheet.width() * 0.5, 10));
                    line(QRectF(sheet.left() + 22, sheet.top() + 182, sheet.width() * 0.35, 10));
                } else if (demoGrab == QLatin1String("overview")) {
                    // The Overview: the windows, side by side, zooming out
                    const double scale = 1.25 - 0.25 * qBound(0.0, show, 1.0);
                    for (int i = 0; i < 3; i++) {
                        const double w = (area.width() - 80) / 3 * scale;
                        QRectF window(area.center().x() + (i - 1) * (w + 16) - w / 2, area.center().y() - w * 0.33, w, w * 0.66);
                        rounded(window, 16, s.surfaceContainerHigh);
                        rounded(QRectF(window.left(), window.top(), window.width(), 18), 16, s.surfaceContainerHighest);
                        line(QRectF(window.left() + 12, window.top() + 30, window.width() * 0.5, 7));
                    }
                } else {
                    // Around you, or the display chooser: a sheet from the bottom
                    const bool displays = demoGrab == QLatin1String("displays");
                    QRectF sheet(area.left() + 20, area.top() + 30 + (1 - show) * area.height(), area.width() - 40, area.height() - 30);
                    rounded(sheet, 28, s.surfaceContainerHigh);
                    line(QRectF(sheet.left() + 24, sheet.top() + 22, 140, 14));
                    for (int i = 0; i < (displays ? 4 : 3); i++) {
                        if (displays) {
                            QRectF tile(sheet.left() + 18 + i * ((sheet.width() - 54) / 4 + 6), sheet.top() + 56, (sheet.width() - 54) / 4, 110);
                            rounded(tile, 22, i == 0 ? s.primaryContainer : s.surfaceContainerHighest);
                        } else {
                            const char* shapes[] = {"Cookie9Sided", "Clover4Leaf", "Gem"};
                            const m3shapes::RoundedPolygon* polygon = &MaterialShapes::circle();
                            for (const auto& named : MaterialShapes::all()) {
                                if (qstrcmp(named.name, shapes[i]) == 0) polygon = &named.shape();
                            }
                            painter.fillPath(m3shapes::toPath(*polygon, QRectF(sheet.left() + 22, sheet.top() + 56 + i * 48, 36, 36)), i == 1 ? s.tertiaryContainer : s.secondaryContainer);
                            line(QRectF(sheet.left() + 70, sheet.top() + 62 + i * 48, 120, 9));
                        }
                    }
                }
                painter.restore();
            }

            void mouseMoveEvent(QMouseEvent* event) override {
                const int next = nextToTry();
                const bool over = next >= 0 && chipRect(next).contains(event->position());
                if (over != hoverChip) {
                    hoverChip = over;
                    this->setCursor(over ? Qt::PointingHandCursor : Qt::ArrowCursor);
                    this->update();
                }
            }

            void mousePressEvent(QMouseEvent* event) override {
                const int next = nextToTry();
                if (next >= 0 && chipRect(next).contains(event->position())) tryIt(tips.at(next).grab);
            }

        private:
            static constexpr int row = 76; // a headline and up to two lines
            static constexpr int columnGap = 24;
            static constexpr int inset = 12;
            int rowsPerColumn() const {
                return int((tips.size() + 1) / 2);
            }
            QList<Tip> tips;
            QTimer frame;
            QElapsedTimer clock;
            qint64 playAt;
            qint64 lastDone = -100000;
            QHash<QString, qint64> doneAt;
            bool hoverChip = false;
            QString demoGrab;
            qint64 demoAt = -1;
            static constexpr qint64 DemoLength = 1700;
    };
} // namespace

struct OnboardingTipsPrivate {
        TipsList* list;
        bool superAlone = false; // Super went down and nothing else since
};

OnboardingTips::OnboardingTips(QWidget* parent) :
    OnboardingPage(parent) {
    d = new OnboardingTipsPrivate();
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new tTitleLabel(this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setText(tr("A few tips"));
    layout->addWidget(title);

    auto* description = new QLabel(tr("Some things worth knowing before you start. Try them: they tick off."), this);
    description->setObjectName(QStringLiteral("descriptionLabel"));
    description->setWordWrap(true);
    layout->addWidget(description);

    layout->addSpacing(4);
    d->list = new TipsList({
                               {QStringLiteral("apps"), tr("Press Super"), tr("The Gateway: your apps, search and settings"), QStringLiteral("gatewayOpen")},
                               {QStringLiteral("dashboard"), tr("Super + O, or click the clock"), tr("Overview: every window and desktop at once"), QStringLiteral("overview")},
                               {QStringLiteral("tune"), tr("Super + Tab"), tr("The Status Center: quick settings, notifications, your day"), QStringLiteral("statusCenter")},
                               {QStringLiteral("devices"), tr("Super + A, or the shapes in the bar"), tr("Around you: your displays, drives, Bluetooth and network"), QStringLiteral("aroundYou")},
                               {QStringLiteral("tv"), tr("Super + P"), tr("Choose how to use a TV or a second display"), QStringLiteral("displays")},
                               {QStringLiteral("swipe"), tr("Swipe a notification"), tr("Drag it sideways to dismiss it")},
                               {QStringLiteral("hourglass_top"), tr("Your loading screen"), tr("Pick its exit and colours in System Settings, Theme")},
                           },
        this);
    layout->addWidget(d->list);
    layout->addStretch();
    qApp->installEventFilter(this);

    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    auto* next = new QPushButton(tr("Next"), this);
    next->setObjectName(QStringLiteral("nextButton"));
    next->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    connect(next, &QPushButton::clicked, this, [] {
        StateManager::onboardingManager()->nextStep();
    });
    bottom->addWidget(next);
    layout->addLayout(bottom);
}

OnboardingTips::~OnboardingTips() {
    delete d;
}

QString OnboardingTips::name() {
    return QStringLiteral("OnboardingTips");
}

QString OnboardingTips::displayName() {
    return tr("Tips");
}

void OnboardingTips::pageActivated() {
    d->list->play();
}

bool OnboardingTips::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched)
    // The shortcuts, tried here: during first-time setup nothing else is
    // listening for them yet (when the shell is, its grabs take the keys and
    // report back through KeyGrab's hub instead)
    if (!this->isVisible() || (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)) return false;
    auto* key = static_cast<QKeyEvent*>(event);
    const bool super = key->key() == Qt::Key_Super_L || key->key() == Qt::Key_Super_R || key->key() == Qt::Key_Meta;
    if (event->type() == QEvent::KeyPress) {
        if (key->isAutoRepeat()) return false;
        if (super) {
            d->superAlone = true;
            return false;
        }
        d->superAlone = false;
        if (!(key->modifiers() & Qt::MetaModifier)) return false;
        QString grab;
        switch (key->key()) {
            case Qt::Key_O:
                grab = QStringLiteral("overview");
                break;
            case Qt::Key_Tab:
                grab = QStringLiteral("statusCenter");
                break;
            case Qt::Key_A:
                grab = QStringLiteral("aroundYou");
                break;
            case Qt::Key_P:
                grab = QStringLiteral("displays");
                break;
            default:
                return false;
        }
        if (!d->list->teaches(grab)) return false;
        d->list->tryIt(grab);
        return true;
    }
    if (super && d->superAlone && !key->isAutoRepeat()) {
        d->superAlone = false;
        d->list->tryIt(QStringLiteral("gatewayOpen"));
        return true;
    }
    return false;
}

struct OnboardingShortcutsPrivate {
        TipsList* list;
};

OnboardingShortcuts::OnboardingShortcuts(QWidget* parent) :
    OnboardingPage(parent) {
    d = new OnboardingShortcutsPrivate();
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new tTitleLabel(this);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setText(tr("Handy shortcuts"));
    layout->addWidget(title);

    auto* description = new QLabel(tr("A few more, for when you need them."), this);
    description->setObjectName(QStringLiteral("descriptionLabel"));
    description->setWordWrap(true);
    layout->addWidget(description);

    layout->addSpacing(4);
    d->list = new TipsList({
                               {QStringLiteral("lock"), tr("Super + L"), tr("Lock the screen")},
                               {QStringLiteral("timer"), tr("Super + Z"), tr("Start or stop a focus session")},
                               {QStringLiteral("volume_off"), tr("The mute key"), tr("Sound, Critical only, No notifications, Mute: it goes round them")},
                               {QStringLiteral("photo_camera"), tr("Print Screen, or Super + Alt + P"), tr("A screenshot, copied and saved in Pictures")},
                               {QStringLiteral("keyboard"), tr("Caps Lock"), tr("The next keyboard layout (Shift + Caps Lock locks capitals)")},
                               {QStringLiteral("block"), tr("Ctrl + Alt + Esc"), tr("Make a stuck app quit")},
                               {QStringLiteral("terminal"), tr("Alt + F2"), tr("Run a command")},
                               {QStringLiteral("healing"), tr("Hold Ctrl as theDesk starts"), tr("Safe Mode, if something's broken")},
                           },
        this);
    layout->addWidget(d->list);
    layout->addStretch();

    auto* bottom = new QHBoxLayout();
    bottom->addStretch();
    auto* next = new QPushButton(tr("Next"), this);
    next->setObjectName(QStringLiteral("nextButton"));
    next->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    connect(next, &QPushButton::clicked, this, [] {
        StateManager::onboardingManager()->nextStep();
    });
    bottom->addWidget(next);
    layout->addLayout(bottom);
}

OnboardingShortcuts::~OnboardingShortcuts() {
    delete d;
}

QString OnboardingShortcuts::name() {
    return QStringLiteral("OnboardingShortcuts");
}

QString OnboardingShortcuts::displayName() {
    return tr("Shortcuts");
}

void OnboardingShortcuts::pageActivated() {
    d->list->play();
}
