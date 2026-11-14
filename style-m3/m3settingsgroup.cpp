/****************************************
 *
 *   theDesk - Material 3 Expressive widget style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Android 16's segmented settings groups over Designer forms (see the
 *   header). Sizes are ListItem's: one line 56 dp, two lines 72 dp.
 *
 * *************************************/
#include "m3settingsgroup.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QBoxLayout>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHoverEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <m3motion.h>

namespace {
    constexpr double Outer = 20, Inner = 4; // the group's outer corners, the inner ones
    constexpr int Inset = 16, OneLine = 56, TwoLine = 72, IconIndent = 40;

    QPainterPath tileShape(const QRectF& r, double top, double bottom) {
        const double m = qMin(r.width(), r.height()) / 2;
        top = qMin(top, m);
        bottom = qMin(bottom, m);
        QPainterPath path;
        path.moveTo(r.left() + top, r.top());
        path.lineTo(r.right() - top, r.top());
        path.arcTo(r.right() - 2 * top, r.top(), 2 * top, 2 * top, 90, -90);
        path.lineTo(r.right(), r.bottom() - bottom);
        path.arcTo(r.right() - 2 * bottom, r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom, 0, -90);
        path.lineTo(r.left() + bottom, r.bottom());
        path.arcTo(r.left(), r.bottom() - 2 * bottom, 2 * bottom, 2 * bottom, 270, -90);
        path.lineTo(r.left(), r.top() + top);
        path.arcTo(r.left(), r.top(), 2 * top, 2 * top, 180, -90);
        path.closeSubpath();
        return path;
    }

    QColor alpha(QColor c, double a) {
        c.setAlphaF(c.alphaF() * a);
        return c;
    }

    bool isHeader(const QWidget* w) {
        auto* label = qobject_cast<const QLabel*>(w);
        if (!label) return false;
        if (label->property("m3Header").toBool() || label->inherits("tSubtitleLabel")) return true;
        if (!label->font().bold()) return false;
        bool letter = false;
        for (QChar c : label->text()) {
            if (c.isLower()) return false;
            letter = letter || c.isLetter();
        }
        return letter;
    }

    bool isValueLabel(const QWidget* w) {
        return w && w->objectName() == QLatin1String("m3RowValue");
    }

    // A widget someone works (as opposed to a label or a plain container)
    bool isControl(const QWidget* w) {
        if (!w || isValueLabel(w)) return false;
        if (w->parentWidget() && (qobject_cast<QComboBox*>(w->parentWidget()) || qobject_cast<QAbstractSpinBox*>(w->parentWidget()))) return false;
        return qobject_cast<const QComboBox*>(w) || qobject_cast<const QAbstractButton*>(w) || qobject_cast<const QAbstractSpinBox*>(w) || qobject_cast<const QAbstractSlider*>(w) || qobject_cast<const QLineEdit*>(w);
    }

    void widgetsOf(QLayoutItem* item, QList<QWidget*>& out) {
        if (QWidget* w = item->widget()) {
            out.append(w);
        } else if (QLayout* l = item->layout()) {
            for (int i = 0; i < l->count(); i++) widgetsOf(l->itemAt(i), out);
        }
    }

    // A row's widgets, looking into plain containers (a row widget with its own layout)
    QList<QWidget*> partsOf(const QList<QLayoutItem*>& items) {
        QList<QWidget*> direct, out;
        for (QLayoutItem* item : items) widgetsOf(item, direct);
        for (QWidget* w : std::as_const(direct)) {
            if (isValueLabel(w)) continue;
            const bool plain = !isControl(w) && !qobject_cast<QLabel*>(w) && w->layout();
            if (!plain) {
                out.append(w);
                continue;
            }
            for (QWidget* child : w->findChildren<QWidget*>()) {
                if (child->isVisibleTo(w) && (isControl(child) || qobject_cast<QLabel*>(child))) out.append(child);
            }
        }
        return out;
    }

    bool visibleIn(QLayoutItem* item, const QWidget* container) {
        QList<QWidget*> ws;
        widgetsOf(item, ws);
        for (QWidget* w : std::as_const(ws)) {
            if (!isValueLabel(w) && w->isVisibleTo(container)) return true;
        }
        return false;
    }

    // Vertical box items, with nested vertical boxes opened out
    void flatten(QLayout* layout, QList<QLayoutItem*>& out) {
        for (int i = 0; i < layout->count(); i++) {
            QLayoutItem* item = layout->itemAt(i);
            auto* box = qobject_cast<QBoxLayout*>(item->layout());
            if (box && (box->direction() == QBoxLayout::TopToBottom || box->direction() == QBoxLayout::BottomToTop)) {
                if (box->spacing() != 2) box->setSpacing(2);
                flatten(box, out);
            } else {
                out.append(item);
            }
        }
    }

    enum class Kind {
        Header,
        Text,
        Free,
        Tile
    };

    Kind kindOf(const QList<QLayoutItem*>& items, bool spansAll) {
        QList<QWidget*> ws;
        for (QLayoutItem* item : items) widgetsOf(item, ws);
        for (QWidget* w : std::as_const(ws)) {
            const QVariant forced = w->property("m3Tile");
            if (forced.isValid()) return forced.toBool() ? Kind::Tile : Kind::Free;
        }
        ws.removeIf(isValueLabel);
        if (items.size() == 1 && ws.size() == 1 && items.first()->widget()) {
            QWidget* w = ws.first();
            if (qobject_cast<QLabel*>(w)) return isHeader(w) ? Kind::Header : Kind::Text;
            if (qobject_cast<QAbstractItemView*>(w)) return Kind::Free;
            if (auto* frame = qobject_cast<QFrame*>(w); frame && (frame->frameShape() == QFrame::HLine || frame->frameShape() == QFrame::VLine)) return Kind::Free;
        }
        // a card across the group (a preview, a demo); a row laid out across it is still a row
        auto* across = items.size() == 1 ? qobject_cast<QBoxLayout*>(items.first()->layout()) : nullptr;
        if (items.size() == 1 && spansAll && !(across && (across->direction() == QBoxLayout::LeftToRight || across->direction() == QBoxLayout::RightToLeft))) return Kind::Free;
        return Kind::Tile;
    }

    struct Tile {
            QList<int> rows;           // grid rows
            QList<QLayoutItem*> items; // box items (only while classifying)
            QList<QPointer<QObject>> covers; // the widgets and layouts it covers, for painting
            bool joinsPrevious = false, joinsNext = false;
            QPointer<QWidget> title, control, value;
            QPointer<QLabel> section;
            int controls = 0;
            QString icon;

            bool activatable() const {
                return control && controls == 1 && control->isEnabled() && !qobject_cast<QAbstractSlider*>(control.data());
            }
    };

    class Group : public QObject {
        public:
            Group(QWidget* container, M3SettingsGroup::SchemeFor scheme) :
                QObject(container), c(container), schemeFor(std::move(scheme)) {
                c->setAttribute(Qt::WA_Hover);
                c->installEventFilter(this);
                flash = new M3SpringAnimation(this);
                flash->setToken(M3Motion::EffectsDefault);
                flash->snapTo(0.0);
                connect(flash, &M3SpringAnimation::valueChanged, c, qOverload<>(&QWidget::update));
                refresh();
            }

        protected:
            bool eventFilter(QObject* watched, QEvent* event) override {
                if (watched != c) {
                    // a row combo repaints whenever its value changes
                    auto* combo = qobject_cast<QComboBox*>(watched);
                    auto* value = combo ? qobject_cast<QLabel*>(combo->property("m3RowValue").value<QObject*>()) : nullptr;
                    if (event->type() == QEvent::Paint && value && value->text() != combo->currentText()) value->setText(combo->currentText());
                    return false;
                }
                switch (event->type()) {
                    case QEvent::LayoutRequest:
                    case QEvent::Show:
                    case QEvent::PaletteChange:
                        refresh();
                        break;
                    case QEvent::Resize:
                        holdHeight();
                        break;
                    case QEvent::Paint:
                        paint();
                        break;
                    case QEvent::HoverMove:
                    case QEvent::HoverEnter:
                        setHovered(tileAt(static_cast<QHoverEvent*>(event)->position()));
                        break;
                    case QEvent::HoverLeave:
                        setHovered(-1);
                        break;
                    case QEvent::MouseButtonPress: {
                        auto* e = static_cast<QMouseEvent*>(event);
                        if (e->button() != Qt::LeftButton) break;
                        const int at = tileAt(e->position());
                        if (at < 0 || !tiles.at(at).activatable()) break;
                        pressed = at;
                        c->update();
                        return true;
                    }
                    case QEvent::MouseButtonRelease: {
                        auto* e = static_cast<QMouseEvent*>(event);
                        if (pressed < 0) break;
                        const int was = pressed;
                        pressed = -1;
                        c->update();
                        if (tileAt(e->position()) == was) activate(was);
                        return true;
                    }
                    case QEvent::DynamicPropertyChange:
                        if (static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() == "m3Flash") pulse(c->property("m3Flash").value<QObject*>());
                        break;
                    default:
                        break;
                }
                return false;
            }

        private:
            QWidget* c;
            M3SettingsGroup::SchemeFor schemeFor;
            QList<Tile> tiles;
            int hovered = -1, pressed = -1, flashing = -1;
            M3SpringAnimation* flash;
            bool refreshing = false;

            QGridLayout* grid() const {
                return qobject_cast<QGridLayout*>(c->layout());
            }

            QRectF rectOf(const Tile& t) const {
                // what its rows' items cover (a grid's cellRect is stale with height-for-width rows)
                QRect r;
                for (const QPointer<QObject>& cover : t.covers) {
                    if (auto* w = qobject_cast<QWidget*>(cover.data()); w && w->isVisible()) r |= w->geometry();
                    else if (auto* l = qobject_cast<QLayout*>(cover.data())) r |= l->geometry();
                }
                if (r.isEmpty()) return {};
                return QRectF(0, r.top(), c->width(), r.height());
            }

            int tileAt(QPointF pos) const {
                for (int i = 0; i < tiles.size(); i++) {
                    if (rectOf(tiles.at(i)).contains(pos)) return i;
                }
                return -1;
            }

            void setHovered(int at) {
                if (at >= 0 && !tiles.at(at).activatable()) at = -1;
                if (at == hovered) return;
                hovered = at;
                c->update();
            }

            void activate(int at) {
                QWidget* w = tiles.at(at).control;
                if (!w) return;
                if (auto* combo = qobject_cast<QComboBox*>(w)) {
                    combo->setFocus(Qt::MouseFocusReason);
                    combo->showPopup();
                } else if (auto* button = qobject_cast<QAbstractButton*>(w)) {
                    button->click();
                } else {
                    w->setFocus(Qt::MouseFocusReason);
                }
            }

            void pulse(QObject* target) {
                auto* w = qobject_cast<QWidget*>(target);
                if (!w) return;
                flashing = -1;
                for (int i = 0; i < tiles.size() && flashing < 0; i++) {
                    for (QWidget* part : {tiles.at(i).title.data(), tiles.at(i).control.data(), tiles.at(i).value.data()}) {
                        if (part && (part == w || part->isAncestorOf(w) || w->isAncestorOf(part))) flashing = i;
                    }
                }
                if (flashing < 0) return;
                // Twice, like Android's highlighted search result
                QPointer<M3SpringAnimation> f = flash;
                const QList<QPair<int, double>> steps = {{0, 1.0}, {380, 0.0}, {640, 1.0}, {1020, 0.0}};
                for (const auto& step : steps) {
                    QTimer::singleShot(step.first, this, [f, step] {
                        if (f) f->animateTo(step.second);
                    });
                }
            }

            // A layout centring this group asks its height for its own width, not
            // the group's, and squeezes the rows: the group holds its real height
            void holdHeight() {
                QLayout* layout = c->layout();
                if (!layout || !layout->hasHeightForWidth() || c->width() <= 0) return;
                const int h = layout->totalHeightForWidth(c->width());
                if (c->minimumHeight() != h) c->setMinimumHeight(h);
            }

            const M3::Scheme& scheme() const {
                return schemeFor(c->palette());
            }

            void colour(QWidget* w, const QColor& ink) {
                QPalette pal = w->palette();
                if (pal.color(QPalette::Active, QPalette::WindowText) == ink) return;
                for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, ink);
                w->setPalette(pal);
            }

            void pixelSize(QWidget* w, int px) {
                if (w->font().pixelSize() == px) return;
                QFont f = w->font();
                f.setPixelSize(px);
                w->setFont(f);
            }

            void refresh() {
                if (refreshing || !c->layout()) return;
                refreshing = true;
                tiles.clear();
                QLayout* layout = c->layout();
                layout->setContentsMargins(Inset, 4, Inset, 12);
                const M3::Scheme& s = scheme();

                Kind last = Kind::Free;
                QPointer<QLabel> section;
                auto take = [&](Kind kind, const QList<int>& rows, const QList<QLayoutItem*>& items) {
                    QList<QWidget*> parts = partsOf(items);
                    if (kind == Kind::Header) {
                        section = qobject_cast<QLabel*>(parts.value(0));
                    } else if (kind == Kind::Text && last == Kind::Tile) {
                        // right under a tile: its supporting text
                        Tile& t = tiles.last();
                        t.rows += rows;
                        t.items += items;
                        for (QWidget* w : std::as_const(parts)) {
                            if (!w->isEnabled() && qobject_cast<QLabel*>(w)) w->setEnabled(true); // greyed out in the form, it's plain supporting text now
                            pixelSize(w, 14);
                            colour(w, s.onSurfaceVariant);
                            w->setContentsMargins(t.icon.isEmpty() ? 0 : IconIndent, 0, 0, 12);
                        }
                        return; // still a tile
                    } else if (kind == Kind::Text || kind == Kind::Free) {
                        for (QWidget* w : std::as_const(parts)) {
                            if (qobject_cast<QLabel*>(w)) colour(w, s.onSurfaceVariant);
                        }
                    } else if (kind == Kind::Tile) {
                        Tile t;
                        t.rows = rows;
                        t.items = items;
                        t.section = section;
                        t.joinsPrevious = last == Kind::Tile;
                        if (t.joinsPrevious) tiles.last().joinsNext = true;
                        for (QWidget* w : std::as_const(parts)) {
                            if (isControl(w)) {
                                t.controls++;
                                if (!t.control) t.control = w;
                            } else if (auto* label = qobject_cast<QLabel*>(w)) {
                                if (!t.title) t.title = label;
                                else if (!t.value) t.value = label;
                            }
                        }
                        if (!t.title) t.title = t.control; // a button or a radio is its own title
                        if (auto* combo = qobject_cast<QComboBox*>(t.control.data()); combo && !t.value) t.value = combo;
                        if (t.title) t.icon = t.title->property("m3Icon").toString();
                        tiles.append(t);
                    }
                    last = kind;
                };

                if (QGridLayout* g = grid()) {
                    if (g->verticalSpacing() != 2) g->setVerticalSpacing(2);
                    QList<QMap<int, QLayoutItem*>> rows(g->rowCount()); // by column: the title is the leftmost label
                    QList<bool> spans(g->rowCount(), false);
                    for (int i = 0; i < g->count(); i++) {
                        int row, col, rowSpan, colSpan;
                        g->getItemPosition(i, &row, &col, &rowSpan, &colSpan);
                        if (row < 0 || !visibleIn(g->itemAt(i), c)) continue;
                        rows[row].insert(col * 10000 + i, g->itemAt(i));
                        if (colSpan >= g->columnCount() && g->columnCount() > 1) spans[row] = true;
                    }
                    for (int r = 0; r < rows.size(); r++) {
                        if (rows.at(r).isEmpty()) continue;
                        const QList<QLayoutItem*> items = rows.at(r).values();
                        take(kindOf(items, spans.at(r)), {r}, items);
                    }
                } else {
                    if (layout->spacing() != 2) layout->setSpacing(2);
                    QList<QLayoutItem*> items;
                    flatten(layout, items);
                    for (QLayoutItem* item : std::as_const(items)) {
                        if (item->spacerItem() || !visibleIn(item, c)) continue;
                        take(kindOf({item}, false), {}, {item});
                    }
                }

                for (Tile& t : tiles) {
                    shapeTile(t, s);
                    for (QLayoutItem* item : std::as_const(t.items)) {
                        if (item->widget()) t.covers.append(item->widget());
                        else if (item->layout()) t.covers.append(item->layout());
                    }
                    t.items.clear(); // layout items can go with their widgets: hold nothing raw
                }
                // A value under its title goes when its row does
                for (QLabel* value : c->findChildren<QLabel*>(QStringLiteral("m3RowValue"), Qt::FindDirectChildrenOnly)) {
                    auto* combo = qobject_cast<QWidget*>(value->property("m3RowValueOf").value<QObject*>());
                    const bool shown = combo && combo->isVisibleTo(c);
                    if (value->isHidden() == shown) value->setVisible(shown);
                }
                if (QGridLayout* g = grid()) {
                    // Only the tiles' rows keep a height of their own (hidden rows fold away)
                    QList<int> heights(g->rowCount(), 0);
                    for (const Tile& t : std::as_const(tiles)) {
                        const int height = t.rows.size() > 1 ? 48 : rowCombo(t) ? TwoLine : OneLine;
                        heights[t.rows.first()] = height;
                        // the title holds the row open too: a grid with height-for-width rows (wrapped text) forgets row minimums
                        if (auto* label = qobject_cast<QLabel*>(t.title.data()); label && label->minimumHeight() != height) label->setMinimumHeight(height);
                    }
                    for (int r = 0; r < heights.size(); r++) {
                        if (g->rowMinimumHeight(r) != heights.at(r)) g->setRowMinimumHeight(r, heights.at(r));
                    }
                }

                QVariantList published;
                for (const Tile& t : std::as_const(tiles)) {
                    published.append(QVariantMap{
                        {QStringLiteral("title"), QVariant::fromValue<QObject*>(t.title)},
                        {QStringLiteral("control"), QVariant::fromValue<QObject*>(t.control)},
                        {QStringLiteral("value"), QVariant::fromValue<QObject*>(t.value)},
                        {QStringLiteral("section"), QVariant::fromValue<QObject*>(t.section)}
                    });
                }
                c->setProperty("m3Tiles", published);
                holdHeight();
                if (hovered >= tiles.size()) hovered = -1;
                if (pressed >= tiles.size()) pressed = -1;
                c->update();
                refreshing = false;
            }

            // A title label and one combo box, nothing else: the value goes under the title
            bool rowCombo(const Tile& t) const {
                auto* combo = qobject_cast<QComboBox*>(t.control.data());
                return grid() && combo && t.controls == 1 && t.rows.size() == 1 && qobject_cast<QLabel*>(t.title.data()) && !combo->isEditable();
            }

            void shapeTile(Tile& t, const M3::Scheme& s) {
                const int indent = t.icon.isEmpty() ? 0 : IconIndent;
                if (auto* label = qobject_cast<QLabel*>(t.title.data())) {
                    pixelSize(label, 16); // BodyLarge
                    colour(label, s.onSurface);
                    label->setContentsMargins(indent, rowCombo(t) ? 13 : 0, 0, 0);
                    const Qt::Alignment align = Qt::AlignLeft | (rowCombo(t) ? Qt::AlignTop : Qt::AlignVCenter);
                    if (label->alignment() != align) label->setAlignment(align);
                }
                if (auto* value = qobject_cast<QLabel*>(t.value.data()); value && value != t.title) colour(value, s.onSurfaceVariant);
                if (rowCombo(t)) {
                    auto* combo = qobject_cast<QComboBox*>(t.control.data());
                    auto* label = qobject_cast<QLabel*>(t.title.data());
                    auto* value = qobject_cast<QLabel*>(combo->property("m3RowValue").value<QObject*>());
                    if (!value) {
                        value = new QLabel(c);
                        value->setObjectName(QStringLiteral("m3RowValue"));
                        grid()->addWidget(value, t.rows.first(), 0, Qt::AlignLeft | Qt::AlignBottom);
                        grid()->setAlignment(label, Qt::AlignLeft | Qt::AlignTop);
                        grid()->setAlignment(combo, Qt::AlignRight | Qt::AlignVCenter);
                        combo->installEventFilter(this); // its value follows it even when its signals are blocked
                        value->setText(combo->currentText());
                        connect(combo, &QComboBox::currentTextChanged, value, &QLabel::setText);
                        combo->setProperty("m3RowValue", QVariant::fromValue<QObject*>(value));
                        value->setProperty("m3RowValueOf", QVariant::fromValue<QObject*>(combo));
                        combo->setProperty("m3RowCombo", true);
                        combo->setFixedSize(40, 40);
                        combo->update();
                    }
                    pixelSize(value, 14);
                    colour(value, s.onSurfaceVariant);
                    value->setContentsMargins(indent, 0, 0, 13);
                    t.value = combo;
                }
                for (QLayoutItem* item : std::as_const(t.items)) {
                    if (grid()) break; // box rows only
                    QWidget* w = item->widget();
                    if (QLayout* inner = item->layout()) inner->setContentsMargins(0, 8, 0, 8);
                    if (!w) continue;
                    if (auto* push = qobject_cast<QPushButton*>(w); push && !push->isCheckable() && t.items.size() == 1) {
                        push->setProperty("m3RowButton", true);
                        if (item->alignment()) c->layout()->setAlignment(push, Qt::Alignment()); // the row's full width
                        if (push->minimumHeight() != OneLine) push->setMinimumHeight(OneLine);
                    } else if (qobject_cast<QAbstractButton*>(w) && w == t.control) {
                        if (w->minimumHeight() != OneLine) w->setMinimumHeight(OneLine); // radios, check boxes
                    } else if (!isControl(w) && !qobject_cast<QLabel*>(w) && w->layout()) {
                        w->layout()->setContentsMargins(0, 10, 0, 10); // a row widget of its own
                    }
                }
            }

            void paint() {
                if (tiles.isEmpty()) return;
                const M3::Scheme& s = scheme();
                QPainter p(c);
                p.setRenderHint(QPainter::Antialiasing);
                const bool active = c->isEnabled();
                for (int i = 0; i < tiles.size(); i++) {
                    const Tile& t = tiles.at(i);
                    const QRectF r = rectOf(t);
                    if (r.isEmpty()) continue;
                    const QPainterPath shape = tileShape(r, t.joinsPrevious ? Inner : Outer, t.joinsNext ? Inner : Outer);
                    p.fillPath(shape, s.surfaceContainerHigh);
                    if (active && (i == hovered || i == pressed)) p.fillPath(shape, alpha(s.onSurface, i == pressed ? 0.10 : 0.08));
                    if (i == flashing) {
                        const double k = qBound(0.0, flash->currentValue().toDouble(), 1.0);
                        if (k > 0.001) p.fillPath(shape, alpha(s.primary, 0.22 * k));
                    }
                    if (!t.icon.isEmpty() && t.title) {
                        // the leading symbol, centred on the title's line (or the whole tile)
                        QFont f(QStringLiteral("Material Symbols Rounded"));
                        f.setPixelSize(24);
                        f.setVariableAxis(QFont::Tag("opsz"), 24);
                        p.setFont(f);
                        p.setPen(t.title->isEnabled() ? s.onSurfaceVariant : alpha(s.onSurface, 0.38));
                        const bool supported = grid() ? t.rows.size() > 1 : t.covers.size() > 1; // with text under it
                        const double cy = supported ? t.title->mapTo(c, t.title->rect().center()).y() + 1 : r.center().y();
                        p.drawText(QRectF(Inset, cy - 16, 24, 32), Qt::AlignCenter, t.icon);
                    }
                }
            }
    };
} // namespace

bool M3SettingsGroup::wanted(const QWidget* widget) {
    return widget && widget->property("m3SettingsGroup").toBool();
}

void M3SettingsGroup::attach(QWidget* container, SchemeFor scheme) {
    if (container->property("m3SettingsGroupAttached").toBool()) return;
    container->setProperty("m3SettingsGroupAttached", true);
    new Group(container, std::move(scheme));
}

bool M3SettingsGroup::rowButton(const QWidget* widget) {
    return widget && widget->property("m3RowButton").toBool();
}

bool M3SettingsGroup::rowCombo(const QWidget* widget) {
    return widget && widget->property("m3RowCombo").toBool();
}
