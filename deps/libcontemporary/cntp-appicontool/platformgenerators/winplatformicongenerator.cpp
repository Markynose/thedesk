#include "winplatformicongenerator.h"
#include <Windows.h>
#include <QSvgRenderer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QDir>
#include <QPainter>
#include <QBuffer>
#include <tuple>

#include <pshpack1.h>

struct IconDirectoryEntry {
    uint8_t bWidth;
    uint8_t bHeight;
    uint8_t bColorCount;
    uint8_t bReserved;
    uint16_t wPlanes;
    uint16_t wBitCount;
    uint32_t dwBytesInRes;
    uint32_t dwImageOffset;
};

struct IconDirectory {
    uint16_t idReserved;
    uint16_t idType;
    uint16_t idCount;
    IconDirectoryEntry idEntries[];
};

#include <poppack.h>

void WinPlatformIconGenerator::generateIcon() {
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) 
        return;

    QList<std::tuple<int, QSize>> buffers;

    QSvgRenderer renderer(inputSvg().toUtf8());
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::ReadWrite);
    for (int sideLength: QList<int>({256, 64, 48, 32, 24, 16})) {
        QSize size{sideLength, sideLength};

        QImage image(size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);

        QPainter imagePainter(&image);
        renderer.render(&imagePainter, QRect(QPoint{}, size));

        buffers.append({buffer.pos(), size});
        image.save(&buffer, "PNG");
    }
    buffers.append({buffer.pos(), QSize{}});
   
    auto directorySize = sizeof(IconDirectory) + (buffers.length() - 1) * sizeof(IconDirectoryEntry);
    IconDirectory *icons = reinterpret_cast<IconDirectory*>(operator new(directorySize));
    icons->idReserved = 0;
    icons->idType = 1; // ICO
    icons->idCount = buffers.length() - 1;

    for (int i = 0; i < buffers.length() - 1; i++) {
        IconDirectoryEntry *entry = icons->idEntries + i;
        auto& [bufferOffset, size] = buffers[i];

        entry->bWidth = size.width();
        entry->bHeight = size.height();
        entry->bColorCount = 0;
        entry->bReserved = 0;
        entry->wPlanes = 0;
        entry->wBitCount = 0;
        entry->dwBytesInRes = std::get<0>(buffers[i + 1]) - bufferOffset;
        entry->dwImageOffset = bufferOffset + directorySize;
    }


    QFile output(outputFile());
    output.remove();
    output.open(QIODevice::WriteOnly);
    output.write(reinterpret_cast<const char*>(icons), directorySize);

    operator delete(icons);

    output.write(ba);
    output.close();
}