#include "pebblenudges.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QLocale>
#include <QStorageInfo>
#include <QTimer>
#include <hudmanager.h>
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
} // namespace

QString PebbleNudges::askAction(const QString& prompt) {
    return QStringLiteral("pebble-ask:") + prompt;
}

void PebbleNudges::setUp() {
    // first look a few minutes in (not during login), then every half hour
    auto* timer = new QTimer(QCoreApplication::instance());
    timer->setInterval(30 * 60 * 1000);
    QObject::connect(timer, &QTimer::timeout, &look);
    QTimer::singleShot(5 * 60 * 1000, timer, [timer] {
        look();
        timer->start();
    });
}
