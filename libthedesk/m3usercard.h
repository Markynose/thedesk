/****************************************
 *
 *   theDesk - Material 3 Expressive user card
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3USERCARD_H
#define M3USERCARD_H

#include "libthedesk_global.h"
#include <QRectF>
#include <QWidget>

namespace m3shapes {
    class RoundedPolygon;
}

struct M3UserCardPrivate;

/**
 * The user's picture in an M3E shape, a time-of-day greeting and their name
 * in heavy, wide Roboto Flex. Each time it is shown, the picture morphs to
 * the next shape and the name springs in (expressive motion).
 *
 * Picture: ~/.face (or AccountsService's icon); falls back to the initial.
 */
class LIBTHEDESK_EXPORT M3UserCard : public QWidget {
        Q_OBJECT
    public:
        enum CardSize {
            Compact, // 72 dp picture, 36 dp name
            Large,   // 96 dp picture, 52 dp name (Gateway)
            Hero     // 128 dp picture, 72 dp name (lock screen)
        };

        explicit M3UserCard(QWidget* parent = nullptr);
        ~M3UserCard() override;

        void setCardSize(CardSize size);
        void setUser(QString displayName, QString picturePath = QString()); // default: the current user
        void setGreeting(QString greeting);                                 // default: by time of day
        void setGreetingShown(bool shown);                                  // false: just the picture and name
        void setNameSuffix(QString suffix);                                 // after the name, in its type ("?")
        void setAlignment(Qt::Alignment alignment);                         // Qt::AlignLeft or Qt::AlignHCenter

        void playEntrance();
        QRectF pictureRect() const; // where the picture is drawn, in this widget
        const m3shapes::RoundedPolygon& pictureShape() const; // the shape it settles in

        static QString currentUserPicture();
        static QString greetingForNow();

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:
        M3UserCardPrivate* d;
};

#endif // M3USERCARD_H
