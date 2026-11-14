/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
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
#include "pluginitemdelegate.h"

#include <QPainter>
#include <QPainterPath>
#include <libcontemporary_global.h>
#include <m3scheme.h>
#include <statemanager.h>
#include <statuscentermanager.h>

namespace {
    // Android 16's segmented list: 72 dp two-line rows 2 dp apart, the
    // group's outer corners 20 and the inner ones 4
    constexpr int RowHeight = 72, Gap = 2;
    constexpr double Outer = 20, Inner = 4;

    QPainterPath tileShape(const QRectF& r, double top, double bottom) {
        QPainterPath path;
        path.moveTo(r.left() + top, r.top());
        path.arcTo(r.right() - 2 * top, r.top(), 2 * top, 2 * top, 90, -90);
        path.arcTo(r.right() - 2 * bottom, r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom, 0, -90);
        path.arcTo(r.left(), r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom, 270, -90);
        path.arcTo(r.left(), r.top(), 2 * top, 2 * top, 180, -90);
        path.closeSubpath();
        return path;
    }
} // namespace

struct PluginItemDelegatePrivate {
        M3::Scheme scheme;
        qint64 paletteKey = -1; // the scheme is read again only when the palette moves on
};

PluginItemDelegate::PluginItemDelegate(QObject* parent) :
    QAbstractItemDelegate(parent) {
    d = new PluginItemDelegatePrivate();
}

PluginItemDelegate::~PluginItemDelegate() {
    delete d;
}

void PluginItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (option.palette.cacheKey() != d->paletteKey) {
        d->paletteKey = option.palette.cacheKey();
        d->scheme = M3::currentScheme();
    }
    const M3::Scheme& s = d->scheme;
    const QString text = index.data().toString();
    const QString desc = index.data(Qt::UserRole).toString();
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    const bool active = index.data(Qt::UserRole + 2).toBool();
    const bool blacklisted = index.data(Qt::UserRole + 3).toBool();
    const bool rtl = option.direction == Qt::RightToLeft;
    const int rows = index.model()->rowCount(index.parent());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // The tile, in the content column like the other panes' groups
    const int width = qMin(option.rect.width(), StateManager::statusCenterManager()->preferredContentWidth());
    const QRectF tile(option.rect.left() + (option.rect.width() - width) / 2.0, option.rect.top(), width, option.rect.height() - Gap);
    const QPainterPath shape = tileShape(tile, index.row() == 0 ? Outer : Inner, index.row() == rows - 1 ? Outer : Inner);
    painter->fillPath(shape, s.surfaceContainerHigh);
    if (option.state & (QStyle::State_MouseOver | QStyle::State_Selected)) {
        QColor layer = s.onSurface;
        layer.setAlphaF(option.state & QStyle::State_Selected ? 0.10f : 0.08f);
        painter->fillPath(shape, layer);
    }

    // The plugin's symbol, crossed out when it's turned off
    const QRectF iconRect(rtl ? tile.right() - 16 - 24 : tile.left() + 16, tile.center().y() - 12, 24, 24);
    if (!icon.isNull()) {
        painter->setOpacity(active ? 1 : 0.38);
        painter->drawPixmap(iconRect.toRect(), icon.pixmap(QSize(24, 24), painter->device()->devicePixelRatioF()));
        painter->setOpacity(1);
    }
    if (blacklisted) {
        painter->setPen(QPen(s.onSurfaceVariant, 2, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(iconRect.topLeft(), iconRect.bottomRight());
    }

    // Name over its state (BodyLarge over BodyMedium), then the chevron
    const QRectF chevron(rtl ? tile.left() + 16 : tile.right() - 16 - 18, tile.center().y() - 9, 18, 18);
    const QRectF words = rtl ? QRectF(chevron.right() + 16, tile.top(), iconRect.left() - 16 - chevron.right() - 16, tile.height())
                             : QRectF(iconRect.right() + 16, tile.top(), chevron.left() - 16 - iconRect.right() - 16, tile.height());
    QFont title = option.font;
    title.setPixelSize(16);
    QFont supporting = option.font;
    supporting.setPixelSize(14);
    const Qt::Alignment side = rtl ? Qt::AlignRight : Qt::AlignLeft;
    painter->setFont(title);
    painter->setPen(active ? s.onSurface : s.onSurfaceVariant);
    painter->drawText(QRectF(words.left(), words.top(), words.width(), words.height() / 2 + 2), side | Qt::AlignBottom, QFontMetrics(title).elidedText(text, Qt::ElideRight, int(words.width())));
    painter->setFont(supporting);
    painter->setPen(s.onSurfaceVariant);
    painter->drawText(QRectF(words.left(), words.center().y() + 2, words.width(), words.height() / 2 - 2), side | Qt::AlignTop, QFontMetrics(supporting).elidedText(desc, Qt::ElideRight, int(words.width())));
    const QIcon next = QIcon::fromTheme(rtl ? QStringLiteral("go-previous") : QStringLiteral("go-next"));
    painter->drawPixmap(chevron.toRect(), next.pixmap(QSize(18, 18), painter->device()->devicePixelRatioF()));

    painter->restore();
}

QSize PluginItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(StateManager::statusCenterManager()->preferredContentWidth(), RowHeight + Gap);
}
