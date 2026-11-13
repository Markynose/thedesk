/****************************************
 *
 *   theDesk - what the Gateway knows: apps, pins, recent files
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "launcherdata.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>
#include <thread>
#include <tpromise.h>
#include <tsettings.h>

namespace {
    constexpr int IndexDepth = 4;        // folders below Documents, Downloads...
    constexpr int IndexLimit = 40000;    // names kept
    constexpr int IndexFreshMs = 300000; // rebuilt on opening when older

    struct Indexed {
            QString folded, path;
    };
} // namespace

struct LauncherDataPrivate {
        QList<LauncherApp> apps;
        QHash<QString, int> byId;
        tSettings settings;
        QList<LauncherFile> recents;
        QDateTime recentsRead;
        QList<Indexed> index;
        QElapsedTimer indexAge;
        bool indexing = false;
};

LauncherData* LauncherData::instance() {
    static LauncherData* instance = new LauncherData();
    return instance;
}

LauncherData::LauncherData() :
    QObject(nullptr) {
    d = new LauncherDataPrivate();
    connect(ApplicationDaemon::instance(), &ApplicationDaemon::appsUpdateRequired, this, &LauncherData::loadApps);
    loadApps();
    readRecents();
    indexFiles();
}

QString LauncherData::fold(const QString& text) {
    // Lower case and without accents, so "cafe" finds "Café"
    QString out;
    const QString decomposed = text.normalized(QString::NormalizationForm_KD);
    out.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing) continue;
        out.append(c.toLower());
    }
    return out;
}

void LauncherData::loadApps() {
    // Desktop entries are read off the main thread; the list is swapped in
    // whole, so the Gateway always has one to show
    (new tPromise<QList<LauncherApp>>([](QString& error) {
        Q_UNUSED(error)
        QList<LauncherApp> apps;
        for (const QString& entry : Application::allApplications()) {
            ApplicationPointer a(new Application(entry));
            if (a->getProperty("Type", "").toString() != QLatin1String("Application")) continue;
            if (a->getProperty("NoDisplay", false).toBool()) continue;
            if (!a->getStringList("OnlyShowIn", {"thedesk"}).contains("thedesk")) continue;
            if (a->getStringList("NotShowIn").contains("thedesk")) continue;
            LauncherApp app;
            app.app = a;
            app.id = a->desktopEntry();
            app.name = a->getProperty("Name", app.id).toString();
            app.generic = a->getProperty("GenericName").toString();
            app.folded = LauncherData::fold(app.name);
            app.alsoFolded = LauncherData::fold(app.generic + QLatin1Char(' ') + a->getStringList("Keywords").join(QLatin1Char(' ')) + QLatin1Char(' ') + app.id);
            app.categories = a->getStringList("Categories");
            app.actions = a->getStringList("Actions");
            apps.append(app);
        }
        std::sort(apps.begin(), apps.end(), [](const LauncherApp& a, const LauncherApp& b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
        return apps;
    }))->then([this](QList<LauncherApp> apps) {
        d->apps = apps;
        d->byId.clear();
        for (int i = 0; i < d->apps.count(); i++) d->byId.insert(d->apps.at(i).id, i);
        emit appsChanged();
    });
}

const QList<LauncherApp>& LauncherData::apps() const {
    return d->apps;
}

const LauncherApp* LauncherData::app(const QString& id) const {
    const int index = d->byId.value(id, -1);
    return index < 0 ? nullptr : &d->apps.at(index);
}

bool LauncherData::hasPins() const {
    // (an unset list reads as one empty entry)
    QStringList pins = d->settings.delimitedList(QStringLiteral("Gateway/pinned"));
    pins.removeAll(QString());
    return !pins.isEmpty();
}

bool LauncherData::isPinned(const QString& id) const {
    return d->settings.delimitedList(QStringLiteral("Gateway/pinned")).contains(id);
}

void LauncherData::setPinned(const QString& id, bool pinned) {
    QStringList pins = d->settings.delimitedList(QStringLiteral("Gateway/pinned"));
    pins.removeAll(QString());
    // The first pin keeps what the row showed until now, so pinning one app
    // doesn't empty the rest of it
    if (pins.isEmpty() && pinned) {
        for (const LauncherApp* shown : this->pinned(7)) pins.append(shown->id);
    }
    pins.removeAll(id);
    if (pinned) pins.append(id);
    d->settings.setDelimitedList(QStringLiteral("Gateway/pinned"), pins);
    emit pinsChanged();
}

int LauncherData::launches(const QString& id) const {
    return d->settings.value(QStringLiteral("GatewayLaunches/") + QString::fromLatin1(id.toUtf8().toHex())).toInt();
}

void LauncherData::launch(const LauncherApp* app, const QString& action) {
    if (!app) return;
    d->settings.setValue(QStringLiteral("GatewayLaunches/") + QString::fromLatin1(app->id.toUtf8().toHex()), launches(app->id) + 1);
    if (action.isEmpty()) {
        app->app->launch();
    } else {
        app->app->launchAction(action);
    }
}

QList<const LauncherApp*> LauncherData::pinned(int count) const {
    QList<const LauncherApp*> row;
    auto add = [&](const LauncherApp* app) {
        if (app && !row.contains(app) && row.count() < count) row.append(app);
    };
    QStringList pins = d->settings.delimitedList(QStringLiteral("Gateway/pinned"));
    pins.removeAll(QString());
    for (const QString& id : std::as_const(pins)) add(this->app(id));
    if (!pins.isEmpty()) return row;

    // Nothing pinned yet: what's launched most...
    QList<const LauncherApp*> used;
    for (const LauncherApp& app : std::as_const(d->apps)) {
        if (launches(app.id) > 0) used.append(&app);
    }
    std::stable_sort(used.begin(), used.end(), [this](const LauncherApp* a, const LauncherApp* b) {
        return launches(a->id) > launches(b->id);
    });
    for (const LauncherApp* app : std::as_const(used)) add(app);
    // ...then one of each thing a desktop is opened for
    for (const char* category : {"WebBrowser", "TerminalEmulator", "FileManager", "TextEditor", "Calculator", "Player", "Settings"}) {
        for (const LauncherApp& app : std::as_const(d->apps)) {
            if (app.categories.contains(QLatin1String(category))) {
                const int before = row.count();
                add(&app);
                if (row.count() > before) break;
            }
        }
    }
    for (const LauncherApp& app : std::as_const(d->apps)) add(&app);
    return row;
}

void LauncherData::readRecents() {
    // The desktop's recently-used list (GTK and Qt file dialogs, and many
    // apps, write it): newest first, only files that are still there
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/recently-used.xbel");
    const QFileInfo info(path);
    if (d->recentsRead.isValid() && info.lastModified() <= d->recentsRead) return;
    d->recentsRead = QDateTime::currentDateTime();
    QList<LauncherFile> files;
    QFile file(path);
    if (file.open(QFile::ReadOnly)) {
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QLatin1String("bookmark")) continue;
            const QUrl url(xml.attributes().value(QLatin1String("href")).toString());
            if (!url.isLocalFile()) continue;
            LauncherFile recent;
            recent.path = url.toLocalFile();
            recent.when = QDateTime::fromString(xml.attributes().value(QLatin1String("visited")).toString(), Qt::ISODate);
            if (!recent.when.isValid()) recent.when = QDateTime::fromString(xml.attributes().value(QLatin1String("modified")).toString(), Qt::ISODate);
            files.append(recent);
        }
    }
    std::sort(files.begin(), files.end(), [](const LauncherFile& a, const LauncherFile& b) {
        return a.when > b.when;
    });
    d->recents.clear();
    for (LauncherFile& recent : files) {
        if (d->recents.count() >= 12) break;
        const QFileInfo there(recent.path);
        if (!there.isFile()) continue;
        recent.name = there.fileName();
        d->recents.append(recent);
    }
    emit recentsChanged();
}

QList<LauncherFile> LauncherData::recentFiles(int count) const {
    return d->recents.mid(0, count);
}

void LauncherData::indexFiles() {
    if (d->indexing) return;
    d->indexing = true;
    QStringList roots;
    for (auto location : {QStandardPaths::DocumentsLocation, QStandardPaths::DownloadLocation, QStandardPaths::PicturesLocation, QStandardPaths::DesktopLocation, QStandardPaths::MusicLocation, QStandardPaths::MoviesLocation}) {
        const QString root = QStandardPaths::writableLocation(location);
        // A missing folder falls back to the home folder itself: not that
        if (!root.isEmpty() && root != QDir::homePath() && !roots.contains(root) && QDir(root).exists()) roots.append(root);
    }
    QPointer<LauncherData> self = this;
    std::thread([self, roots] {
        QList<Indexed> index;
        for (const QString& root : roots) {
            const int rootDepth = root.count(QLatin1Char('/'));
            QDirIterator walk(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (walk.hasNext() && index.count() < IndexLimit) {
                const QString path = walk.next();
                // Not below hidden folders, not too deep (a checked-out
                // project is thousands of files nobody searches for by name)
                if (path.contains(QLatin1String("/.")) || path.count(QLatin1Char('/')) - rootDepth > IndexDepth) continue;
                if (path.contains(QLatin1String("/node_modules/")) || path.contains(QLatin1String("/build/"))) continue;
                index.append({LauncherData::fold(walk.fileName()), path});
            }
        }
        QMetaObject::invokeMethod(qApp, [self, index] {
            if (!self) return;
            self->d->index = index;
            self->d->indexAge.start();
            self->d->indexing = false;
        }, Qt::QueuedConnection);
    }).detach();
}

QList<LauncherFile> LauncherData::findFiles(const QString& query, int count) const {
    QList<LauncherFile> found;
    const QString folded = fold(query);
    if (folded.size() < 2) return found;
    // Names that start with it first, then names that hold it; recent ones
    // ahead within each
    QSet<QString> seen;
    QList<LauncherFile> starts, holds;
    auto consider = [&](const QString& name, const QString& path, const QDateTime& when) {
        if (seen.contains(path)) return;
        const int at = name.indexOf(folded);
        if (at < 0) return;
        seen.insert(path);
        LauncherFile file;
        file.path = path;
        file.name = QFileInfo(path).fileName();
        file.when = when;
        (at == 0 ? starts : holds).append(file);
    };
    for (const LauncherFile& recent : std::as_const(d->recents)) consider(fold(recent.name), recent.path, recent.when);
    for (const Indexed& entry : std::as_const(d->index)) {
        if (starts.count() >= count * 4) break;
        consider(entry.folded, entry.path, QDateTime());
    }
    for (const QList<LauncherFile>& list : {starts, holds}) {
        for (LauncherFile file : list) {
            if (found.count() >= count) return found;
            if (!file.when.isValid()) file.when = QFileInfo(file.path).lastModified();
            found.append(file);
        }
    }
    return found;
}

void LauncherData::refresh() {
    readRecents();
    if (!d->indexAge.isValid() || d->indexAge.elapsed() > IndexFreshMs) indexFiles();
}
