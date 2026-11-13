//
// Created by Victor Tran on 2/4/2022.
//

#include "common.h"
#include <QDirIterator>
#include <QProcess>

QStringList Common::findInPaths(const QString &file, const QStringList &paths, bool subdirs) {
    QStringList files;
    for (const QString& path : paths) {
        QDirIterator iterator(path, {file}, QDir::Files | QDir::Hidden, QDirIterator::FollowSymlinks | (subdirs ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags));
        while (iterator.hasNext()) {
            iterator.next();
            files.append(iterator.filePath());
        }
    }
    return files;
}

void Common::copyDirRecursively(QDir dir, QDir destination) {
    if (!destination.exists()) destination.mkpath(".");
    for (QFileInfo entry : dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        if (entry.isSymLink()) {
            QProcess symlinkInfoProc;
            symlinkInfoProc.start("readlink", {entry.absoluteFilePath()});
            symlinkInfoProc.waitForFinished(-1);
            QFile::link(symlinkInfoProc.readAll().trimmed(), destination.absoluteFilePath(entry.fileName()));
        } else if (entry.isDir()) {
            copyDirRecursively(entry.absoluteFilePath(), destination.absoluteFilePath(entry.fileName()));
        } else {
            QFile::copy(entry.absoluteFilePath(), destination.absoluteFilePath(entry.fileName()));
        }
    }
}
