#include "alias.h"
#include "diskimage.h"
#include "dsstore.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSvgRenderer>
#include <QTemporaryDir>

#include <AppKit/AppKit.h>

void copyDir(QDir dir, QString to) {
    QDir toDir(to);
    QDir::root().mkpath(toDir.absolutePath());

    auto entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const auto& entry : entries) {
        auto destination = toDir.absoluteFilePath(entry.fileName());
        if (entry.isDir()) {
            copyDir(entry.absoluteFilePath(), destination);
        } else if (entry.isFile()) {
            QFile file(entry.absoluteFilePath());
            file.open(QFile::ReadOnly);

            QFile destFile(destination);
            destFile.open(QFile::WriteOnly);

            qulonglong read = 0;
            while (read < file.size()) {
                auto buf = file.read(8192);
                destFile.write(buf);
                read += buf.length();
            }
            destFile.close();
            file.close();

            destFile.setPermissions(file.permissions());
        } else if (entry.isSymLink()) {
            QFile::link(entry.symLinkTarget(), destination);
        }
    }
}

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument("bundle", "Application bundle to package");
    parser.addPositionalArgument("output", "Output path of the final DMG");
    QCommandLineOption helpOption = parser.addHelpOption();
    parser.parse(a.arguments());

    QTextStream output(stdout);
    QTextStream eoutput(stderr);
    if (parser.isSet(helpOption)) {
        output << parser.helpText() << "\n";
        return 0;
    }

    if (parser.positionalArguments().length() != 2) {
        eoutput << "error: incorrect number of arguments.\n";
        return 1;
    }

    auto bundle = QDir(parser.positionalArguments().at(0));
    auto dmgOut = parser.positionalArguments().at(1);

    if (!bundle.exists()) {
        eoutput << "error: application bundle does not exist.\n";
        return 1;
    }

    // Count the size of the application bundle
    qulonglong size = 0;
    QDirIterator iterator(bundle, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        auto fileInfo = iterator.nextFileInfo();
        if (fileInfo.isFile()) {
            size += fileInfo.size() * 2 + 32;
        }
    }

    if (size < 52428800) {
        size = 52428800;
    }

    QTemporaryDir tempDir;
    {
        auto infoPlistPath = bundle.absoluteFilePath("Contents/Info.plist");
        if (!QFile::exists(infoPlistPath)) {
            eoutput << "error: Info.plist file cannot be found.\n";
            return 1;
        }

        auto infoPlist = [NSDictionary dictionaryWithContentsOfFile:infoPlistPath.toNSString()];
        NSString *bundleExecutable = [infoPlist valueForKey:@"CFBundleExecutable"];

        auto temporaryImage = DiskImage(size, QString::fromNSString(bundleExecutable), tempDir.filePath("testdmg.dmg"));
        if (!temporaryImage.isValid()) {
            eoutput << "error: unable to create temporary disk image.\n";
            return 1;
        }

        if (!temporaryImage.mount()) {
            eoutput << "error: unable to mount temporary disk image.\n";
            return 1;
        }

        // Render out the background
        auto dmgDir = QDir(bundle.absoluteFilePath("Contents/Resources/cntp/dmg"));
        QString backgroundSourceFile;

        for (auto entry : dmgDir.entryInfoList()) {
            if (entry.suffix() == "svg" && entry.isFile()) {
                backgroundSourceFile = entry.absoluteFilePath();
            }
        }

        if (backgroundSourceFile.isEmpty()) {
            eoutput << "error: background source file does not exist.\n";
        }

        QSvgRenderer renderer(backgroundSourceFile);
        if (!renderer.isValid()) {
            eoutput << "error: unable to load background file.\n";
            return 1;
        }

        if (!renderer.elementExists("applications")) {
            eoutput << "error: background SVG does not contain an element with ID #applications.\n";
            return 1;
        }

        if (!renderer.elementExists("app")) {
            eoutput << "error: background SVG does not contain an element with ID #app.\n";
            return 1;
        }

        temporaryImage.mountPath().mkdir(".background");
        auto backgroundDir = QDir(temporaryImage.mountPath().absoluteFilePath(".background"));

        QSize imageSize = renderer.defaultSize();
        double qualityScale = 2;

        const double dpiToDpm = 39.37;

        auto finderScale = imageSize.width() / renderer.viewBoxF().width();

        auto applicationsLinkLocation = (renderer.transformForElement("applications").mapRect(renderer.boundsOnElement("applications")).center().toPoint() * finderScale) - QPoint(0, 10);
        auto appLocation = (renderer.transformForElement("app").mapRect(renderer.boundsOnElement("app")).center().toPoint() * finderScale) - QPoint(0, 10);

        QImage image(imageSize * qualityScale, QImage::Format_ARGB32);
        image.setDotsPerMeterX(72 * dpiToDpm * qualityScale);
        image.setDotsPerMeterY(72 * dpiToDpm * qualityScale);
        QPainter painter(&image);
        renderer.render(&painter, QRectF(QPointF(0, 0), imageSize * qualityScale));
        painter.end();

        image.save(backgroundDir.absoluteFilePath("background.tiff"), "TIFF");

        // Copy the application bundle
        copyDir(bundle, temporaryImage.mountPath().absoluteFilePath(bundle.dirName()));

        // Create link to Applications folder
        QFile::link("/Applications", temporaryImage.mountPath().absoluteFilePath("Applications"));

        // Create DS_Store file
        DsStore dsStore;
        dsStore.setWindowGeometry(QRect(QPoint(100, 100), imageSize));
        dsStore.setWindowProperties(48, Alias::aliasFor(backgroundDir.absoluteFilePath("background.tiff")));
        dsStore.vSrn(1);
        dsStore.moveIcon("Applications", applicationsLinkLocation.x(), applicationsLinkLocation.y());
        dsStore.moveIcon(bundle.dirName(), appLocation.x(), appLocation.y());
        dsStore.write(temporaryImage.mountPath().absoluteFilePath(".DS_Store"));
    }

    // Finalise the disk image
    QFile::copy(tempDir.filePath("testdmg.dmg"), tempDir.filePath("stagingdmg.dmg"));

    DiskImage staging(tempDir.filePath("stagingdmg.dmg"));
    staging.mount();
    staging.unmount();

    if (!DiskImage::convert(tempDir.filePath("stagingdmg.dmg"), dmgOut)) {
        eoutput << "error: unable to create compressed image at the output.\n";
        return 1;
    }

    output << "Created deployable disk image at " << dmgOut << "\n";
    return 0;
}
