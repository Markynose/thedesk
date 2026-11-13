#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QDirIterator>
#include "applicationbundle.h"
#include "common.h"

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);

    //Make sure to search /usr/local/Cellar first
    QStringList libraryPaths = a.libraryPaths();
    for (QString path : libraryPaths) {
        if (path.startsWith("/opt/homebrew")) {
            libraryPaths.prepend(path.replace("/opt/homebrew", "/usr/local"));
        }
    }

    QCommandLineParser parser;
    parser.addPositionalArgument("bundle", "Application bundle to prepare for deployment");
    QCommandLineOption helpOption = parser.addHelpOption();
    parser.parse(a.arguments());

    QTextStream output(stdout);
    QTextStream eoutput(stderr);
    if (parser.isSet(helpOption)) {
        output << parser.helpText() << "\n";
        return 0;
    }

    if (parser.positionalArguments().length() != 1) {
        eoutput << "error: incorrect number of arguments.\n";
        return 1;
    }

    ApplicationBundle bundle(parser.positionalArguments().first());
    if (!bundle.exists()) {
        eoutput << "error: application bundle does not exist.\n";
        return 1;
    }

    QStringList contemporaryPath = Common::findInPaths("contemporary.dylib", libraryPaths);
    if (contemporaryPath.isEmpty()) {
        eoutput << "error: Contemporary theme not installed.\n";
        return 1;
    }

    output << "Copying Contemporary...\n";
    output.flush();
    QFile::copy(contemporaryPath.first(), bundle.bundleDir(ApplicationBundle::QtStylePlugins).absoluteFilePath("contemporary.dylib"));

    QDir pluginFolderDir = bundle.bundleDir().absoluteFilePath("../../plugins");
    if (pluginFolderDir.exists()) {
        for (QString pluginFile : Common::findInPaths("*.dylib", {pluginFolderDir.absolutePath()})) {
            QFileInfo fileInfo(pluginFile);
            QFile::copy(pluginFile, bundle.bundleDir(ApplicationBundle::ApplicationPlugins).absoluteFilePath(fileInfo.fileName()));
        }
    }

    output << "Copying support libraries...\n";
    QStringList supportLibraryPaths;
    QDir rootProjectDir = bundle.bundleDir().absoluteFilePath("../../");
    for (QString markerFile : Common::findInPaths(".cntp-is-support-library", {rootProjectDir.absolutePath()})) {
        QFileInfo markerFileInfo(markerFile);
        QDir libDir = markerFileInfo.dir();

        //Search for frameworks
        for (QFileInfo frameworkFileInfo : libDir.entryInfoList({"*.framework"}, QDir::Dirs)) {
            supportLibraryPaths.append(frameworkFileInfo.dir().absolutePath());
        }
    }

    output << "Making bundle self contained...\n";
    output.flush();
    bundle.makeSelfContained();

    output << "Re-signing bundle...\n";
    output.flush();
    bundle.sign("-");
    return 0;
}