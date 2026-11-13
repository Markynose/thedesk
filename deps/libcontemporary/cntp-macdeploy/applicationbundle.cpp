//
// Created by Victor Tran on 2/4/2022.
//

#include "applicationbundle.h"
#include "common.h"
#include "library.h"
#include <QDirIterator>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>

struct ApplicationBundlePrivate {
        QDir dir;
        QMap<QString, Library*> libraries;
        QStringList processed;
};

ApplicationBundle::ApplicationBundle(QString path, QObject* parent) :
    QObject(parent) {
    d = new ApplicationBundlePrivate();
    d->dir = path;
}

ApplicationBundle::~ApplicationBundle() {
    delete d;
}

QDir ApplicationBundle::bundleDir() {
    return d->dir;
}

QDir ApplicationBundle::bundleDir(ApplicationBundle::BundleDirectories directory) {
    QDir dir;
    switch (directory) {
        case Resources:
            dir = d->dir.absoluteFilePath("Contents/Resources");
            break;
        case QtPlugins:
            dir = d->dir.absoluteFilePath("Contents/Resources/QtPlugins");
            break;
        case ApplicationPlugins:
            dir = d->dir.absoluteFilePath("Contents/Resources/Plugins");
            break;
        case Frameworks:
            dir = d->dir.absoluteFilePath("Contents/Frameworks");
            break;
        case QtStylePlugins:
            dir = d->dir.absoluteFilePath("Contents/Resources/QtPlugins/styles");
            break;
        case ExecutableBinary:
            dir = d->dir.absoluteFilePath("Contents/MacOS");
            break;
        case IconResources:
            dir = d->dir.absoluteFilePath("Contents/Resources/icons");
            break;
    }
    if (!dir.exists()) dir.mkpath(".");
    return dir;
}

bool ApplicationBundle::exists() {
    return d->dir.exists("Contents");
}

void ApplicationBundle::makeSelfContained() {
    // Copy over required Qt plugins
    copySystemPlugins({"iconengines/libqsvgicon.dylib",
        "imageformats/libqgif.dylib",
        "imageformats/libqicns.dylib",
        "imageformats/libqico.dylib",
        "imageformats/libqjp2.dylib",
        "imageformats/libqjpeg.dylib",
        "imageformats/libqmacheif.dylib",
        "imageformats/libqmng.dylib",
        "imageformats/libqsvg.dylib",
        "imageformats/libqtga.dylib",
        "imageformats/libqtiff.dylib",
        "imageformats/libqwbmp.dylib",
        "imageformats/libqwebp.dylib",
        "platforms/libqcocoa.dylib",
        "sqldrivers/libqsqlite.dylib",
        "styles/libqmacstyle.dylib",
        "tls/libqcertonlybackend.dylib",
        "tls/libqopensslbackend.dylib",
        "tls/libqsecuretransportbackend.dylib",
        "multimedia/libdarwinmediaplugin.dylib"});

    installContemporaryIcons();
    installQtConfigurationFile();

    // Continuously attempt to make the bundle self-contained until no more changes have been made
    while (!doMakeSelfContained())
        ;
}

void ApplicationBundle::sign(QString identity) {
    QDirIterator iterator(d->dir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();

        QProcess signProc;
        signProc.setProcessChannelMode(QProcess::ForwardedChannels);
        signProc.start("codesign", {"--force", "-s", identity, iterator.filePath()});
        signProc.waitForFinished(-1);
    }
}

bool ApplicationBundle::doMakeSelfContained() {
    bool success = true;
    QDirIterator iterator(d->dir, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();

        if (iterator.fileInfo().isSymLink()) continue;
        if (d->processed.contains(iterator.filePath())) continue;
        d->processed.append(iterator.filePath());

        QMap<QString, QString> librariesToUpdate;

        QMultiMap<QString, QString> libraries = otool(iterator.filePath());
        for (QString arch : libraries.keys()) {
            for (QString library : libraries.values(arch)) {
                if (qEnvironmentVariableIsSet("RUNNER_DEBUG")) {
                    QTextStream(stdout) << "DBG: processing library: " << library << "\n";
                }

                if (library.startsWith("/System/Library/") || library.startsWith("/usr/lib/") || library.startsWith("@executable_path/")) continue;

                QString originalLibrary = library;
                if (library.contains("@rpath")) {
                    QStringList rpaths = this->rpaths(iterator.filePath());
                    for (QString rpath : rpaths) {
                        if (QFile::exists(QString(library).replace("@rpath", rpath))) {
                            // TODO: Resolve @loader_path
                            library = library.replace("@rpath", rpath);
                            break;
                        }
                    }
                }
                if (library.contains("@loader_path")) {
                    // Resolve the loader path from the current library name
                    QString identifier = Library::extractIdentifierFromPath(iterator.filePath());
                    Library* originatingLibrary = d->libraries.value(identifier);
                    library = library.replace("@loader_path", QFileInfo(originatingLibrary->libraryPath(arch)).dir().absolutePath());
                }

                QString identifier = Library::extractIdentifierFromPath(library);
                Library* lib = d->libraries.value(identifier);
                if (!lib) {
                    lib = new Library(identifier, this);
                    d->libraries.insert(identifier, lib);
                }

                lib->addLibraryPath(arch, library);
                librariesToUpdate.insert(originalLibrary, identifier);

                if (qEnvironmentVariableIsSet("RUNNER_DEBUG")) {
                    QTextStream(stdout) << "DBG: libraries to update: " << originalLibrary << " -> " << identifier << "\n";
                }
            }
        }

        if (!librariesToUpdate.isEmpty()) {
            QTextStream(stdout) << "Processing " << iterator.fileName() << "...";
            for (QString library : librariesToUpdate.keys()) {
                installNameTool(library, d->libraries.value(librariesToUpdate.value(library))->outputPath(),
                    iterator.filePath());
            }
            QTextStream(stdout) << " ✅\n";
            success = false;
        }
    }
    return success;
}

QMultiMap<QString, QString> ApplicationBundle::otool(QString filePath) {
    if (qEnvironmentVariableIsSet("RUNNER_DEBUG")) {
        QTextStream(stdout) << "DBG: otool -L " << filePath << " -arch "
                            << "all"
                            << "\n";
    }

    QProcess otoolProc;
    otoolProc.start("otool", {"-L", filePath, "-arch", "all"});
    otoolProc.waitForFinished(-1);

    QRegularExpression architectureSearch("\\(architecture (.+)\\)");

    QMultiMap<QString, QString> libraries;
    QString currentArchitecture;
    while (otoolProc.canReadLine()) {
        QString line = otoolProc.readLine().trimmed();
        if (qEnvironmentVariableIsSet("RUNNER_DEBUG")) {
            QTextStream(stdout) << "DBG: " << line << "\n";
        }
        QRegularExpressionMatch architectureSearchMatch = architectureSearch.match(line);
        if (architectureSearchMatch.hasMatch()) {
            currentArchitecture = architectureSearchMatch.captured(1);
        }
        if (!line.contains(" (compatibility ")) continue;
        libraries.insert(currentArchitecture, line.left(line.indexOf(" (compatibility ")));
    }
    return libraries;
}

void ApplicationBundle::installNameTool(QString oldPath, QString newPath, QString filePath) {
    if (qEnvironmentVariableIsSet("RUNNER_DEBUG")) {
        QTextStream(stdout) << "DBG: install_name_tool -change " << oldPath << " " << newPath << " " << filePath;
    }

    QProcess installNameToolProc;
    installNameToolProc.start("install_name_tool", {"-change", oldPath, newPath, filePath});
    installNameToolProc.waitForFinished(-1);
}

QStringList ApplicationBundle::rpaths(QString filePath) {
    QProcess otoolProc;
    otoolProc.start("otool", {"-l", filePath});
    otoolProc.waitForFinished(-1);

    QRegularExpression rpathSearch("path (.+) \\(offset");

    QStringList rpaths;
    bool readNextPath = false;
    while (otoolProc.canReadLine()) {
        QString line = otoolProc.readLine().trimmed();
        if (line == "cmd LC_RPATH") {
            readNextPath = true;
        } else if (line.startsWith("path") && readNextPath) {
            QRegularExpressionMatch rpathSearchMatch = rpathSearch.match(line);
            rpaths.append(rpathSearchMatch.captured(1));
            readNextPath = false;
        }
    }
    return rpaths;
}

void ApplicationBundle::copySystemPlugins(QStringList plugins) {
    QMap<QString, QString> systemPluginDirs;

    QProcess qmakeProc;
    qmakeProc.start("qmake6", {"-query", "QT_INSTALL_PLUGINS"});
    qmakeProc.waitForFinished(-1);
    QString pluginDir = qmakeProc.readAll().trimmed();

    QStringList systemDirArchs = Library::archs(QDir(pluginDir).absoluteFilePath("platforms/libqcocoa.dylib"));
    if (systemDirArchs.contains("arm64")) systemPluginDirs.insert("arm64", pluginDir);
    if (systemDirArchs.contains("x86_64")) systemPluginDirs.insert("x86_64", pluginDir);
    if (systemDirArchs.isEmpty()) systemPluginDirs.insert(systemDirArchs.first(), pluginDir);

    // Special handling for Homebrew on Rosetta
    if (pluginDir.startsWith("/opt/homebrew") && systemDirArchs.contains("arm64")) {
        systemPluginDirs.insert("x86_64", pluginDir.replace("/opt/homebrew", "/usr/local"));
    }

    for (QString plugin : plugins) {
        QMap<QString, QString> pluginPaths;
        for (QString arch : systemPluginDirs.keys()) {
            pluginPaths.insert(arch, QDir(pluginDir).absoluteFilePath(plugin));
        }

        QString output = this->bundleDir(QtPlugins).absoluteFilePath(plugin);
        QFileInfo(output).dir().mkpath(".");
        Library::lipo(pluginPaths, output);
    }
}

void ApplicationBundle::installContemporaryIcons() {
    QTemporaryDir tempDir;
    QNetworkAccessManager mgr;
    QEventLoop loop;

    QProcess ghProcess;
    ghProcess.start("curl", {"-L", "https://api.github.com/repos/vicr123/contemporary-icons/releases/latest"});
    ghProcess.waitForFinished(-1);

    if (ghProcess.exitCode() != 0) {
        QTextStream(stderr) << "Could not obtain latest Contemporary Icons release\n";
        QTextStream(stderr) << ghProcess.readAllStandardError();
        return;
    }

    QJsonObject githubRoot = QJsonDocument::fromJson(ghProcess.readAll()).object();
    QString version = githubRoot.value("tag_name").toString();
    QString zipUrl = QStringLiteral("https://github.com/vicr123/contemporary-icons/archive/refs/tags/%1.zip").arg(version);

    auto zipFile = tempDir.filePath("archive.zip");
    QProcess zipProcess;
    zipProcess.start("curl", {"-L", zipUrl, "-o", zipFile});
    zipProcess.waitForFinished(-1);

    if (zipProcess.exitCode() != 0) {
        QTextStream(stderr) << "Could not obtain Contemporary Icons\n";
        QTextStream(stderr) << zipProcess.readAllStandardError();
        return;
    }

    QProcess zipProc;
    zipProc.setProcessChannelMode(QProcess::ForwardedChannels);
    zipProc.setWorkingDirectory(this->bundleDir(IconResources).absolutePath());
    zipProc.start("unzip", {zipFile});
    zipProc.waitForFinished();

    this->bundleDir(IconResources).rename(QStringLiteral("contemporary-icons-%1").arg(version.remove('v')), "contemporary-icons");
}

void ApplicationBundle::installQtConfigurationFile() {
    QSettings qtConfigurationFile(this->bundleDir(Resources).absoluteFilePath("qt.conf"), QSettings::IniFormat);
    qtConfigurationFile.setValue("Paths/Plugins", "Resources/QtPlugins");
    qtConfigurationFile.setValue("Paths/Imports", "Resources/qml");
    qtConfigurationFile.setValue("Qml2Imports/Imports", "Resources/qml");

    qtConfigurationFile.sync();
}
