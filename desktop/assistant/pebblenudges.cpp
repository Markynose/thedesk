#include "pebblenudges.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QLocale>
#include <cmath>
#include <QStorageInfo>
#include <QTimer>
#include <Wm/desktopwm.h>
#include <hudmanager.h>
#include <powermanager.h>
#include <statemanager.h>
#include <tsettings.h>

namespace {
    QString tr(const char* text) {
        return QCoreApplication::translate("PebbleNudges", text);
    }

    constexpr qint64 Again = 3 * 24 * 3600; // a kind of nudge comes back after three days at the soonest

    bool due(const QString& kind) {
        tSettings settings;
        const qint64 last = settings.value(QStringLiteral("Assistant/nudged.%1").arg(kind)).toLongLong();
        return QDateTime::currentSecsSinceEpoch() - last > Again;
    }

    void nudge(const QString& kind, const QString& icon, const QString& title, const QString& text, const QString& action, const QString& prompt) {
        tSettings settings;
        settings.setValue(QStringLiteral("Assistant/nudged.%1").arg(kind), QDateTime::currentSecsSinceEpoch());
        StateManager::hudManager()->showHud({
            {QStringLiteral("icon"), icon},
            {QStringLiteral("title"), title},
            {QStringLiteral("text"), text},
            {QStringLiteral("action"), action},
            {QStringLiteral("actionId"), PebbleNudges::askAction(prompt)}
        });
    }

    // One check; at most one nudge
    void look() {
        if (!tSettings().value(QStringLiteral("Assistant/nudges")).toBool()) return;

        const QStorageInfo root = QStorageInfo::root();
        if (due(QStringLiteral("disk")) && root.bytesTotal() > 0 && double(root.bytesAvailable()) / root.bytesTotal() < 0.10) {
            nudge(QStringLiteral("disk"), QStringLiteral("drive-harddisk"), tr("Your disk is nearly full"), tr("%1 left. Pebble can find what's taking the space").arg(QLocale().formattedDataSize(root.bytesAvailable())),
                tr("Find it"), tr("My disk is nearly full. Find what's taking the most space in my home folder and tell me what I could clear. Don't remove anything yet."));
            return;
        }

        if (due(QStringLiteral("downloads"))) {
            // files in Downloads untouched for two weeks
            qint64 old = 0;
            int count = 0;
            const QDateTime cutoff = QDateTime::currentDateTime().addDays(-14);
            QDirIterator it(QDir::homePath() + QStringLiteral("/Downloads"), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext() && count < 5000) {
                it.next();
                if (it.fileInfo().lastModified() < cutoff) {
                    old += it.fileInfo().size();
                    count++;
                }
            }
            if (old > qint64(1) << 30) {
                nudge(QStringLiteral("downloads"), QStringLiteral("folder-download"), tr("Downloads is holding %1 of old files").arg(QLocale().formattedDataSize(old)), tr("Pebble can tidy it up"),
                    tr("Tidy up"), tr("Tidy up my Downloads: list the biggest files older than two weeks, then ask me which ones go in the trash."));
            }
        }
    }
    constexpr qint64 AwayMs = 15 * 60 * 1000; // locked or untouched this long = away

    double batteryPercent() {
        QDBusInterface device(QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"), QStringLiteral("org.freedesktop.UPower.Device"), QDBusConnection::systemBus());
        return device.property("IsPresent").toBool() ? device.property("Percentage").toDouble() : -1;
    }

    // Catch me up: back after a while away with notifications waiting, one
    // snackbar says what came; its button has Pebble sum it up
    class Away : public QObject {
            Q_OBJECT
        public:
            qint64 since = -1;   // when the user went away (ms since epoch), -1 while here
            qint64 counting = -1; // notifications from this time on are counted
            bool locked = false;
            double battery = -1;
            QStringList apps;    // one per notification that came while away
            QTimer poll;

            Away() :
                QObject(QCoreApplication::instance()) {
                QDBusConnection::sessionBus().connect({}, QStringLiteral("/com/vicr123/theDesk/Notifications"), QStringLiteral("com.vicr123.theDesk.Notifications"), QStringLiteral("Posted"), this, SLOT(posted(QString, QString, QString)));
                connect(StateManager::powerManager(), &PowerManager::lockedChanged, this, [this](bool isLocked) {
                    locked = isLocked;
                    if (isLocked) leave(DesktopWm::msecsIdle()); // the idle time before an auto-lock counts too
                    else back();
                });
                // once a minute while here; every few seconds while away, to catch the return
                poll.setInterval(60000);
                connect(&poll, &QTimer::timeout, this, [this] {
                    const quint64 idle = DesktopWm::msecsIdle();
                    if (since < 0 && idle >= quint64(AwayMs)) leave(idle);
                    else if (since >= 0 && !locked && idle < 10000) back();
                });
                poll.start();
            }

            void leave(quint64 idle) {
                if (since >= 0) return;
                since = counting = QDateTime::currentMSecsSinceEpoch() - qint64(idle);
                apps.clear();
                battery = batteryPercent();
                poll.setInterval(3000);
            }

            void back() {
                if (since < 0) return;
                const qint64 from = since;
                since = -1;
                poll.setInterval(60000);
                if (QDateTime::currentMSecsSinceEpoch() - from < AwayMs) { // a quick lock
                    counting = -1;
                    return;
                }
                // a few seconds for the phone and mail to catch up after waking
                QTimer::singleShot(5000, this, [this, from] {
                    if (since < 0) tell(from);
                });
            }

            void tell(qint64 from) {
                const QStringList came = apps;
                counting = -1;
                apps.clear();
                if (came.isEmpty() || !tSettings().value(QStringLiteral("Assistant/nudges")).toBool()) return;

                QStringList names;
                for (const QString& app : came)
                    if (!app.isEmpty() && !names.contains(app)) names.append(app);
                const QString count = came.size() == 1 ? ::tr("A notification") : ::tr("%1 notifications").arg(came.size());
                QString text = names.isEmpty()   ? count
                               : names.size() == 1 ? ::tr("%1 from %2").arg(count, names.at(0))
                               : names.size() == 2 ? ::tr("%1 from %2 and %3").arg(count, names.at(0), names.at(1))
                                                   : ::tr("%1 from %2, %3 and others").arg(count, names.at(0), names.at(1));
                QString batteryLine;
                const double now = batteryPercent();
                if (battery >= 0 && now >= 0 && std::abs(now - battery) >= 5) {
                    text += ::tr(" · battery %1% → %2%").arg(qRound(battery)).arg(qRound(now));
                    batteryLine = ::tr(" My battery went from %1% to %2%.").arg(qRound(battery)).arg(qRound(now));
                }

                const QString start = QLocale().toString(QDateTime::fromMSecsSinceEpoch(from).time(), QLocale::ShortFormat);
                const QString end = QLocale().toString(QTime::currentTime(), QLocale::ShortFormat);
                StateManager::hudManager()->showHud({
                    {QStringLiteral("icon"), QStringLiteral("notifications")},
                    {QStringLiteral("title"), ::tr("While you were away")},
                    {QStringLiteral("text"), text},
                    {QStringLiteral("action"), ::tr("Catch me up")},
                    {QStringLiteral("actionId"), PebbleNudges::askAction(::tr("I was away from %1 to %2. Catch me up: check my notifications from then and tell me what I missed in one short card, most important first, and say if anything needs me.%3").arg(start, end, batteryLine))}
                });
            }

        public slots:
            void posted(QString app, QString icon, QString summary) {
                Q_UNUSED(icon)
                Q_UNUSED(summary)
                if (counting >= 0) apps.append(app);
            }
    };
} // namespace

QString PebbleNudges::askAction(const QString& prompt) {
    return QStringLiteral("pebble-ask:") + prompt;
}

void PebbleNudges::setUp() {
    new Away();

    // first look a few minutes in (not during login), then every half hour
    auto* timer = new QTimer(QCoreApplication::instance());
    timer->setInterval(30 * 60 * 1000);
    QObject::connect(timer, &QTimer::timeout, &look);
    QTimer::singleShot(5 * 60 * 1000, timer, [timer] {
        look();
        timer->start();
    });
}

#include "pebblenudges.moc"
