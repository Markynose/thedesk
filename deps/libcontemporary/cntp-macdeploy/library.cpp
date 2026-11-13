//
// Created by Victor Tran on 2/4/2022.
//

#include <QRegularExpression>
#include <QProcess>
#include <QTemporaryFile>
#include "library.h"
#include "common.h"

struct LibraryPrivate {
    QString identifier;
    QMap<QString, QString> paths;
    ApplicationBundle* bundle;

    QString pathInsideFramework;
    QString outputPath;
};

Library::Library(QString identifier, ApplicationBundle* bundle, QObject *parent) : QObject(parent) {
    d = new LibraryPrivate;
    d->identifier = identifier;
    d->bundle = bundle;
}

Library::~Library() {
    delete d;
}

QString Library::extractIdentifierFromPath(QString path) {
    QRegularExpression identifierExpression(".+\\/(.+\\.(?:framework|dylib))");
    QRegularExpressionMatch match = identifierExpression.match(path);
    if (match.hasMatch()) {
        return match.captured(1);
    } else {
        return path;
    }
}

void Library::addLibraryPath(QString arch, QString path) {
    if (arch == "") {
        addLibraryPath("arm64", path);
        addLibraryPath("x86_64", path);
        return;
    }
    if (d->paths.contains(arch)) return;

    if (path.contains(".framework/")) {
        d->pathInsideFramework = path.mid(path.indexOf(".framework/") + 11);
        path = path.left(path.indexOf(".framework/") + 11);
    }
    d->paths.insert(arch, path);
}

QString Library::outputPath() {
    if (d->outputPath.isEmpty()) {
        QString outputPath = d->bundle->bundleDir(ApplicationBundle::Frameworks).filePath(d->identifier);
        QString absoluteOutputPath;
        if (d->pathInsideFramework.isEmpty()) {
            //Just lipo all the dylibs together
            lipo(d->paths, outputPath);
            absoluteOutputPath = outputPath;
        } else {
            QDir outputDir(outputPath);
            //Copy a framework directory, and then lipo the binary
            Common::copyDirRecursively(d->paths.first(), outputPath);

            QMap<QString, QString> toMerge;
            for (QString arch : d->paths.keys()) {
                toMerge.insert(arch, QDir(d->paths.value(arch)).absoluteFilePath(d->pathInsideFramework));
            }
            lipo(toMerge, outputDir.absoluteFilePath(d->pathInsideFramework));

            absoluteOutputPath = outputDir.absoluteFilePath(d->pathInsideFramework);
        }

        d->outputPath = "@executable_path/" + d->bundle->bundleDir(ApplicationBundle::ExecutableBinary).relativeFilePath(absoluteOutputPath);
    }
    return d->outputPath;
}

void Library::lipo(QMap<QString, QString> toMerge, QString output) {
    bool first = true;
    for (QString arch : toMerge.keys()) {
        QFileInfo libFileInfo(toMerge.value(arch));
        QString libFile;

        if (libFileInfo.isSymLink()) {
            libFile = libFileInfo.symLinkTarget();
        } else {
            libFile = libFileInfo.absoluteFilePath();
        }

        if (!QFile::exists(libFile)) {
            QTextStream(stderr) << libFile << " does not exist. The application bundle may not be complete.\n";
//            continue;
        }

        QTemporaryFile temporary;
        temporary.open();
        QString tempFile = temporary.fileName();
        temporary.close();
        temporary.remove();

        QStringList archs = Library::archs(libFile);

        if (archs.length() == 1 && archs.first() == arch) {
            tempFile = libFile;
        } else {
            QProcess extractProc;
            extractProc.start("lipo", {libFile, "-extract", arch, "-output", tempFile});
            extractProc.waitForFinished(-1);
        }

        QStringList mergeArgs;
        mergeArgs.append("-create");
        mergeArgs.append(tempFile);
        if (!first) mergeArgs.append(output);
        mergeArgs.append("-output");
        mergeArgs.append(output);
        QProcess mergeProc;
        mergeProc.start("lipo", mergeArgs);
        mergeProc.waitForFinished(-1);

        temporary.remove();

        first = false;
    }
}

QString Library::libraryPath(QString arch) {
    if (arch.isEmpty()) arch = "x86_64";
    return d->paths.value(arch);
}

QStringList Library::archs(QString path) {
    QProcess archsProc;
    archsProc.start("lipo", {"-archs", path});
    archsProc.waitForFinished();
    return QString(archsProc.readAllStandardOutput().trimmed()).split(" ");
}
