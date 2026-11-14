/****************************************
 *
 *   theDesk - Material 3 Expressive Status Center overview
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3overview.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSysInfo>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStorageInfo>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QAbstractItemView>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QMenu>
#include <QStandardItemModel>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
#include <m3motion.h>
#include <m3scheme.h>
#include <claude/claudeusage.h>
#include <m3usercard.h>
#include <m3fabmenu.h>
#include <m3rollinglabel.h>
#include <keygrab.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <mpris/mprisengine.h>
#include <mpris/mprisplayer.h>
#include <shapes/materialshapes.h>
#include <shapes/morph.h>
#include <shapes/shapepath.h>
#include <tsettings.h>
#include <unistd.h>

namespace {
    QString tr(const char* text) {
        return QCoreApplication::translate("M3Overview", text);
    }

    QFont flex(const QFont& base, double px, double wght, double wdth = 100) {
        QFont font = base;
        font.setFamily(QStringLiteral("Roboto Flex"));
        font.setPixelSize(qRound(px));
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(8.0, px, 144.0)));
        // in steps: the counting numbers and the clock move their axes each frame, and each new value is a new font instance
        font.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, std::round(wght / 10) * 10, 1000.0)));
        font.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, std::round(wdth), 151.0)));
        return font;
    }

    QColor alpha(QColor colour, double a) {
        colour.setAlphaF(colour.alphaF() * qBound(0.0, a, 1.0));
        return colour;
    }

    QPainterPath rounded(const QRectF& rect, double radius) {
        QPainterPath path;
        radius = qMin(radius, qMin(rect.width(), rect.height()) / 2);
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    QString duration(qint64 seconds) {
        seconds = qMax<qint64>(0, seconds);
        qint64 hours = seconds / 3600, minutes = (seconds % 3600) / 60;
        if (hours == 0) return tr("%1m").arg(minutes);
        return tr("%1h %2m").arg(hours).arg(minutes);
    }

    QString mediaTime(qint64 micro) {
        qint64 s = qMax<qint64>(0, micro / 1000000);
        return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    }

    QString rate(double bytesPerSecond) {
        if (bytesPerSecond >= 1e6) return tr("%1 MB/s").arg(QLocale().toString(bytesPerSecond / 1e6, 'f', 1));
        return tr("%1 kB/s").arg(QLocale().toString(bytesPerSecond / 1e3, 'f', 0));
    }

    QString readFile(const QString& path) {
        QFile file(path);
        if (!file.open(QFile::ReadOnly)) return {};
        return QString::fromLatin1(file.readAll()).trimmed();
    }

    // M3 Expressive wavy progress (WavyProgressIndicator): the wave up to
    // `value`, a 4 dp gap, the flat inactive track and its stop dot.
    // `amplitude` 0..1 scales the 3 dp wave (0 = flat, as when paused).
    void paintWave(QPainter* p, const QRectF& track, double value, double amplitude, double phase, QColor active, QColor inactive, double stroke = 4) {
        const double y = track.center().y(), wavelength = 40, gap = 4;
        const double end = track.left() + track.width() * qBound(0.0, value, 1.0);
        p->save();
        p->setBrush(Qt::NoBrush);
        if (end > track.left() + 0.5) {
            QPainterPath wave;
            for (double x = track.left(); x <= end; x += 1) {
                double wy = y + 3 * amplitude * qSin(((x - track.left()) / wavelength + phase) * 2 * M_PI);
                if (x == track.left()) wave.moveTo(x, wy);
                else wave.lineTo(x, wy);
            }
            p->setPen(QPen(active, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawPath(wave);
        }
        double rest = end > track.left() + 0.5 ? end + gap + stroke : track.left();
        if (rest < track.right()) {
            p->setPen(QPen(inactive, stroke, Qt::SolidLine, Qt::RoundCap));
            p->drawLine(QPointF(rest, y), QPointF(track.right(), y));
            p->setPen(Qt::NoPen);
            p->setBrush(active);
            p->drawEllipse(QPointF(track.right(), y), stroke / 2, stroke / 2);
        }
        p->restore();
    }

    void paintLinear(QPainter* p, const QRectF& track, double value, QColor active, QColor inactive, double stroke = 4) {
        const double end = track.width() * qBound(0.0, value, 1.0), gap = 4;
        const double y = track.center().y() - stroke / 2;
        if (end > 0.5) p->fillPath(rounded(QRectF(track.left(), y, qMax(stroke, end), stroke), stroke / 2), active);
        double from = end > 0.5 ? end + gap : 0;
        if (from < track.width()) p->fillPath(rounded(QRectF(track.left() + from, y, track.width() - from, stroke), stroke / 2), inactive);
    }

    // A Material Symbols glyph centred on `c`
    void symbol(QPainter* p, QPointF c, const QString& name, QColor colour, double px) {
        QFont font(QStringLiteral("Material Symbols Rounded"));
        font.setPixelSize(int(px));
        font.setVariableAxis(QFont::Tag("FILL"), 1);
        font.setVariableAxis(QFont::Tag("wght"), 500);
        font.setVariableAxis(QFont::Tag("opsz"), float(qBound(20.0, px, 48.0)));
        p->setFont(font);
        p->setPen(colour);
        p->drawText(QRectF(c.x() - px, c.y() - px, 2 * px, 2 * px), Qt::AlignCenter, name);
    }

    void shapeAt(QPainter* p, const m3shapes::RoundedPolygon& polygon, QPointF c, double size, double turn, QColor colour) {
        p->save();
        p->translate(c);
        p->rotate(turn);
        p->fillPath(m3shapes::toPath(polygon, QRectF(-size / 2, -size / 2, size, size)), colour);
        p->restore();
    }

    // M3 Expressive circular wavy progress: the wavy arc from the top up to
    // `value`, a gap, then the flat track
    void paintRing(QPainter* p, QPointF c, double radius, double stroke, double value, double phase, QColor active, QColor track) {
        value = qBound(0.0, value, 1.0);
        const double gap = (stroke + 4) / radius, end = value * 2 * M_PI, amplitude = 2;
        const int waves = qMax(6, qRound(2 * M_PI * radius / 22));
        auto at = [&](double a, double r) {
            return c + QPointF(r * qSin(a), -r * qCos(a));
        };
        p->save();
        p->setBrush(Qt::NoBrush);
        if (end + gap < 2 * M_PI - gap) {
            QPainterPath rest(at(end + gap, radius));
            for (double a = end + gap; a <= 2 * M_PI - gap; a += 0.02) rest.lineTo(at(a, radius));
            p->setPen(QPen(track, stroke, Qt::SolidLine, Qt::RoundCap));
            p->drawPath(rest);
        }
        if (end > 0.01) {
            QPainterPath wave(at(0, radius + amplitude * qSin(-phase)));
            for (double a = 0; a <= end; a += 0.01) wave.lineTo(at(a, radius + amplitude * qSin(waves * a - phase)));
            p->setPen(QPen(active, stroke, Qt::SolidLine, Qt::RoundCap));
            p->drawPath(wave);
        }
        p->restore();
    }

    // ---- Cards ------------------------------------------------------------

    // A rounded card that rises into place on the expressive spring
    class Card : public QWidget {
        public:
            Card(int height, QWidget* parent) :
                QWidget(parent) {
                this->setFixedHeight(height);
                enter = new M3SpringAnimation(this);
                enter->setToken(M3Motion::SpatialDefault);
                enter->snapTo(1.0);
                QObject::connect(enter, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
            }

            virtual void playEntrance(int delay) {
                enter->snapTo(0.0);
                this->update();
                QTimer::singleShot(delay, this, [this] {
                    enter->animateTo(1.0);
                });
            }

        protected:
            M3SpringAnimation* enter;
            double radius = 28;

            virtual QColor container(const M3::Scheme& s) const {
                return s.surfaceContainerHigh;
            }
            virtual void paintContent(QPainter* p, const M3::Scheme& s) = 0;

            void paintEvent(QPaintEvent*) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                const double v = enter->currentValue().toDouble();
                p.setOpacity(qBound(0.0, v * 1.4, 1.0));
                p.translate(0, (1 - v) * 28);
                M3::Scheme s = M3::currentScheme();
                p.fillPath(rounded(QRectF(this->rect()), radius), container(s));
                paintContent(&p, s);
            }
    };

    // A stat as a pill: its symbol in a MaterialShape holder, a big number
    // that counts up, its label, and a level or a detail line
    class StatCard : public Card {
        public:
            enum Footer {
                None,
                Wave,
                Linear
            };

            StatCard(Footer footer, const m3shapes::RoundedPolygon& holder, QString symbolName, bool tertiary, QWidget* parent) :
                Card(76, parent), holder(holder), symbolName(std::move(symbolName)), tertiary(tertiary), footer(footer) {
                radius = 38;
                count = new M3SpringAnimation(this);
                count->setToken(M3Motion::SpatialSlow);
                count->snapTo(1.0);
                QObject::connect(count, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
                if (footer == Wave) {
                    drift.setInterval(50);
                    QObject::connect(&drift, &QTimer::timeout, this, [this] {
                        phase += 0.02; // the wave flows back towards the start
                        this->update(QRect(76 - 8, this->height() - 18 - 10, this->width() - 76 - 24 + 16, 8 + 20)); // only the wave
                    });
                }
            }

            void playEntrance(int delay) override {
                Card::playEntrance(delay);
                count->snapTo(0.0);
                QTimer::singleShot(delay + 60, this, [this] {
                    count->animateTo(1.0);
                });
            }

            void setValue(double target, std::function<QString(double)> formatter) {
                value = target;
                format = std::move(formatter);
                // A level has no room for the detail line: it shows on hover
                this->setToolTip(footer == None ? QString() : detail);
                this->update();
            }

            QString label, detail, symbolName;
            double level = 0;
            double holderTurn = 0;
            QTimer drift;

        protected:
            void showEvent(QShowEvent*) override {
                if (footer == Wave) drift.start();
            }
            void hideEvent(QHideEvent*) override {
                drift.stop();
            }

            void paintContent(QPainter* p, const M3::Scheme& s) override {
                const double t = qBound(0.0, count->currentValue().toDouble(), 1.2);
                const QPointF holderCentre(40, this->height() / 2.0);
                shapeAt(p, holder, holderCentre, 50, holderTurn, tertiary ? s.tertiaryContainer : s.secondaryContainer);
                symbol(p, holderCentre, symbolName, tertiary ? s.onTertiaryContainer : s.onSecondaryContainer, 22);

                const double x = 76, w = this->width() - x - 24;
                // The number gains weight as it counts up
                p->setFont(flex(this->font(), 22, 400 + 380 * qMin(1.0, t), 115));
                p->setPen(s.onSurface);
                p->drawText(QRectF(x, 10, w, 30), Qt::AlignLeft | Qt::AlignVCenter, format ? format(value * qMin(1.0, t)) : QString());
                p->setFont(flex(this->font(), 12, 500));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(x, 38, w, 16), Qt::AlignLeft | Qt::AlignVCenter, label);
                const QRectF track(x, this->height() - 18, w, 8);
                if (footer == Wave) paintWave(p, track, level * qMin(1.0, t), 1, phase, s.primary, s.secondaryContainer);
                if (footer == Linear) paintLinear(p, track, level * qMin(1.0, t), s.tertiary, s.secondaryContainer);
                if (footer == None) {
                    p->setFont(flex(this->font(), 11, 450));
                    p->setPen(s.outline);
                    p->drawText(QRectF(x, this->height() - 24, w, 16), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(detail, Qt::ElideRight, int(w)));
                }
            }

        private:
            const m3shapes::RoundedPolygon& holder;
            bool tertiary;
            Footer footer;
            M3SpringAnimation* count;
            double value = 0;
            double phase = 0;
            std::function<QString(double)> format;
    };

    // Your day as one ribbon on the living horizon's clock: night and day, the
    // sun's rise and set as MaterialShapes and this session. The big clock
    // rides the "now" line, which sweeps in from midnight as the pane opens
    // while the clock counts up with it. World clocks (the24's list) are pins
    // at their own hour; "Add city" searches and drops a new one on.
    class RibbonCard : public Card {
        public:
            static constexpr double Band = 112, RibbonHeight = 74, Lane = 46, LabelWidth = 120, ClockPx = 96;
            // The horizon's own schedule (m3/horizonclock.cpp): dawn at
            // 07:00, the golden hour ending in sunset at 18:30
            static constexpr double Dawn = 7.0, Sunset = 18.5;
            static constexpr int CityRole = Qt::UserRole + 1, ZoneRole = Qt::UserRole + 2;

            QWidget* header = nullptr;
            QString notice, noticeDetails;

            RibbonCard(QDateTime sessionStart, QWidget* parent) :
                Card(int(heightFor(0)), parent), sessionStart(std::move(sessionStart)), worldClocks(QStringLiteral("theSuite"), QStringLiteral("the24")) {
                auto spring = [this](M3Motion::Token token, double at) {
                    auto* animation = new M3SpringAnimation(this);
                    animation->setToken(token);
                    animation->snapTo(at);
                    QObject::connect(animation, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
                    return animation;
                };
                reveal = spring(M3Motion::SpatialSlow, 1);
                fly = spring(M3Motion::SpatialDefault, 1);
                labels = spring(M3Motion::EffectsDefault, 1);
                field = spring(M3Motion::SpatialFast, 0);
                QObject::connect(reveal, &M3SpringAnimation::finished, this, [this] {
                    tick();
                });
                QObject::connect(field, &M3SpringAnimation::valueChanged, this, [this] {
                    maskSearch();
                });
                QObject::connect(field, &M3SpringAnimation::finished, this, [this] {
                    if (field->endValue().toDouble() > 0.5) return;
                    const bool hadFocus = search->hasFocus();
                    search->hide();
                    add->show();
                    if (hadFocus) add->setFocus();
                });

                add = new QToolButton(this);
                add->setProperty("m3IconButtonStyle", QStringLiteral("tonal"));
                add->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
                add->setIconSize(QSize(24, 24));
                add->setFixedSize(48, 48);
                add->setToolTip(tr("Add city"));
                add->setAccessibleName(tr("Add city"));
                QObject::connect(add, &QToolButton::clicked, this, [this] {
                    openSearch();
                });

                search = new QLineEdit(this);
                search->setProperty("m3SearchBar", true);
                search->setPlaceholderText(tr("Search for a city"));
                search->setFont(flex(this->font(), 16, 500));
                search->addAction(QIcon::fromTheme(QStringLiteral("edit-find")), QLineEdit::LeadingPosition);
                search->installEventFilter(this);
                search->hide();
                completer = new QCompleter(this);
                completer->setCaseSensitivity(Qt::CaseInsensitive);
                completer->setCompletionRole(CityRole);
                completer->setMaxVisibleItems(6);
                search->setCompleter(completer);
                QObject::connect(completer, qOverload<const QModelIndex&>(&QCompleter::activated), this, [this](const QModelIndex& index) {
                    addCity(index.data(ZoneRole).toByteArray());
                });
                QObject::connect(search, &QLineEdit::returnPressed, this, [this] {
                    // Enter without picking takes the top suggestion
                    if (completer->completionCount() > 0) addCity(completer->completionModel()->index(0, 0).data(ZoneRole).toByteArray());
                });

                QObject::connect(&worldClocks, &tSettings::settingChanged, this, [this](const QString& key) {
                    if (key == QStringLiteral("WorldClock/timezones")) reload();
                });
                this->setMouseTracking(true);
                reload();
            }

            void playEntrance(int delay) override {
                Card::playEntrance(delay);
                // The clock is painted while it counts, then handed to a
                // rolling label (a fresh one, so it doesn't roll from the last opening)
                delete clock;
                clock = nullptr;
                reveal->snapTo(0.0);
                QTimer::singleShot(delay + 150, this, [this] {
                    reveal->animateTo(1.0);
                });
            }

            // Once a second: the clock's minutes roll, pins move along
            void tick() {
                layoutLanes();
                placeChildren();
                if (reveal->state() == QAbstractAnimation::Running || reveal->currentValue().toDouble() < 0.99) return;
                const Layout l = layout(nowHour());
                if (!clock) {
                    clock = new M3RollingLabel(this);
                    clock->setAlignment(Qt::AlignCenter);
                    clock->setAttribute(Qt::WA_TransparentForMouseEvents);
                }
                QPalette palette = clock->palette();
                palette.setColor(QPalette::WindowText, M3::currentScheme().onSurface);
                clock->setPalette(palette);
                clock->setFont(clockFont(l.px, 1));
                clock->setGeometry(l.clock.toAlignedRect());
                clock->setText(clockText(nowHour()));
                clock->show();
                this->update();
            }

        protected:
            QColor container(const M3::Scheme&) const override {
                return Qt::transparent; // the ribbon sits on the page itself
            }

            void resizeEvent(QResizeEvent*) override {
                placeChildren();
                if (clock) clock->setGeometry(layout(nowHour()).clock.toAlignedRect());
            }

            void paintContent(QPainter* p, const M3::Scheme& s) override {
                const double rv = qBound(0.0, reveal->currentValue().toDouble(), 1.0);
                const double hour = nowHour() * rv;
                const QRectF rib = ribbonRect();
                const Layout l = layout(hour);

                // The ribbon draws in from midnight
                p->save();
                const QRectF grown(rib.left(), rib.top(), qMax(rib.height(), rib.width() * rv), rib.height());
                p->setClipPath(rounded(grown, rib.height() / 2), Qt::IntersectClip);
                const QPainterPath body = rounded(rib, rib.height() / 2);
                p->fillPath(body, s.surfaceContainerHighest);
                QLinearGradient day(rib.topLeft(), rib.topRight());
                const QColor clear = alpha(s.tertiaryContainer, 0);
                day.setColorAt(0, clear);
                day.setColorAt((Dawn - 1) / 24, clear);
                day.setColorAt((Dawn + 1) / 24, s.tertiaryContainer);
                day.setColorAt((Sunset - 1) / 24, s.tertiaryContainer);
                day.setColorAt((Sunset + 1) / 24, clear);
                day.setColorAt(1, clear);
                p->fillPath(body, day);
                p->setFont(flex(this->font(), 11, 500));
                p->setPen(alpha(s.onSurfaceVariant, 0.8));
                for (int h = 3; h < 24; h += 3) p->drawText(QRectF(xAt(h) - 20, rib.top() + 6, 40, 14), Qt::AlignCenter, QStringLiteral("%1").arg(h, 2, 10, QLatin1Char('0')));
                // This session (it may have started yesterday)
                const double from = sessionStart.date() < QDate::currentDate() ? 0 : sessionStart.time().msecsSinceStartOfDay() / 3600000.0;
                if (xAt(hour) > xAt(from)) p->fillPath(rounded(QRectF(xAt(from), rib.bottom() - 16, qMax(6.0, xAt(hour) - xAt(from)), 6), 3), s.primary);
                shapeAt(p, MaterialShapes::sunny(), QPointF(xAt(Dawn), rib.center().y()), 28, 0, s.tertiary);
                shapeAt(p, MaterialShapes::cookie9Sided(), QPointF(xAt(Sunset), rib.center().y()), 26, 0, s.tertiary);
                p->restore();

                // The cities, the one just added last as it falls in
                for (const Pin& pin : pins) {
                    if (pin.zone == flying && fly->currentValue().toDouble() < 1) continue;
                    paintPin(p, s, pin, QPointF(xAt(pinHour(pin, hour)), rib.center().y()), rv, rv, hour);
                }
                for (const Pin& pin : pins) {
                    if (pin.zone != flying || fly->currentValue().toDouble() >= 1) continue;
                    const double u = qBound(0.0, fly->currentValue().toDouble(), 1.1);
                    const QPointF to(xAt(pinHour(pin, hour)), rib.center().y());
                    const QPointF at = flyFrom + (to - flyFrom) * u + QPointF(0, -90 * qSin(M_PI * qBound(0.0, u, 1.0)));
                    paintPin(p, s, pin, at, u, labels->currentValue().toDouble(), hour);
                }

                // Now, with the clock riding it
                const double nx = xAt(hour);
                p->setPen(QPen(s.onSurface, 2.5, Qt::SolidLine, Qt::RoundCap));
                p->drawLine(QPointF(nx, Band - 12), QPointF(nx, rib.bottom() + 4));
                if (!clock) {
                    p->setFont(clockFont(l.px, rv));
                    p->drawText(l.clock, Qt::AlignCenter, clockText(hour));
                }
                p->save();
                p->setOpacity(p->opacity() * rv);
                const Qt::Alignment side = (l.dateLeft ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
                const QDate today = QDate::currentDate();
                p->setFont(flex(this->font(), 24, 760, 110));
                p->drawText(QRectF(l.date.left(), Band / 2 - 30, l.date.width(), 30), side, QLocale().dayName(today.dayOfWeek()));
                p->setFont(flex(this->font(), 16, 500));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(l.date.left(), Band / 2, l.date.width(), 24), side, QLocale().toString(today, QStringLiteral("d MMMM")));
                if (!notice.isEmpty()) {
                    // The clocks are about to change (the DST notice)
                    const QRectF n = noticeRect(l);
                    p->fillPath(rounded(n, n.height() / 2), s.tertiaryContainer);
                    symbol(p, QPointF(n.left() + 14, n.center().y()), QStringLiteral("schedule"), s.onTertiaryContainer, 16);
                    p->setFont(flex(this->font(), 12, 560));
                    p->setPen(s.onTertiaryContainer);
                    p->drawText(n.adjusted(26, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter, notice);
                }
                p->restore();
            }

            bool event(QEvent* event) override {
                if (event->type() == QEvent::ToolTip) {
                    auto* help = static_cast<QHelpEvent*>(event);
                    const int index = pinAt(help->pos());
                    QString text;
                    if (index >= 0) {
                        const Pin& pin = pins.at(index);
                        text = QStringLiteral("%1  ·  %2").arg(pin.city, offsetText(pin));
                    } else if (!notice.isEmpty() && noticeRect(layout(nowHour())).contains(help->pos())) {
                        text = noticeDetails;
                    } else if (ribbonRect().contains(help->pos())) {
                        text = tr("Dawn %1  ·  Sunset %2  ·  Session %3").arg(QStringLiteral("07:00"), QStringLiteral("18:30"), duration(sessionStart.secsTo(QDateTime::currentDateTime())));
                    }
                    if (text.isEmpty()) QToolTip::hideText();
                    else QToolTip::showText(help->globalPos(), text, this);
                    return true;
                }
                return Card::event(event);
            }

            void contextMenuEvent(QContextMenuEvent* event) override {
                const int index = pinAt(event->pos());
                if (index < 0) return;
                const QByteArray zone = pins.at(index).zone;
                QMenu menu(this);
                menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), tr("Remove %1").arg(pins.at(index).city), this, [this, zone] {
                    QStringList zones = worldClocks.delimitedList(QStringLiteral("WorldClock/timezones"));
                    zones.removeAll(QString::fromLatin1(zone));
                    worldClocks.setDelimitedList(QStringLiteral("WorldClock/timezones"), zones);
                    reload();
                });
                menu.exec(event->globalPos());
            }

            bool eventFilter(QObject* watched, QEvent* event) override {
                if (watched == search) {
                    auto* key = static_cast<QKeyEvent*>(event);
                    // Esc closes the search before it reaches the Status Center
                    if (event->type() == QEvent::ShortcutOverride && key->key() == Qt::Key_Escape) {
                        event->accept();
                        return true;
                    }
                    if (event->type() == QEvent::KeyPress && key->key() == Qt::Key_Escape) {
                        closeSearch();
                        return true;
                    }
                    if (event->type() == QEvent::FocusOut && static_cast<QFocusEvent*>(event)->reason() != Qt::PopupFocusReason) closeSearch();
                }
                return Card::eventFilter(watched, event);
            }

        private:
            struct Pin {
                    QByteArray zone;
                    QString city;
                    QTimeZone tz;
                    int lane = 0;
            };
            struct Layout {
                    QRectF clock, date, header;
                    double px;
                    bool dateLeft;
            };

            QDateTime sessionStart;
            tSettings worldClocks;
            QList<Pin> pins;
            int lanes = 0;
            M3SpringAnimation* reveal;
            M3SpringAnimation* fly;
            M3SpringAnimation* labels;
            M3SpringAnimation* field;
            QByteArray flying;
            QPointF flyFrom;
            QPointer<M3RollingLabel> clock;
            QToolButton* add;
            QLineEdit* search;
            QCompleter* completer;
            QStandardItemModel* cities = nullptr;

            static double heightFor(int lanes) {
                return Band + 8 + RibbonHeight + (lanes > 0 ? 6 + lanes * Lane : 8);
            }
            static double nowHour() {
                return QTime::currentTime().msecsSinceStartOfDay() / 3600000.0;
            }
            static QString hourText(double hour) {
                return QLocale().toString(QTime::fromMSecsSinceStartOfDay(int(qBound(0.0, hour, 23.9999) * 3600000)), QLocale::ShortFormat);
            }
            static QString clockText(double hour) {
                return hourText(hour);
            }
            static QString cityName(const QString& zone) {
                return zone.section(QLatin1Char('/'), -1).replace(QLatin1Char('_'), QLatin1Char(' '));
            }
            static double offsetHours(const QTimeZone& tz) {
                const QDateTime now = QDateTime::currentDateTime();
                return (tz.offsetFromUtc(now) - now.offsetFromUtc()) / 3600.0;
            }
            static double pinHour(const Pin& pin, double hour) {
                return std::fmod(hour + offsetHours(pin.tz) + 48, 24);
            }
            static QString offsetText(const Pin& pin) {
                const double h = offsetHours(pin.tz);
                if (qFuzzyIsNull(h)) return tr("same time");
                return tr("%1 h").arg((h > 0 ? QStringLiteral("+") : QString()) + QLocale().toString(h, 'g', 3));
            }

            QFont clockFont(double px, double k) const {
                QFont font = flex(this->font(), px, 300 + 520 * k, 80 + 35 * k);
                font.setFeature(QFont::Tag("tnum"), 1);
                return font;
            }

            QRectF ribbonRect() const {
                return QRectF(0, Band + 8, this->width() - 64, RibbonHeight);
            }
            double xAt(double hour) const {
                const QRectF rib = ribbonRect();
                return rib.left() + rib.width() * qBound(0.0, hour, 24.0) / 24.0;
            }
            QRectF addRect() const {
                return QRectF(this->width() - 48, ribbonRect().center().y() - 24, 48, 48);
            }
            QRectF fieldRect() const {
                return QRectF(this->width() - 320, ribbonRect().center().y() - 28, 320, 56);
            }
            QRectF noticeRect(const Layout& l) const {
                const double w = QFontMetricsF(flex(this->font(), 12, 560)).horizontalAdvance(notice) + 36;
                return QRectF(l.dateLeft ? l.date.right() - w : l.date.left(), Band / 2 + 28, w, 24);
            }

            // The band over the ribbon: the clock centred on the now line and
            // the date beside it (on its left late in the day), the greeting on
            // the far side of the day so the clock never runs into it
            Layout layout(double hour) const {
                Layout l;
                const double w = this->width();
                const QSizeF hs = header ? QSizeF(header->sizeHint()) : QSizeF();
                const double hw = qMin(hs.width(), w * 0.34);
                const bool headerLeft = xAt(nowHour()) > w / 2;
                l.header = QRectF(headerLeft ? 0 : w - hw, (Band - hs.height()) / 2, hw, hs.height());
                const double freeLeft = headerLeft && header ? hw + 24 : 0, freeRight = !headerLeft && header ? w - hw - 24 : w;

                const QFont day = flex(this->font(), 24, 760, 110), date = flex(this->font(), 16, 500);
                const QDate today = QDate::currentDate();
                double dateW = qMax(QFontMetricsF(day).horizontalAdvance(QLocale().dayName(today.dayOfWeek())), QFontMetricsF(date).horizontalAdvance(QLocale().toString(today, QStringLiteral("d MMMM")))) + 4;
                if (!notice.isEmpty()) dateW = qMax(dateW, QFontMetricsF(flex(this->font(), 12, 560)).horizontalAdvance(notice) + 36);

                double clockW = QFontMetricsF(clockFont(ClockPx, 1)).horizontalAdvance(clockText(nowHour())) + 8;
                // Narrow panes get a smaller clock rather than a collision
                l.px = ClockPx * qBound(0.4, (freeRight - freeLeft - 20 - dateW) / clockW, 1.0);
                clockW *= l.px / ClockPx;

                const double nx = xAt(hour);
                l.dateLeft = nx + clockW / 2 + 20 + dateW > freeRight;
                const double lo = freeLeft + (l.dateLeft ? dateW + 20 : 0);
                const double hi = freeRight - clockW - (l.dateLeft ? 0 : 20 + dateW);
                const double left = qBound(lo, nx - clockW / 2, qMax(lo, hi));
                l.clock = QRectF(left, 0, clockW, Band);
                l.date = l.dateLeft ? QRectF(left - 20 - dateW, 0, dateW, Band) : QRectF(left + clockW + 20, 0, dateW, Band);
                return l;
            }

            void placeChildren() {
                if (header) header->setGeometry(layout(nowHour()).header.toAlignedRect());
                add->setGeometry(addRect().toAlignedRect());
                search->setGeometry(fieldRect().toAlignedRect());
                maskSearch();
            }

            void paintPin(QPainter* p, const M3::Scheme& s, const Pin& pin, QPointF c, double morph, double labelAlpha, double hour) {
                static const m3shapes::Morph shape(MaterialShapes::circle(), MaterialShapes::cookie7Sided());
                const double h = pinHour(pin, hour);
                const bool night = h < Dawn || h >= Sunset;
                p->save();
                p->translate(c);
                p->rotate(morph * 60);
                p->fillPath(m3shapes::toPath(shape, float(qBound(0.0, morph, 1.0)), QRectF(-17, -17, 34, 34)), night ? s.secondary : s.primary);
                p->restore();
                p->setFont(flex(this->font(), 14, 800));
                p->setPen(night ? s.onSecondary : s.onPrimary);
                p->drawText(QRectF(c.x() - 17, c.y() - 17, 34, 34), Qt::AlignCenter, pin.city.left(1));

                // Its name and time under the ribbon, in its own lane if it would cover another
                const QDateTime there = QDateTime::currentDateTime().toTimeZone(pin.tz);
                const qint64 days = QDate::currentDate().daysTo(there.date());
                QString time = hourText(h);
                if (days != 0) time += QStringLiteral(" %1%2").arg(days > 0 ? QStringLiteral("+") : QString()).arg(days);
                const double top = ribbonRect().bottom() + 6 + pin.lane * Lane;
                const QRectF label(qBound(0.0, c.x() - LabelWidth / 2, this->width() - LabelWidth), top, LabelWidth, Lane);
                p->save();
                p->setOpacity(p->opacity() * qBound(0.0, labelAlpha, 1.0));
                p->setFont(flex(this->font(), 13, 600));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(label.left(), label.top(), label.width(), 18), Qt::AlignCenter, p->fontMetrics().elidedText(pin.city, Qt::ElideRight, int(LabelWidth)));
                p->setFont(flex(this->font(), 20, 760, 110));
                p->setPen(s.onSurface);
                p->drawText(QRectF(label.left(), label.top() + 18, label.width(), 26), Qt::AlignCenter, time);
                p->restore();
            }

            int pinAt(QPointF pos) const {
                const QRectF rib = ribbonRect();
                for (int i = pins.size() - 1; i >= 0; i--) {
                    const double x = xAt(pinHour(pins.at(i), nowHour()));
                    const QRectF label(qBound(0.0, x - LabelWidth / 2, this->width() - LabelWidth), rib.bottom() + 6 + pins.at(i).lane * Lane, LabelWidth, Lane);
                    if (QLineF(pos, QPointF(x, rib.center().y())).length() < 20 || label.contains(pos)) return i;
                }
                return -1;
            }

            void reload() {
                pins.clear();
                for (const QString& zone : worldClocks.delimitedList(QStringLiteral("WorldClock/timezones"))) {
                    QTimeZone tz(zone.toUtf8());
                    const QString city = cityName(zone);
                    // Skip unnamed zones and your own (the clock already has that time)
                    if (!tz.isValid() || city.isEmpty() || tz == QTimeZone::systemTimeZone()) continue;
                    if (std::any_of(pins.cbegin(), pins.cend(), [&](const Pin& pin) { return pin.tz == tz; })) continue;
                    pins.append({zone.toUtf8(), city, tz});
                }
                layoutLanes();
                this->update();
            }

            void layoutLanes() {
                // Left to right, each label in the first lane where it clears the last one
                QList<int> order;
                for (int i = 0; i < pins.size(); i++) order.append(i);
                std::sort(order.begin(), order.end(), [this](int a, int b) {
                    return pinHour(pins.at(a), nowHour()) < pinHour(pins.at(b), nowHour());
                });
                QList<double> laneEnds;
                for (int i : order) {
                    const double x = xAt(pinHour(pins.at(i), nowHour()));
                    int lane = 0;
                    while (lane < laneEnds.size() && x - laneEnds.at(lane) < LabelWidth + 4) lane++;
                    if (lane == laneEnds.size()) laneEnds.append(x);
                    laneEnds[lane] = x;
                    pins[i].lane = lane;
                }
                if (laneEnds.size() != lanes) {
                    lanes = int(laneEnds.size());
                    this->setFixedHeight(int(heightFor(lanes)));
                }
            }

            void openSearch() {
                if (!cities) {
                    cities = new QStandardItemModel(completer);
                    for (const QByteArray& id : QTimeZone::availableTimeZoneIds()) {
                        const QString zone = QString::fromLatin1(id);
                        if (!zone.contains(QLatin1Char('/')) || zone.startsWith(QLatin1String("Etc/"))) continue;
                        auto* item = new QStandardItem();
                        item->setData(cityName(zone), CityRole);
                        item->setData(id, ZoneRole);
                        cities->appendRow(item);
                    }
                    completer->setModel(cities);
                }
                // Each suggestion: day or night there, the city, its country and its time
                const QDateTime now = QDateTime::currentDateTime();
                for (int i = 0; i < cities->rowCount(); i++) {
                    QStandardItem* item = cities->item(i);
                    const Pin pin{item->data(ZoneRole).toByteArray(), item->data(CityRole).toString(), QTimeZone(item->data(ZoneRole).toByteArray())};
                    const double h = pinHour(pin, nowHour());
                    const QString country = QLocale::territoryToString(pin.tz.territory());
                    item->setText(QStringLiteral("%1  ·  %2  ·  %3").arg(pin.city, country, QLocale().toString(now.toTimeZone(pin.tz).time(), QLocale::ShortFormat)));
                    item->setIcon(QIcon::fromTheme(h < Dawn || h >= Sunset ? QStringLiteral("weather-clear-night") : QStringLiteral("weather-clear")));
                }
                // The suggestions as an M3 menu
                const M3::Scheme s = M3::currentScheme();
                QAbstractItemView* popup = completer->popup();
                QPalette palette = popup->palette();
                palette.setColor(QPalette::Base, s.surfaceContainerHigh);
                palette.setColor(QPalette::Text, s.onSurface);
                palette.setColor(QPalette::Highlight, M3::layer(s.surfaceContainerHigh, s.onSurface, 0.12));
                palette.setColor(QPalette::HighlightedText, s.onSurface);
                popup->setPalette(palette);
                popup->setFont(flex(this->font(), 15, 500));
                popup->setIconSize(QSize(24, 24));
                search->clear();
                search->show();
                search->raise();
                add->hide();
                field->animateTo(1.0);
                search->setFocus(Qt::OtherFocusReason);
            }

            void closeSearch() {
                if (field->endValue().toDouble() < 0.5) return;
                completer->popup()->hide();
                field->animateTo(0.0);
            }

            void maskSearch() {
                // The search grows out of the add button
                const double k = qBound(0.0, field->currentValue().toDouble(), 1.1);
                const QRectF a = addRect(), f = fieldRect();
                const QRectF r(QPointF(a.left() + (f.left() - a.left()) * k, a.top() + (f.top() - a.top()) * k), QPointF(f.right(), a.bottom() + (f.bottom() - a.bottom()) * k));
                search->setMask(QRegion(rounded(r.translated(-f.topLeft()), r.height() / 2).toFillPolygon().toPolygon()));
            }

            void addCity(const QByteArray& zone) {
                if (zone.isEmpty()) return;
                QStringList zones = worldClocks.delimitedList(QStringLiteral("WorldClock/timezones"));
                if (!zones.contains(QString::fromLatin1(zone))) {
                    zones.append(QString::fromLatin1(zone));
                    worldClocks.setDelimitedList(QStringLiteral("WorldClock/timezones"), zones);
                }
                // It falls out of the search onto its hour, a circle turning into a cookie
                flying = zone;
                flyFrom = QPointF(fieldRect().left() + 24, fieldRect().bottom() + 30);
                reload();
                fly->snapTo(0.0);
                fly->animateTo(1.0);
                labels->snapTo(0.0);
                QTimer::singleShot(350, this, [this] {
                    labels->animateTo(1.0);
                });
                closeSearch();
            }
    };

    // Now playing: the cover turning in a big cookie, the controls, and the
    // M3 Expressive wavy seek bar (it waves while playing, flattens when
    // paused or while you drag it)
    class MediaCard : public Card {
        public:
            explicit MediaCard(QWidget* parent) :
                Card(176, parent) {
                amplitude = new M3SpringAnimation(this);
                amplitude->setToken(M3Motion::SpatialDefault);
                amplitude->snapTo(0.0);
                playShape = new M3SpringAnimation(this);
                playShape->setToken(M3Motion::SpatialFast);
                playShape->snapTo(0.0);
                for (auto* spring : {amplitude, playShape}) QObject::connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));

                frame.setInterval(33);
                QObject::connect(&frame, &QTimer::timeout, this, [this] {
                    double dt = clock.restart() / 1000.0;
                    if (!playing()) {
                        frame.stop(); // nothing moves until it plays (statusChanged starts it again)
                        return;
                    }
                    phase += dt * 0.9; // the wave flows back towards the start
                    spin += dt * 8;
                    positionMicro += qint64(dt * 1e6);
                    this->update();
                });
                poll.setInterval(1000);
                QObject::connect(&poll, &QTimer::timeout, this, [this] {
                    if (!player) return;
                    const qint64 position = player->position();
                    if (position == positionMicro) return;
                    positionMicro = position;
                    if (!frame.isActive()) this->update();
                });

                QObject::connect(MprisEngine::instance(), &MprisEngine::newPlayer, this, [this] {
                    choosePlayer();
                });
                QObject::connect(MprisEngine::instance(), &MprisEngine::playerGone, this, [this] {
                    QTimer::singleShot(0, this, [this] {
                        choosePlayer();
                    });
                });
                choosePlayer();
                this->setMouseTracking(true);
            }

        protected:
            QColor container(const M3::Scheme& s) const override {
                return s.primaryContainer;
            }

            void showEvent(QShowEvent*) override {
                clock.restart();
                frame.start();
                poll.start();
                if (player) positionMicro = player->position();
            }
            void hideEvent(QHideEvent*) override {
                frame.stop();
                poll.stop();
            }

            void paintContent(QPainter* p, const M3::Scheme& s) override {
                const QColor on = s.onPrimaryContainer;
                const QRectF artRect(20, (this->height() - 132) / 2.0, 132, 132);
                QPainterPath shape = m3shapes::toPath(MaterialShapes::cookie9Sided(), artRect);
                QTransform turn;
                turn.translate(artRect.center().x(), artRect.center().y());
                turn.rotate(spin);
                turn.translate(-artRect.center().x(), -artRect.center().y());
                shape = turn.map(shape);
                if (!art.isNull()) {
                    p->save();
                    p->setClipPath(shape);
                    // scaled once per cover, not on every frame of the turn
                    const QSize want = artRect.size().toSize() * this->devicePixelRatioF();
                    if (scaledArtKey != art.cacheKey() || scaledArtSize != want) {
                        scaledArt = art.scaled(want, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                        scaledArt.setDevicePixelRatio(this->devicePixelRatioF());
                        scaledArtKey = art.cacheKey();
                        scaledArtSize = want;
                    }
                    p->drawImage(artRect.topLeft(), scaledArt);
                    p->restore();
                } else {
                    p->fillPath(shape, s.primary);
                    p->setFont(flex(this->font(), 52, 800));
                    p->setPen(s.onPrimary);
                    p->drawText(artRect, Qt::AlignCenter, QStringLiteral("♪"));
                }

                const double x = TextLeft, w = this->width() - 24 - x;
                p->setFont(flex(this->font(), 22, 720, 105));
                p->setPen(on);
                p->drawText(QRectF(x, 16, w, 30), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(player ? title : tr("Nothing playing"), Qt::ElideRight, int(w)));
                p->setFont(flex(this->font(), 15, 460));
                p->setPen(alpha(on, 0.85));
                p->drawText(QRectF(x, 46, w, 20), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(player ? artist : tr("Play something"), Qt::ElideRight, int(w)));

                // Previous, play/pause (a rounded square while playing, a
                // circle when paused), next
                const bool enabled = !player.isNull();
                const QRectF prev = buttonRect(0), play = buttonRect(1), next = buttonRect(2);
                double shape01 = qBound(-0.2, playShape->currentValue().toDouble(), 1.2);
                double corner = play.height() / 2 + (play.height() * 0.3 - play.height() / 2) * shape01;
                p->fillPath(rounded(play, qMax(8.0, corner)), enabled ? s.primary : alpha(s.primary, 0.38));
                symbol(p, play.center(), playing() ? QStringLiteral("pause") : QStringLiteral("play_arrow"), s.onPrimary, 26);
                symbol(p, prev.center(), QStringLiteral("skip_previous"), alpha(on, enabled ? 1 : 0.38), 24);
                symbol(p, next.center(), QStringLiteral("skip_next"), alpha(on, enabled ? 1 : 0.38), 24);

                // The wave, and where it is
                const QRectF track = seekTrack();
                double progress = lengthMicro > 0 ? double(seeking ? seekMicro : positionMicro) / lengthMicro : 0;
                double amp = qBound(0.0, amplitude->currentValue().toDouble(), 1.3) * (seeking ? 0.0 : 1.0);
                paintWave(p, track, progress, amp, phase, on, alpha(on, 0.24));
                if (lengthMicro > 0) {
                    p->setFont(flex(this->font(), 12, 500));
                    p->setPen(alpha(on, 0.8));
                    p->drawText(QRectF(next.right(), play.top(), this->width() - 24 - next.right(), play.height()), Qt::AlignRight | Qt::AlignVCenter, mediaTime(seeking ? seekMicro : positionMicro) + QStringLiteral(" / ") + mediaTime(lengthMicro));
                }
            }

            void mousePressEvent(QMouseEvent* event) override {
                if (!player) return;
                const QPointF pos = event->position();
                if (seekTrack().adjusted(-4, -14, 4, 14).contains(pos) && player->canSeek() && lengthMicro > 0) {
                    seeking = true;
                    seekTo(pos);
                } else if (buttonRect(1).contains(pos)) {
                    player->playPause();
                } else if (buttonRect(0).contains(pos)) {
                    player->previous();
                } else if (buttonRect(2).contains(pos)) {
                    player->next();
                }
            }
            void mouseMoveEvent(QMouseEvent* event) override {
                if (seeking) seekTo(event->position());
            }
            void mouseReleaseEvent(QMouseEvent* event) override {
                if (!seeking) return;
                seekTo(event->position());
                seeking = false;
                positionMicro = seekMicro;
                if (player) player->setPosition(seekMicro);
                this->update();
            }

        private:
            MprisPlayerPtr player;
            QString title, artist, artUrl;
            QImage art, scaledArt;
            qint64 scaledArtKey = 0;
            QSize scaledArtSize;
            qint64 lengthMicro = 0, positionMicro = 0, seekMicro = 0;
            bool seeking = false;
            double phase = 0, spin = 0;
            M3SpringAnimation* amplitude;
            M3SpringAnimation* playShape;
            QTimer frame, poll;
            QElapsedTimer clock;

            bool playing() const {
                return player && player->playbackStatus() == MprisPlayerInterface::Playing;
            }

            static constexpr double TextLeft = 172;
            QRectF buttonRect(int index) const {
                // previous and next are icon buttons either side of the 56 dp play button
                const double top = 72;
                if (index == 1) return QRectF(TextLeft + 52, top, 56, 56);
                return QRectF(index == 0 ? TextLeft : TextLeft + 120, top + 8, 40, 40);
            }
            QRectF seekTrack() const {
                return QRectF(TextLeft, this->height() - 27, this->width() - 24 - TextLeft, 10);
            }

            void seekTo(QPointF pos) {
                QRectF t = seekTrack();
                double f = qBound(0.0, (pos.x() - t.left()) / t.width(), 1.0);
                seekMicro = qint64(f * lengthMicro);
                this->update();
            }

            void choosePlayer() {
                // Prefer something that's playing
                MprisPlayerPtr chosen;
                for (const MprisPlayerPtr& candidate : MprisEngine::players()) {
                    if (!chosen || candidate->playbackStatus() == MprisPlayerInterface::Playing) chosen = candidate;
                    if (candidate->playbackStatus() == MprisPlayerInterface::Playing) break;
                }
                if (chosen == player) return;
                if (player) QObject::disconnect(player.data(), nullptr, this, nullptr);
                player = chosen;
                if (player) {
                    QObject::connect(player.data(), &MprisPlayerInterface::metadataChanged, this, [this] {
                        readMetadata();
                    });
                    QObject::connect(player.data(), &MprisPlayerInterface::playbackStatusChanged, this, [this] {
                        statusChanged();
                    });
                    QObject::connect(player.data(), &MprisPlayerInterface::seeked, this, [this](qint64 position) {
                        positionMicro = position;
                        this->update();
                    });
                    positionMicro = player->position();
                }
                readMetadata();
                statusChanged();
            }

            void readMetadata() {
                title.clear();
                artist.clear();
                lengthMicro = 0;
                QString url;
                if (player) {
                    MetadataMap m = player->metadata();
                    title = m.value(QStringLiteral("xesam:title")).toString();
                    artist = m.value(QStringLiteral("xesam:artist")).toStringList().join(QStringLiteral(", "));
                    if (artist.isEmpty()) artist = player->identity();
                    lengthMicro = m.value(QStringLiteral("mpris:length")).toLongLong();
                    url = m.value(QStringLiteral("mpris:artUrl")).toString();
                    if (title.isEmpty()) title = player->identity();
                }
                if (url != artUrl) {
                    artUrl = url;
                    // Local art only (no network fetches from the shell)
                    QUrl u(url);
                    art = u.isLocalFile() ? QImage(u.toLocalFile()) : QImage();
                }
                this->update();
            }

            void statusChanged() {
                amplitude->animateTo(playing() ? 1.0 : 0.0);
                playShape->animateTo(playing() ? 1.0 : 0.0);
                if (player) positionMicro = player->position();
                if (playing() && this->isVisible() && !frame.isActive()) {
                    clock.restart();
                    frame.start();
                }
                this->update();
            }
    };

    // Live readings: CPU with a sparkline, temperature, network throughput
    class SystemCard : public Card {
        public:
            explicit SystemCard(QWidget* parent) :
                Card(176, parent) {}

            void push(double cpuFraction, double celsius, double down, double up) {
                cpu.append(cpuFraction);
                while (cpu.size() > 40) cpu.removeFirst();
                temperature = celsius;
                rx = down;
                tx = up;
                this->update();
            }

        protected:
            void paintContent(QPainter* p, const M3::Scheme& s) override {
                const QRectF r = QRectF(this->rect()).adjusted(20, 14, -20, -14);
                double now = cpu.isEmpty() ? 0 : cpu.last();
                p->setFont(flex(this->font(), 26, 800, 120));
                p->setPen(s.onSurface);
                p->drawText(QRectF(r.left(), r.top(), r.width(), 34), Qt::AlignLeft | Qt::AlignVCenter, tr("CPU %1%").arg(qRound(now * 100)));
                p->setFont(flex(this->font(), 13, 450));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(r.left(), r.top() + 34, r.width(), 18), Qt::AlignLeft | Qt::AlignVCenter, temperature > 0 ? tr("%1 °C").arg(qRound(temperature)) : QString());

                // Sparkline of the last 40 seconds
                QRectF spark(r.left(), r.top() + 56, r.width(), qMax(10.0, r.height() - 56 - 28));
                if (cpu.size() > 1) {
                    QPainterPath line;
                    for (int i = 0; i < cpu.size(); i++) {
                        QPointF pt(spark.left() + spark.width() * i / 39.0, spark.bottom() - spark.height() * qBound(0.0, cpu.at(i), 1.0));
                        if (i == 0) line.moveTo(pt);
                        else line.lineTo(pt);
                    }
                    p->setPen(QPen(s.tertiary, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    p->setBrush(Qt::NoBrush);
                    p->drawPath(line);
                }
                p->setFont(flex(this->font(), 14, 600));
                p->setPen(s.onSurface);
                p->drawText(QRectF(r.left(), r.bottom() - 20, r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("↓ %1   ↑ %2").arg(rate(rx), rate(tx)));
            }

        private:
            QList<double> cpu;
            double temperature = 0, rx = 0, tx = 0;
    };

    // ---- Readings -----------------------------------------------------------

    QDateTime bootTime() {
        for (const QString& line : readFile(QStringLiteral("/proc/stat")).split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1String("btime "))) return QDateTime::fromSecsSinceEpoch(line.mid(6).toLongLong());
        }
        return QDateTime::currentDateTime();
    }

    // Usage: the 5-hour and weekly limits, as Claude Code last measured them,
    // as two M3 Expressive wavy rings, one inside the other
    class ClaudeCard : public Card {
        public:
            ClaudeCard(QWidget* parent) :
                Card(176, parent) { // as tall as the media card beside it
                count = new M3SpringAnimation(this);
                count->setToken(M3Motion::SpatialSlow);
                count->snapTo(1.0);
                QObject::connect(count, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
                drift.setInterval(50);
                QObject::connect(&drift, &QTimer::timeout, this, [this] {
                    phase += 0.12; // the waves flow back towards the start
                    // only the rings (as paintContent places them)
                    const double outer = qMin(60.0, this->height() / 2.0 - 26), cx = 20 + outer + 6, cy = this->height() / 2.0;
                    this->update(QRectF(cx - outer - 12, cy - outer - 12, 2 * outer + 24, 2 * outer + 24).toAlignedRect());
                });
                QObject::connect(ClaudeUsage::instance(), &ClaudeUsage::changed, this, [this] {
                    this->setVisible(ClaudeUsage::instance()->available());
                    updateTip();
                    this->update();
                });
                this->setVisible(ClaudeUsage::instance()->available());
                updateTip();
            }

            void playEntrance(int delay) override {
                Card::playEntrance(delay);
                count->snapTo(0.0);
                QTimer::singleShot(delay + 60, this, [this] {
                    count->animateTo(1.0);
                });
            }

        protected:
            void showEvent(QShowEvent*) override {
                drift.start();
            }
            void hideEvent(QHideEvent*) override {
                drift.stop();
            }

            void paintContent(QPainter* p, const M3::Scheme& s) override {
                ClaudeUsage* usage = ClaudeUsage::instance();
                const double t = qBound(0.0, count->currentValue().toDouble(), 1.05);
                // Red from 90 %
                auto colour = [&](const ClaudeUsage::Limit& limit, QColor normal) {
                    return limit.percent >= 90 ? s.error : normal;
                };
                const double outer = qMin(60.0, this->height() / 2.0 - 26);
                const QPointF centre(20 + outer + 6, this->height() / 2.0);
                paintRing(p, centre, outer, 9, usage->week().percent / 100 * t, phase, colour(usage->week(), s.tertiary), s.secondaryContainer);
                paintRing(p, centre, outer - 19, 9, usage->session().percent / 100 * t, phase * 1.3, colour(usage->session(), s.primary), s.secondaryContainer);
                symbol(p, centre, QStringLiteral("data_usage"), s.onSurfaceVariant, 22);

                const double x = centre.x() + outer + 22, w = this->width() - 14 - x;
                p->setFont(flex(this->font(), 16, 640, 105));
                p->setPen(s.onSurface);
                p->drawText(QRectF(x, 14, w, 22), Qt::AlignLeft | Qt::AlignVCenter, tr("Usage"));
                // Each limit: what it is, the figure (it gains weight as it
                // counts up) and when it resets
                auto line = [&](double top, const QString& label, const ClaudeUsage::Limit& limit, QColor dot) {
                    p->setPen(Qt::NoPen);
                    p->setBrush(dot);
                    p->drawEllipse(QPointF(x + 4, top + 9), 4, 4);
                    p->setFont(flex(this->font(), 12, 600));
                    p->setPen(s.onSurface);
                    p->drawText(QRectF(x + 14, top, w - 14, 18), Qt::AlignLeft | Qt::AlignVCenter, label);
                    p->setFont(flex(this->font(), 26, 400 + 400 * qMin(1.0, t), 115));
                    p->setPen(limit.known ? s.onSurface : s.outline);
                    p->drawText(QRectF(x, top + 16, w, 30), Qt::AlignLeft | Qt::AlignVCenter, limit.known ? QLocale().toString(qRound(limit.percent * qMin(1.0, t))) + QStringLiteral("%") : QStringLiteral("–"));
                    p->setFont(flex(this->font(), 11, 450));
                    p->setPen(s.onSurfaceVariant);
                    p->drawText(QRectF(x, top + 46, w, 14), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(ClaudeUsage::resetText(limit), Qt::ElideRight, int(w)));
                };
                line(42, tr("Session"), usage->session(), colour(usage->session(), s.primary));
                line(106, tr("Week"), usage->week(), colour(usage->week(), s.tertiary));
            }

        private:
            M3SpringAnimation* count;
            QTimer drift;
            double phase = 0;

            void updateTip() {
                // Where it comes from and how fresh it is (no room on the card)
                const QString updated = ClaudeUsage::instance()->updatedText();
                const QString from = tr("From the theDesk Claude Code mod, while Claude Code runs");
                this->setToolTip(updated.isEmpty() ? from : updated + QStringLiteral("\n") + from);
            }
    };

    // When this process (theDesk, started at login) began
    QDateTime sessionStartTime() {
        QString stat = readFile(QStringLiteral("/proc/self/stat"));
        QStringList fields = stat.mid(stat.lastIndexOf(QLatin1Char(')')) + 2).split(QLatin1Char(' '));
        if (fields.size() > 19) {
            double ticks = fields.at(19).toDouble();
            return bootTime().addMSecs(qint64(ticks / sysconf(_SC_CLK_TCK) * 1000));
        }
        return QDateTime::currentDateTime();
    }
} // namespace

struct M3OverviewPagePrivate {
        QDateTime sessionStart = sessionStartTime();
        QDateTime boot = bootTime();
        M3UserCard* user;
        QLabel* handle;
        StatCard* session;
        StatCard* battery;
        StatCard* storage;
        StatCard* memory;
        RibbonCard* ribbon;
        MediaCard* media;
        SystemCard* system;
        ClaudeCard* claude;
        QList<Card*> cards;
        QTimer refresh;

        quint64 cpuIdle = 0, cpuTotal = 0;
        quint64 netRx = 0, netTx = 0;
        QElapsedTimer netClock;
};

M3OverviewPage::M3OverviewPage(QWidget* parent) :
    QWidget(parent) {
    d = new M3OverviewPagePrivate();
    // As wide as the pane allows, up to ColumnWidth
    this->setMinimumWidth(640);
    this->setMaximumWidth(ColumnWidth);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Everything on one screen: a four-column grid
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 12, 0, 24);
    grid->setSpacing(12);

    // Your day across the top, the greeting in its band
    d->ribbon = new RibbonCard(d->sessionStart, this);
    auto* header = new QWidget(d->ribbon);
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);
    d->user = new M3UserCard(header);
    d->user->setCardSize(M3UserCard::Compact);
    headerLayout->addWidget(d->user);
    d->handle = new QLabel(header);
    // README screenshots: a stand-in user on a stand-in machine
    if (qEnvironmentVariableIsSet("TD_DISPLAY_NAME")) d->handle->setText(QStringLiteral("@%1  ·  thedesk").arg(qEnvironmentVariable("TD_DISPLAY_NAME").toLower()));
    else d->handle->setText(QStringLiteral("@%1  ·  %2").arg(qEnvironmentVariable("USER"), QSysInfo::machineHostName()));
    d->handle->setFont(flex(this->font(), 13, 450));
    d->handle->setContentsMargins(4, 0, 0, 0);
    headerLayout->addWidget(d->handle);
    d->ribbon->header = header;
    grid->addWidget(d->ribbon, 0, 0, 1, 4);

    d->media = new MediaCard(this);
    d->system = new SystemCard(this);
    grid->addWidget(d->system, 1, 3);
    // Usage, while the Claude Code mod has written some: beside the media
    // card, which gives up a column for it
    d->claude = new ClaudeCard(this);
    auto place = [this, grid] {
        const bool claude = ClaudeUsage::instance()->available();
        grid->removeWidget(d->media);
        grid->removeWidget(d->claude);
        grid->addWidget(d->media, 1, 0, 1, claude ? 2 : 3);
        if (claude) grid->addWidget(d->claude, 1, 2);
    };
    connect(ClaudeUsage::instance(), &ClaudeUsage::changed, this, place);
    place();

    // The stats as pills, each symbol in its own shape; they leave the
    // corner to the Tools FAB
    d->session = new StatCard(StatCard::None, MaterialShapes::cookie4Sided(), QStringLiteral("schedule"), false, this);
    d->session->label = tr("Session");
    d->battery = new StatCard(StatCard::Wave, MaterialShapes::pill(), QStringLiteral("battery_full"), true, this);
    d->battery->label = tr("Battery");
    d->battery->holderTurn = 90;
    d->storage = new StatCard(StatCard::Linear, MaterialShapes::square(), QStringLiteral("hard_drive"), false, this);
    d->storage->label = tr("Free on /");
    d->memory = new StatCard(StatCard::Linear, MaterialShapes::gem(), QStringLiteral("memory"), true, this);
    d->memory->label = tr("Memory in use");
    auto* stats = new QHBoxLayout();
    stats->setContentsMargins(0, 0, 72, 0);
    stats->setSpacing(12);
    for (StatCard* card : {d->session, d->battery, d->storage, d->memory}) stats->addWidget(card, 1);
    grid->addLayout(stats, 2, 0, 1, 4);
    grid->setRowStretch(3, 1);
    for (int c = 0; c < 4; c++) grid->setColumnStretch(c, 1);

    d->cards = {d->ribbon, d->media, d->claude, d->system, d->session, d->battery, d->storage, d->memory};

    // theDesk's tools, a FAB menu away (they only had shortcuts): the Status
    // Center steps aside, then the tool starts as if its keys were pressed
    auto* tools = new M3FabMenu(this, QStringLiteral("construction"), tr("Tools"));
    auto tool = [](const QString& shortcut) {
        return [shortcut] {
            StateManager::statusCenterManager()->hide();
            QTimer::singleShot(350, [shortcut] { KeyGrab::trigger(shortcut); });
        };
    };
    tools->addItem(QStringLiteral("screenshot_region"), tr("Screenshot or record"), tool(QStringLiteral("screenshot")));
    tools->addItem(QStringLiteral("colorize"), tr("Pick a colour"), tool(QStringLiteral("colourPicker")));
    tools->addItem(QStringLiteral("mood"), tr("Emoji"), tool(QStringLiteral("emoji")));
    tools->addItem(QStringLiteral("content_paste"), tr("Clipboard"), tool(QStringLiteral("clipboard")));
    tools->addItem(QStringLiteral("chat"), tr("Ask Pebble"), tool(QStringLiteral("assistant")));

    d->refresh.setInterval(1000);
    connect(&d->refresh, &QTimer::timeout, this, &M3OverviewPage::refresh);
    refresh();
}

M3OverviewPage::~M3OverviewPage() {
    delete d;
}

void M3OverviewPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    QPalette pal = d->handle->palette();
    pal.setColor(QPalette::WindowText, M3::currentScheme().onSurfaceVariant);
    d->handle->setPalette(pal);
    refresh();
    d->refresh.start();
    // Cards rise in one after another
    for (int i = 0; i < d->cards.size(); i++) d->cards.at(i)->playEntrance(120 + i * 55);
}

void M3OverviewPage::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    d->refresh.stop();
}

void M3OverviewPage::setNotice(const QString& notice, const QString& details) {
    d->ribbon->notice = notice;
    d->ribbon->noticeDetails = details;
    d->ribbon->tick();
}

void M3OverviewPage::refresh() {
    const QDateTime now = QDateTime::currentDateTime();

    // Session
    qint64 seconds = d->sessionStart.secsTo(now);
    d->session->detail = tr("Up since %1").arg(QLocale().toString(d->boot.time(), QLocale::ShortFormat));
    d->session->setValue(seconds, [](double v) {
        return duration(qint64(v));
    });

    // Battery: all batteries together
    double energyNow = 0, energyFull = 0, power = 0;
    QString status;
    for (const QString& name : QDir(QStringLiteral("/sys/class/power_supply")).entryList({QStringLiteral("BAT*")}, QDir::Dirs | QDir::System)) {
        QString base = QStringLiteral("/sys/class/power_supply/") + name + QLatin1Char('/');
        QString prefix = QFile::exists(base + QStringLiteral("energy_now")) ? QStringLiteral("energy_") : QStringLiteral("charge_");
        energyNow += readFile(base + prefix + QStringLiteral("now")).toDouble();
        energyFull += readFile(base + prefix + QStringLiteral("full")).toDouble();
        power += readFile(base + (prefix == QStringLiteral("energy_") ? QStringLiteral("power_now") : QStringLiteral("current_now"))).toDouble();
        QString st = readFile(base + QStringLiteral("status"));
        if (status.isEmpty() || st == QStringLiteral("Discharging") || st == QStringLiteral("Charging")) status = st;
    }
    d->battery->setVisible(energyFull > 0);
    if (energyFull > 0) {
        double level = energyNow / energyFull;
        d->battery->level = level;
        const int bars = qRound(level * 6);
        d->battery->symbolName = status == QStringLiteral("Charging") ? QStringLiteral("battery_charging_full") : bars >= 6 ? QStringLiteral("battery_full") : QStringLiteral("battery_%1_bar").arg(bars);
        if (status == QStringLiteral("Discharging") && power > 0) {
            d->battery->detail = tr("On battery · about %1 left").arg(duration(qint64(energyNow / power * 3600)));
        } else if (status == QStringLiteral("Charging")) {
            d->battery->detail = tr("Charging");
        } else if (status == QStringLiteral("Full")) {
            d->battery->detail = tr("Full");
        } else {
            d->battery->detail = tr("Not charging");
        }
        d->battery->setValue(level * 100, [](double v) {
            return QStringLiteral("%1%").arg(qRound(v));
        });
    }

    // Storage
    QStorageInfo root = QStorageInfo::root();
    double total = root.bytesTotal(), free = root.bytesAvailable();
    d->storage->level = total > 0 ? 1 - free / total : 0;
    d->storage->detail = tr("of %1").arg(QLocale().formattedDataSize(qint64(total), 0, QLocale::DataSizeSIFormat));
    d->storage->setValue(free / 1e9, [](double v) {
        return QStringLiteral("%1 GB").arg(qRound(v));
    });

    // Memory and CPU
    double memTotal = 0, memAvailable = 0;
    for (const QString& line : readFile(QStringLiteral("/proc/meminfo")).split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String("MemTotal:"))) memTotal = line.section(QLatin1Char(' '), -2, -2).toDouble() * 1024;
        if (line.startsWith(QLatin1String("MemAvailable:"))) memAvailable = line.section(QLatin1Char(' '), -2, -2).toDouble() * 1024;
    }
    double used = memTotal - memAvailable;
    d->memory->level = memTotal > 0 ? used / memTotal : 0;
    d->memory->detail = tr("of %1").arg(QLocale().formattedDataSize(qint64(memTotal), 1, QLocale::DataSizeTraditionalFormat));
    d->memory->setValue(used / (1024.0 * 1024 * 1024), [](double v) {
        return QStringLiteral("%1 GB").arg(QLocale().toString(v, 'f', 1));
    });

    double cpuFraction = 0;
    QStringList cpu = readFile(QStringLiteral("/proc/stat")).section(QLatin1Char('\n'), 0, 0).simplified().split(QLatin1Char(' '));
    if (cpu.size() > 5) {
        quint64 totalTicks = 0;
        for (int i = 1; i < cpu.size(); i++) totalTicks += cpu.at(i).toULongLong();
        quint64 idle = cpu.at(4).toULongLong() + cpu.at(5).toULongLong();
        if (d->cpuTotal > 0 && totalTicks > d->cpuTotal) cpuFraction = 1 - double(idle - d->cpuIdle) / double(totalTicks - d->cpuTotal);
        d->cpuTotal = totalTicks;
        d->cpuIdle = idle;
    }

    // Package temperature, else the first zone
    double celsius = 0;
    for (const QString& zone : QDir(QStringLiteral("/sys/class/thermal")).entryList({QStringLiteral("thermal_zone*")}, QDir::Dirs | QDir::System)) {
        QString base = QStringLiteral("/sys/class/thermal/") + zone + QLatin1Char('/');
        double t = readFile(base + QStringLiteral("temp")).toDouble() / 1000;
        if (celsius == 0) celsius = t;
        if (readFile(base + QStringLiteral("type")) == QStringLiteral("x86_pkg_temp")) {
            celsius = t;
            break;
        }
    }

    // Network: everything but loopback
    quint64 rx = 0, tx = 0;
    for (const QString& line : readFile(QStringLiteral("/proc/net/dev")).split(QLatin1Char('\n')).mid(2)) {
        QString iface = line.section(QLatin1Char(':'), 0, 0).trimmed();
        if (iface.isEmpty() || iface == QStringLiteral("lo")) continue;
        QStringList v = line.section(QLatin1Char(':'), 1).simplified().split(QLatin1Char(' '));
        if (v.size() > 8) {
            rx += v.at(0).toULongLong();
            tx += v.at(8).toULongLong();
        }
    }
    double down = 0, up = 0;
    if (d->netClock.isValid() && d->netRx > 0) {
        double dt = qMax<qint64>(1, d->netClock.elapsed()) / 1000.0;
        down = (rx - d->netRx) / dt;
        up = (tx - d->netTx) / dt;
    }
    d->netClock.restart();
    d->netRx = rx;
    d->netTx = tx;
    d->system->push(cpuFraction, celsius, down, up);

    d->ribbon->tick();
}
