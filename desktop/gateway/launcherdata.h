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
#ifndef LAUNCHERDATA_H
#define LAUNCHERDATA_H

#include <Applications/application.h>
#include <QDateTime>
#include <QObject>
#include <QStringList>

struct LauncherApp {
        ApplicationPointer app;
        QString id; // the desktop entry
        QString name, generic;
        QString folded;     // the name, for matching
        QString alsoFolded; // generic name, keywords, the entry: matched after the name
        QStringList categories, actions;
};

struct LauncherFile {
        QString path, name;
        QDateTime when; // last opened (recents) or changed (found files)
};

struct LauncherDataPrivate;

/**
 * The installed apps (kept loaded, so the Gateway opens at once), which of
 * them are pinned and how often each is launched, the files opened lately
 * (the desktop's recently-used list) and an index of the names of the files
 * in the person's folders, for search.
 */
class LauncherData : public QObject {
        Q_OBJECT
    public:
        static LauncherData* instance();

        const QList<LauncherApp>& apps() const;
        const LauncherApp* app(const QString& id) const;

        // The Gateway's first row: the pinned apps; until something is
        // pinned, the ones launched most, then a browser, a terminal, files...
        QList<const LauncherApp*> pinned(int count) const;
        bool hasPins() const;
        bool isPinned(const QString& id) const;
        void setPinned(const QString& id, bool pinned);

        void launch(const LauncherApp* app, const QString& action = QString());
        int launches(const QString& id) const;

        QList<LauncherFile> recentFiles(int count) const;
        QList<LauncherFile> findFiles(const QString& query, int count) const;
        void refresh(); // on opening: recents, and the index when it's old

        static QString fold(const QString& text); // lower case, no accents

    signals:
        void appsChanged();
        void pinsChanged();
        void recentsChanged();

    private:
        explicit LauncherData();
        LauncherDataPrivate* d;
        void loadApps();
        void readRecents();
        void indexFiles();
};

#endif // LAUNCHERDATA_H
