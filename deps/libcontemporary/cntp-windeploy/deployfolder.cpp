//
// Created by victo on 11/04/2022.
//

#include <QSettings>
#include <QTemporaryDir>
#include <QProcess>
#include <QDirIterator>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include "deployfolder.h"
#include "systemlibrarydatabase.h"
#include "common.h"
#include "unzip.h"

struct DeployFolderPrivate {
    QDir qtPath;
    QDir hostQtPath;
    QDir dir;
    QStringList processed;
    QStringList foundLibraries;
};

DeployFolder::DeployFolder(QString qtPath, QString hostQtPath, QString folder, QObject* parent) {
    d = new DeployFolderPrivate;
    d->dir = folder;
    d->qtPath = qtPath;
    d->hostQtPath = hostQtPath;
}

DeployFolder::~DeployFolder() {
    delete d;
}

QDir DeployFolder::destinationDir() {
    return d->dir;
}

QDir DeployFolder::destinationDir(DeployFolderDirectories directory) {
    QDir dir;
    switch (directory) {
        case DeployFolderDirectories::QtPlugins:
            dir = d->dir.absoluteFilePath("QtPlugins");
            break;
        case DeployFolderDirectories::ApplicationPlugins:
            dir = d->dir.absoluteFilePath("Plugins");
            break;
        case DeployFolderDirectories::QtStylePlugins:
            dir = d->dir.absoluteFilePath("QtPlugins/styles");
            break;
        case DeployFolderDirectories::IconResources:
            dir = d->dir.absoluteFilePath("icons");
            break;
        case DeployFolderDirectories::Libraries:
            dir = d->dir.absoluteFilePath(".");
            break;
    }
    if (!dir.exists()) dir.mkpath(".");
    return dir;
}

void DeployFolder::makeSelfContained(SystemLibraryDatabase* libraryDatabase) {
    //Copy over required Qt plugins
    copySystemPlugins({
        "iconengines/qsvgicon.dll",
        "imageformats/qgif.dll",
        "imageformats/qicns.dll",
        "imageformats/qico.dll",
        "imageformats/qjp2.dll",
        "imageformats/qjpeg.dll",
        "imageformats/qmacheif.dll",
        "imageformats/qmng.dll",
        "imageformats/qsvg.dll",
        "imageformats/qtga.dll",
        "imageformats/qtiff.dll",
        "imageformats/qwbmp.dll",
        "imageformats/qwebp.dll",
        "platforms/qwindows.dll",
        "sqldrivers/qsqlite.dll",
        "styles/qwindowsvistastyle.dll",
        "tls/qcertonlybackend.dll",
        "tls/qopensslbackend.dll",
        "tls/qschannelbackend.dll",
        "multimedia/windowsmediaplugin.dll"
    });

    installContemporaryIcons();
    installQtConfigurationFile();

    //Continuously attempt to make the bundle self-contained until no more changes have been made
    while (!doMakeSelfContained(libraryDatabase));
}
void DeployFolder::copySystemPlugins(QStringList plugins) {
    QString pluginDir;
    if (qEnvironmentVariableIsSet("QT_PLUGIN_PATH")) {
        pluginDir = qEnvironmentVariable("QT_PLUGIN_PATH");
    } else {
        QString qmakePath = d->hostQtPath.absoluteFilePath("bin/qmake6.exe");

        QStringList args{"-query", "QT_INSTALL_PLUGINS"};
        if (d->hostQtPath != d->qtPath) {
            args.append("-qtconf");
            args.append(d->qtPath.absoluteFilePath("bin/target_qt.conf"));
        }

        QProcess qmakeProc;
        qmakeProc.setProgram(qmakePath);
        qmakeProc.setArguments(args);
        qmakeProc.start();
        qmakeProc.waitForFinished(-1);
        pluginDir = qmakeProc.readAll().trimmed();
    }

    for (QString plugin : plugins) {
        QString pluginPath = QDir(pluginDir).absoluteFilePath(plugin);
        QString output = this->destinationDir(DeployFolderDirectories::QtPlugins).absoluteFilePath(plugin);
        QFileInfo(output).dir().mkpath(".");

        if (QFile::exists(pluginPath)) {
            QFile::copy(pluginPath, output);
        } else {
            QTextStream(stderr) << "Can't find plugin at " << pluginPath << ": skipping\n";
        }
    }
}

void DeployFolder::installContemporaryIcons() {
    QTemporaryDir tempDir;
    QNetworkAccessManager mgr;
    QEventLoop loop;

    QNetworkReply* ghReply = mgr.get(QNetworkRequest(QUrl("https://api.github.com/repos/vicr123/contemporary-icons/releases/latest")));
    connect(ghReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonObject githubRoot = QJsonDocument::fromJson(ghReply->readAll()).object();
    QString version = githubRoot.value("tag_name").toString();
    QString zipUrl = QStringLiteral("https://github.com/vicr123/contemporary-icons/archive/refs/tags/%1.zip").arg(version);

    QNetworkReply* zipReply = mgr.get(QNetworkRequest(QUrl(zipUrl)));
    connect(zipReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QFile zipFile(tempDir.filePath("archive.zip"));
    zipFile.open(QFile::WriteOnly);
    zipFile.write(zipReply->readAll());
    zipFile.close();

    // zipFile.absoluteFilePath()
    Unzip(zipFile.fileName(), this->destinationDir(DeployFolderDirectories::IconResources).absolutePath());

    this->destinationDir(DeployFolderDirectories::IconResources).rename(QStringLiteral("contemporary-icons-%1").arg(version.remove('v')), "contemporary-icons");
}

void DeployFolder::installQtConfigurationFile() {
    QSettings qtConfigurationFile(this->destinationDir().absoluteFilePath("qt.conf"), QSettings::IniFormat);
    qtConfigurationFile.setValue("Paths/Plugins", "QtPlugins");
    qtConfigurationFile.setValue("Paths/Imports", "qml");
    qtConfigurationFile.setValue("Qml2Imports/Imports", "qml");

    qtConfigurationFile.sync();
}

bool DeployFolder::doMakeSelfContained(SystemLibraryDatabase* libraryDatabase) {
    bool done = true;

    QDirIterator iterator(d->dir.absolutePath(), QDir::NoDotAndDotDot | QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();

        if (iterator.fileInfo().isSymLink()) continue;
        if (iterator.fileInfo().suffix() == "svg") continue;
        if (d->processed.contains(iterator.filePath())) continue;
        d->processed.append(iterator.filePath());


        QStringList libraries = dumpBin(iterator.filePath());
        for (QString library : libraries) {
            if (libraryDatabase->ignore(library)) continue;
            if (d->foundLibraries.contains(library)) continue;
            d->foundLibraries.append(library);

            QString libraryPath = libraryDatabase->library(library);
            if (libraryPath.isEmpty()) {
                QTextStream(stderr) << "Can't find library " << library << ", referenced from "
                    << iterator.fileName() << "\n";
                continue;
            }

            QFile::copy(libraryPath, this->destinationDir(DeployFolderDirectories::Libraries).absoluteFilePath(library));
            done = false;
        }
    }


    return done;
}
QStringList DeployFolder::dumpBin(QString file) {
    QStringList referenced;

    QProcess proc;
    proc.start("dumpbin", {"/dependents", file});
    proc.waitForFinished(-1);

    bool readingLibraries = false;
    while (proc.canReadLine()) {
        QString line = proc.readLine().trimmed();
        if (line.contains("Image has the following dependencies") || line.contains("Image has the following delay load dependencies")) {
            readingLibraries = true;
            proc.readLine();
        } else if (line.isEmpty()) {
            readingLibraries = false;
        } else if (readingLibraries) {
            referenced.append(line);
        }
    }

    referenced.removeDuplicates();
    return referenced;
}
