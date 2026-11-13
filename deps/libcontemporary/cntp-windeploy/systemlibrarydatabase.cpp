#include "systemlibrarydatabase.h"

#include <QMap>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include "portableexecutablefile.h"

struct SystemLibraryDatabasePrivate {
    QMap<QString, QString> libraries;
    QSet<QString> ignoreLibraries;
    TargetMachine targetArch;
};

SystemLibraryDatabase::SystemLibraryDatabase(QStringList extraSearchPaths, QString sdkVersion, TargetMachine arch, QStringList deepSearchPaths, QObject* parent) : QObject(parent) {
    d = new SystemLibraryDatabasePrivate;
    d->targetArch = arch;

    QSettings winSdkSettings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", QSettings::NativeFormat);

    QString archString;
    switch (arch) {
        case TargetMachine::Amd64:
            archString = "x64";
            break;
        case TargetMachine::Arm64:
            archString = "arm64";
            break;
        default:
            break;
    }

    QDir winSdkPath = QDir(winSdkSettings.value("KitsRoot10").toString()).absoluteFilePath(QStringLiteral("Lib/%1/um/%2").arg(sdkVersion, archString));

    for (auto& entry : winSdkPath.entryList(QDir::Files)) {
        d->ignoreLibraries.insert(entry.toLower());
    }

    //Find DLL files
    QStringList searchPaths = extraSearchPaths;
    searchPaths.append(qEnvironmentVariable("Path").split(";"));
    searchPaths.removeAll("");
    searchPaths.removeDuplicates();

    for (QString path : searchPaths) {
        QDir dir(path);
        for (QFileInfo libFile : dir.entryInfoList({"*.dll", "*.DLL"}, QDir::Files)) {
            if (d->libraries.contains(libFile.fileName().toLower())) continue;
            if (!libFile.fileName().toLower().startsWith("msvc") && isInDisallowedPath(libFile.absoluteFilePath().toLower())) continue;
            if (!isLibraryOk(libFile.absoluteFilePath())) continue;
            d->libraries.insert(libFile.fileName().toLower(), libFile.absoluteFilePath());
        }
    }

    // Exclude C++ runtime
    QProcess vsWhere;
    auto vsWherePath = QDir(qEnvironmentVariable("ProgramFiles(x86)")).absoluteFilePath("Microsoft Visual Studio/Installer/vswhere.exe");
    vsWhere.start(vsWherePath, {
        "-prerelease",
        "-latest",
        "-products", "*",
        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.*",
        "-property", "installationPath"
    });
    vsWhere.waitForFinished(-1);

    while (vsWhere.canReadLine()) {
        auto vsPath = vsWhere.readLine().trimmed();
        auto dllRoot = QDir(vsPath).absoluteFilePath("VC/Redist/MSVC/");
        QDirIterator iterator(dllRoot, {"*.dll"}, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();

            if (!isLibraryOk(iterator.filePath())) continue;

            d->ignoreLibraries.insert(iterator.fileName().toLower());
        }
    }

    d->ignoreLibraries << "ucrtbase.dll"
        << "ucrtbased.dll";

    for (QString path : deepSearchPaths) {
        QDirIterator iterator(
            QDir(path).absolutePath(), {"*.dll"}, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();

            if (d->libraries.contains(iterator.fileName().toLower())) continue;
            if (!isLibraryOk(iterator.filePath())) continue;

            d->libraries.insert(iterator.fileName().toLower(), iterator.filePath());
        }
    }
}

SystemLibraryDatabase::~SystemLibraryDatabase() {
    delete d;
}

QString SystemLibraryDatabase::library(QString libraryName) {
    return d->libraries.value(libraryName.toLower());
}

bool SystemLibraryDatabase::ignore(QString libraryName) {
    if (libraryName.startsWith("api-ms") || libraryName.startsWith("ext-ms")) return true;

    if (d->ignoreLibraries.contains(libraryName.toLower())) return true;
    libraryName.replace(".dll", ".lib");
    if (d->ignoreLibraries.contains(libraryName.toLower())) return true;

    return false;
}

bool SystemLibraryDatabase::isInDisallowedPath(QString path) {
    QStringList disallowedPaths({"/windows/system32", "/windows/syswow64/"});
    for (QString disallowedPath : disallowedPaths) {
        if (path.contains(disallowedPath)) return true;
    }
    return false;
}

bool SystemLibraryDatabase::isLibraryOk(QString libraryPath) {
    PortableExecutableFile peFile(libraryPath);
    if (!peFile.isValid()) {
        return false;
    }

    if (peFile.targetMachine() != d->targetArch) {
        return false;
    }

    return true;
}
