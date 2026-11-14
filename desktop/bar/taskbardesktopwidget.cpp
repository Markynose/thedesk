/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
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
#include "taskbardesktopwidget.h"

#include <m3motion.h>
#include <m3scheme.h>
#include "ui_taskbardesktopwidget.h"

#include "taskbarapplicationwidget.h"
#include <Applications/application.h>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <Wm/desktopwm.h>
#include <tvariantanimation.h>

namespace {
    constexpr double buttonHeight = 40;
    constexpr double innerCorner = 8;  // ButtonGroup's connected inner corners
    constexpr double pressGrow = 8;    // the pressed button widens by this much...
    constexpr double pressGiveWay = 4; // ...and each neighbour gives this much up

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t, a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
    }

    QColor alpha(QColor c, double a) {
        c.setAlphaF(float(qBound(0.0, a, 1.0)));
        return c;
    }

    // A rounded rect with one radius on the left and another on the right
    QPainterPath segment(const QRectF& r, double left, double right) {
        left = qMin(left, qMin(r.width(), r.height()) / 2);
        right = qMin(right, qMin(r.width(), r.height()) / 2);
        QPainterPath path;
        path.moveTo(r.left() + left, r.top());
        path.lineTo(r.right() - right, r.top());
        path.arcTo(QRectF(r.right() - 2 * right, r.top(), 2 * right, 2 * right), 90, -90);
        path.lineTo(r.right(), r.bottom() - right);
        path.arcTo(QRectF(r.right() - 2 * right, r.bottom() - 2 * right, 2 * right, 2 * right), 0, -90);
        path.lineTo(r.left() + left, r.bottom());
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * left, 2 * left, 2 * left), 270, -90);
        path.lineTo(r.left(), r.top() + left);
        path.arcTo(QRectF(r.left(), r.top(), 2 * left, 2 * left), 180, -90);
        path.closeSubpath();
        return path;
    }

    QFont label(double tone) {
        // Roboto Flex swells in weight and width as the desktop is selected (in steps: each axis value is a new font instance)
        QFont font(QStringLiteral("Roboto Flex"));
        font.setPixelSize(15);
        font.setVariableAxis(QFont::Tag("wght"), float(qRound((500 + 300 * tone) / 50) * 50));
        font.setVariableAxis(QFont::Tag("wdth"), float(qRound((100 + 15 * tone) / 5) * 5));
        return font;
    }
} // namespace

DesktopButton::DesktopButton(QWidget* parent) :
    QPushButton(parent) {
    this->setCheckable(true);
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    this->setAttribute(Qt::WA_Hover);

    selected = new M3SpringAnimation(this);
    selected->setToken(M3Motion::SpatialFast);
    tone = new M3SpringAnimation(this);
    tone->setToken(M3Motion::EffectsDefault);
    press = new M3SpringAnimation(this);
    press->setToken(M3Motion::SpatialFast);
    for (auto* spring : {selected, tone, press}) {
        spring->snapTo(0.0);
        connect(spring, &M3SpringAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    }
    connect(press, &M3SpringAnimation::valueChanged, this, [this] {
        for (DesktopButton* button : group()) button->updateWidth();
    });
    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        selected->animateTo(checked ? 1.0 : 0.0);
        tone->animateTo(checked ? 1.0 : 0.0);
    });
    connect(this, &QAbstractButton::pressed, this, [this] {
        press->animateTo(1.0);
    });
    connect(this, &QAbstractButton::released, this, [this] {
        press->animateTo(0.0);
    });
}

void DesktopButton::setAppCount(int count) {
    apps = count;
    this->update();
}

QSize DesktopButton::sizeHint() const {
    const int text = this->text().isEmpty() ? 0 : QFontMetrics(label(1)).horizontalAdvance(this->text());
    return QSize(qMax(int(buttonHeight), text + 24), int(buttonHeight));
}

QSize DesktopButton::minimumSizeHint() const {
    return QSize(0, int(buttonHeight));
}

QList<DesktopButton*> DesktopButton::group() const {
    // The group in layout order
    QList<DesktopButton*> buttons;
    QLayout* layout = this->parentWidget() ? this->parentWidget()->layout() : nullptr;
    if (!layout) return buttons;
    for (int i = 0; i < layout->count(); i++) {
        auto* button = qobject_cast<DesktopButton*>(layout->itemAt(i)->widget());
        if (button && !button->isHidden()) buttons.append(button);
    }
    return buttons;
}

void DesktopButton::updateWidth() {
    const QList<DesktopButton*> buttons = group();
    const int index = buttons.indexOf(const_cast<DesktopButton*>(this));
    double neighbours = 0;
    if (index > 0) neighbours += buttons.at(index - 1)->press->currentValue().toDouble();
    if (index >= 0 && index < buttons.count() - 1) neighbours += buttons.at(index + 1)->press->currentValue().toDouble();
    const int width = qMax(0, qRound(sizeHint().width() + pressGrow * press->currentValue().toDouble() - pressGiveWay * neighbours));
    if (width == sizeHint().width()) {
        // At rest: back to the size hint, which follows the text
        this->setMinimumWidth(0);
        this->setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
        this->setFixedWidth(width);
    }
}

void DesktopButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    const M3::Scheme s = M3::currentScheme();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QList<DesktopButton*> buttons = group();
    bool first = buttons.isEmpty() || buttons.first() == this, last = buttons.isEmpty() || buttons.last() == this;
    if (this->layoutDirection() == Qt::RightToLeft) std::swap(first, last);

    const double sel = qBound(0.0, selected->currentValue().toDouble(), 1.0);
    const double t = qBound(0.0, tone->currentValue().toDouble(), 1.0);
    const double pressed = qBound(0.0, press->currentValue().toDouble(), 1.0);
    QRectF r(QRectF(this->rect()));
    r.setTop(r.center().y() - buttonHeight / 2);
    r.setHeight(buttonHeight);
    const double full = r.height() / 2;
    const double left = (first ? full : innerCorner) + (full - (first ? full : innerCorner)) * sel;
    const double right = (last ? full : innerCorner) + (full - (last ? full : innerCorner)) * sel;
    const QPainterPath shape = segment(r, left, right);

    const QColor ink = mix(s.onSecondaryContainer, s.onPrimary, t);
    painter.fillPath(shape, mix(s.secondaryContainer, s.primary, t));
    QStyleOption option;
    option.initFrom(this);
    double layer = this->underMouse() ? 0.08 : 0;
    if ((option.state & QStyle::State_HasFocus) && (option.state & QStyle::State_KeyboardFocusChange)) layer = 0.10;
    layer = qMax(layer, 0.10 * pressed);
    if (layer > 0) painter.fillPath(shape, alpha(ink, layer));

    if (this->text().isEmpty()) {
        QFont symbols(QStringLiteral("Material Symbols Rounded"));
        symbols.setPixelSize(20);
        symbols.setVariableAxis(QFont::Tag("opsz"), 20);
        painter.setFont(symbols);
        painter.setPen(ink);
        painter.drawText(r, Qt::AlignCenter, QStringLiteral("add"));
        return;
    }

    painter.setFont(label(t));
    painter.setPen(ink);
    painter.drawText(r.adjusted(0, apps > 0 ? -3 : 0, 0, apps > 0 ? -3 : 0), Qt::AlignCenter, this->text());

    // A dot per app on the desktop (the icons were too small to make out)
    painter.setPen(Qt::NoPen);
    painter.setBrush(alpha(ink, 0.8));
    const int dots = qMin(apps, 5);
    for (int i = 0; i < dots; i++) painter.drawEllipse(QPointF(r.center().x() + (i - (dots - 1) / 2.0) * 6, r.bottom() - 7), 1.6, 1.6);
}

struct TaskbarDesktopWidgetPrivate {
        QMap<QString, TaskbarApplicationWidget*> applicationWidgets;
        QList<TaskbarApplicationWidget*> applicationWidgetOrder;
        uint desktop;
        DesktopButton* button;

        tVariantAnimation* progressAnim;
};

TaskbarDesktopWidget::TaskbarDesktopWidget(uint desktop, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::TaskbarDesktopWidget) {
    ui->setupUi(this);
    d = new TaskbarDesktopWidgetPrivate();
    d->desktop = desktop;

    d->button = new DesktopButton();
    connect(d->button, &QAbstractButton::clicked, this, [this] {
        DesktopWm::setCurrentDesktop(d->desktop);
    });
    d->button->setChecked(DesktopWm::currentDesktop() == d->desktop);

    d->progressAnim = new tVariantAnimation();
    d->progressAnim->setStartValue(0.0);
    d->progressAnim->setEndValue(1.0);
    d->progressAnim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
    d->progressAnim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
    connect(d->progressAnim, &tVariantAnimation::valueChanged, this, &TaskbarDesktopWidget::updateAnimation);
    connect(d->progressAnim, &tVariantAnimation::finished, this, &TaskbarDesktopWidget::finishAnimation);

    if (DesktopWm::currentDesktop() == d->desktop) d->progressAnim->setCurrentTime(d->progressAnim->duration());
    finishAnimation();

    connect(DesktopWm::instance(), &DesktopWm::currentDesktopChanged, this, &TaskbarDesktopWidget::updateDesktop);
    connect(DesktopWm::instance(), &DesktopWm::desktopCountChanged, this, &TaskbarDesktopWidget::updateButton);
    connect(DesktopWm::instance(), &DesktopWm::windowAdded, this, &TaskbarDesktopWidget::windowAdded);
    connect(DesktopWm::instance(), &DesktopWm::windowRemoved, this, &TaskbarDesktopWidget::windowRemoved);

    for (DesktopWmWindowPtr window : DesktopWm::openWindows()) {
        this->windowAdded(window);
    }

    updateButton();
}

TaskbarDesktopWidget::~TaskbarDesktopWidget() {
    delete d->button; // it lives in the taskbar's button group
    delete d;
    delete ui;
}

DesktopButton* TaskbarDesktopWidget::desktopButton() {
    return d->button;
}

void TaskbarDesktopWidget::moveDesktop(uint newDesktop) {
    d->desktop = newDesktop;

    QList<DesktopWmWindowPtr> windowsToMove;
    for (TaskbarApplicationWidget* widget : d->applicationWidgetOrder) {
        windowsToMove.append(widget->trackedWindows());
    }

    for (DesktopWmWindowPtr window : windowsToMove) {
        window->moveToDesktop(newDesktop);
    }
}

void TaskbarDesktopWidget::windowAdded(DesktopWmWindowPtr window) {
    connect(window, &DesktopWmWindow::desktopChanged, this, [this, window] {
        if (window->isOnDesktop(d->desktop) && window->shouldShowInTaskbar()) {
            registerOnDesktop(window);
        } else {
            deregisterFromDesktop(window);
        }
    });

    if (window->isOnDesktop(d->desktop) && window->shouldShowInTaskbar()) registerOnDesktop(window);
}

void TaskbarDesktopWidget::windowRemoved(DesktopWmWindowPtr window) {
    deregisterFromDesktop(window);
}

void TaskbarDesktopWidget::registerOnDesktop(DesktopWmWindowPtr window) {
    QString desktopEntry = QString::number(window->pid());
    if (window->application()) desktopEntry = window->application()->desktopEntry();

    TaskbarApplicationWidget* widget;
    if (d->applicationWidgets.contains(desktopEntry)) {
        widget = d->applicationWidgets.value(desktopEntry);
    } else {
        widget = new TaskbarApplicationWidget(desktopEntry, d->desktop, this);
        connect(widget, &TaskbarApplicationWidget::windowsRemoved, this, [=] {
            d->applicationWidgets.remove(desktopEntry);
            d->applicationWidgetOrder.removeOne(widget);
            ui->applicationsLayout->removeWidget(widget);
            widget->deleteLater();

            updateButton();
        });
        ui->applicationsLayout->addWidget(widget);
        d->applicationWidgets.insert(desktopEntry, widget);
        d->applicationWidgetOrder.append(widget);
    }

    widget->trackWindow(window);
    updateButton();
}

void TaskbarDesktopWidget::deregisterFromDesktop(DesktopWmWindowPtr window) {
    QString desktopEntry = QString::number(window->pid());
    if (window->application()) desktopEntry = window->application()->desktopEntry();

    if (d->applicationWidgets.contains(desktopEntry)) {
        TaskbarApplicationWidget* widget = d->applicationWidgets.value(desktopEntry);
        widget->removeTrackedWindow(window);
    }
}

void TaskbarDesktopWidget::updateButton() {
    // Number each desktop with the name the WM gives it (bspwm: 1..10)
    QString name = DesktopWm::desktops().value(d->desktop);
    if (name.isEmpty()) name = QString::number(d->desktop + 1);
    d->button->setText(name);
    d->button->setToolTip(tr("Desktop %1").arg(name));
    d->button->setAppCount(d->applicationWidgetOrder.count());
}

void TaskbarDesktopWidget::updateDesktop() {
    d->button->setChecked(DesktopWm::currentDesktop() == d->desktop);
    if (DesktopWm::currentDesktop() == d->desktop && d->progressAnim->currentValue().toReal() < 0.5) {
        d->progressAnim->setDirection(tVariantAnimation::Forward);
        d->progressAnim->start();
    } else if (DesktopWm::currentDesktop() != d->desktop && d->progressAnim->currentValue().toReal() > 0.5) {
        d->progressAnim->setDirection(tVariantAnimation::Backward);
        d->progressAnim->start();
    }
}

void TaskbarDesktopWidget::updateAnimation() {
    // Only the current desktop's apps show, after the group; they slide open
    double progress = d->progressAnim->currentValue().toReal();
    ui->applicationsWidget->setFixedWidth(ui->applicationsWidget->sizeHint().width() * progress);
}

void TaskbarDesktopWidget::finishAnimation() {
    ui->applicationsWidget->setFixedWidth(d->progressAnim->currentValue().toReal() > 0.5 ? QWIDGETSIZE_MAX : 0);
}

void TaskbarDesktopWidget::resizeEvent(QResizeEvent* event) {
    if (d->progressAnim->state() == tVariantAnimation::Running) {
        updateAnimation();
    } else {
        finishAnimation();
    }
}
