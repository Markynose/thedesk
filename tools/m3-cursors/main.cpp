/****************************************
 *
 *   theDesk - Material 3 Expressive pointer theme generator
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
// m3-cursors <icons dir>            neutral theme (what gets installed)
// m3-cursors --scheme [<icons dir>] in the session's M3 colours (default ~/.local/share/icons)
// m3-cursors --preview <out.png>    a sheet of every cursor, neutral and scheme
// m3-cursors --preview-extra <out.png>  the symbol cursors (pointer, resize...)
#include <QGuiApplication>
#include <QPainter>
#include <m3cursor.h>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    if (args.isEmpty()) {
        qWarning("usage: m3-cursors <icons dir> | --scheme [<icons dir>] | --preview <out.png>");
        return 1;
    }
    if (args.first() == QStringLiteral("--preview") && args.size() > 1) {
        const int size = qEnvironmentVariableIntValue("PREVIEW_SIZE") > 0 ? qEnvironmentVariableIntValue("PREVIEW_SIZE") : 48, cell = size * 4 / 3;
        QList<M3Cursor::Colours> rows = {M3Cursor::neutralColours(), M3Cursor::schemeColours()};
        QImage sheet(cell * 8, cell * rows.size(), QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(0x80, 0x80, 0x80));
        QPainter p(&sheet);
        for (int r = 0; r < rows.size(); r++) {
            QList<QImage> images = {M3Cursor::renderArrow(size, rows[r]), M3Cursor::renderArrow(size, rows[r], 0.6), M3Cursor::renderArrow(size, rows[r], 0.25),
                M3Cursor::renderText(size, rows[r]), M3Cursor::renderBusy(size, rows[r], 0, false), M3Cursor::renderBusy(size, rows[r], 300, false),
                M3Cursor::renderBusy(size, rows[r], 0, true), M3Cursor::renderBusy(size, rows[r], 900, true)};
            // left half of each row on white, right half on dark, so both are judged
            p.fillRect(QRect(0, r * cell, cell * 4, cell), QColor(0xf4, 0xf4, 0xf4));
            p.fillRect(QRect(cell * 4, r * cell, cell * 4, cell), QColor(0x1a, 0x1c, 0x18));
            for (int i = 0; i < images.size(); i++) p.drawImage(i * cell + (cell - size) / 2, r * cell + (cell - size) / 2, images[i]);
        }
        p.end();
        return sheet.save(args.at(1)) ? 0 : 1;
    }
    if (args.first() == QStringLiteral("--preview-extra") && args.size() > 1) {
        // Every extra cursor in the session's colours on light and dark, the
        // hotspot marked with a small red dot
        const int size = 48, cell = 64;
        const QStringList names = M3Cursor::extraCursorNames();
        QImage sheet(cell * names.size(), cell * 2, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&sheet);
        p.fillRect(QRect(0, 0, sheet.width(), cell), QColor(0xf4, 0xf4, 0xf4));
        p.fillRect(QRect(0, cell, sheet.width(), cell), QColor(0x1a, 0x1c, 0x18));
        for (int r = 0; r < 2; r++) {
            for (int i = 0; i < names.size(); i++) {
                QPointF hot;
                QImage image = M3Cursor::renderExtra(names[i], size, M3Cursor::schemeColours(), &hot);
                const QPoint at(i * cell + (cell - size) / 2, r * cell + (cell - size) / 2);
                p.drawImage(at, image);
                p.fillRect(QRectF(at + hot * size / M3Cursor::DesignSize - QPointF(1, 1), QSizeF(2, 2)), Qt::red);
            }
        }
        p.end();
        return sheet.save(args.at(1)) ? 0 : 1;
    }
    bool scheme = args.first() == QStringLiteral("--scheme");
    QString dir = scheme ? (args.size() > 1 ? args.at(1) : M3Cursor::userIconsDir()) : args.first();
    return M3Cursor::writeTheme(dir, scheme ? M3Cursor::schemeColours() : M3Cursor::neutralColours()) ? 0 : 1;
}
