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
#include <QVBoxLayout>
#include <QtMath>
#include <functional>
#include <m3motion.h>
#include <m3scheme.h>
#include <claude/claudeusage.h>
#include <m3usercard.h>
#include <mpris/mprisengine.h>
#include <mpris/mprisplayer.h>
#include <shapes/materialshapes.h>
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
        font.setVariableAxis(QFont::Tag("wght"), float(qBound(100.0, wght, 1000.0)));
        font.setVariableAxis(QFont::Tag("wdth"), float(qBound(25.0, wdth, 151.0)));
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
                p.fillPath(rounded(QRectF(this->rect()), 28), container(s));
                paintContent(&p, s);
            }
    };

    // A big number that counts up, its label and a detail line, and an
    // optional level along the bottom
    class StatCard : public Card {
        public:
            enum Footer {
                None,
                Wave,
                Linear
            };

            StatCard(Footer footer, QWidget* parent) :
                Card(120, parent), footer(footer) {
                count = new M3SpringAnimation(this);
                count->setToken(M3Motion::SpatialSlow);
                count->snapTo(1.0);
                QObject::connect(count, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
                if (footer == Wave) {
                    drift.setInterval(50);
                    QObject::connect(&drift, &QTimer::timeout, this, [this] {
                        phase += 0.02; // the wave flows back towards the start
                        this->update();
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
                this->update();
            }

            QString label, detail;
            double level = 0;
            bool drifting = false;
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
                const QRectF r = QRectF(this->rect()).adjusted(20, 14, -20, -14);
                // The number gains weight as it counts up
                p->setFont(flex(this->font(), 30, 400 + 400 * qMin(1.0, t), 120));
                p->setPen(s.onSurface);
                p->drawText(QRectF(r.left(), r.top(), r.width(), 38), Qt::AlignLeft | Qt::AlignVCenter, format ? format(value * qMin(1.0, t)) : QString());
                p->setFont(flex(this->font(), 15, 450));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(r.left(), r.top() + 38, r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter, label);
                p->setFont(flex(this->font(), 12, 450));
                p->setPen(s.outline);
                p->drawText(QRectF(r.left(), r.top() + 58, r.width(), 16), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(detail, Qt::ElideRight, int(r.width())));
                QRectF track(r.left(), r.bottom() - 6, r.width(), 8);
                if (footer == Wave) paintWave(p, track, level * qMin(1.0, t), 1, phase, s.primary, s.secondaryContainer);
                if (footer == Linear) paintLinear(p, track, level * qMin(1.0, t), s.tertiary, s.secondaryContainer);
            }

        private:
            Footer footer;
            M3SpringAnimation* count;
            double value = 0;
            double phase = 0;
            std::function<QString(double)> format;
    };

    // Today: the day in expressive type, the date and world clocks, and this
    // week with today picked out
    class TodayCard : public Card {
        public:
            explicit TodayCard(QWidget* parent) :
                Card(120, parent), worldClocks(QStringLiteral("theSuite"), QStringLiteral("the24")) {
                pill = new M3SpringAnimation(this);
                pill->setToken(M3Motion::SpatialDefault);
                pill->snapTo(1.0);
                QObject::connect(pill, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
            }

            void playEntrance(int delay) override {
                Card::playEntrance(delay);
                pill->snapTo(0.0);
                QTimer::singleShot(delay + 160, this, [this] {
                    pill->animateTo(1.0);
                });
            }

        protected:
            void paintContent(QPainter* p, const M3::Scheme& s) override {
                const QDate today = QDate::currentDate();
                const double cell = 38;
                const QRectF r = QRectF(this->rect()).adjusted(22, 16, -18, -16);
                const double textWidth = r.width() - 7 * cell - 8;
                p->setFont(flex(this->font(), 28, 800, 120));
                p->setPen(s.onSurface);
                p->drawText(QRectF(r.left(), r.top(), textWidth, 40), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(QLocale().dayName(today.dayOfWeek()), Qt::ElideRight, int(textWidth)));

                QStringList line = {QLocale().toString(today, QStringLiteral("d MMMM"))};
                const QStringList zones = worldClocks.delimitedList(QStringLiteral("WorldClock/timezones")).mid(0, 2);
                for (const QString& zone : zones) {
                    QTimeZone tz(zone.toUtf8());
                    QString city = zone.section(QLatin1Char('/'), -1).replace(QLatin1Char('_'), QLatin1Char(' '));
                    // Skip unnamed zones and your own (the bar already has that time)
                    if (!tz.isValid() || city.isEmpty() || tz == QTimeZone::systemTimeZone()) continue;
                    line.append(QStringLiteral("%1 %2").arg(city, QLocale().toString(QDateTime::currentDateTime().toTimeZone(tz).time(), QLocale::ShortFormat)));
                }
                p->setFont(flex(this->font(), 15, 450));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(r.left(), r.top() + 42, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, line.value(0));
                p->setFont(flex(this->font(), 13, 450));
                p->drawText(QRectF(r.left(), r.top() + 64, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(line.mid(1).join(QStringLiteral("  ·  ")), Qt::ElideRight, int(textWidth)));

                // This week, today on a Primary pill that springs in
                const int first = QLocale().firstDayOfWeek();
                const QDate weekStart = today.addDays(-((today.dayOfWeek() - first + 7) % 7));
                for (int i = 0; i < 7; i++) {
                    QDate day = weekStart.addDays(i);
                    QRectF c(r.right() - (7 - i) * cell + 2, r.center().y() - 29, cell - 4, 58);
                    bool isToday = day == today;
                    if (isToday) {
                        double v = qBound(0.0, pill->currentValue().toDouble(), 1.3);
                        QRectF pr(c.center().x() - c.width() / 2 * v, c.center().y() - c.height() / 2 * v, c.width() * v, c.height() * v);
                        p->fillPath(rounded(pr, 20), s.primary);
                    }
                    p->setFont(flex(this->font(), 12, 500));
                    p->setPen(isToday ? s.onPrimary : s.onSurfaceVariant);
                    p->drawText(QRectF(c.left(), c.top() + 8, c.width(), 16), Qt::AlignCenter, QLocale().dayName(day.dayOfWeek(), QLocale::ShortFormat).left(1).toUpper());
                    p->setFont(flex(this->font(), 16, isToday ? 800 : 600));
                    p->setPen(isToday ? s.onPrimary : s.onSurface);
                    p->drawText(QRectF(c.left(), c.top() + 26, c.width(), 22), Qt::AlignCenter, QString::number(day.day()));
                }
            }

        private:
            tSettings worldClocks;
            M3SpringAnimation* pill;
    };

    // Your day on the living horizon's clock: night and day, the sun's rise
    // and set as MaterialShapes, this session, and a "now" marker
    class DayCard : public Card {
        public:
            DayCard(QDateTime sessionStart, QWidget* parent) :
                Card(116, parent), sessionStart(std::move(sessionStart)) {
                draw = new M3SpringAnimation(this);
                draw->setToken(M3Motion::SpatialSlow);
                draw->snapTo(1.0);
                QObject::connect(draw, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
            }

            void playEntrance(int delay) override {
                Card::playEntrance(delay);
                draw->snapTo(0.0);
                QTimer::singleShot(delay + 120, this, [this] {
                    draw->animateTo(1.0);
                });
            }

        protected:
            void paintContent(QPainter* p, const M3::Scheme& s) override {
                // The horizon's own schedule (m3/horizonclock.cpp): dawn at
                // 07:00, the golden hour ending in sunset at 18:30
                constexpr double dawn = 7.0, sunset = 18.5;
                const QDateTime now = QDateTime::currentDateTime();
                const double hour = now.time().msecsSinceStartOfDay() / 3600000.0;
                const QRectF r = QRectF(this->rect()).adjusted(24, 14, -24, -12);
                const QRectF line(r.left(), r.top() + 50, r.width(), 10);
                auto xAt = [&](double h) {
                    return line.left() + line.width() * qBound(0.0, h, 24.0) / 24.0;
                };
                const double reveal = qBound(0.0, draw->currentValue().toDouble(), 1.0);

                qint64 sessionSeconds = sessionStart.secsTo(now);
                QStringList summary = {tr("Dawn %1").arg(QStringLiteral("07:00")), tr("Sunset %1").arg(QStringLiteral("18:30")), tr("Session %1").arg(duration(sessionSeconds))};
                p->setFont(flex(this->font(), 14, 500));
                p->setPen(s.onSurfaceVariant);
                p->drawText(QRectF(r.left(), r.top(), r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter, summary.join(QStringLiteral("  ·  ")));

                // The line draws itself in from midnight
                p->save();
                p->setClipRect(QRectF(0, 0, line.left() + line.width() * reveal + 30, this->height()));
                p->fillPath(rounded(line, 5), s.surfaceContainerHighest);
                p->fillPath(rounded(QRectF(xAt(dawn), line.top(), xAt(sunset) - xAt(dawn), line.height()), 5), s.tertiaryContainer);
                // This session (it may have started yesterday)
                double from = sessionStart.date() < now.date() ? 0 : sessionStart.time().msecsSinceStartOfDay() / 3600000.0;
                p->fillPath(rounded(QRectF(xAt(from), line.top() - 14, qMax(6.0, xAt(hour) - xAt(from)), 6), 3), s.primary);
                // Sunrise and sunset sit on the line as MaterialShapes
                p->fillPath(m3shapes::toPath(MaterialShapes::sunny(), QRectF(xAt(dawn) - 11, line.center().y() - 11, 22, 22)), s.tertiary);
                p->fillPath(m3shapes::toPath(MaterialShapes::cookie9Sided(), QRectF(xAt(sunset) - 10, line.center().y() - 10, 20, 20)), s.tertiary);
                p->restore();

                // Now
                double nx = xAt(hour);
                p->setPen(QPen(s.onSurface, 2, Qt::SolidLine, Qt::RoundCap));
                p->drawLine(QPointF(nx, line.top() - 22), QPointF(nx, line.bottom() + 12));
                p->setFont(flex(this->font(), 12, 600));
                p->setPen(s.onSurface);
                bool labelLeft = nx > line.right() - 60;
                p->drawText(QRectF(labelLeft ? nx - 56 : nx + 6, line.top() - 28, 50, 16), (labelLeft ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, tr("now"));

                p->setFont(flex(this->font(), 12, 450));
                p->setPen(s.onSurfaceVariant);
                for (int h : {0, 6, 12, 18, 24}) p->drawText(QRectF(xAt(h) - 20, line.bottom() + 12, 40, 16), Qt::AlignCenter, QStringLiteral("%1").arg(h, 2, 10, QLatin1Char('0')));
            }

        private:
            QDateTime sessionStart;
            M3SpringAnimation* draw;
    };

    // Now playing: art in a turning MaterialShape, the controls, and the
    // M3 Expressive wavy seek bar (it waves while playing, flattens when
    // paused or while you drag it)
    class MediaCard : public Card {
        public:
            explicit MediaCard(QWidget* parent) :
                Card(150, parent) {
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
                    if (playing()) {
                        phase += dt * 0.9; // the wave flows back towards the start
                        spin += dt * 8;
                        positionMicro += qint64(dt * 1e6);
                    }
                    this->update();
                });
                poll.setInterval(1000);
                QObject::connect(&poll, &QTimer::timeout, this, [this] {
                    if (player) positionMicro = player->position();
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
                const QRectF artRect(20, 20, 72, 72);
                QPainterPath shape = m3shapes::toPath(MaterialShapes::cookie9Sided(), artRect);
                QTransform turn;
                turn.translate(artRect.center().x(), artRect.center().y());
                turn.rotate(spin);
                turn.translate(-artRect.center().x(), -artRect.center().y());
                shape = turn.map(shape);
                if (!art.isNull()) {
                    p->save();
                    p->setClipPath(shape);
                    QImage scaled = art.scaled(artRect.size().toSize() * this->devicePixelRatioF(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                    scaled.setDevicePixelRatio(this->devicePixelRatioF());
                    p->drawImage(artRect.topLeft(), scaled);
                    p->restore();
                } else {
                    p->fillPath(shape, s.primary);
                    p->setFont(flex(this->font(), 30, 800));
                    p->setPen(s.onPrimary);
                    p->drawText(artRect, Qt::AlignCenter, QStringLiteral("♪"));
                }

                const QRectF controls = controlsRect();
                const QRectF text(108, 22, controls.left() - 108 - 8, 50);
                p->setFont(flex(this->font(), 18, 700));
                p->setPen(on);
                p->drawText(QRectF(text.left(), text.top(), text.width(), 26), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(player ? title : tr("Nothing playing"), Qt::ElideRight, int(text.width())));
                p->setFont(flex(this->font(), 14, 450));
                p->setPen(alpha(on, 0.8));
                p->drawText(QRectF(text.left(), text.top() + 26, text.width(), 22), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(player ? artist : tr("Play something"), Qt::ElideRight, int(text.width())));

                // Previous, play/pause (a rounded square while playing, a
                // circle when paused), next
                const bool enabled = !player.isNull();
                auto glyph = [&](QRectF b, const char* icon, QColor colour) {
                    QPixmap pm = QIcon::fromTheme(QLatin1String(icon)).pixmap(QSize(20, 20), this->devicePixelRatioF());
                    QPainter tint(&pm);
                    tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    tint.fillRect(pm.rect(), colour);
                    tint.end();
                    p->drawPixmap(QPointF(b.center().x() - 10, b.center().y() - 10), pm);
                };
                QRectF prev = buttonRect(0), play = buttonRect(1), next = buttonRect(2);
                double shape01 = qBound(-0.2, playShape->currentValue().toDouble(), 1.2);
                double corner = play.height() / 2 + (14 - play.height() / 2) * shape01;
                p->fillPath(rounded(play, qMax(6.0, corner)), enabled ? s.primary : alpha(s.primary, 0.38));
                glyph(play, playing() ? "media-playback-pause" : "media-playback-start", s.onPrimary);
                glyph(prev, "media-skip-backward", alpha(on, enabled ? 1 : 0.38));
                glyph(next, "media-skip-forward", alpha(on, enabled ? 1 : 0.38));

                // The wave
                const QRectF track = seekTrack();
                double progress = lengthMicro > 0 ? double(seeking ? seekMicro : positionMicro) / lengthMicro : 0;
                double amp = qBound(0.0, amplitude->currentValue().toDouble(), 1.3) * (seeking ? 0.0 : 1.0);
                paintWave(p, track, progress, amp, phase, on, alpha(on, 0.24));
                if (lengthMicro > 0) {
                    p->setFont(flex(this->font(), 12, 500));
                    p->setPen(alpha(on, 0.8));
                    p->drawText(QRectF(track.left(), track.bottom() + 4, 80, 16), Qt::AlignLeft | Qt::AlignVCenter, mediaTime(seeking ? seekMicro : positionMicro));
                    p->drawText(QRectF(track.right() - 80, track.bottom() + 4, 80, 16), Qt::AlignRight | Qt::AlignVCenter, mediaTime(lengthMicro));
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
            QImage art;
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

            QRectF controlsRect() const {
                return QRectF(this->width() - 20 - 3 * 48, 26, 3 * 48, 48);
            }
            QRectF buttonRect(int index) const {
                QRectF c = controlsRect();
                return QRectF(c.left() + index * 48 + 2, c.top() + 2, 44, 44);
            }
            QRectF seekTrack() const {
                return QRectF(24, 104, this->width() - 48, 10);
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
                this->update();
            }
    };

    // Live readings: CPU with a sparkline, temperature, network throughput
    class SystemCard : public Card {
        public:
            explicit SystemCard(QWidget* parent) :
                Card(150, parent) {}

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
                QRectF spark(r.left(), r.top() + 56, r.width(), 30);
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

    // Claude: the 5-hour and weekly limits, as Claude Code last measured them
    class ClaudeCard : public Card {
        public:
            ClaudeCard(QWidget* parent) :
                Card(150, parent) { // as tall as the media card beside it
                count = new M3SpringAnimation(this);
                count->setToken(M3Motion::SpatialSlow);
                count->snapTo(1.0);
                QObject::connect(count, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
                drift.setInterval(50);
                QObject::connect(&drift, &QTimer::timeout, this, [this] {
                    phase += 0.02; // the waves flow back towards the start
                    this->update();
                });
                QObject::connect(ClaudeUsage::instance(), &ClaudeUsage::changed, this, [this] {
                    this->setVisible(ClaudeUsage::instance()->available());
                    this->update();
                });
                this->setVisible(ClaudeUsage::instance()->available());
                this->setToolTip(tr("From the theDesk Claude Code mod, while Claude Code runs"));
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
                const double t = qBound(0.0, count->currentValue().toDouble(), 1.2);
                const QRectF r = QRectF(this->rect()).adjusted(18, 16, -18, -14);

                // The logo in a soft container, the name, how fresh it is
                const QColor claude(0xD9, 0x77, 0x57);
                p->setPen(Qt::NoPen);
                p->setBrush(M3::layer(container(s), claude, 0.18));
                p->drawEllipse(QRectF(r.left(), r.top(), 32, 32));
                ClaudeUsage::paintLogo(p, QRectF(r.left() + 8, r.top() + 8, 16, 16));
                p->setFont(flex(this->font(), 16, 640, 105));
                p->setPen(s.onSurface);
                p->drawText(QRectF(r.left() + 42, r.top() - 2, r.width() - 42, 20), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Claude"));
                p->setFont(flex(this->font(), 11, 450));
                p->setPen(s.outline);
                p->drawText(QRectF(r.left() + 42, r.top() + 17, r.width() - 42, 14), Qt::AlignLeft | Qt::AlignVCenter, p->fontMetrics().elidedText(usage->updatedText(), Qt::ElideRight, int(r.width() - 42)));

                // This session, then this week: the figure, what it is and
                // when it resets, a wavy level under them
                auto row = [&](double top, const QString& label, const ClaudeUsage::Limit& limit, QColor colour) {
                    if (limit.percent >= 90) colour = s.error;
                    const double shown = limit.percent * qMin(1.0, t);
                    // The figure gains weight as it counts up
                    p->setFont(flex(this->font(), 22, 400 + 360 * qMin(1.0, t), 115));
                    p->setPen(limit.known ? s.onSurface : s.outline);
                    p->drawText(QRectF(r.left(), top, 64, 26), Qt::AlignLeft | Qt::AlignVCenter, limit.known ? QLocale().toString(qRound(shown)) + QStringLiteral("%") : QStringLiteral("–"));
                    p->setFont(flex(this->font(), 12, 560));
                    p->setPen(s.onSurface);
                    p->drawText(QRectF(r.left() + 64, top, r.width() - 64, 13), Qt::AlignRight | Qt::AlignVCenter, label);
                    p->setFont(flex(this->font(), 11, 450));
                    p->setPen(s.onSurfaceVariant);
                    p->drawText(QRectF(r.left() + 64, top + 13, r.width() - 64, 13), Qt::AlignRight | Qt::AlignVCenter, p->fontMetrics().elidedText(ClaudeUsage::resetText(limit), Qt::ElideLeft, int(r.width() - 64)));
                    paintWave(p, QRectF(r.left(), top + 30, r.width(), 8), qMin(1.0, shown / 100), 1, phase, colour, s.secondaryContainer);
                };
                row(r.top() + 40, tr("This session"), usage->session(), s.primary);
                row(r.top() + 82, tr("This week"), usage->week(), s.tertiary);
            }

        private:
            M3SpringAnimation* count;
            QTimer drift;
            double phase = 0;
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
        TodayCard* today;
        DayCard* day;
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
    grid->setContentsMargins(0, 24, 0, 24);
    grid->setSpacing(12);

    // The profile header, beside today
    auto* header = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(8, 0, 0, 0);
    headerLayout->setSpacing(2);
    d->user = new M3UserCard(header);
    d->user->setCardSize(M3UserCard::Large);
    headerLayout->addWidget(d->user);
    d->handle = new QLabel(header);
    // README screenshots: a stand-in user on a stand-in machine
    if (qEnvironmentVariableIsSet("TD_DISPLAY_NAME")) d->handle->setText(QStringLiteral("@%1  ·  thedesk").arg(qEnvironmentVariable("TD_DISPLAY_NAME").toLower()));
    else d->handle->setText(QStringLiteral("@%1  ·  %2").arg(qEnvironmentVariable("USER"), QSysInfo::machineHostName()));
    d->handle->setFont(flex(this->font(), 14, 450));
    d->handle->setContentsMargins(4, 0, 0, 0);
    headerLayout->addWidget(d->handle);
    headerLayout->addStretch();
    grid->addWidget(header, 0, 0, 1, 2);
    d->today = new TodayCard(this);
    grid->addWidget(d->today, 0, 2, 1, 2, Qt::AlignVCenter);

    d->session = new StatCard(StatCard::None, this);
    d->session->label = tr("Session");
    d->battery = new StatCard(StatCard::Wave, this);
    d->battery->label = tr("Battery");
    d->storage = new StatCard(StatCard::Linear, this);
    d->storage->label = tr("Free on /");
    d->memory = new StatCard(StatCard::Linear, this);
    d->memory->label = tr("Memory in use");
    grid->addWidget(d->session, 1, 0);
    grid->addWidget(d->battery, 1, 1);
    grid->addWidget(d->storage, 1, 2);
    grid->addWidget(d->memory, 1, 3);

    d->day = new DayCard(d->sessionStart, this);
    grid->addWidget(d->day, 2, 0, 1, 4);

    d->media = new MediaCard(this);
    d->system = new SystemCard(this);
    grid->addWidget(d->system, 3, 3);
    // Claude's limits, while the Claude Code mod has written some: beside
    // the media card, which gives up a column for it
    d->claude = new ClaudeCard(this);
    auto place = [this, grid] {
        const bool claude = ClaudeUsage::instance()->available();
        grid->removeWidget(d->media);
        grid->removeWidget(d->claude);
        grid->addWidget(d->media, 3, 0, 1, claude ? 2 : 3);
        if (claude) grid->addWidget(d->claude, 3, 2);
    };
    connect(ClaudeUsage::instance(), &ClaudeUsage::changed, this, place);
    place();
    grid->setRowStretch(4, 1);
    for (int c = 0; c < 4; c++) grid->setColumnStretch(c, 1);

    d->cards = {d->today, d->session, d->battery, d->storage, d->memory, d->day, d->media, d->system, d->claude};

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

    d->today->update();
    d->day->update();
}
