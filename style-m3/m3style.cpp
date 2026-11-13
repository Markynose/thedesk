/****************************************
 *
 *   theDesk - Material 3 Expressive widget style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Token values (sizes, shapes, colour roles, opacities) are from Jetpack
 *   Compose Material 3 (androidx, Apache 2.0), compose/material3/material3/
 *   src/commonMain/kotlin/androidx/compose/material3/tokens/.
 *
 *   Units: every size here is in Qt logical pixels, which are dp. Qt
 *   multiplies by the screen's device pixel ratio itself (QT_SCALE_FACTOR,
 *   QT_SCREEN_SCALE_FACTORS); theDesk's own SC_DPI is 1 on Linux for the same
 *   reason. Never scale these again, or scaled screens get it twice.
 *
 * *************************************/
#include "m3style.h"

#include "m3animator.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEasingCurve>
#include <QHeaderView>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QTextCharFormat>
#include <QCalendarWidget>
#include <QLayout>
#include <QDialogButtonBox>
#include <QErrorMessage>
#include <QProgressDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSet>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTabBar>
#include <QToolButton>
#include <QtMath>
#include <QMouseEvent>
#include <m3motion.h>
#include <tsettings.h>

namespace {
    // ---- Tokens -------------------------------------------------------------
    namespace Shape { // ShapeTokens
        constexpr double ExtraSmall = 4;
        constexpr double Small = 8;
        constexpr double Medium = 12;
        constexpr double Large = 16;
        constexpr double ExtraLarge = 28;
    } // namespace Shape

    namespace Opacity { // StateTokens
        constexpr double Hover = 0.08;
        constexpr double Focus = 0.10;
        constexpr double Pressed = 0.10;
        constexpr double Dragged = 0.16;
        constexpr double DisabledContent = 0.38;
        constexpr double DisabledContainer = 0.12;
    } // namespace Opacity

    namespace Button { // ButtonSmallTokens
        constexpr int Height = 40;
        constexpr int Padding = 16;   // leading/trailing space
        constexpr int TextPadding = 12; // text buttons
        constexpr int IconSize = 20;
        constexpr int IconGap = 8;
    } // namespace Button

    constexpr int StateLayerSize = 40; // checkbox, radio, switch, icon button
    constexpr int FieldHeight = 40;    // filled text field at density -4
    constexpr int FieldPadding = 12;
    constexpr int MenuItemHeight = 40;
    constexpr int MenuPadding = 4;     // SegmentedMenuTokens.GroupPadding

    // ---- Painting helpers ---------------------------------------------------
    QPainterPath roundedRect(const QRectF& r, double radius) {
        QPainterPath path;
        radius = qMin(radius, qMin(r.width(), r.height()) / 2);
        path.addRoundedRect(r, radius, radius);
        return path;
    }

    // Per-corner radii: top-left, top-right, bottom-right, bottom-left
    QPainterPath roundedRect(const QRectF& r, double tl, double tr, double br, double bl) {
        const double m = qMin(r.width(), r.height()) / 2;
        tl = qMin(tl, m);
        tr = qMin(tr, m);
        br = qMin(br, m);
        bl = qMin(bl, m);
        QPainterPath path;
        path.moveTo(r.left() + tl, r.top());
        path.lineTo(r.right() - tr, r.top());
        if (tr > 0) path.arcTo(r.right() - 2 * tr, r.top(), 2 * tr, 2 * tr, 90, -90);
        path.lineTo(r.right(), r.bottom() - br);
        if (br > 0) path.arcTo(r.right() - 2 * br, r.bottom() - 2 * br, 2 * br, 2 * br, 0, -90);
        path.lineTo(r.left() + bl, r.bottom());
        if (bl > 0) path.arcTo(r.left(), r.bottom() - 2 * bl, 2 * bl, 2 * bl, 270, -90);
        path.lineTo(r.left(), r.top() + tl);
        if (tl > 0) path.arcTo(r.left(), r.top(), 2 * tl, 2 * tl, 180, -90);
        path.closeSubpath();
        return path;
    }

    QColor alpha(QColor c, double a) {
        c.setAlphaF(c.alphaF() * a);
        return c;
    }

    double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }

    QColor mixColor(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(lerp(a.redF(), b.redF(), t), lerp(a.greenF(), b.greenF(), t), lerp(a.blueF(), b.blueF(), t), lerp(a.alphaF(), b.alphaF(), t));
    }

    void fill(QPainter* painter, const QPainterPath& path, const QColor& color) {
        if (color.alpha() == 0) return;
        painter->fillPath(path, color);
    }

    bool enabled(const QStyleOption* option) {
        return option->state & QStyle::State_Enabled;
    }

    bool keyboardFocus(const QStyleOption* option) {
        return (option->state & QStyle::State_HasFocus) && (option->state & QStyle::State_KeyboardFocusChange);
    }

    QFont withWeight(QFont font, QFont::Weight weight) {
        font.setWeight(weight);
        return font;
    }

    QRectF centred(const QRectF& outer, double w, double h) {
        return QRectF(outer.center().x() - w / 2, outer.center().y() - h / 2, w, h);
    }

    bool insideComposite(const QWidget* widget) {
        // Line edits inside spin boxes and combo boxes: the parent draws the field
        return widget && widget->parentWidget() && (qobject_cast<const QAbstractSpinBox*>(widget->parentWidget()) || qobject_cast<const QComboBox*>(widget->parentWidget()));
    }
} // namespace

namespace {
    bool hoverMerge(const QWidget* widget);

    // Navigation drawer items (NavigationDrawerTokens): flat, left-aligned,
    // SecondaryContainer pill when selected. Set m3NavItem=true on a
    // checkable QPushButton.
    bool navItem(const QWidget* widget) {
        return widget && widget->property("m3NavItem").toBool();
    }
    // M3 search bar (SearchBarTokens): a 56 dp pill, no indicator line.
    // Set m3SearchBar=true on a QLineEdit.
    bool searchBar(const QWidget* widget) {
        return widget && widget->property("m3SearchBar").toBool();
    }
} // namespace

M3Style::M3Style() :
    QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {
    this->setObjectName(QStringLiteral("Material3"));
    animator = new M3Animator(this);
    // The pointer melts into buttons that fill solid (labelled ones); icon
    // buttons only grow a state layer, so the arrow stays
    animator->setCursorMergePredicate([](const QWidget* widget) {
        if (!hoverMerge(widget)) return false;
        if (qobject_cast<const QPushButton*>(widget)) return true;
        if (auto* tool = qobject_cast<const QToolButton*>(widget)) return tool->toolButtonStyle() != Qt::ToolButtonIconOnly && !tool->text().isEmpty();
        return false;
    });
}

M3Style::~M3Style() = default;

const M3::Scheme& M3Style::scheme(const QPalette& palette) const {
    const QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
    const QColor window = palette.color(QPalette::Active, QPalette::Window);
    const quint64 key = (quint64(highlight.rgba()) << 32) | window.rgba();
    auto it = schemes.constFind(key);
    if (it != schemes.constEnd()) return *it;

    if (schemes.size() > 32) schemes.clear();
    M3::Scheme platform = M3::currentScheme();
    if (platform.primary.rgb() == highlight.rgb() && platform.surface.rgb() == window.rgb()) {
        return *schemes.insert(key, platform);
    }
    // A palette that isn't the platform's (another base, or a widget with its
    // own colours): build an M3 scheme around its accent
    bool dark = window.lightnessF() < 0.5;
    return *schemes.insert(key, M3::Scheme::generate(highlight, dark, M3::Config::fromSettings().variant));
}

void M3Style::polish(QPalette& palette) {
    QProxyStyle::polish(palette);
}

namespace {
    // Calendar days: the selected one in a Primary circle (DatePickerTokens)
    class CalendarDayDelegate : public QStyledItemDelegate {
        public:
            using QStyledItemDelegate::QStyledItemDelegate;
            void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
                const M3::Scheme s = M3::currentScheme();
                const QString text = index.data(Qt::DisplayRole).toString();
                const bool header = index.row() == 0;
                const bool selected = (option.state & QStyle::State_Selected) && !header;
                QColor colour = header ? s.onSurfaceVariant : s.onSurface;
                QVariant fg = index.data(Qt::ForegroundRole);
                if (!header && fg.canConvert<QBrush>()) {
                    QColor c = fg.value<QBrush>().color();
                    if (c.alpha() > 0 && c.lightnessF() < 0.5 == (s.surface.lightnessF() < 0.5)) colour = s.outline; // other months
                }
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                const double d = qMin(option.rect.width(), option.rect.height()) - 4;
                QRectF circle(option.rect.center().x() - d / 2.0 + 0.5, option.rect.center().y() - d / 2.0 + 0.5, d, d);
                if (selected) {
                    QPainterPath dot;
                    dot.addEllipse(circle);
                    p->fillPath(dot, s.primary);
                    colour = s.onPrimary;
                } else if ((option.state & QStyle::State_MouseOver) && !header) {
                    QPainterPath dot;
                    dot.addEllipse(circle);
                    QColor layer = s.onSurface;
                    layer.setAlphaF(0.08);
                    p->fillPath(dot, layer);
                }
                QFont f = option.font;
                f.setWeight(selected ? QFont::DemiBold : QFont::Normal);
                p->setFont(f);
                p->setPen(colour);
                p->drawText(option.rect, Qt::AlignCenter, text);
                p->restore();
            }
    };

    // "NETWORK STATUS" -> "Network status"; common acronyms stay upper case
    QString sentenceCase(const QString& text) {
        static const QSet<QString> acronyms = {"VPN", "DNS", "IP", "IPV4", "IPV6", "USB", "CPU", "GPU", "MAC", "SSID", "LAN", "WAN", "ID", "PIN", "DPI",
            "URL", "AM", "PM", "HUD", "NTP", "UTC", "GMT", "OS", "TV", "PC", "UI", "API", "DE", "WM", "SSH", "GPS", "NFC", "SIM", "HDMI", "VGA", "QR"};
        QStringList words = text.split(QLatin1Char(' '));
        for (int i = 0; i < words.size(); i++) {
            QString& w = words[i];
            QString letters = w;
            letters.remove(QRegularExpression(QStringLiteral("[^\\p{L}0-9]")));
            if (acronyms.contains(letters)) continue;
            w = w.toLower();
            if (i == 0 && !w.isEmpty()) w[0] = w[0].toUpper();
        }
        return words.join(QLatin1Char(' '));
    }

    // Section headers in theSuite .ui files are bold QLabels with their text
    // in capitals ("COLOURS", "TIMEOUTS"): M3 makes them TitleSmall in Primary
    bool isCapsHeader(const QLabel* label) {
        if (!label->font().bold() || label->textFormat() == Qt::RichText) return false;
        const QString text = label->text();
        if (text.size() < 3 || text.contains(QLatin1Char('\n')) || text.startsWith(QLatin1Char('<'))) return false;
        bool hasLetter = false;
        for (QChar c : text) {
            if (c.isLetter()) {
                hasLetter = true;
                if (c.isLower()) return false;
            }
        }
        return hasLetter;
    }
} // namespace

void M3Style::applyHeader(QLabel* label) {
    if (label->property("m3NoHeader").toBool()) return; // opted out (e.g. snackbar titles)
    const bool header = label->property("m3Header").toBool();
    if (!header && !isCapsHeader(label)) return;
    label->setProperty("m3Header", true);
    const QString text = label->text();
    bool caps = true;
    for (QChar c : text) {
        if (c.isLetter() && c.isLower()) caps = false;
    }
    if (caps) label->setText(sentenceCase(text));
    QFont font = label->font();
    font.setBold(false);
    font.setWeight(QFont::DemiBold);
    font.setCapitalization(QFont::MixedCase);
    font.setPixelSize(14);
    label->setFont(font);
    QPalette pal = label->palette();
    for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, pal.color(group, QPalette::Highlight));
    label->setPalette(pal);
    label->setContentsMargins(0, 8, 0, 2);
}

bool M3Style::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Show || event->type() == QEvent::LanguageChange) {
        if (auto* label = qobject_cast<QLabel*>(watched)) {
            QPointer<QLabel> guard(label);
            // After the retranslation that triggered it has set the text
            QTimer::singleShot(0, this, [this, guard] {
                if (guard) applyHeader(guard);
            });
        }
    }
    return QProxyStyle::eventFilter(watched, event);
}

namespace {
    // M3E button sizes (defined with the rest of the button family below)
    struct ButtonSize {
            int height, padding, icon, fontPx, weight;
            double square, pressed, inner, splitInner;
            int narrow, wide; // icon button widths
    };
    const ButtonSize& buttonSize(const QWidget* widget);
    bool hasButtonSize(const QWidget* widget);
    bool connectedGroup(const QWidget* container);
    enum class ButtonKind {
        Filled,
        Tonal,
        Outlined,
        Elevated,
        Text,
        Standard // icon buttons only: no container
    };
    ButtonKind pushButtonKind(const QStyleOptionButton* button, const QWidget* widget);
} // namespace

namespace {
    /**
     * A standard M3E button group (m3ButtonGroup="standard" on a container):
     * the pressed button springs about 15% wider and its neighbours give up
     * the room, so the row keeps its width; on release everything springs
     * back (ButtonGroup's "standard" interaction in Compose).
     */
    class StandardButtonGroup : public QObject {
        public:
            static void attach(QWidget* container) {
                if (container->property("m3StandardGroupAttached").toBool()) return;
                container->setProperty("m3StandardGroupAttached", true);
                new StandardButtonGroup(container);
            }

            bool eventFilter(QObject* watched, QEvent* event) override {
                if (watched == container) {
                    if (event->type() == QEvent::ChildPolished) {
                        if (auto* added = qobject_cast<QAbstractButton*>(static_cast<QChildEvent*>(event)->child())) added->installEventFilter(this);
                    }
                    return false;
                }
                auto* button = qobject_cast<QAbstractButton*>(watched);
                if (!button || !button->isEnabled()) return false;
                if (event->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                    press(button);
                } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Leave) {
                    if (button == pressed) spring->animateTo(0.0);
                }
                return false;
            }

        private:
            explicit StandardButtonGroup(QWidget* container) :
                QObject(container), container(container) {
                spring = new M3SpringAnimation(this);
                spring->setToken(M3Motion::SpatialFast);
                spring->setStartValue(0.0);
                QObject::connect(spring, &M3SpringAnimation::valueChanged, this, [this](QVariant value) {
                    apply(value.toDouble());
                });
                QObject::connect(spring, &QAbstractAnimation::finished, this, [this] {
                    if (spring->currentValue().toDouble() < 0.001) restore();
                });
                // Every button in it, shown yet or not (polish comes before
                // showing), and any added later
                for (QAbstractButton* button : container->findChildren<QAbstractButton*>(Qt::FindDirectChildrenOnly)) button->installEventFilter(this);
                container->installEventFilter(this);
            }

            QList<QAbstractButton*> buttons() const {
                // In the layout's order, the visible ones
                QList<QAbstractButton*> list;
                QLayout* layout = container->layout();
                if (!layout) return list;
                for (int i = 0; i < layout->count(); i++) {
                    if (auto* button = qobject_cast<QAbstractButton*>(layout->itemAt(i)->widget()); button && button->isVisible()) list.append(button);
                }
                return list;
            }

            void press(QAbstractButton* button) {
                const QList<QAbstractButton*> row = buttons();
                if (!widths.isEmpty() && pressed != button) {
                    restore(); // a different one: start from the natural widths
                }
                if (widths.isEmpty()) {
                    for (QAbstractButton* b : row) widths.insert(b, {b->width(), b->minimumWidth(), b->maximumWidth(), b});
                }
                pressed = button;
                const int at = row.indexOf(button);
                neighbours.clear();
                if (at > 0) neighbours.append(row.at(at - 1));
                if (at >= 0 && at + 1 < row.size()) neighbours.append(row.at(at + 1));
                spring->animateTo(1.0);
            }

            void apply(double k) {
                if (!pressed || !widths.contains(pressed)) return;
                const int base = widths.value(pressed).width;
                const double grow = 0.15 * base * k;
                pressed->setFixedWidth(qMax(1, int(std::round(base + grow))));
                for (QAbstractButton* n : std::as_const(neighbours)) {
                    if (!widths.contains(n) || !widths.value(n).alive) continue;
                    n->setFixedWidth(qMax(1, int(std::round(widths.value(n).width - grow / neighbours.size()))));
                }
            }

            void restore() {
                for (const Widths& w : std::as_const(widths)) {
                    if (!w.alive) continue;
                    w.alive->setMinimumWidth(w.minimum);
                    w.alive->setMaximumWidth(w.maximum);
                }
                widths.clear();
                neighbours.clear();
                pressed = nullptr;
            }

            struct Widths {
                    int width, minimum, maximum;
                    QPointer<QAbstractButton> alive;
            };
            QWidget* container;
            M3SpringAnimation* spring;
            QPointer<QAbstractButton> pressed;
            QList<QAbstractButton*> neighbours;
            QHash<QAbstractButton*, Widths> widths;
    };
} // namespace

namespace {
    /**
     * The date picker's month change, in the manner of Android's: the day
     * grid slides sideways on a spring, the old month out and the new one in
     * from the side it lies on. QCalendarWidget swaps the grid at once, so
     * this keeps a picture of the shown month and slides pictures over the
     * grid while the spring runs.
     */
    class CalendarSlide : public QWidget {
        public:
            static void attach(QCalendarWidget* calendar) {
                auto* view = calendar->findChild<QAbstractItemView*>(QStringLiteral("qt_calendar_calendarview"));
                if (!view) return;
                new CalendarSlide(calendar, view);
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                QPainter painter(this);
                const double k = spring->currentValue().toDouble();
                const double w = this->width();
                painter.drawPixmap(QPointF(-direction * w * k, 0), from);
                painter.drawPixmap(QPointF(direction * w * (1 - k), 0), to);
            }

            bool eventFilter(QObject* watched, QEvent* event) override {
                if (watched == view && (event->type() == QEvent::Show || event->type() == QEvent::Resize)) remember();
                return false;
            }

        private:
            CalendarSlide(QCalendarWidget* calendar, QAbstractItemView* view) :
                QWidget(view->parentWidget()), calendar(calendar), view(view) {
                this->hide();
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
                spring = new M3SpringAnimation(this);
                spring->setToken(M3Motion::SpatialDefault);
                spring->setStartValue(0.0);
                QObject::connect(spring, &M3SpringAnimation::valueChanged, this, [this] {
                    this->update();
                });
                QObject::connect(spring, &QAbstractAnimation::finished, this, [this] {
                    this->hide();
                });
                QObject::connect(calendar, &QCalendarWidget::currentPageChanged, this, [this](int year, int month) {
                    slide(year * 12 + month);
                });
                QObject::connect(calendar, &QCalendarWidget::selectionChanged, this, [this] {
                    QTimer::singleShot(0, this, [this] {
                        if (!this->isVisible()) remember();
                    });
                });
                view->installEventFilter(this);
                page = calendar->yearShown() * 12 + calendar->monthShown();
            }

            void remember() {
                QTimer::singleShot(0, this, [this] {
                    if (view->isVisible() && !view->size().isEmpty()) last = view->grab();
                });
            }

            void slide(int newPage) {
                const int oldPage = page;
                page = newPage;
                if (last.isNull() || !view->isVisible() || newPage == oldPage) {
                    remember();
                    return;
                }
                from = last;
                to = view->grab(); // the new month (the grid has already changed)
                last = to;
                direction = newPage > oldPage ? 1 : -1;
                this->setGeometry(view->geometry());
                this->raise();
                this->show();
                spring->snapTo(0.0);
                spring->animateTo(1.0);
            }

            QCalendarWidget* calendar;
            QAbstractItemView* view;
            M3SpringAnimation* spring;
            QPixmap last, from, to;
            int page = 0;
            int direction = 1;
    };
} // namespace

void M3Style::polish(QWidget* widget) {
    QProxyStyle::polish(widget);
    // Button sizes: the label's type and the icon grow with the button
    if (auto* button = qobject_cast<QAbstractButton*>(widget); button && hasButtonSize(button) && !button->property("m3SizePolished").toBool()) {
        button->setProperty("m3SizePolished", true);
        const ButtonSize& size = buttonSize(button);
        QFont font = button->font();
        font.setPixelSize(size.fontPx);
        font.setWeight(QFont::Weight(size.weight));
        button->setFont(font);
        button->setIconSize(QSize(size.icon, size.icon));
    }
    // Connected button groups sit 2 dp apart
    if (connectedGroup(widget) && widget->layout()) widget->layout()->setSpacing(2);
    // Standard button groups: the pressed button widens, its neighbours give way
    if (widget->property("m3ButtonGroup").toString().toLower() == QLatin1String("standard") && widget->layout()) StandardButtonGroup::attach(widget);
    // QCalendarWidget as an M3 date picker: a quiet header in the surface's
    // colours, OnSurfaceVariant weekday names, weekends like any other day
    if (auto* calendar = qobject_cast<QCalendarWidget*>(widget); calendar && !calendar->property("m3Calendar").toBool()) {
        calendar->setProperty("m3Calendar", true);
        CalendarSlide::attach(calendar);
        const M3::Scheme& sc = scheme(calendar->palette());
        QTextCharFormat weekday;
        weekday.setForeground(sc.onSurfaceVariant);
        QTextCharFormat day;
        day.setForeground(sc.onSurface);
        for (Qt::DayOfWeek d : {Qt::Monday, Qt::Tuesday, Qt::Wednesday, Qt::Thursday, Qt::Friday, Qt::Saturday, Qt::Sunday}) calendar->setWeekdayTextFormat(d, day);
        calendar->setHeaderTextFormat(weekday);
        calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        if (auto* view = calendar->findChild<QAbstractItemView*>(QStringLiteral("qt_calendar_calendarview"))) {
            view->setItemDelegate(new CalendarDayDelegate(view));
            view->viewport()->setAttribute(Qt::WA_Hover);
            view->setMouseTracking(true);
        }
        if (auto* nav = calendar->findChild<QWidget*>(QStringLiteral("qt_calendar_navigationbar"))) {
            QPalette pal = nav->palette();
            for (auto group : {QPalette::Active, QPalette::Inactive}) {
                pal.setColor(group, QPalette::Window, sc.surfaceContainerHigh);
                pal.setColor(group, QPalette::Highlight, sc.surfaceContainerHigh);
                pal.setColor(group, QPalette::ButtonText, sc.onSurface);
                pal.setColor(group, QPalette::WindowText, sc.onSurface);
                pal.setColor(group, QPalette::HighlightedText, sc.onSurface);
            }
            nav->setPalette(pal);
            nav->setAutoFillBackground(true);
        }
    }

    // Qt's own dialogs as M3 dialogs (DialogTokens): SurfaceContainerHigh,
    // 24 dp padding, a HeadlineSmall headline, BodyMedium supporting text in
    // OnSurfaceVariant, and text buttons for the actions. Only Qt's standard
    // dialogs: shell dialogs (Gateway, drawers) style themselves.
    if (widget->isWindow() && (qobject_cast<QMessageBox*>(widget) || qobject_cast<QInputDialog*>(widget) || qobject_cast<QProgressDialog*>(widget) || qobject_cast<QErrorMessage*>(widget))
        && !widget->property("m3Dialog").toBool()) {
        widget->setProperty("m3Dialog", true);
        const M3::Scheme& sc = scheme(widget->palette());
        QPalette pal = widget->palette();
        for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) pal.setColor(group, QPalette::Window, sc.surfaceContainerHigh);
        widget->setPalette(pal);
        if (widget->layout()) widget->layout()->setContentsMargins(24, 24, 24, 20);
        if (auto* headline = widget->findChild<QLabel*>(QStringLiteral("qt_msgbox_label"))) {
            QFont f = headline->font();
            f.setPixelSize(22); // HeadlineSmall
            f.setWeight(QFont::Normal);
            headline->setFont(f);
        }
        if (auto* support = widget->findChild<QLabel*>(QStringLiteral("qt_msgbox_informativelabel"))) {
            QFont f = support->font();
            f.setPixelSize(14);
            support->setFont(f);
            QPalette sp = support->palette();
            for (auto group : {QPalette::Active, QPalette::Inactive}) sp.setColor(group, QPalette::WindowText, sc.onSurfaceVariant);
            support->setPalette(sp);
        }
        for (auto* box : widget->findChildren<QDialogButtonBox*>()) {
            for (QAbstractButton* b : box->buttons()) {
                if (auto* push = qobject_cast<QPushButton*>(b)) push->setFlat(true);
            }
        }
    }
    if (auto* label = qobject_cast<QLabel*>(widget)) {
        // Panes retranslate (resetting the text) after they're polished, so
        // headers are re-checked whenever a bold label shows or changes language
        if (label->font().bold() || label->property("m3Header").toBool()) {
            label->removeEventFilter(this);
            label->installEventFilter(this);
            // Next turn of the event loop: widgets get polished during
            // setupUi (layouts ask for size hints), before their owner can
            // set m3NoHeader
            QPointer<QLabel> guard(label);
            QTimer::singleShot(0, this, [this, guard] {
                if (guard) applyHeader(guard);
            });
        }
    }
    if (qobject_cast<QAbstractButton*>(widget) || qobject_cast<QAbstractSlider*>(widget) || qobject_cast<QComboBox*>(widget) || qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QTabBar*>(widget) || qobject_cast<QLineEdit*>(widget) || qobject_cast<QHeaderView*>(widget)) {
        widget->setAttribute(Qt::WA_Hover);
        animator->watch(widget);
    }
    if (auto* view = qobject_cast<QAbstractItemView*>(widget)) {
        view->viewport()->setAttribute(Qt::WA_Hover);
    }
    if (auto* progress = qobject_cast<QProgressBar*>(widget)) {
        // Indeterminate bars animate; the animator only repaints visible ones
        if (progress->minimum() == progress->maximum()) animator->watchBusy(progress);
        if (!progress->property("m3BusyHooked").toBool()) {
            progress->setProperty("m3BusyHooked", true);
            connect(progress, &QProgressBar::valueChanged, this, [this, progress] {
                if (progress->minimum() == progress->maximum()) animator->watchBusy(progress);
            });
        }
    }
    if (qobject_cast<QMenu*>(widget) || widget->inherits("QTipLabel") || widget->inherits("QComboBoxPrivateContainer")) {
        widget->setAttribute(Qt::WA_Hover);
    }
}

void M3Style::unpolish(QWidget* widget) {
    animator->unwatch(widget);
    if (auto* progress = qobject_cast<QProgressBar*>(widget)) {
        disconnect(progress, nullptr, this, nullptr);
        progress->setProperty("m3BusyHooked", false);
    }
    QProxyStyle::unpolish(widget);
}

// ---- Metrics ------------------------------------------------------------------

int M3Style::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const {
    switch (metric) {
        case PM_ButtonMargin:
            return Button::Padding;
        case PM_ButtonDefaultIndicator:
        case PM_DefaultFrameWidth:
            if (qobject_cast<const QLineEdit*>(widget)) return 2;
            return 0;
        case PM_ButtonShiftHorizontal:
        case PM_ButtonShiftVertical:
            return 0;
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorWidth:
        case PM_ExclusiveIndicatorHeight:
            return StateLayerSize;
        case PM_CheckBoxLabelSpacing:
        case PM_RadioButtonLabelSpacing:
            return 4;
        case PM_SliderThickness:
        case PM_SliderControlThickness:
            return 44; // SliderTokens.HandleHeight
        case PM_SliderLength:
            return 4 + 6 + 6; // handle + gap on each side
        case PM_SliderTickmarkOffset:
            return 0;
        case PM_ScrollBarExtent:
            return 12;
        case PM_ScrollBarSliderMin:
            return 32;
        case PM_ScrollView_ScrollBarOverlap:
            return 0;
        case PM_ToolBarIconSize:
        case PM_ButtonIconSize:
            return metric == PM_ButtonIconSize ? Button::IconSize : 24;
        case PM_SmallIconSize:
            return 20;
        case PM_ToolBarItemSpacing:
            return 4;
        case PM_ToolBarFrameWidth:
        case PM_ToolBarItemMargin:
            return 4;
        case PM_ToolBarHandleExtent:
        case PM_ToolBarSeparatorExtent:
            return 9;
        case PM_MenuPanelWidth:
        case PM_MenuHMargin:
            return 0;
        case PM_MenuVMargin:
            return MenuPadding + 4;
        case PM_MenuBarPanelWidth:
        case PM_MenuBarVMargin:
        case PM_MenuBarHMargin:
            return 0;
        case PM_MenuBarItemSpacing:
            return 4;
        case PM_SubMenuOverlap:
            return 0;
        case PM_MenuButtonIndicator:
            // A split button's trailing menu button (SplitButtonSmallTokens)
            return widget && hasButtonSize(widget) ? buttonSize(widget).height * 9 / 10 : 36;
        case PM_ComboBoxFrameWidth:
        case PM_SpinBoxFrameWidth:
            return 0;
        case PM_TabBarTabHSpace:
            return 32;
        case PM_TabBarTabVSpace:
            return 20;
        case PM_TabBarBaseOverlap:
        case PM_TabBarBaseHeight:
            return 1;
        case PM_TabBarTabShiftHorizontal:
        case PM_TabBarTabShiftVertical:
            return 0;
        case PM_ToolTipLabelFrameWidth:
            return 6;
        case PM_HeaderMargin:
            return 8;
        case PM_FocusFrameHMargin:
        case PM_FocusFrameVMargin:
            return 2;
        case PM_SplitterWidth:
            return 6;
        case PM_LayoutHorizontalSpacing:
        case PM_LayoutVerticalSpacing:
            return 8;
        case PM_ProgressBarChunkWidth:
            return 0;
        default:
            return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int M3Style::styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget, QStyleHintReturn* returnData) const {
    if (hint == SH_M3Style) return 1;
    switch (hint) {
        case SH_Menu_Mask:
        case SH_ToolTip_Mask: {
            // No compositor on this X11 session: round the window with a mask
            if (auto* mask = qstyleoption_cast<QStyleHintReturnMask*>(returnData)) {
                if (!option) return 0;
                double radius = hint == SH_Menu_Mask ? Shape::Large : Shape::ExtraSmall;
                mask->region = QRegion(roundedRect(QRectF(option->rect), radius).toFillPolygon().toPolygon());
                return 1;
            }
            return 0;
        }
        case SH_ComboBox_Popup:
            return 1; // M3: an exposed dropdown opens a menu
        case SH_ComboBox_PopupFrameStyle:
            return QFrame::NoFrame;
        case SH_DialogButtonBox_ButtonsHaveIcons:
            return 0;
        case SH_Slider_AbsoluteSetButtons:
            return Qt::LeftButton;
        case SH_Slider_PageSetButtons:
            return Qt::MiddleButton;
        case SH_EtchDisabledText:
        case SH_DitherDisabledText:
            return 0;
        case SH_ItemView_ShowDecorationSelected:
            return 1;
        case SH_Menu_SubMenuPopupDelay:
            return 150;
        case SH_ToolButtonStyle:
            return Qt::ToolButtonIconOnly;
        case SH_ScrollBar_ContextMenu:
            return 0;
        case SH_ScrollView_FrameOnlyAroundContents:
            return 0;
        case SH_TabBar_Alignment:
            return Qt::AlignLeft;
        case SH_FocusFrame_AboveWidget:
            return 0;
        case SH_LineEdit_PasswordCharacter:
            return 0x2022;
        default:
            return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
}

QSize M3Style::sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size, const QWidget* widget) const {
    switch (type) {
        case CT_PushButton: {
            auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
            const ButtonSize& bs = buttonSize(widget);
            bool text = button && pushButtonKind(button, widget) == ButtonKind::Text;
            int padding = text ? (bs.height == Button::Height ? Button::TextPadding : bs.padding * 3 / 4) : bs.padding;
            int w = size.width() + 2 * padding;
            if (button && (button->features & QStyleOptionButton::HasMenu)) w += 20 + 4;
            // QPushButton adds the margin itself; size is just the label
            return QSize(qMax(w, 48), qMax(size.height() + 8, bs.height));
        }
        case CT_ToolButton: {
            auto* tool = qstyleoption_cast<const QStyleOptionToolButton*>(option);
            bool iconOnly = tool && (tool->toolButtonStyle == Qt::ToolButtonIconOnly || tool->text.isEmpty());
            const bool split = tool && (tool->features & QStyleOptionToolButton::MenuButtonPopup);
            const int menu = split ? proxy()->pixelMetric(PM_MenuButtonIndicator, option, widget) : 0;
            const ButtonSize& bs = buttonSize(widget);
            if (iconOnly) {
                if (hasButtonSize(widget) || (widget && widget->property("m3IconButtonWidth").isValid())) {
                    // IconButton*Tokens: the size's height; narrow, default or wide
                    const QString width = widget->property("m3IconButtonWidth").toString().toLower();
                    int w = width == QLatin1String("narrow") ? bs.narrow : width == QLatin1String("wide") ? bs.wide : bs.height;
                    return QSize(w + (split ? 2 + menu : 0), bs.height);
                }
                // SmallIconButtonTokens: 40 px container around a 24 px icon
                int extent = qMax(StateLayerSize, qMax(size.width() - menu, size.height()) + 16);
                return QSize(extent + (split ? 2 + menu : 0), extent);
            }
            return QSize(size.width() + 2 * bs.padding + (split ? 2 : 0), qMax(size.height() + 8, bs.height));
        }
        case CT_LineEdit:
            if (insideComposite(widget)) return QProxyStyle::sizeFromContents(type, option, size, widget);
            if (searchBar(widget)) return QSize(size.width() + 2 * FieldPadding, qMax(size.height() + 8, 56));
            return QSize(size.width() + 2 * FieldPadding, qMax(size.height() + 8, FieldHeight));
        case CT_ComboBox:
            return QSize(size.width() + 2 * FieldPadding + 24 + 8, qMax(size.height() + 8, FieldHeight));
        case CT_SpinBox:
            return QSize(size.width() + 2 * FieldPadding + 24, qMax(size.height() + 8, FieldHeight));
        case CT_CheckBox:
        case CT_RadioButton: {
            QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
            return QSize(s.width() + 4, qMax(s.height(), StateLayerSize));
        }
        case CT_MenuItem: {
            auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
            if (!item) break;
            if (item->menuItemType == QStyleOptionMenuItem::Separator) return QSize(size.width(), 9);
            int w = size.width() + 2 * 12 + 2 * MenuPadding;
            if (item->maxIconWidth > 0 || item->checkType != QStyleOptionMenuItem::NotCheckable) w += 24 + 12;
            if (item->menuItemType == QStyleOptionMenuItem::SubMenu) w += 24;
            if (item->text.contains(QLatin1Char('\t'))) w += 32;
            return QSize(qMax(w, 112 + 2 * MenuPadding), MenuItemHeight); // M3 menus are at least 112 wide
        }
        case CT_MenuBarItem:
            return QSize(size.width() + 24, qMax(size.height() + 8, 32));
        case CT_TabBarTab: {
            QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
            return QSize(s.width(), qMax(s.height(), 48)); // PrimaryNavigationTabTokens.ContainerHeight
        }
        case CT_HeaderSection: {
            QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
            return QSize(s.width() + 8, qMax(s.height(), 36));
        }
        case CT_ItemViewItem: {
            QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
            return QSize(s.width() + 8, qMax(s.height() + 4, 28));
        }
        case CT_ProgressBar: {
            auto* bar = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
            QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
            bool horizontal = !bar || (bar->state & State_Horizontal);
            if (horizontal) return QSize(s.width(), qMax(16, option->fontMetrics.height()));
            return QSize(16, s.height());
        }
        default:
            break;
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

QRect M3Style::subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const {
    switch (element) {
        case SE_LineEditContents:
            if (insideComposite(widget)) return option->rect;
            if (searchBar(widget)) return option->rect.adjusted(FieldPadding + 4, 2, -FieldPadding - 4, -2);
            return option->rect.adjusted(FieldPadding, 2, -FieldPadding, -2);
        case SE_ProgressBarGroove:
        case SE_ProgressBarContents:
        case SE_ProgressBarLabel: {
            auto* bar = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
            QRect r = option->rect;
            bool horizontal = !bar || (bar->state & State_Horizontal);
            bool text = bar && bar->textVisible && horizontal && !bar->text.isEmpty();
            int textWidth = text ? option->fontMetrics.horizontalAdvance(QStringLiteral("100%")) + 12 : 0;
            if (element == SE_ProgressBarLabel) return text ? QRect(r.right() - textWidth + 1, r.top(), textWidth, r.height()) : QRect();
            return r.adjusted(0, 0, -textWidth, 0);
        }
        case SE_PushButtonFocusRect:
            return option->rect;
        case SE_ItemViewItemText:
        case SE_ItemViewItemDecoration:
        case SE_ItemViewItemCheckIndicator: {
            // Keep content off the rounded ends of the selection pill
            auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
            if (!item) break;
            QStyleOptionViewItem copy = *item;
            bool first = item->viewItemPosition == QStyleOptionViewItem::Beginning || item->viewItemPosition == QStyleOptionViewItem::OnlyOne || item->viewItemPosition == QStyleOptionViewItem::Invalid;
            bool last = item->viewItemPosition == QStyleOptionViewItem::End || item->viewItemPosition == QStyleOptionViewItem::OnlyOne || item->viewItemPosition == QStyleOptionViewItem::Invalid;
            copy.rect.adjust(first ? 8 : 0, 0, last ? -8 : 0, 0);
            return QProxyStyle::subElementRect(element, &copy, widget);
        }
        default:
            break;
    }
    return QProxyStyle::subElementRect(element, option, widget);
}

QRect M3Style::subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl subControl, const QWidget* widget) const {
    switch (control) {
        case CC_ComboBox: {
            QRect r = option->rect;
            switch (subControl) {
                case SC_ComboBoxFrame:
                    return r;
                case SC_ComboBoxArrow:
                    return QRect(r.right() - 36, r.top(), 36, r.height());
                case SC_ComboBoxEditField:
                    return r.adjusted(FieldPadding + 4, 4, -40, -4);
                case SC_ComboBoxListBoxPopup:
                    return r;
                default:
                    break;
            }
            break;
        }
        case CC_SpinBox: {
            auto* spin = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
            if (!spin) break;
            QRect r = option->rect;
            bool buttons = spin->buttonSymbols != QAbstractSpinBox::NoButtons;
            int bw = buttons ? 28 : 0;
            switch (subControl) {
                case SC_SpinBoxFrame:
                    return r;
                case SC_SpinBoxEditField:
                    return r.adjusted(FieldPadding + 4, 4, -bw - 4, -4);
                case SC_SpinBoxUp:
                    return buttons ? QRect(r.right() - bw, r.top() + 2, bw - 4, r.height() / 2 - 2) : QRect();
                case SC_SpinBoxDown:
                    return buttons ? QRect(r.right() - bw, r.top() + r.height() / 2, bw - 4, r.height() / 2 - 2) : QRect();
                default:
                    break;
            }
            break;
        }
        case CC_ScrollBar: {
            auto* bar = qstyleoption_cast<const QStyleOptionSlider*>(option);
            if (!bar) break;
            // No arrow buttons: the whole length is the groove
            QRect r = option->rect;
            bool horizontal = bar->orientation == Qt::Horizontal;
            int length = horizontal ? r.width() : r.height();
            int range = bar->maximum - bar->minimum;
            int sliderLength = range > 0 ? qMax(pixelMetric(PM_ScrollBarSliderMin, option, widget), int(qint64(length) * bar->pageStep / (range + bar->pageStep))) : length;
            sliderLength = qMin(sliderLength, length);
            int pos = sliderPositionFromValue(bar->minimum, bar->maximum, bar->sliderPosition, length - sliderLength, bar->upsideDown);
            QRect slider = horizontal ? QRect(r.left() + pos, r.top(), sliderLength, r.height()) : QRect(r.left(), r.top() + pos, r.width(), sliderLength);
            switch (subControl) {
                case SC_ScrollBarSlider:
                    return slider;
                case SC_ScrollBarGroove:
                    return r;
                case SC_ScrollBarAddLine:
                case SC_ScrollBarSubLine:
                    return QRect();
                case SC_ScrollBarSubPage:
                    return horizontal ? QRect(r.left(), r.top(), slider.left() - r.left(), r.height()) : QRect(r.left(), r.top(), r.width(), slider.top() - r.top());
                case SC_ScrollBarAddPage:
                    return horizontal ? QRect(slider.right() + 1, r.top(), r.right() - slider.right(), r.height()) : QRect(r.left(), slider.bottom() + 1, r.width(), r.bottom() - slider.bottom());
                default:
                    break;
            }
            break;
        }
        case CC_Slider: {
            auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option);
            if (!slider) break;
            QRect r = option->rect;
            bool horizontal = slider->orientation == Qt::Horizontal;
            const int handle = pixelMetric(PM_SliderLength, option, widget);
            int length = horizontal ? r.width() : r.height();
            int pos = sliderPositionFromValue(slider->minimum, slider->maximum, slider->sliderPosition, length - handle, horizontal ? slider->upsideDown : !slider->upsideDown);
            switch (subControl) {
                case SC_SliderHandle:
                    return horizontal ? QRect(r.left() + pos, r.top(), handle, r.height()) : QRect(r.left(), r.top() + pos, r.width(), handle);
                case SC_SliderGroove:
                    return r;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
    return QProxyStyle::subControlRect(control, option, subControl, widget);
}

// ---- Shared pieces ------------------------------------------------------------

void M3Style::drawFocusRing(QPainter* painter, const QPainterPath& shape, const M3::Scheme& s) const {
    // M3 focus indicator: 3 dp ring in Secondary, 2 dp outside the shape
    QPainterPathStroker outer;
    outer.setWidth(2 * (2 + 3));
    outer.setJoinStyle(Qt::RoundJoin);
    QPainterPathStroker inner;
    inner.setWidth(2 * 2);
    inner.setJoinStyle(Qt::RoundJoin);
    QPainterPath ring = (outer.createStroke(shape) + shape) - (inner.createStroke(shape) + shape);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(ring, s.secondary);
    painter->restore();
}

void M3Style::drawChevron(QPainter* painter, const QRectF& rect, Qt::ArrowType direction, QColor color, double size) const {
    // Material Symbols "expand_more" proportions: a 2 dp stroke, round caps
    double w = size > 0 ? size : qMin(12.0, qMin(rect.width(), rect.height()) * 0.5);
    double h = w / 2;
    QPointF c = rect.center();
    QPolygonF points;
    switch (direction) {
        case Qt::UpArrow:
            points << QPointF(-w / 2, h / 2) << QPointF(0, -h / 2) << QPointF(w / 2, h / 2);
            break;
        case Qt::LeftArrow:
            points << QPointF(h / 2, -w / 2) << QPointF(-h / 2, 0) << QPointF(h / 2, w / 2);
            break;
        case Qt::RightArrow:
            points << QPointF(-h / 2, -w / 2) << QPointF(h / 2, 0) << QPointF(-h / 2, w / 2);
            break;
        default:
            points << QPointF(-w / 2, -h / 2) << QPointF(0, h / 2) << QPointF(w / 2, -h / 2);
            break;
    }
    points.translate(c);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPolyline(points);
    painter->restore();
}

// ---- Buttons ------------------------------------------------------------------

namespace {
    // Platform/hover=merge (default) or layer (plain M3 state layer); a widget
    // can opt out with the property m3HoverMerge=false
    bool hoverMerge(const QWidget* widget) {
        static const bool enabled = [] {
            tSettings settings(QStringLiteral("theSuite"), QStringLiteral("theDesk.platform"));
            return settings.value(QStringLiteral("Platform/hover")).toString() != QStringLiteral("layer");
        }();
        if (!enabled || !widget) return false;
        QVariant opt = widget->property("m3HoverMerge");
        return !opt.isValid() || opt.toBool();
    }

    // Symbolic (single-colour) icons follow the label into the merge fill;
    // full-colour icons keep their colours
    QIcon mergeIcon(const QIcon& icon, QSize size, QColor colour, QIcon::State state = QIcon::Off) {
        if (icon.isNull()) return icon;
        QImage image = icon.pixmap(size, QIcon::Normal, state).toImage().convertToFormat(QImage::Format_ARGB32);
        double minL = 1, maxL = 0;
        for (int y = 0; y < image.height(); y++) {
            auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); x++) {
                if (qAlpha(line[x]) < 128) continue;
                QColor c = QColor::fromRgb(line[x]);
                minL = qMin(minL, c.lightnessF());
                maxL = qMax(maxL, c.lightnessF());
            }
        }
        if (maxL - minL > 0.25) return icon; // shaded: a full-colour icon
        QPainter tint(&image);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(image.rect(), colour);
        tint.end();
        QIcon tinted;
        tinted.addPixmap(QPixmap::fromImage(image), QIcon::Normal, QIcon::Off);
        tinted.addPixmap(QPixmap::fromImage(image), QIcon::Normal, QIcon::On);
        return tinted;
    }
} // namespace

namespace {
    // ---- M3 Expressive buttons ----------------------------------------------
    // Widget properties (set before the widget is shown):
    //   m3ButtonSize    xs | s (default) | m | l | xl
    //   m3ButtonStyle   filled | tonal | outlined | elevated | text
    //                   (default: the default button filled, flat ones text,
    //                   the rest tonal)
    //   m3ButtonShape   round (default) | square
    //   m3IconButtonStyle  standard | filled | tonal | outlined (icon-only
    //                   tool buttons; default standard when auto-raised)
    //   m3IconButtonWidth  narrow | default | wide
    //   m3ButtonGroup="connected" on a container: its buttons join up
    // Checkable buttons are toggle buttons: the shape morphs (round to square,
    // or square to round) and the colours change when selected.

    const ButtonSize& buttonSize(const QWidget* widget) {
        // ButtonXSmall/Small/Medium/Large/XLargeTokens, IconButton*Tokens
        static const ButtonSize sizes[] = {
            {32, 12, 20, 14, 500, 12, 8, 4, 4, 28, 40},
            {40, 16, 20, 14, 500, 12, 8, 8, 4, 32, 52},
            {56, 24, 24, 16, 500, 16, 12, 8, 4, 48, 72},
            {96, 48, 32, 24, 400, 28, 16, 16, 8, 64, 128},
            {136, 64, 40, 32, 400, 28, 16, 20, 12, 104, 184},
        };
        const QString v = widget ? widget->property("m3ButtonSize").toString().toLower() : QString();
        if (v == QLatin1String("xs")) return sizes[0];
        if (v == QLatin1String("m")) return sizes[2];
        if (v == QLatin1String("l")) return sizes[3];
        if (v == QLatin1String("xl")) return sizes[4];
        return sizes[1];
    }

    bool hasButtonSize(const QWidget* widget) {
        return widget && widget->property("m3ButtonSize").isValid();
    }


    ButtonKind kindFromString(const QString& v, ButtonKind fallback) {
        const QString k = v.toLower();
        if (k == QLatin1String("filled")) return ButtonKind::Filled;
        if (k == QLatin1String("tonal")) return ButtonKind::Tonal;
        if (k == QLatin1String("outlined")) return ButtonKind::Outlined;
        if (k == QLatin1String("elevated")) return ButtonKind::Elevated;
        if (k == QLatin1String("text")) return ButtonKind::Text;
        if (k == QLatin1String("standard")) return ButtonKind::Standard;
        return fallback;
    }

    bool isToggle(const QWidget* widget) {
        auto* button = qobject_cast<const QAbstractButton*>(widget);
        return button && button->isCheckable();
    }

    bool squareFamily(const QWidget* widget) {
        return widget && widget->property("m3ButtonShape").toString().toLower() == QLatin1String("square");
    }

    // Where a button sits in a connected button group
    enum class GroupPosition {
        None,
        First,
        Middle,
        Last
    };

    bool connectedGroup(const QWidget* container) {
        return container && container->property("m3ButtonGroup").toString().toLower() == QLatin1String("connected");
    }

    GroupPosition groupPosition(const QWidget* widget) {
        if (!widget || !connectedGroup(widget->parentWidget())) return GroupPosition::None;
        QList<const QWidget*> buttons;
        for (QObject* child : widget->parentWidget()->children()) {
            auto* button = qobject_cast<QAbstractButton*>(child);
            if (button && button->isVisibleTo(widget->parentWidget())) buttons.append(button);
        }
        if (buttons.size() < 2) return GroupPosition::None;
        std::sort(buttons.begin(), buttons.end(), [](const QWidget* a, const QWidget* b) {
            return a->x() < b->x();
        });
        const int index = buttons.indexOf(widget);
        bool first = index == 0, last = index == buttons.size() - 1;
        if (widget->layoutDirection() == Qt::RightToLeft) std::swap(first, last);
        return first ? GroupPosition::First : last ? GroupPosition::Last : GroupPosition::Middle;
    }

    // The container: round or square, morphing to the other when selected,
    // tighter while pressed; in a connected group the inner corners are small
    // and the selected button rounds off completely
    QPainterPath buttonShape(const QRectF& r, const ButtonSize& size, bool square, double checked, double press, GroupPosition position) {
        const double full = r.height() / 2;
        if (position == GroupPosition::None) {
            double radius = lerp(square ? size.square : full, square ? full : size.square, checked);
            return roundedRect(r, qMax(0.0, lerp(radius, size.pressed, press)));
        }
        const double outer = lerp(full, size.pressed, press);
        const double inner = lerp(lerp(size.inner, full, checked), size.inner / 2, press);
        const double left = position == GroupPosition::First ? outer : inner;
        const double right = position == GroupPosition::Last ? outer : inner;
        return roundedRect(r, left, right, right, left);
    }

    struct ButtonColours {
            QColor container;
            QColor content;
            QColor mergeFill;    // hover-merge: the fill that grows in from the pointer
            QColor mergeContent; // and the label colour on top of it
            QColor outline = Qt::transparent;
            double elevation = 0; // 0..1: the elevated button's shadow
    };

    // Unselected and selected colours for a kind (FilledButtonTokens and the
    // toggle-button colours of the other ButtonTokens)
    ButtonColours kindColours(ButtonKind kind, bool toggle, bool selected, const M3::Scheme& s) {
        auto opaque = [](QColor container, QColor content) {
            return ButtonColours{container, content, content, container};
        };
        if (selected) {
            switch (kind) {
                case ButtonKind::Outlined:
                    return opaque(s.inverseSurface, s.inverseOnSurface);
                case ButtonKind::Tonal:
                    return opaque(s.secondary, s.onSecondary);
                case ButtonKind::Standard:
                    return {Qt::transparent, s.primary, s.primary, s.surface};
                default:
                    return {s.primary, s.onPrimary, s.inverseSurface, s.inversePrimary};
            }
        }
        switch (kind) {
            case ButtonKind::Filled:
                // A filled button would turn pale on a pale surface, so it
                // merges into the inverse surface (the snackbar pair)
                if (toggle) return opaque(s.surfaceContainer, s.onSurfaceVariant);
                return {s.primary, s.onPrimary, s.inverseSurface, s.inversePrimary};
            case ButtonKind::Tonal:
                return opaque(s.secondaryContainer, s.onSecondaryContainer);
            case ButtonKind::Outlined: {
                ButtonColours c{Qt::transparent, s.onSurfaceVariant, s.onSurfaceVariant, s.surface};
                c.outline = s.outlineVariant;
                return c;
            }
            case ButtonKind::Elevated: {
                ButtonColours c = opaque(s.surfaceContainerLow, s.primary);
                c.elevation = 1;
                return c;
            }
            case ButtonKind::Text:
                return {Qt::transparent, s.primary, s.primary, s.onPrimary};
            case ButtonKind::Standard:
                return {Qt::transparent, s.onSurfaceVariant, s.onSurfaceVariant, s.surface};
        }
        return opaque(s.secondaryContainer, s.onSecondaryContainer);
    }

    ButtonColours blend(const ButtonColours& a, const ButtonColours& b, double t) {
        t = qBound(0.0, t, 1.0);
        if (t <= 0) return a;
        if (t >= 1) return b;
        return {mixColor(a.container, b.container, t), mixColor(a.content, b.content, t), mixColor(a.mergeFill, b.mergeFill, t),
            mixColor(a.mergeContent, b.mergeContent, t), mixColor(a.outline, b.outline, t), lerp(a.elevation, b.elevation, t)};
    }

    ButtonColours disabledColours(ButtonKind kind, const M3::Scheme& s) {
        const bool contained = kind != ButtonKind::Text && kind != ButtonKind::Outlined && kind != ButtonKind::Standard;
        ButtonColours c{contained ? alpha(s.onSurface, 0.10) : QColor(Qt::transparent), alpha(s.onSurface, Opacity::DisabledContent), Qt::transparent, Qt::transparent};
        if (kind == ButtonKind::Outlined) c.outline = alpha(s.onSurface, 0.12);
        return c;
    }

    ButtonKind pushButtonKind(const QStyleOptionButton* button, const QWidget* widget) {
        ButtonKind fallback = ButtonKind::Tonal;
        if (button->features & QStyleOptionButton::Flat) fallback = ButtonKind::Text;
        else if (button->features & QStyleOptionButton::DefaultButton) fallback = ButtonKind::Filled;
        else if (groupPosition(widget) != GroupPosition::None) fallback = ButtonKind::Filled; // connected groups: filled toggles
        ButtonKind kind = kindFromString(widget ? widget->property("m3ButtonStyle").toString() : QString(), fallback);
        return kind == ButtonKind::Standard ? ButtonKind::Text : kind;
    }

    ButtonColours buttonColours(const QStyleOptionButton* button, const M3::Scheme& s, const QWidget* widget, double checked) {
        bool on = button->state & QStyle::State_Enabled;
        if (navItem(widget)) {
            // Hovering grows a SurfaceContainerHighest pill in from the pointer
            // (distinct from the SecondaryContainer selection)
            bool selected = button->state & QStyle::State_On;
            if (!on) return {Qt::transparent, alpha(s.onSurface, Opacity::DisabledContent)};
            if (selected) return {s.secondaryContainer, s.onSecondaryContainer, s.onSecondaryContainer, s.secondaryContainer};
            return {Qt::transparent, s.onSurfaceVariant, s.surfaceContainerHighest, s.onSurface};
        }
        const ButtonKind kind = pushButtonKind(button, widget);
        if (!on) return disabledColours(kind, s);
        const bool toggle = isToggle(widget) || (button->state & QStyle::State_On);
        if (!toggle) return kindColours(kind, false, false, s);
        return blend(kindColours(kind, true, false, s), kindColours(kind, true, true, s), checked);
    }

    // The push button's container: its size's height, centred
    QRectF pushButtonRect(const QStyleOptionButton* button, const QWidget* widget) {
        QRectF r = QRectF(button->rect);
        if (navItem(widget)) return r;
        const int height = buttonSize(widget).height;
        if (r.height() > height + 8) r = centred(r, r.width(), height);
        return r;
    }

    QPainterPath pushButtonShape(const QStyleOptionButton* button, const QWidget* widget, double checked, double press) {
        const QRectF r = pushButtonRect(button, widget);
        if (navItem(widget)) return roundedRect(r, qMax(0.0, lerp(r.height() / 2, Shape::Small, press)));
        const bool toggle = isToggle(widget) || (button->state & QStyle::State_On);
        return buttonShape(r, buttonSize(widget), squareFamily(widget), toggle ? checked : 0, press, groupPosition(widget));
    }

    // A level-1 shadow under elevated buttons (no compositor: drawn, not a
    // window effect)
    void drawElevation(QPainter* painter, const QPainterPath& shape, double amount) {
        if (amount <= 0.01) return;
        for (int i = 3; i >= 1; i--) {
            QPainterPath spread = shape.translated(0, 1);
            QPainterPathStroker stroker;
            stroker.setWidth(i * 1.2);
            fill(painter, stroker.createStroke(spread).united(spread), QColor(0, 0, 0, qRound(amount * 18)));
        }
    }
} // namespace

void M3Style::drawButton(const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
    if (!button) return;
    const M3::Scheme& s = scheme(option->palette);
    const double checked = qBound(0.0, animator->checked(widget, option->state & State_On), 1.0);
    const double press = animator->press(widget, option->state & State_Sunken);
    ButtonColours colours = buttonColours(button, s, widget, checked);

    // M3E: the shape squares off towards the pressed corner while pressed,
    // and toggles morph round <-> square when selected
    const QRectF r = pushButtonRect(button, widget);
    QPainterPath shape = pushButtonShape(button, widget, checked, press);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    drawElevation(painter, shape, colours.elevation * (1 - press * 0.5));
    fill(painter, shape, colours.container);
    if (colours.outline.alpha() > 0) {
        painter->setPen(QPen(colours.outline, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(shape); // the 1 dp outline
        painter->setPen(Qt::NoPen);
    }
    if (enabled(option)) {
        double mergeOpacity = 0;
        QPainterPath merged = hoverMerge(widget) ? animator->mergePath(widget, shape, &mergeOpacity) : QPainterPath();
        if (!merged.isEmpty()) {
            fill(painter, merged, alpha(colours.mergeFill, mergeOpacity));
        } else if (!hoverMerge(widget)) {
            double hover = animator->hover(widget, option->state & State_MouseOver);
            fill(painter, shape, alpha(colours.content, Opacity::Hover * hover));
        }
        if (keyboardFocus(option)) fill(painter, shape, alpha(colours.content, Opacity::Focus));
        animator->paintRipples(painter, widget, shape, mergeOpacity > 0.5 ? colours.mergeContent : colours.content);
    }
    if (button->features & QStyleOptionButton::HasMenu) {
        drawChevron(painter, QRectF(r.right() - buttonSize(widget).padding - 16, r.top(), 16, r.height()), Qt::DownArrow, colours.content, 8);
    }
    painter->restore();
    if (keyboardFocus(option)) drawFocusRing(painter, shape, s);
}

void M3Style::drawToolButton(const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const {
    auto* tool = qstyleoption_cast<const QStyleOptionToolButton*>(option);
    if (!tool) return;
    const M3::Scheme& s = scheme(option->palette);

    QRect buttonRect = subControlRect(CC_ToolButton, tool, SC_ToolButton, widget);
    QRect menuRect = subControlRect(CC_ToolButton, tool, SC_ToolButtonMenu, widget);
    const bool split = tool->subControls & SC_ToolButtonMenu;
    const bool iconOnly = tool->toolButtonStyle == Qt::ToolButtonIconOnly || tool->text.isEmpty();
    const bool raised = !(option->state & State_AutoRaise);
    const bool on = option->state & State_On;
    const bool toggle = isToggle(widget) || on;
    const ButtonSize& size = buttonSize(widget);
    const double checked = qBound(0.0, animator->checked(widget, on), 1.0);
    const double press = animator->press(widget, option->state & State_Sunken);

    // Icon buttons: standard (no container), filled, tonal, outlined; text
    // and split buttons: the push button kinds
    ButtonKind kind;
    if (split) kind = kindFromString(widget ? widget->property("m3ButtonStyle").toString() : QString(), ButtonKind::Tonal);
    else if (iconOnly) kind = kindFromString(widget ? widget->property("m3IconButtonStyle").toString() : QString(), raised ? ButtonKind::Tonal : ButtonKind::Standard);
    else kind = kindFromString(widget ? widget->property("m3ButtonStyle").toString() : QString(), raised ? ButtonKind::Tonal : ButtonKind::Standard);
    ButtonColours colours = !enabled(option) ? disabledColours(kind, s)
                          : toggle           ? blend(kindColours(kind, true, false, s), kindColours(kind, true, true, s), checked)
                                             : kindColours(kind, false, false, s);
    const QColor content = colours.content;

    QPainterPath shape;
    QRectF r;
    if (split) {
        // Split button: a leading action and a trailing menu button 2 dp apart,
        // their inner corners small; the trailing one rounds into a circle
        // while its menu is open
        const bool menuOpen = (tool->activeSubControls & SC_ToolButtonMenu) && (option->state & State_Sunken);
        const bool menuOnRight = menuRect.center().x() > buttonRect.center().x();
        QRectF lead = QRectF(buttonRect).adjusted(menuOnRight ? 0 : 1, 0, menuOnRight ? -1 : 0, 0);
        QRectF trail = QRectF(menuRect).adjusted(menuOnRight ? 1 : 0, 0, menuOnRight ? 0 : -1, 0);
        const double full = lead.height() / 2;
        const double leadPress = (option->state & State_Sunken) && !menuOpen ? press : 0;
        const double outer = lerp(full, size.pressed, leadPress);
        const double inner = size.splitInner;
        const double trailInner = menuOpen ? full : inner;
        if (menuOpen) trail = centred(trail, trail.height(), trail.height());
        QPainterPath leadShape = menuOnRight ? roundedRect(lead, outer, inner, inner, outer) : roundedRect(lead, inner, outer, outer, inner);
        QPainterPath trailShape = menuOnRight ? roundedRect(trail, trailInner, full, full, trailInner) : roundedRect(trail, full, trailInner, trailInner, full);
        shape = leadShape.united(trailShape);
        r = lead.united(trail);
        menuRect = trail.toRect();
    } else {
        r = QRectF(buttonRect);
        if (iconOnly) {
            // Icon buttons: square by default; sized ones take their height and width
            double h = qMin(r.width(), r.height()), w = h;
            if (hasButtonSize(widget) || (widget && widget->property("m3IconButtonWidth").isValid())) {
                h = qMin<double>(r.height(), size.height);
                const QString width = widget->property("m3IconButtonWidth").toString().toLower();
                w = width == QLatin1String("narrow") ? size.narrow : width == QLatin1String("wide") ? size.wide : h;
                w = qMin(w, r.width());
            }
            r = centred(r, w, h);
        }
        ButtonSize shapeSize = size;
        if (!hasButtonSize(widget)) shapeSize.square = Shape::Medium; // SmallIconButtonTokens
        shape = buttonShape(r, shapeSize, squareFamily(widget), toggle ? checked : 0, press, groupPosition(widget));
    }

    // Hover-merge: buttons with a label fill solid with their content colour
    // and the label inverts; icon-only ones grow a state layer from the pointer
    const bool fullMerge = hoverMerge(widget) && !iconOnly;
    const QColor mergeContent = colours.mergeContent;
    double mergeOpacity = 0;
    QPainterPath merged;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    drawElevation(painter, shape, colours.elevation * (1 - press * 0.5));
    fill(painter, shape, colours.container);
    if (colours.outline.alpha() > 0) {
        painter->setPen(QPen(colours.outline, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(shape);
        painter->setPen(Qt::NoPen);
    }
    if (enabled(option)) {
        if (hoverMerge(widget)) merged = animator->mergePath(widget, shape, &mergeOpacity);
        if (fullMerge) {
            if (!merged.isEmpty()) fill(painter, merged, alpha(colours.mergeFill, mergeOpacity));
        } else if (hoverMerge(widget)) {
            if (!merged.isEmpty()) fill(painter, merged, alpha(content, Opacity::Hover * mergeOpacity));
        } else {
            double hover = animator->hover(widget, option->state & State_MouseOver);
            fill(painter, shape, alpha(content, Opacity::Hover * hover));
        }
        animator->paintRipples(painter, widget, shape, fullMerge && mergeOpacity > 0.5 ? mergeContent : content);
    }
    if (split) {
        const bool menuOpen = (tool->activeSubControls & SC_ToolButtonMenu) && (option->state & State_Sunken);
        drawChevron(painter, menuRect, menuOpen ? Qt::UpArrow : Qt::DownArrow, content, 8);
    }
    painter->restore();

    QStyleOptionToolButton label = *tool;
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        label.palette.setColor(group, QPalette::ButtonText, content);
        label.palette.setColor(group, QPalette::WindowText, content);
    }
    label.rect = split ? buttonRect : r.toRect();
    label.state &= ~State_Sunken; // no label shift; State_On stays so selected symbols fill in
    label.font = withWeight(tool->font, QFont::Weight(size.weight));
    // Symbols take the content colour (filled buttons need OnPrimary, not the
    // window text the icon engine paints with); full-colour icons stay as they are
    label.icon = mergeIcon(label.icon, label.iconSize, content, on ? QIcon::On : QIcon::Off);
    if ((tool->features & QStyleOptionToolButton::HasMenu) && !split && !iconOnly) label.rect.adjust(0, 0, -16, 0);
    proxy()->drawControl(CE_ToolButtonLabel, &label, painter, widget);
    if (fullMerge && !merged.isEmpty()) {
        QStyleOptionToolButton inverted = label;
        for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            inverted.palette.setColor(group, QPalette::ButtonText, mergeContent);
            inverted.palette.setColor(group, QPalette::WindowText, mergeContent);
        }
        inverted.icon = mergeIcon(tool->icon, inverted.iconSize, mergeContent, on ? QIcon::On : QIcon::Off);
        painter->save();
        painter->setClipPath(merged, Qt::IntersectClip);
        painter->setOpacity(painter->opacity() * mergeOpacity);
        proxy()->drawControl(CE_ToolButtonLabel, &inverted, painter, widget);
        painter->restore();
    }
    if ((tool->features & QStyleOptionToolButton::HasMenu) && !split) {
        QRectF arrow = iconOnly ? QRectF(r.right() - 10, r.bottom() - 10, 8, 8) : QRectF(r.right() - size.padding - 8, r.top(), 12, r.height());
        drawChevron(painter, arrow, Qt::DownArrow, content, iconOnly ? 6 : 8);
    }
    if (keyboardFocus(option)) drawFocusRing(painter, shape, s);
}

// ---- Selection controls -------------------------------------------------------

void M3Style::drawCheckBox(const QStyleOption* option, QPainter* painter, const QWidget* widget, bool stateLayer) const {
    const M3::Scheme& s = scheme(option->palette);
    QRectF r = QRectF(option->rect);
    bool partial = option->state & State_NoChange;
    bool on = (option->state & State_On) || partial;
    double progress = stateLayer ? qBound(0.0, animator->checked(widget, on), 1.0) : (on ? 1 : 0);
    if (partial) progress = 1;
    double box = qMin(18.0, qMin(r.width(), r.height()) - 2);
    QRectF b = centred(r, box, box);
    bool active = enabled(option);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (stateLayer && active) {
        // CheckboxTokens.StateLayerSize: a 40 dp circle
        QPainterPath layer;
        layer.addEllipse(centred(r, StateLayerSize, StateLayerSize));
        QColor layerColour = on ? s.primary : s.onSurface;
        double hover = animator->hover(widget, option->state & State_MouseOver);
        fill(painter, layer, alpha(layerColour, Opacity::Hover * hover + (keyboardFocus(option) ? Opacity::Focus : 0)));
        animator->paintRipples(painter, widget, layer, layerColour);
    }

    QColor selected = active ? s.primary : alpha(s.onSurface, Opacity::DisabledContent);
    QColor outline = active ? ((option->state & State_MouseOver) ? s.onSurface : s.onSurfaceVariant) : alpha(s.onSurface, Opacity::DisabledContent);
    QPainterPath container = roundedRect(b, 2);
    if (progress < 1) {
        painter->setPen(QPen(outline, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(roundedRect(b.adjusted(1, 1, -1, -1), 1));
    }
    if (progress > 0) fill(painter, container, alpha(selected, progress));

    QColor mark = active ? s.onPrimary : s.surface;
    painter->setPen(QPen(mark, 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    const double u = box / 18.0;
    if (partial) {
        painter->drawLine(QPointF(b.left() + 4 * u, b.center().y()), QPointF(b.right() - 4 * u, b.center().y()));
    } else if (progress > 0) {
        // The check draws itself in as the box fills (Compose CheckDrawingCache)
        QPointF a(b.left() + 4.2 * u, b.top() + 9.2 * u), m(b.left() + 7.4 * u, b.top() + 12.4 * u), e(b.left() + 13.8 * u, b.top() + 5.8 * u);
        double l1 = QLineF(a, m).length(), l2 = QLineF(m, e).length();
        double drawn = (l1 + l2) * qBound(0.0, progress, 1.0);
        QPainterPath check(a);
        if (drawn <= l1) {
            check.lineTo(a + (m - a) * (drawn / l1));
        } else {
            check.lineTo(m);
            check.lineTo(m + (e - m) * ((drawn - l1) / l2));
        }
        painter->drawPath(check);
    }
    painter->restore();
}

void M3Style::drawRadio(const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    const M3::Scheme& s = scheme(option->palette);
    QRectF r = QRectF(option->rect);
    bool on = option->state & State_On;
    double progress = qBound(0.0, animator->checked(widget, on), 1.2);
    bool active = enabled(option);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (active) {
        QPainterPath layer;
        layer.addEllipse(centred(r, StateLayerSize, StateLayerSize));
        QColor layerColour = on ? s.primary : s.onSurface;
        double hover = animator->hover(widget, option->state & State_MouseOver);
        fill(painter, layer, alpha(layerColour, Opacity::Hover * hover + (keyboardFocus(option) ? Opacity::Focus : 0)));
        animator->paintRipples(painter, widget, layer, layerColour);
    }
    // RadioButtonTokens: 20 dp icon, 2 dp stroke, 12 dp dot
    QColor colour = !active ? alpha(s.onSurface, Opacity::DisabledContent) : on ? s.primary
                                                                              : ((option->state & State_MouseOver) ? s.onSurface : s.onSurfaceVariant);
    painter->setPen(QPen(colour, 2));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(r.center(), 9, 9);
    if (progress > 0.01) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(colour);
        painter->drawEllipse(r.center(), 6 * progress, 6 * progress);
    }
    painter->restore();
}

void M3Style::drawSwitch(const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    const M3::Scheme& s = scheme(option->palette);
    QRectF r = QRectF(option->rect);
    // SwitchTokens: 52x32 track, 2 dp outline; handle 16 off, 24 on, 28 pressed
    QRectF track = centred(r, 52, 32);
    bool on = option->state & State_On;
    bool active = enabled(option);
    bool pressed = option->state & State_Sunken;

    double drag = widget ? widget->property("m3DragPosition").toDouble() : -1;
    double position = drag >= 0 ? drag : animator->checked(widget, on);
    double colourMix = qBound(0.0, position, 1.0);
    double hover = active ? animator->hover(widget, option->state & State_MouseOver) : 0;

    auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
    const QIcon icon = button ? button->icon : QIcon();
    double handleSize = pressed ? 28 : lerp(16, 24, colourMix);
    if (!icon.isNull() && !pressed) handleSize = 24;
    double left = track.left() + 16, right = track.right() - 16;
    QPointF handleCentre(lerp(left, right, position), track.center().y());

    QColor trackOff = active ? s.surfaceContainerHighest : alpha(s.surfaceContainerHighest, Opacity::DisabledContainer);
    QColor trackOn = active ? s.primary : alpha(s.onSurface, Opacity::DisabledContainer);
    QColor outline = active ? s.outline : alpha(s.onSurface, Opacity::DisabledContainer);
    QColor handleOff = active ? (hover > 0.5 || pressed ? s.onSurfaceVariant : s.outline) : alpha(s.onSurface, Opacity::DisabledContent);
    QColor handleOn = active ? (hover > 0.5 || pressed ? s.primaryContainer : s.onPrimary) : s.surface;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath trackPath = roundedRect(track, 16);
    fill(painter, trackPath, mixColor(trackOff, trackOn, colourMix));
    if (colourMix < 1) {
        painter->setPen(QPen(alpha(outline, 1 - colourMix), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(roundedRect(track.adjusted(1, 1, -1, -1), 15));
    }

    if (active) {
        QPainterPath layer;
        layer.addEllipse(handleCentre, StateLayerSize / 2.0, StateLayerSize / 2.0);
        QColor layerColour = on ? s.primary : s.onSurface;
        fill(painter, layer, alpha(layerColour, Opacity::Hover * hover + (pressed ? Opacity::Pressed : 0) + (keyboardFocus(option) ? Opacity::Focus : 0)));
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(mixColor(handleOff, handleOn, colourMix));
    painter->drawEllipse(handleCentre, handleSize / 2, handleSize / 2);
    if (!icon.isNull() && colourMix > 0.5) {
        QRect iconRect = centred(QRectF(handleCentre - QPointF(8, 8), QSizeF(16, 16)), 16, 16).toRect();
        icon.paint(painter, iconRect);
    }
    painter->restore();
    if (keyboardFocus(option)) drawFocusRing(painter, trackPath, s);
}

// ---- Fields ---------------------------------------------------------------------

void M3Style::drawField(const QRectF& rect, const QStyleOption* option, QPainter* painter, const QWidget* widget, bool focused) const {
    // FilledTextFieldTokens: SurfaceContainerHighest, CornerExtraSmallTop,
    // 1 dp OnSurfaceVariant indicator, 2 dp Primary when focused
    const M3::Scheme& s = scheme(option->palette);
    bool active = enabled(option);
    if (searchBar(widget)) {
        // SearchBarTokens: SurfaceContainerHighest pill; focus shows in the caret
        QPainterPath pill = roundedRect(rect, rect.height() / 2);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        fill(painter, pill, active ? s.surfaceContainerHighest : alpha(s.onSurface, 0.04));
        double hover = active ? animator->hover(widget, option->state & State_MouseOver) : 0;
        if (hover > 0) fill(painter, pill, alpha(s.onSurface, Opacity::Hover * hover));
        painter->restore();
        return;
    }
    QRectF r = rect.height() > FieldHeight + 16 ? centred(rect, rect.width(), FieldHeight) : rect;
    QPainterPath shape = roundedRect(r, Shape::ExtraSmall, Shape::ExtraSmall, 0, 0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    fill(painter, shape, active ? s.surfaceContainerHighest : alpha(s.onSurface, 0.04));
    double hover = active ? animator->hover(widget, option->state & State_MouseOver) : 0;
    if (hover > 0 && !focused) fill(painter, shape, alpha(s.onSurface, Opacity::Hover * hover));

    QColor indicator = !active ? alpha(s.onSurface, Opacity::DisabledContent) : focused ? s.primary
                                                                                      : (hover > 0.5 ? s.onSurface : s.onSurfaceVariant);
    double thickness = focused ? 2 : 1;
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->fillRect(QRectF(r.left(), r.bottom() - thickness + 1, r.width(), thickness), indicator);
    painter->restore();
}

// ---- Slider, scroll bar, progress ---------------------------------------------

void M3Style::drawSlider(const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const {
    auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option);
    if (!slider) return;
    const M3::Scheme& s = scheme(option->palette);
    const bool horizontal = slider->orientation == Qt::Horizontal;
    const bool active = enabled(option);
    const QRectF r = QRectF(option->rect);
    const QRectF handleRect = QRectF(subControlRect(CC_Slider, slider, SC_SliderHandle, widget));

    // Work along the slider's axis: "along" runs from the minimum end
    const bool reversed = horizontal ? (slider->upsideDown != (slider->direction == Qt::RightToLeft)) : !slider->upsideDown;
    const double length = horizontal ? r.width() : r.height();
    double handleCentre = horizontal ? handleRect.center().x() - r.left() : handleRect.center().y() - r.top();
    if (reversed) handleCentre = length - handleCentre;
    auto band = [&](double from, double to, double thickness) {
        // A rect from..to along the axis, `thickness` across it, centred
        if (reversed) {
            double f = length - to;
            to = length - from;
            from = f;
        }
        if (horizontal) return QRectF(r.left() + from, r.center().y() - thickness / 2, to - from, thickness);
        return QRectF(r.center().x() - thickness / 2, r.top() + from, thickness, to - from);
    };
    // Corner radii for a band whose "min" end and "max" end differ
    auto bandPath = [&](const QRectF& b, double minEnd, double maxEnd) {
        double startR = reversed ? maxEnd : minEnd, endR = reversed ? minEnd : maxEnd;
        if (horizontal) return roundedRect(b, startR, endR, endR, startR);
        return roundedRect(b, startR, startR, endR, endR);
    };

    // SliderTokens: 16 dp track, 4 dp handle (2 dp pressed/focused), 6 dp gaps
    const double press = qBound(0.0, animator->press(widget, option->state & State_Sunken), 1.0);
    const double handleWidth = lerp(4, 2, qMax(press, keyboardFocus(option) ? 1.0 : 0.0));
    const double gap = 6;
    const double track = 16;
    const double outer = track / 2, inner = 2; // full outside, 2 dp at the handle

    QColor activeTrack = active ? s.primary : alpha(s.onSurface, Opacity::DisabledContent);
    QColor inactiveTrack = active ? s.secondaryContainer : alpha(s.onSurface, Opacity::DisabledContainer);
    QColor handleColour = active ? s.primary : alpha(s.onSurface, Opacity::DisabledContent);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    double activeEnd = handleCentre - handleWidth / 2 - gap;
    double inactiveStart = handleCentre + handleWidth / 2 + gap;
    if (activeEnd > 0) fill(painter, bandPath(band(0, activeEnd, track), outer, inner), activeTrack);
    if (inactiveStart < length) {
        fill(painter, bandPath(band(inactiveStart, length, track), inner, outer), inactiveTrack);
    }

    // Stop indicators: at each tick, or just the end of the track
    auto dot = [&](double at, QColor colour) {
        QRectF d = band(at - 2, at + 2, 4);
        fill(painter, roundedRect(d, 2), colour);
    };
    if (slider->tickPosition != QSlider::NoTicks && slider->maximum > slider->minimum) {
        int interval = slider->tickInterval > 0 ? slider->tickInterval : slider->pageStep;
        if (interval > 0 && (slider->maximum - slider->minimum) / interval <= 64) {
            for (int v = slider->minimum; v <= slider->maximum; v += interval) {
                double at = outer + (length - 2 * outer) * double(v - slider->minimum) / (slider->maximum - slider->minimum);
                if (at < activeEnd - 2) dot(at, active ? s.secondaryContainer : s.surface);
                else if (at > inactiveStart + 2) dot(at, activeTrack);
            }
        }
    } else if (inactiveStart < length - outer) {
        dot(length - outer, activeTrack);
    }

    // The handle: a bar, 44 dp long
    QRectF handle = horizontal ? centred(handleRect, handleWidth, 44) : centred(handleRect, 44, handleWidth);
    fill(painter, roundedRect(handle, handleWidth / 2), handleColour);
    painter->restore();
}

void M3Style::drawScrollBar(const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const {
    auto* bar = qstyleoption_cast<const QStyleOptionSlider*>(option);
    if (!bar) return;
    const M3::Scheme& s = scheme(option->palette);
    bool horizontal = bar->orientation == Qt::Horizontal;
    QRectF slider = QRectF(subControlRect(CC_ScrollBar, bar, SC_ScrollBarSlider, widget));
    if (bar->minimum == bar->maximum) return;

    // A thin pill that thickens while hovered or dragged
    double hover = animator->hover(widget, option->state & State_MouseOver);
    bool dragging = (option->activeSubControls & SC_ScrollBarSlider) && (option->state & State_Sunken);
    double thickness = lerp(4, 8, dragging ? 1 : hover);
    QRectF thumb = horizontal ? QRectF(slider.left() + 2, slider.bottom() - 2 - thickness, slider.width() - 4, thickness)
                              : QRectF(slider.right() - 2 - thickness, slider.top() + 2, thickness, slider.height() - 4);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (hover > 0) {
        QRectF track = horizontal ? QRectF(option->rect.left(), thumb.top(), option->rect.width(), thickness) : QRectF(thumb.left(), option->rect.top(), thickness, option->rect.height());
        fill(painter, roundedRect(track, thickness / 2), alpha(s.onSurface, 0.06 * hover));
    }
    QColor colour = dragging ? s.onSurface : mixColor(alpha(s.onSurfaceVariant, 0.6), s.onSurfaceVariant, hover);
    fill(painter, roundedRect(thumb, thickness / 2), colour);
    painter->restore();
}

namespace {
    // Compose's linear indeterminate keyframes (ProgressIndicator.kt):
    // 1750 ms cycle, two lines with their own head/tail easing
    double bezier(double t, double x1, double y1, double x2, double y2) {
        QEasingCurve c(QEasingCurve::BezierSpline);
        c.addCubicBezierSegment(QPointF(x1, y1), QPointF(x2, y2), QPointF(1, 1));
        return c.valueForProgress(qBound(0.0, t, 1.0));
    }
} // namespace

void M3Style::drawProgress(const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    auto* bar = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
    if (!bar) return;
    const M3::Scheme& s = scheme(option->palette);
    const bool horizontal = bar->state & State_Horizontal;
    const bool active = enabled(option);
    QRectF r = QRectF(subElementRect(SE_ProgressBarGroove, option, widget));
    const double thickness = 4, gap = 4; // LinearProgressIndicatorTokens
    const double length = horizontal ? r.width() : r.height();
    const bool reversed = horizontal ? (bar->invertedAppearance != (bar->direction == Qt::RightToLeft)) : !bar->invertedAppearance;

    auto segment = [&](double from, double to) {
        from = qBound(0.0, from, length);
        to = qBound(0.0, to, length);
        if (to - from < 0.5) return QPainterPath();
        if (reversed) {
            double f = length - to;
            to = length - from;
            from = f;
        }
        QRectF b = horizontal ? QRectF(r.left() + from, r.center().y() - thickness / 2, to - from, thickness)
                              : QRectF(r.center().x() - thickness / 2, r.top() + from, thickness, to - from);
        return roundedRect(b, thickness / 2);
    };

    QColor activeColour = active ? s.primary : alpha(s.onSurface, Opacity::DisabledContent);
    QColor trackColour = active ? s.secondaryContainer : alpha(s.onSurface, Opacity::DisabledContainer);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (bar->minimum == bar->maximum) {
        double t = M3Animator::now() % 1750;
        double head1 = bezier(t / 1000, 0.2, 0, 0.8, 1), tail1 = bezier((t - 250) / 1000, 0.4, 0, 1, 1);
        double head2 = bezier((t - 650) / 850, 0, 0, 0.65, 1), tail2 = bezier((t - 900) / 850, 0.1, 0, 0.45, 1);
        QList<QPair<double, double>> lines = {{tail1 * length, head1 * length}, {tail2 * length, head2 * length}};
        // Track fills the space between the lines, with gaps
        double cursor = 0;
        for (auto line : {lines[1], lines[0]}) {
            if (line.second - line.first < 0.5) continue;
            fill(painter, segment(cursor, line.first - gap), trackColour);
            fill(painter, segment(line.first, line.second), activeColour);
            cursor = qMax(cursor, line.second + gap);
        }
        fill(painter, segment(cursor, length), trackColour);
    } else {
        double fraction = double(bar->progress - bar->minimum) / (bar->maximum - bar->minimum);
        double end = fraction * length;
        fill(painter, segment(0, end), activeColour);
        fill(painter, segment(end > 0 ? end + gap : 0, length), trackColour);
        // Stop indicator: a 4 dp dot at the end of the track
        if (end + gap < length - thickness) fill(painter, segment(length - thickness, length), activeColour);
    }
    painter->restore();
}

// ---- Menus ----------------------------------------------------------------------

void M3Style::drawMenuItem(const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
    if (!item) return;
    const M3::Scheme& s = scheme(option->palette);
    QRectF r = QRectF(option->rect).adjusted(MenuPadding, 0, -MenuPadding, 0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (item->menuItemType == QStyleOptionMenuItem::Separator) {
        painter->fillRect(QRectF(r.left() + 12, r.center().y(), r.width() - 24, 1), s.outlineVariant);
        painter->restore();
        return;
    }
    if (item->menuItemType == QStyleOptionMenuItem::EmptyArea) {
        painter->restore();
        return;
    }

    bool active = enabled(option);
    bool selected = (option->state & State_Selected) && active;
    bool checked = item->checkType != QStyleOptionMenuItem::NotCheckable && item->checked;

    // M3E menus: a selected (checked) item sits in TertiaryContainer with
    // CornerMedium; hover is an OnSurface state layer
    QColor content = !active ? alpha(s.onSurface, Opacity::DisabledContent) : checked ? s.onTertiaryContainer
                                                                                    : s.onSurface;
    QPainterPath shape = roundedRect(r.adjusted(0, 2, 0, -2), checked ? Shape::Medium : Shape::Small);
    if (checked && active) fill(painter, shape, s.tertiaryContainer);
    if (selected) fill(painter, shape, alpha(content, Opacity::Hover + (option->state & State_Sunken ? Opacity::Pressed : 0)));

    double x = r.left() + 12;
    const bool leadingColumn = item->maxIconWidth > 0 || item->checkType != QStyleOptionMenuItem::NotCheckable;
    QRectF leading(x, r.top(), 20, r.height());
    if (!item->icon.isNull()) {
        QIcon::Mode mode = active ? (selected ? QIcon::Active : QIcon::Normal) : QIcon::Disabled;
        item->icon.paint(painter, centred(leading, 20, 20).toRect(), Qt::AlignCenter, mode, checked ? QIcon::On : QIcon::Off);
    } else if (checked) {
        painter->setPen(QPen(content, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QRectF b = centred(leading, 18, 18);
        if (item->checkType == QStyleOptionMenuItem::Exclusive) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(content);
            painter->drawEllipse(b.center(), 4, 4);
        } else {
            QPainterPath check(QPointF(b.left() + 3.5, b.top() + 9.5));
            check.lineTo(b.left() + 7.5, b.top() + 13.5);
            check.lineTo(b.left() + 15, b.top() + 5.5);
            painter->drawPath(check);
        }
    }
    if (leadingColumn) x += 20 + 12;

    QString text = item->text;
    QString shortcut;
    int tab = text.indexOf(QLatin1Char('\t'));
    if (tab >= 0) {
        shortcut = text.mid(tab + 1);
        text = text.left(tab);
    }
    double right = r.right() - 12;
    if (item->menuItemType == QStyleOptionMenuItem::SubMenu) {
        drawChevron(painter, QRectF(right - 20, r.top(), 20, r.height()), option->direction == Qt::RightToLeft ? Qt::LeftArrow : Qt::RightArrow, active ? s.onSurfaceVariant : content, 8);
        right -= 24;
    }
    int flags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextSingleLine;
    if (!styleHint(SH_UnderlineShortcut, option, widget)) flags |= Qt::TextHideMnemonic;
    painter->setFont(withWeight(item->font, QFont::Medium)); // LabelLarge
    if (!shortcut.isEmpty()) {
        painter->setPen(active ? (checked ? s.onTertiaryContainer : s.onSurfaceVariant) : content);
        painter->setFont(item->font);
        painter->drawText(QRectF(x, r.top(), right - x, r.height()), flags | Qt::AlignRight, shortcut);
        painter->setFont(withWeight(item->font, QFont::Medium));
    }
    painter->setPen(content);
    painter->drawText(QRectF(x, r.top(), right - x, r.height()), flags | Qt::AlignLeft, text);
    painter->restore();
}

// ---- Loading indicator ----------------------------------------------------------

#if __has_include("shapes/m3loadingindicator.h")
    #include "shapes/m3loadingindicator.h"
    #define M3_HAVE_LOADING_INDICATOR 1
#endif

void M3Style::drawLoadingIndicator(const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    Q_UNUSED(widget)
    const M3::Scheme& s = scheme(option->palette);
    QRectF r = QRectF(option->rect);
    double side = qMin(r.width(), r.height());
    QRectF box = centred(r, side, side);
#ifdef M3_HAVE_LOADING_INDICATOR
    M3LoadingIndicator::paint(painter, box, M3Animator::now(), s.primary, QColor());
#else
    // Until the shape port lands: M3's circular indeterminate indicator
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    double t = M3Animator::now() % 1333 / 1333.0;
    painter->setPen(QPen(s.primary, qMax(2.0, side / 12), Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(box.adjusted(side / 8, side / 8, -side / 8, -side / 8), int(-t * 360 * 16), int(-(90 + 180 * std::sin(t * M_PI)) * 16));
    painter->restore();
#endif
}

// ---- Dispatch -------------------------------------------------------------------

void M3Style::drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    if (element == PE_M3Switch) return drawSwitch(option, painter, widget);
    if (element == PE_M3LoadingIndicator) return drawLoadingIndicator(option, painter, widget);
    const M3::Scheme& s = scheme(option->palette);

    switch (element) {
        case PE_PanelButtonCommand:
            return drawButton(option, painter, widget);
        case PE_IndicatorCheckBox:
            return drawCheckBox(option, painter, widget, true);
        case PE_IndicatorItemViewItemCheck:
            return drawCheckBox(option, painter, widget, false);
        case PE_IndicatorRadioButton:
            return drawRadio(option, painter, widget);
        case PE_PanelLineEdit: {
            auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
            if (frame && searchBar(widget)) return drawField(QRectF(option->rect), option, painter, widget, option->state & State_HasFocus);
            if (insideComposite(widget) || !frame || frame->lineWidth <= 0) return;
            return drawField(QRectF(option->rect), option, painter, widget, option->state & State_HasFocus);
        }
        case PE_PanelMenu: {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->fillRect(option->rect, s.surfaceContainer);
            painter->restore();
            return;
        }
        case PE_PanelTipLabel:
            painter->fillRect(option->rect, s.inverseSurface);
            return;
        case PE_FrameGroupBox: {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            fill(painter, roundedRect(QRectF(option->rect), Shape::Medium), s.surfaceContainerLow);
            painter->restore();
            return;
        }
        case PE_PanelItemViewItem: {
            auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
            QRectF r = QRectF(option->rect).adjusted(0, 1, 0, -1);
            double radius = qMin(Shape::Large, r.height() / 2);
            double startR = radius, endR = radius;
            if (item) {
                if (item->viewItemPosition == QStyleOptionViewItem::Middle) startR = endR = 0;
                else if (item->viewItemPosition == QStyleOptionViewItem::Beginning) endR = 0;
                else if (item->viewItemPosition == QStyleOptionViewItem::End) startR = 0;
            }
            if (option->direction == Qt::RightToLeft) std::swap(startR, endR);
            QPainterPath shape = roundedRect(r, startR, endR, endR, startR);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            if (option->state & State_Selected) fill(painter, shape, enabled(option) ? s.secondaryContainer : alpha(s.onSurface, Opacity::DisabledContainer));
            if ((option->state & State_MouseOver) && enabled(option)) fill(painter, shape, alpha(s.onSurface, Opacity::Hover));
            painter->restore();
            return;
        }
        case PE_PanelItemViewRow:
            return; // no alternating stripes in M3
        case PE_IndicatorBranch: {
            if (!(option->state & State_Children)) return;
            bool open = option->state & State_Open;
            drawChevron(painter, QRectF(option->rect), open ? Qt::DownArrow : (option->direction == Qt::RightToLeft ? Qt::LeftArrow : Qt::RightArrow), s.onSurfaceVariant, 8);
            return;
        }
        case PE_IndicatorArrowUp:
        case PE_IndicatorArrowDown:
        case PE_IndicatorArrowLeft:
        case PE_IndicatorArrowRight: {
            Qt::ArrowType type = element == PE_IndicatorArrowUp ? Qt::UpArrow : element == PE_IndicatorArrowDown ? Qt::DownArrow
                                                                            : element == PE_IndicatorArrowLeft   ? Qt::LeftArrow
                                                                                                                 : Qt::RightArrow;
            drawChevron(painter, QRectF(option->rect), type, enabled(option) ? s.onSurfaceVariant : alpha(s.onSurface, Opacity::DisabledContent));
            return;
        }
        case PE_IndicatorSpinUp:
        case PE_IndicatorSpinPlus:
            drawChevron(painter, QRectF(option->rect), Qt::UpArrow, enabled(option) ? s.onSurfaceVariant : alpha(s.onSurface, Opacity::DisabledContent), 8);
            return;
        case PE_IndicatorSpinDown:
        case PE_IndicatorSpinMinus:
            drawChevron(painter, QRectF(option->rect), Qt::DownArrow, enabled(option) ? s.onSurfaceVariant : alpha(s.onSurface, Opacity::DisabledContent), 8);
            return;
        case PE_IndicatorHeaderArrow: {
            auto* header = qstyleoption_cast<const QStyleOptionHeader*>(option);
            if (!header) return;
            drawChevron(painter, QRectF(option->rect), header->sortIndicator == QStyleOptionHeader::SortUp ? Qt::UpArrow : Qt::DownArrow, s.onSurfaceVariant, 8);
            return;
        }
        case PE_IndicatorTabClose: {
            QRectF r = centred(QRectF(option->rect), 16, 16);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            if (option->state & State_MouseOver) {
                QPainterPath layer;
                layer.addEllipse(r.adjusted(-4, -4, 4, 4));
                fill(painter, layer, alpha(s.onSurface, Opacity::Hover));
            }
            painter->setPen(QPen(s.onSurfaceVariant, 2, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(r.topLeft() + QPointF(4, 4), r.bottomRight() - QPointF(4, 4));
            painter->drawLine(r.topRight() + QPointF(-4, 4), r.bottomLeft() + QPointF(4, -4));
            painter->restore();
            return;
        }
        case PE_IndicatorToolBarSeparator: {
            QRectF r = QRectF(option->rect);
            if (option->state & State_Horizontal) painter->fillRect(QRectF(r.center().x(), r.top() + 8, 1, r.height() - 16), s.outlineVariant);
            else painter->fillRect(QRectF(r.left() + 8, r.center().y(), r.width() - 16, 1), s.outlineVariant);
            return;
        }
        case PE_FrameTabBarBase: {
            auto* base = qstyleoption_cast<const QStyleOptionTabBarBase*>(option);
            QRect r = base ? base->tabBarRect.united(base->selectedTabRect) : option->rect;
            painter->fillRect(QRect(option->rect.left(), r.bottom(), option->rect.width(), 1), s.surfaceVariant);
            return;
        }
        case PE_PanelButtonTool: {
            QPainterPath shape = roundedRect(QRectF(option->rect), option->rect.height() / 2.0);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            if (option->state & (State_On | State_Sunken)) fill(painter, shape, s.secondaryContainer);
            if (option->state & State_MouseOver) fill(painter, shape, alpha(s.onSurface, Opacity::Hover));
            painter->restore();
            return;
        }
        case PE_FrameLineEdit:
        case PE_Frame:
        case PE_FrameMenu:
        case PE_FrameWindow:
        case PE_FrameDockWidget:
        case PE_FrameStatusBarItem:
        case PE_FrameTabWidget:
        case PE_FrameButtonBevel:
        case PE_FrameButtonTool:
        case PE_FrameDefaultButton:
        case PE_FrameFocusRect:
        case PE_PanelScrollAreaCorner:
        case PE_PanelStatusBar:
        case PE_PanelToolBar:
        case PE_PanelMenuBar:
        case PE_IndicatorToolBarHandle:
            return; // flat M3 surfaces; focus is drawn by each control
        default:
            return QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
}

void M3Style::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    const M3::Scheme& s = scheme(option->palette);
    switch (element) {
        case CE_PushButtonBevel:
            return drawButton(option, painter, widget);
        case CE_PushButtonLabel: {
            auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
            if (!button) break;
            QStyleOptionButton label = *button;
            const double checked = qBound(0.0, animator->checked(widget, option->state & State_On), 1.0);
            ButtonColours colours = buttonColours(button, s, widget, checked);
            QColor content = colours.content;
            for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) label.palette.setColor(group, QPalette::ButtonText, content);
            // Symbols take the label's colour (OnPrimary on a filled button)
            label.icon = mergeIcon(label.icon, label.iconSize, content, (option->state & State_On) ? QIcon::On : QIcon::Off);
            label.state &= ~(State_Sunken | State_On);
            if (button->features & QStyleOptionButton::HasMenu) label.rect.adjust(0, 0, -20, 0);
            painter->save();
            {
                // LabelLarge (XS, S), TitleMedium (M), HeadlineSmall (L), HeadlineLarge (XL)
                const ButtonSize& size = buttonSize(widget);
                QFont font = painter->font();
                if (hasButtonSize(widget)) font.setPixelSize(size.fontPx);
                painter->setFont(withWeight(font, QFont::Weight(size.weight)));
            }
            auto drawLabel = [&](const QStyleOptionButton& o) {
                if (!navItem(widget)) return QProxyStyle::drawControl(element, &o, painter, widget);
                // Leading icon and text, 16 dp in (NavigationDrawerTokens)
                QRect r = o.rect.adjusted(16, 0, -16, 0);
                if (!o.icon.isNull()) {
                    QSize iconSize = o.iconSize.isValid() ? o.iconSize : QSize(20, 20);
                    QRect iconRect(QPoint(r.left(), r.center().y() - iconSize.height() / 2 + 1), iconSize);
                    o.icon.paint(painter, iconRect, Qt::AlignCenter, enabled(&o) ? QIcon::Normal : QIcon::Disabled);
                    r.setLeft(iconRect.right() + 12);
                }
                painter->setPen(o.palette.color(QPalette::ButtonText));
                painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeading, painter->fontMetrics().elidedText(o.text, Qt::ElideRight, r.width()));
            };
            drawLabel(label);
            if (enabled(option) && hoverMerge(widget)) {
                // Where the merge fill has reached, the label inverts
                double press = animator->press(widget, option->state & State_Sunken);
                double mergeOpacity = 0;
                QPainterPath merged = animator->mergePath(widget, pushButtonShape(button, widget, checked, press), &mergeOpacity);
                if (!merged.isEmpty()) {
                    QStyleOptionButton inverted = label;
                    QColor mergeContent = colours.mergeContent;
                    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) inverted.palette.setColor(group, QPalette::ButtonText, mergeContent);
                    inverted.icon = mergeIcon(inverted.icon, inverted.iconSize, mergeContent);
                    painter->setClipPath(merged, Qt::IntersectClip);
                    painter->setOpacity(painter->opacity() * mergeOpacity);
                    drawLabel(inverted);
                }
            }
            painter->restore();
            return;
        }
        case CE_ProgressBar:
            drawProgress(option, painter, widget);
            proxy()->drawControl(CE_ProgressBarLabel, option, painter, widget);
            return;
        case CE_ProgressBarGroove:
        case CE_ProgressBarContents:
            return drawProgress(option, painter, widget);
        case CE_ProgressBarLabel: {
            auto* bar = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
            QRect r = subElementRect(SE_ProgressBarLabel, option, widget);
            if (!bar || r.isEmpty()) return;
            painter->save();
            painter->setPen(enabled(option) ? s.onSurfaceVariant : alpha(s.onSurface, Opacity::DisabledContent));
            painter->drawText(r, Qt::AlignRight | Qt::AlignVCenter, bar->text);
            painter->restore();
            return;
        }
        case CE_MenuItem:
            return drawMenuItem(option, painter, widget);
        case CE_MenuBarItem: {
            auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
            if (!item) break;
            QRectF r = QRectF(option->rect).adjusted(2, 2, -2, -2);
            QPainterPath shape = roundedRect(r, r.height() / 2);
            bool open = option->state & State_Sunken;
            QColor content = !enabled(option) ? alpha(s.onSurface, Opacity::DisabledContent) : open ? s.onSecondaryContainer
                                                                                                  : s.onSurface;
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            if (open) fill(painter, shape, s.secondaryContainer);
            else if (option->state & State_Selected) fill(painter, shape, alpha(s.onSurface, Opacity::Hover));
            painter->setPen(content);
            painter->setFont(withWeight(item->font, QFont::Medium));
            int flags = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextSingleLine;
            if (!styleHint(SH_UnderlineShortcut, option, widget)) flags |= Qt::TextHideMnemonic;
            painter->drawText(r, flags, item->text);
            painter->restore();
            return;
        }
        case CE_MenuBarEmptyArea:
        case CE_MenuEmptyArea:
        case CE_ToolBar:
        case CE_Splitter:
        case CE_SizeGrip:
            return;
        case CE_TabBarTabShape: {
            auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
            if (!tab || (tab->shape != QTabBar::RoundedNorth && tab->shape != QTabBar::TriangularNorth)) break;
            QRectF r = QRectF(option->rect);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            if ((option->state & State_MouseOver) && enabled(option)) painter->fillRect(r, alpha(s.onSurface, Opacity::Hover));
            if (option->state & State_Selected) {
                // PrimaryNavigationTabTokens: 3 dp indicator as wide as the label
                double textWidth = qMax(24, option->fontMetrics.horizontalAdvance(tab->text) + (tab->icon.isNull() ? 0 : tab->iconSize.width() + 8));
                QRectF indicator(r.center().x() - textWidth / 2, r.bottom() - 2, textWidth, 3);
                fill(painter, roundedRect(indicator, 3, 3, 0, 0), enabled(option) ? s.primary : alpha(s.onSurface, Opacity::DisabledContent));
            }
            painter->restore();
            return;
        }
        case CE_TabBarTabLabel: {
            auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
            if (!tab) break;
            QStyleOptionTab label = *tab;
            QColor content = !enabled(option) ? alpha(s.onSurface, Opacity::DisabledContent) : (option->state & State_Selected) ? s.primary
                                                                                                                             : s.onSurfaceVariant;
            for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
                label.palette.setColor(group, QPalette::WindowText, content);
                label.palette.setColor(group, QPalette::ButtonText, content);
            }
            painter->save();
            painter->setFont(withWeight(painter->font(), QFont::Medium)); // TitleSmall
            QProxyStyle::drawControl(element, &label, painter, widget);
            painter->restore();
            return;
        }
        case CE_HeaderSection: {
            QRectF r = QRectF(option->rect);
            painter->fillRect(r, s.surface);
            if ((option->state & State_MouseOver) && enabled(option)) painter->fillRect(r, alpha(s.onSurface, Opacity::Hover));
            painter->fillRect(QRectF(r.left(), r.bottom(), r.width(), 1), s.outlineVariant);
            return;
        }
        case CE_HeaderLabel: {
            auto* header = qstyleoption_cast<const QStyleOptionHeader*>(option);
            if (!header) break;
            QStyleOptionHeader label = *header;
            for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
                label.palette.setColor(group, QPalette::ButtonText, s.onSurfaceVariant);
                label.palette.setColor(group, QPalette::WindowText, s.onSurfaceVariant);
            }
            painter->save();
            painter->setFont(withWeight(painter->font(), QFont::Medium));
            QProxyStyle::drawControl(element, &label, painter, widget);
            painter->restore();
            return;
        }
        case CE_ItemViewItem: {
            auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
            if (!item) break;
            QStyleOptionViewItem copy = *item;
            for (auto group : {QPalette::Active, QPalette::Inactive}) {
                copy.palette.setColor(group, QPalette::Highlight, s.secondaryContainer);
                copy.palette.setColor(group, QPalette::HighlightedText, s.onSecondaryContainer);
            }
            QProxyStyle::drawControl(element, &copy, painter, widget);
            return;
        }
        case CE_ShapedFrame: {
            auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
            if (frame && (frame->frameShape == QFrame::HLine || frame->frameShape == QFrame::VLine)) {
                QRectF r = QRectF(option->rect);
                if (frame->frameShape == QFrame::HLine) painter->fillRect(QRectF(r.left(), r.center().y(), r.width(), 1), s.outlineVariant);
                else painter->fillRect(QRectF(r.center().x(), r.top(), 1, r.height()), s.outlineVariant);
            }
            return;
        }
        case CE_RubberBand: {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            QPainterPath shape = roundedRect(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5), Shape::ExtraSmall);
            fill(painter, shape, alpha(s.primary, 0.12));
            painter->setPen(QPen(s.primary, 1));
            painter->drawPath(shape);
            painter->restore();
            return;
        }
        default:
            break;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void M3Style::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const {
    const M3::Scheme& s = scheme(option->palette);
    switch (control) {
        case CC_ToolButton:
            return drawToolButton(option, painter, widget);
        case CC_Slider:
            return drawSlider(option, painter, widget);
        case CC_ScrollBar:
            return drawScrollBar(option, painter, widget);
        case CC_ComboBox: {
            auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option);
            if (!combo) break;
            bool open = option->state & State_On;
            if (combo->frame) drawField(QRectF(option->rect), option, painter, widget, (option->state & State_HasFocus) || open);
            QRectF arrow = QRectF(subControlRect(CC_ComboBox, combo, SC_ComboBoxArrow, widget));
            drawChevron(painter, arrow, open ? Qt::UpArrow : Qt::DownArrow, enabled(option) ? s.onSurfaceVariant : alpha(s.onSurface, Opacity::DisabledContent), 10);
            return;
        }
        case CC_SpinBox: {
            auto* spin = qstyleoption_cast<const QStyleOptionSpinBox*>(option);
            if (!spin) break;
            if (spin->frame) drawField(QRectF(option->rect), option, painter, widget, option->state & State_HasFocus);
            for (auto sub : {SC_SpinBoxUp, SC_SpinBoxDown}) {
                QRectF r = QRectF(subControlRect(CC_SpinBox, spin, sub, widget));
                if (r.isEmpty()) continue;
                bool stepEnabled = enabled(option) && (spin->stepEnabled & (sub == SC_SpinBoxUp ? QAbstractSpinBox::StepUpEnabled : QAbstractSpinBox::StepDownEnabled));
                if (stepEnabled && (option->activeSubControls & sub) && (option->state & State_MouseOver)) {
                    painter->save();
                    painter->setRenderHint(QPainter::Antialiasing);
                    fill(painter, roundedRect(r, Shape::Small), alpha(s.onSurface, (option->state & State_Sunken) ? Opacity::Pressed : Opacity::Hover));
                    painter->restore();
                }
                drawChevron(painter, r, sub == SC_SpinBoxUp ? Qt::UpArrow : Qt::DownArrow, stepEnabled ? s.onSurfaceVariant : alpha(s.onSurface, Opacity::DisabledContent), 8);
            }
            return;
        }
        default:
            break;
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}
