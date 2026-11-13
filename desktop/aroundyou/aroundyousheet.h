/****************************************
 *
 *   theDesk - Around you: the sheet, the bar's shapes, USB drives
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef AROUNDYOUSHEET_H
#define AROUNDYOUSHEET_H

#include <QWidget>
#include <chunk.h>
#include <m3scheme.h>

struct AroundYouSheetPrivate;

// The sheet's contents: this computer in the middle, what's in use on the
// inner ring joined to it, what's nearby on the outer ring. Drag a device
// onto the computer to connect it, away to let it go; click it to open it
class AroundYouSheet : public QWidget {
        Q_OBJECT
    public:
        explicit AroundYouSheet(QString highlight, QWidget* parent = nullptr);
        ~AroundYouSheet() override;

        int preferredHeight() const;

        // Opens it as a bottom sheet (AroundYou::sheetRequested, Super+A)
        static void open(QString highlight);
        // Connects AroundYou, Super+A, the bar's shapes and the USB drives
        static void setUp();

    signals:
        void done();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        bool focusNextPrevChild(bool next) override;

    private:
        AroundYouSheetPrivate* d;

        void devicesChanged();
        void tick();
        QPointF centre() const;
        double choicesTop() const;
        bool isConnected(int index) const; // as shown: what it's becoming
        bool canMove(int index) const;     // to the other ring
        QList<int> order() const;
        QPointF home(int index) const; // its place on its ring
        QString stateOf(int index) const;
        QRectF labelRect(int index, QPointF at, bool outward) const;
        void relayout();
        int deviceAt(QPointF pos) const;
        QList<QRectF> choiceRects() const;
        int choiceAt(QPointF pos) const;
        void connectDevice(int index);
        void disconnectDevice(int index);
        void pickChoice(int choice);
        void cancelChoices();
        void snapLink(const QString& id);
        void paintDrops(QPainter* painter, const M3::Scheme& s, int index) const;
};

struct AroundYouChunkPrivate;

// The devices' shapes in the bar; clicking them opens the sheet
class AroundYouChunk : public Chunk {
        Q_OBJECT
    public:
        AroundYouChunk();
        ~AroundYouChunk() override;

        QString name() override;
        int expandedHeight() override;
        int statusBarHeight() override;
        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        AroundYouChunkPrivate* d;
};

#endif // AROUNDYOUSHEET_H
