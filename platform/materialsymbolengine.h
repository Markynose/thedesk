/****************************************
 *
 *   theDesk - Material Symbols icon engine
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef MATERIALSYMBOLENGINE_H
#define MATERIALSYMBOLENGINE_H

#include <QIconEngine>

/**
 * Freedesktop symbolic and action icons drawn from the Material Symbols
 * Rounded variable font (ttf-material-symbols-variable-git): the font's
 * ligatures turn a symbol's name into its glyph. Icons in the On state (a
 * checked button) use the filled glyph (FILL 1), as M3 does; the optical
 * size axis follows the icon's size. Colour comes from the application
 * palette (WindowText, HighlightedText when selected, disabled WindowText).
 *
 * Full-colour icons (apps, files) stay with the icon theme.
 * Platform/icons.symbols=theme in theDesk.platform turns this off.
 */
class MaterialSymbolEngine : public QIconEngine {
    public:
        // The Material Symbols name for a freedesktop icon name, or empty
        static QString symbolFor(const QString& iconName);
        static bool enabled();

        MaterialSymbolEngine(const QString& iconName, const QString& symbol);

        void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
        QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;
        QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override;
        QSize actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) override;
        QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override;
        QString key() const override;
        QString iconName() override;
        QIconEngine* clone() const override;
        bool isNull() override;

    private:
        QString name;
        QString symbol;
};

#endif // MATERIALSYMBOLENGINE_H
