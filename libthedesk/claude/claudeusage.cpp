/****************************************
 *
 *   theDesk - Claude's usage limits
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "claude/claudeusage.h"

#include <QDir>
#include <QFile>
#include <QApplication>
#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSet>
#include <QSvgRenderer>
#include <QTimer>
#include <cmath>
#include "hudmanager.h"
#include "statemanager.h"

struct ClaudeUsagePrivate {
        QString path;
        QFileSystemWatcher* watcher;
        bool available = false;
        ClaudeUsage::Limit session, week;
        QDateTime updated;
        bool firstRead = true;

        // The open sessions, as the mod last wrote them
        struct Session {
                ClaudeUsage::Activity activity = ClaudeUsage::Idle;
                QString project, ask;
                QDateTime finished;
        };
        QString dir;
        QHash<QString, Session> sessions;
        QSet<QString> told; // waiting, and said so
        bool firstSessionsRead = true;
        bool announces = false;
        QDateTime finished;
        QTimer* stale;
};

namespace {
    ClaudeUsage::Limit limitFrom(const QJsonValue& value) {
        ClaudeUsage::Limit limit;
        if (!value.isObject()) return limit;
        const QJsonObject object = value.toObject();
        limit.known = true;
        limit.percent = object.value(QStringLiteral("percent")).toDouble();
        limit.resetsAt = QDateTime::fromString(object.value(QStringLiteral("resetsAt")).toString(), Qt::ISODateWithMs);
        if (!limit.resetsAt.isValid()) limit.resetsAt = QDateTime::fromString(object.value(QStringLiteral("resetsAt")).toString(), Qt::ISODate);
        return limit;
    }

    // A session whose heartbeat (every 20 s) stopped was killed, or the mod
    // was unloaded under it: it no longer counts, and a day on its file goes
    constexpr int GoneAfter = 60;
    constexpr int ForgottenAfter = 24 * 60 * 60;

    QString wants(const QString& tool) {
        if (tool == QLatin1String("Bash")) return ClaudeUsage::tr("wants to run a command");
        if (tool == QLatin1String("Edit") || tool == QLatin1String("Write") || tool == QLatin1String("NotebookEdit")) return ClaudeUsage::tr("wants to change a file");
        if (tool == QLatin1String("WebFetch") || tool == QLatin1String("WebSearch")) return ClaudeUsage::tr("wants to use the web");
        if (tool == QLatin1String("AskUserQuestion")) return ClaudeUsage::tr("has a question");
        if (tool == QLatin1String("ExitPlanMode")) return ClaudeUsage::tr("has a plan for you");
        return ClaudeUsage::tr("asks for a permission");
    }

    // Claude's snackbars wear its logo, the shape behind it tinted its colour
    QVariantMap snackbar(const QString& title, const QString& text) {
        const double ratio = qApp->devicePixelRatio();
        QPixmap logo(QSize(20, 20) * ratio);
        logo.setDevicePixelRatio(ratio);
        logo.fill(Qt::transparent);
        QPainter painter(&logo);
        ClaudeUsage::paintLogo(&painter, QRectF(0, 0, 20, 20));
        painter.end();
        return {
            {QStringLiteral("pixmap"), logo                    },
            {QStringLiteral("accent"), QColor(0xD9, 0x77, 0x57)},
            {QStringLiteral("title"),  title                   },
            {QStringLiteral("text"),   text                    }
        };
    }

    // Past its reset the window starts again from nothing
    ClaudeUsage::Limit current(ClaudeUsage::Limit limit) {
        if (limit.known && limit.resetsAt.isValid() && limit.resetsAt <= QDateTime::currentDateTime()) limit.percent = 0;
        return limit;
    }
} // namespace

ClaudeUsage* ClaudeUsage::instance() {
    static ClaudeUsage* instance = new ClaudeUsage();
    return instance;
}

ClaudeUsage::ClaudeUsage() :
    QObject(nullptr) {
    d = new ClaudeUsagePrivate();
    const QString dir = QDir::homePath() + QStringLiteral("/.cache/thedesk");
    QDir().mkpath(dir);
    d->dir = dir;
    d->path = dir + QStringLiteral("/claude-usage.json");
    d->watcher = new QFileSystemWatcher(this);
    d->watcher->addPath(dir);
    // The mod may replace the file: watch the folder, and the file again
    auto again = [this] {
        if (QFile::exists(d->path) && !d->watcher->files().contains(d->path)) d->watcher->addPath(d->path);
        read();
        readSessions();
    };
    connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, again);
    connect(d->watcher, &QFileSystemWatcher::fileChanged, this, again);
    // A heartbeat that stops changes no file: look again while a session counts
    d->stale = new QTimer(this);
    d->stale->setInterval(20000);
    connect(d->stale, &QTimer::timeout, this, &ClaudeUsage::readSessions);
    again();

    // "Updated … ago" and resets that pass
    auto* minute = new QTimer(this);
    minute->setInterval(60000);
    connect(minute, &QTimer::timeout, this, &ClaudeUsage::changed);
    minute->start();
}

void ClaudeUsage::read() {
    QFile file(d->path);
    if (!file.open(QFile::ReadOnly)) {
        if (d->available) {
            d->available = false;
            emit changed();
        }
        return;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    if (object.isEmpty()) return; // half-written: the next change has it
    const Limit oldSession = d->session, oldWeek = d->week;
    d->session = limitFrom(object.value(QStringLiteral("fiveHour")));
    d->week = limitFrom(object.value(QStringLiteral("sevenDay")));
    d->updated = QDateTime::fromString(object.value(QStringLiteral("updated")).toString(), Qt::ISODateWithMs);
    d->available = d->session.known || d->week.known;

    // Crossing 80, 90 or 100%: say so once
    if (!d->firstRead && d->announces) {
        auto warn = [](const Limit& before, const Limit& now, const QString& what) {
            for (int line : {100, 90, 80}) {
                if (before.percent < line && now.percent >= line) {
                    const QString text = line >= 100 ? tr("Claude: %1 used up").arg(what) : tr("Claude: %1% of %2 used").arg(line).arg(what);
                    const QString reset = resetText(now);
                    StateManager::hudManager()->showHud(snackbar(text, reset));
                    return;
                }
            }
        };
        if (d->session.known) warn(oldSession, d->session, tr("this session"));
        if (d->week.known) warn(oldWeek, d->week, tr("this week"));
    }
    d->firstRead = false;
    emit changed();
}

void ClaudeUsage::readSessions() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const Activity before = activity();
    const QDateTime finishedBefore = finished();
    QHash<QString, ClaudeUsagePrivate::Session> sessions;
    const QFileInfoList files = QDir(d->dir).entryInfoList({QStringLiteral("claude-session-*.json")}, QDir::Files);
    for (const QFileInfo& info : files) {
        QFile file(info.filePath());
        if (!file.open(QFile::ReadOnly)) continue;
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        const QString id = object.value(QStringLiteral("id")).toString();
        if (object.isEmpty() || id.isEmpty()) {
            // Half-written: as it was, until the next change
            if (info.lastModified().secsTo(QDateTime::currentDateTime()) > ForgottenAfter) QFile::remove(info.filePath());
            for (auto i = d->sessions.constBegin(); i != d->sessions.constEnd(); ++i) {
                if (info.fileName().contains(i.key())) sessions.insert(i.key(), i.value());
            }
            continue;
        }
        const QString state = object.value(QStringLiteral("state")).toString();
        const qint64 quiet = QDateTime::fromString(object.value(QStringLiteral("seen")).toString(), Qt::ISODateWithMs).secsTo(now);
        if (state == QLatin1String("ended") || quiet > ForgottenAfter) {
            QFile::remove(info.filePath());
            continue;
        }
        if (quiet > GoneAfter) continue;
        // The folder only says a file came or went: what's written into one
        // is its own to say
        if (!d->watcher->files().contains(info.filePath())) d->watcher->addPath(info.filePath());
        ClaudeUsagePrivate::Session session;
        session.project = object.value(QStringLiteral("project")).toString();
        session.ask = object.value(QStringLiteral("ask")).toString();
        session.activity = state == QLatin1String("waiting") ? Waiting : state == QLatin1String("working") ? Working : Idle;
        if (state == QLatin1String("done")) session.finished = QDateTime::fromString(object.value(QStringLiteral("since")).toString(), Qt::ISODateWithMs);
        sessions.insert(id, session);
    }
    d->sessions = sessions;

    // A session that starts waiting on the person: say so once
    for (auto i = sessions.constBegin(); i != sessions.constEnd(); ++i) {
        if (i->activity != Waiting) {
            d->told.remove(i.key());
        } else if (!d->told.contains(i.key())) {
            d->told.insert(i.key());
            if (!d->firstSessionsRead && d->announces) {
                // "thedesk · wants to run a command"
                const QString what = wants(i->ask);
                StateManager::hudManager()->showHud(snackbar(tr("Claude needs you"), i->project.isEmpty() ? what : QStringLiteral("%1 · %2").arg(i->project, what)));
            }
        }
    }
    d->firstSessionsRead = false;

    if (sessions.isEmpty()) {
        d->stale->stop();
    } else if (!d->stale->isActive()) {
        d->stale->start();
    }
    if (activity() != before || finished() != finishedBefore) emit changed();
}

ClaudeUsage::Activity ClaudeUsage::activity() const {
    Activity activity = Idle;
    for (const ClaudeUsagePrivate::Session& session : std::as_const(d->sessions)) activity = qMax(activity, session.activity);
    return activity;
}

QString ClaudeUsage::waitingText() const {
    for (const ClaudeUsagePrivate::Session& session : std::as_const(d->sessions)) {
        if (session.activity != Waiting) continue;
        return session.project.isEmpty() ? wants(session.ask) : QStringLiteral("%1 %2").arg(session.project, wants(session.ask));
    }
    return QString();
}

QDateTime ClaudeUsage::finished() const {
    QDateTime finished;
    for (const ClaudeUsagePrivate::Session& session : std::as_const(d->sessions)) {
        if (session.finished.isValid() && (!finished.isValid() || session.finished > finished)) finished = session.finished;
    }
    return finished;
}

void ClaudeUsage::setAnnounces(bool announces) {
    d->announces = announces;
}

bool ClaudeUsage::available() const {
    return d->available;
}

ClaudeUsage::Limit ClaudeUsage::session() const {
    return current(d->session);
}

ClaudeUsage::Limit ClaudeUsage::week() const {
    return current(d->week);
}

QDateTime ClaudeUsage::updated() const {
    return d->updated;
}

QString ClaudeUsage::resetText(const Limit& limit) {
    if (!limit.resetsAt.isValid()) return QString();
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime at = limit.resetsAt.toLocalTime();
    if (at <= now) return tr("Reset at %1").arg(QLocale().toString(at.time(), QLocale::ShortFormat));
    const qint64 minutes = (now.secsTo(at) + 59) / 60;
    if (minutes < 60) return tr("Resets in %n min", nullptr, int(minutes));
    if (minutes < 6 * 60) return tr("Resets in %1 h %2 min").arg(minutes / 60).arg(minutes % 60);
    if (at.date() == now.date()) return tr("Resets today at %1").arg(QLocale().toString(at.time(), QLocale::ShortFormat));
    if (at.date() == now.date().addDays(1)) return tr("Resets tomorrow at %1").arg(QLocale().toString(at.time(), QLocale::ShortFormat));
    return tr("Resets %1 at %2").arg(QLocale().dayName(at.date().dayOfWeek()), QLocale().toString(at.time(), QLocale::ShortFormat));
}

QString ClaudeUsage::updatedText() const {
    if (!d->updated.isValid()) return QString();
    const qint64 minutes = d->updated.secsTo(QDateTime::currentDateTime()) / 60;
    if (minutes < 1) return tr("Updated just now");
    if (minutes < 60) return tr("Updated %n min ago", nullptr, int(minutes));
    if (minutes < 24 * 60) return tr("Updated %n h ago", nullptr, int(minutes / 60));
    return tr("Updated %n days ago", nullptr, int(minutes / (24 * 60)));
}

void ClaudeUsage::paintLogo(QPainter* painter, const QRectF& rect, double phase) {
    static QSvgRenderer logo(QStringLiteral(":/libthedesk/claude.svg"));
    if (phase < 0) {
        logo.render(painter, rect);
        return;
    }
    // One beat: the rays grow out of the middle, some ahead of others (the
    // lead turns around the logo), reach their whole length and draw back in.
    // The logo is one path, so its rays are shown through a shape that does it
    const double grown = 0.5 - 0.5 * std::cos(2 * M_PI * phase);
    const QPointF centre = rect.center();
    const double whole = rect.width() / 24 * 13.5;
    QPainterPath shown;
    for (int i = 0; i < 72; i++) {
        const double angle = 2 * M_PI * i / 72;
        const double lead = 0.5 + 0.5 * std::sin(5 * angle - 2 * M_PI * phase);
        const double radius = whole * (0.30 + 0.70 * qMin(1.0, grown * (1 + 0.9 * lead)));
        const QPointF point = centre + QPointF(radius * std::cos(angle), radius * std::sin(angle));
        if (i == 0) {
            shown.moveTo(point);
        } else {
            shown.lineTo(point);
        }
    }
    shown.closeSubpath();
    painter->save();
    painter->setClipPath(shown, Qt::IntersectClip);
    logo.render(painter, rect);
    painter->restore();
}
