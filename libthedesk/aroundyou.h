/****************************************
 *
 *   theDesk - the devices around you
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef AROUNDYOU_H
#define AROUNDYOU_H

#include "libthedesk_global.h"
#include <QObject>
#include <QPointF>
#include <functional>

class QPainter;

// A way to use a device, asked after it's dragged onto this computer (a
// display: extend, mirror...)
struct AroundYouChoice {
        QString label;
        std::function<void()> pick;
};

struct AroundYouDevice {
        QString id;     // stable: the same device gets the same id (EDID, address, serial...)
        QString symbol; // a Material Symbol
        QString name;
        QString state;
        std::function<void()> activate;     // clicking its row
        QString actionSymbol;               // an optional button at the end of the row
        std::function<void()> action;

        // In use (the sheet's inner ring, the bar's shapes) or only nearby
        bool connected = true;
        // Dragging it onto this computer / away from it in the sheet; a
        // device without them stays where it is
        std::function<void()> connect;
        std::function<void()> disconnect;
        QList<AroundYouChoice> choices; // asked instead of connect when set
        // Among the bar's shapes (false for what has a bar item of its own,
        // like the network)
        bool inBar = true;
};

struct AroundYouPrivate;

/**
 * The devices around you: displays, the network, Bluetooth devices, drives.
 * Plugins report them here; the bar shows their shapes and "Around you"
 * (the sheet, Super+A) lists them.
 *
 * Every device wears a shape of its own, chosen from its id: the same TV is
 * always the same shape, wherever it shows.
 */
class LIBTHEDESK_EXPORT AroundYou : public QObject {
        Q_OBJECT
    public:
        static AroundYou* instance();

        // Adds or updates a device. announce: it has just connected (not at
        // startup), so a snackbar says so, with Open
        void setDevice(const AroundYouDevice& device, bool announce = false);
        void removeDevice(const QString& id);
        QList<AroundYouDevice> devices() const;
        void activate(const QString& id);
        void triggerAction(const QString& id);

        void showSheet(const QString& highlight = QString());

        struct Look {
                const char* shape; // a MaterialShapes name
                int role;          // 0 Primary, 1 Secondary, 2 Tertiary (containers)
                double turn;       // degrees
        };
        static Look lookFor(const QString& id);
        static void paintShape(QPainter* painter, const QString& id, const QString& symbol, QPointF centre, double size, double opacity = 1, double spin = 0);

    signals:
        void devicesChanged();
        void sheetRequested(QString highlight);

    private:
        explicit AroundYou();
        AroundYouPrivate* d;
};

#endif // AROUNDYOU_H
