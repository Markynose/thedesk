#include "macplatformicongenerator.h"

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QSvgRenderer>
#include <QTemporaryDir>

MacPlatformIconGenerator::MacPlatformIconGenerator(QObject* parent) :
    PlatformIconGenerator{parent} {
}

void MacPlatformIconGenerator::generateIcon() {
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return;

    QString tempDirPath = tempDir.path();

    QDir iconSetDir(tempDir.filePath("icon.iconset"));
    iconSetDir.mkpath(".");

    struct IconDimensions {
            int size;
            int scaleFactor;
    };

    for (IconDimensions dimensions : QList<IconDimensions>({
             {16,  1},
             {16,  2},
             {32,  1},
             {32,  2},
             {128, 1},
             {128, 2},
             {256, 1},
             {256, 2},
             {512, 1},
             {512, 2},
    })) {
        QString filename = QStringLiteral("icon_%1x%1%2.png").arg(dimensions.size).arg(dimensions.scaleFactor == 1 ? "" : QStringLiteral("@%1x").arg(dimensions.scaleFactor));
        renderPng(iconSetDir.absoluteFilePath(filename), QSize(dimensions.size, dimensions.size) * dimensions.scaleFactor);
    }

    QProcess icnsGenerator;
    icnsGenerator.setWorkingDirectory(tempDir.path());
    icnsGenerator.start("iconutil", {"-c", "icns", "icon.iconset"});
    icnsGenerator.waitForFinished(-1);

    if (QFile::exists(outputFile())) QFile::remove(outputFile());
    QFile::copy(tempDir.filePath("icon.icns"), outputFile());
}

void MacPlatformIconGenerator::renderPng(QString output, QSize size) {
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter imagePainter(&image);

    QSvgRenderer renderer(inputSvg().toUtf8());
    renderer.render(&imagePainter, QRect(QPoint(0, 0), size));
    imagePainter.end();

    image.save(output);
}
