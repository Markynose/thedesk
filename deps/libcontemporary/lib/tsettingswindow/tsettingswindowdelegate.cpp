/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2022 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "tsettingswindowdelegate.h"

#include "libcontemporary_global.h"
#include <QPainter>

tSettingsWindowDelegate::tSettingsWindowDelegate(QObject* parent) :
    QStyledItemDelegate{parent} {
}

void tSettingsWindowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (index.data(Qt::UserRole + 1).value<QObject*>()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    painter->save();

    if (index.data().toString().isEmpty()) {
        painter->fillRect(option.rect, option.palette.color(QPalette::WindowText));
    } else {
        auto font = option.font;
        font.setBold(true);
        painter->setFont(font);

        auto textRect = option.rect;
        textRect.setHeight(painter->fontMetrics().height());
        textRect.setWidth(textRect.width() - 6);
        textRect.moveCenter(option.rect.center());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, index.data().toString().toUpper());
    }

    painter->restore();
}

QSize tSettingsWindowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto sh = QStyledItemDelegate::sizeHint(option, index);
    if (!index.data(Qt::UserRole + 1).value<QObject*>() && index.data().toString().isEmpty()) {
        sh.setHeight(1);
    }
    return sh;
}
