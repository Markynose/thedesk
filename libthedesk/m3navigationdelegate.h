/****************************************
 *
 *   theDesk - Material 3 navigation list rows
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3NAVIGATIONDELEGATE_H
#define M3NAVIGATIONDELEGATE_H

#include "libthedesk_global.h"
#include <QStyledItemDelegate>

/**
 * Side-list rows in the manner of Android Settings: an inset pill
 * (SecondaryContainer when selected, a state layer on hover), the item's
 * symbol in a tonal circle (Primary when selected), the label, and a chevron
 * when Qt::UserRole is true (the item opens a sub-list).
 */
class LIBTHEDESK_EXPORT M3NavigationDelegate : public QStyledItemDelegate {
        Q_OBJECT
    public:
        explicit M3NavigationDelegate(QObject* parent = nullptr);

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

#endif // M3NAVIGATIONDELEGATE_H
