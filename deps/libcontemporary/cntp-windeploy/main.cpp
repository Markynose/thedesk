#include <QCoreApplication>
#include <QCommandLineParser>
#include "deployfolder.h"
#include "common.h"
#include "systemlibrarydatabase.h"
#include "portableexecutablefile.h"

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument("directory", "Directory containing application prepare for deployment");
    parser.addOption({{"s", "sdk"}, "SDK version to deploy (defaults to 10.0.22000.0 if not specified)", "sdk-version"});
    parser.addOption({{"q", "qt-path"}, "Path to Qt installation to use", "qt-path"});
    parser.addOption({{"h", "host-qt-path"}, "When deploying a cross-compiled application, path to Host Qt installation", "host-qt-path"});
    parser.addOption({{"l", "library-path"}, "Path to search for libraries to deploy (defaults to Program Files)", "library-path"});
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

    auto sdkVersion = QStringLiteral("10.0.22000.0");
    if (parser.isSet("sdk")) sdkVersion = parser.value("sdk");

    auto qtPath = qEnvironmentVariable("QT_ROOT_DIR", qEnvironmentVariable("Qt6_DIR"));
    if (parser.isSet("qt-path")) qtPath = parser.value("qt-path");

    auto hostQtPath = qEnvironmentVariable("Qt6_DIR");
    if (parser.isSet("host-qt-path")) hostQtPath = parser.value("host-qt-path");

    QStringList libraryPaths{"/Program Files", "/Program Files (x86)"};
    if (parser.isSet("library-path")) libraryPaths = parser.values("library-path");
    libraryPaths.append(qtPath);

    if (qtPath.isEmpty()) {
        eoutput << "error: Path to Qt not specified.\n";
        return 1;
    }

    PortableExecutableFile qt6CorePe(QDir(qtPath).absoluteFilePath("bin/Qt6Core.dll"));
    if (!qt6CorePe.isValid()) {
        eoutput << "error: Invalid Qt installation path.\n";
        return 1;
    }

    auto contemporaryPath = QDir(qtPath).absoluteFilePath("plugins/styles/contemporary.dll");
    if (!QFile::exists(contemporaryPath)) {
        eoutput << "error: Contemporary theme not installed.\n";
        return 1;
    }

    switch (qt6CorePe.targetMachine()) {

        case TargetMachine::Amd64:
            output << "Detected Qt for x86_64\n";
            output.flush();
            break;
        case TargetMachine::Arm64:
            output << "Detected Qt for aarch64\n";
            output.flush();
            break;
        default:
            eoutput << "error: Qt installation path contains Qt install for invalid architecture.\n";
            return 1;
    }

    output << "Copying Qt plugins\n";
    output.flush();
    DeployFolder deployFolder(qtPath, hostQtPath, parser.positionalArguments().first());
    QFile::copy(contemporaryPath, deployFolder.destinationDir(DeployFolder::DeployFolderDirectories::QtStylePlugins).absoluteFilePath("contemporary.dll"));

    output << "Searching for support libraries...\n";
    output.flush();
    QStringList supportLibraryPaths;

    QDir rootProjectDir = deployFolder.destinationDir().absoluteFilePath("./");
    for (QString markerFile : Common::findInPaths(".cntp-is-support-library", {rootProjectDir.absolutePath()}, true)) {
        QFileInfo markerFileInfo(markerFile);
        QDir libDir = markerFileInfo.dir();

        //Search for libraries
        for (QFileInfo libraryInfo : libDir.entryInfoList({"*.dll"}, QDir::Files)) {
            supportLibraryPaths.append(libraryInfo.dir().absolutePath());
        }
    }

    QStringList extraSearchPaths;
    extraSearchPaths.append(supportLibraryPaths);

    output << "Creating System Library Database...\n";
    output.flush();
    SystemLibraryDatabase* libraryDatabase = new SystemLibraryDatabase(supportLibraryPaths, sdkVersion, qt6CorePe.targetMachine(), libraryPaths);

    output << "Making folder self contained\n";
    output.flush();
    deployFolder.makeSelfContained(libraryDatabase);

    return 0;
}
