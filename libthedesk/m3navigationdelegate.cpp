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
#include "m3navigationdelegate.h"

#include <QAbstractItemView>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QTimer>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include <libcontemporary_global.h>
#include <m3scheme.h>

// theDesk M3: a navigation list row in the manner of Android Settings - an
// inset pill (SecondaryContainer when selected, a state layer on hover), the
// page's symbol in a tonal circle, the label, and a chevron for sub-pages.

namespace {
    constexpr int rowHeight = 52;
    constexpr int circle = 36;

    QPixmap tinted(const QIcon& icon, int size, qreal dpr, QColor colour) {
        QPixmap pixmap = icon.pixmap(QSize(size, size), dpr);
        QPainter p(&pixmap);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pixmap.rect(), colour);
        return pixmap;
    }

    QRectF pillRect(const QRect& row) {
        return QRectF(row).adjusted(8, 2, -8, -2);
    }

    // Step response of an M3 spring (0 at t <= 0; t in seconds)
    double spring(double t, double damping, double stiffness) {
        if (t <= 0) return 0;
        const double w = std::sqrt(stiffness);
        if (damping >= 1) return 1 - std::exp(-w * t) * (1 + w * t);
        const double wd = w * std::sqrt(1 - damping * damping);
        return 1 - std::exp(-damping * w * t) * (std::cos(wd * t) + damping * w / wd * std::sin(wd * t));
    }

    /**
     * The selected row's pill, sliding: one pill behind the rows that springs
     * from the old selection to the new one. Its leading edge goes ahead on
     * the expressive fast spring and the trailing edge follows on the default
     * one, so it stretches as it travels and settles back into a pill.
     */
    class PillSlide : public QObject {
        public:
            static void attach(QAbstractItemView* view) {
                if (!view || view->property("m3PillSlide").toBool()) return;
                view->setProperty("m3PillSlide", true);
                new PillSlide(view);
            }

            bool eventFilter(QObject* watched, QEvent* event) override {
                if (watched == view->viewport() && event->type() == QEvent::Paint) {
                    hook();
                    paintPill();
                }
                return false;
            }

        private:
            explicit PillSlide(QAbstractItemView* view) :
                QObject(view), view(view) {
                view->viewport()->installEventFilter(this);
                frame.setInterval(16);
                frame.setTimerType(Qt::PreciseTimer);
                QObject::connect(&frame, &QTimer::timeout, this, [this] {
                    if (clock.elapsed() > 900) frame.stop();
                    this->view->viewport()->update();
                });
                hook();
                current = selected();
                view->viewport()->update();
            }

            QModelIndex selected() const {
                if (!view->selectionModel()) return {};
                const QModelIndexList list = view->selectionModel()->selectedIndexes();
                return list.isEmpty() ? QModelIndex() : list.first();
            }

            void hook() {
                // The model (and with it the selection model) can be replaced
                if (view->selectionModel() == hooked) return;
                hooked = view->selectionModel();
                if (!hooked) return;
                QObject::connect(hooked, &QItemSelectionModel::selectionChanged, this, [this] {
                    const QModelIndex now = selected();
                    if (QPersistentModelIndex(now) == current) return;
                    previous = current;
                    current = now;
                    clock.start();
                    frame.start();
                    view->viewport()->update();
                });
            }

            void paintPill() {
                if (!current.isValid()) return;
                const QRectF to = pillRect(view->visualRect(current));
                QRectF rect = to;
                if (previous.isValid() && clock.isValid() && clock.elapsed() < 900) {
                    const QRectF from = pillRect(view->visualRect(previous));
                    const double t = clock.elapsed() / 1000.0;
                    const double lead = spring(t, 0.6, 800);  // SpatialFast (expressive)
                    const double trail = spring(t, 0.8, 380); // SpatialDefault
                    const bool down = to.top() >= from.top();
                    const double top = from.top() + (to.top() - from.top()) * (down ? trail : lead);
                    const double bottom = from.bottom() + (to.bottom() - from.bottom()) * (down ? lead : trail);
                    rect = QRectF(to.left(), qMin(top, bottom - 4), to.width(), qMax(4.0, bottom - top));
                }
                QPainter painter(view->viewport());
                painter.setRenderHint(QPainter::Antialiasing);
                QPainterPath shape;
                const double radius = qMin(rect.height(), to.height()) / 2;
                shape.addRoundedRect(rect, radius, radius);
                painter.fillPath(shape, M3::currentScheme().secondaryContainer);
            }

            QAbstractItemView* view;
            QPointer<QItemSelectionModel> hooked;
            QPersistentModelIndex current, previous;
            QElapsedTimer clock;
            QTimer frame;
    };
} // namespace

M3NavigationDelegate::M3NavigationDelegate(QObject* parent) :
    QStyledItemDelegate(parent) {
}

QSize M3NavigationDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    return QSize(size.width() + circle, rowHeight);
}

void M3NavigationDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const M3::Scheme s = M3::currentScheme();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool rtl = option.direction == Qt::RightToLeft;
    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // The pill
    QRectF pill = QRectF(option.rect).adjusted(8, 2, -8, -2);
    QPainterPath shape;
    shape.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
    // The selected pill slides between rows (PillSlide, behind the rows)
    auto* view = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(option.widget));
    PillSlide::attach(view);
    if (selected && !(view && view->property("m3PillSlide").toBool())) painter->fillPath(shape, s.secondaryContainer);
    if (hovered) {
        QColor layer = selected ? s.onSecondaryContainer : s.onSurface;
        layer.setAlphaF(0.08);
        painter->fillPath(shape, layer);
    }

    // The symbol in a tonal circle; colours take turns down the list
    static const int roles = 3;
    const int role = index.row() % roles;
    // (the selected row's circle turns Primary so it stands out from the pill)
    const QColor circleColour = selected ? s.primary : role == 0 ? s.primaryContainer : role == 1 ? s.tertiaryContainer : s.secondaryContainer;
    const QColor symbolColour = selected ? s.onPrimary : role == 0 ? s.onPrimaryContainer : role == 1 ? s.onTertiaryContainer : s.onSecondaryContainer;
    QRectF dot(rtl ? pill.right() - 8 - circle : pill.left() + 8, pill.center().y() - circle / 2.0, circle, circle);
    QPainterPath circlePath;
    circlePath.addEllipse(dot);
    painter->fillPath(circlePath, circleColour);

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!icon.isNull()) {
        const int size = 20;
        QPointF at(dot.center().x() - size / 2.0, dot.center().y() - size / 2.0);
        QIcon symbolic = icon.name().isEmpty() ? QIcon() : QIcon::fromTheme(icon.name() + QStringLiteral("-symbolic"));
        if (!symbolic.isNull()) {
            painter->drawPixmap(at, tinted(symbolic, size, dpr, symbolColour));
        } else {
            painter->drawPixmap(at, icon.pixmap(QSize(size, size), dpr));
        }
    } else {
        // No icon: the label's first letter
        QFont initial = option.font;
        initial.setPixelSize(16);
        initial.setWeight(QFont::DemiBold);
        painter->setFont(initial);
        painter->setPen(symbolColour);
        painter->drawText(dot, Qt::AlignCenter, index.data(Qt::DisplayRole).toString().left(1).toUpper());
    }

    // Label and chevron
    const bool hasChildren = index.data(Qt::UserRole).toBool();
    QRectF text = pill.adjusted(8 + circle + 12, 0, -(hasChildren ? 36 : 12), 0);
    if (rtl) text = pill.adjusted(hasChildren ? 36 : 12, 0, -(8 + circle + 12), 0);
    QFont label = option.font;
    label.setPixelSize(15);
    label.setWeight(selected ? QFont::DemiBold : QFont::Medium);
    painter->setFont(label);
    painter->setPen(selected ? s.onSecondaryContainer : s.onSurface);
    painter->drawText(text, Qt::AlignVCenter | (rtl ? Qt::AlignRight : Qt::AlignLeft),
        QFontMetrics(label).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(text.width())));

    if (hasChildren) {
        QIcon chevron = QIcon::fromTheme(rtl ? QStringLiteral("go-previous") : QStringLiteral("go-next"));
        const int size = 18;
        QPointF at(rtl ? pill.left() + 12 : pill.right() - 12 - size, pill.center().y() - size / 2.0);
        QIcon materialChevron = QIcon::fromTheme(rtl ? QStringLiteral("pan-start-symbolic") : QStringLiteral("pan-end-symbolic"));
        painter->drawPixmap(at, tinted(materialChevron.isNull() ? chevron : materialChevron, size, dpr, s.onSurfaceVariant));
    }
    painter->restore();
}
