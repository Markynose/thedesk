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
#include "quickwidgetcontainer.h"

#include "ui_quickwidgetcontainer.h"
#include <m3motion.h>
#include <m3scheme.h>

#include "chunk.h"
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <Wm/desktopwm.h>
#include <barmanager.h>
#include <compositor.h>
#include <libcontemporary_global.h>
#include <statemanager.h>

namespace {
    constexpr int padding = 8;          // between the panel's edge and its contents
    constexpr double panelCorner = 28;  // CornerExtraLarge
    constexpr int barInset = 8;         // the bar floats this far off the screen edge

    double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(lerp(a.redF(), b.redF(), t), lerp(a.greenF(), b.greenF(), t), lerp(a.blueF(), b.blueF(), t));
    }
} // namespace

struct QuickWidgetContainerPrivate {
        QWidget* parentChunk;
        QWidget* quickWidget;
        QWidget* content;
        QWidget* veil; // over the contents while they fade in (an opacity effect blanked them in the translucent window)

        QRect target; // where the panel ends up (global)
        M3SpringAnimation* transform; // 0: the chunk, 1: the panel
        bool isShowing = false;

        BarManager::BarLockPtr barLock;
};

QuickWidgetContainer::QuickWidgetContainer(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::QuickWidgetContainer) {
    ui->setupUi(this);

    this->setWindowFlags(Qt::Popup);
    this->setAttribute(Qt::WA_TranslucentBackground);

    d = new QuickWidgetContainerPrivate();
    d->parentChunk = parent;

    d->content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(d->content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    ui->chunkContainerLayout->addWidget(d->content);
    d->veil = new QWidget(this);
    d->veil->setAttribute(Qt::WA_TransparentForMouseEvents);
    d->veil->installEventFilter(this);
    d->veil->hide();

    // M3 container transform: the panel grows out of the chunk that opened it
    d->transform = new M3SpringAnimation(this);
    d->transform->setToken(M3Motion::SpatialDefault);
    d->transform->snapTo(0.0);
    connect(d->transform, &M3SpringAnimation::valueChanged, this, &QuickWidgetContainer::applyTransform);
    connect(d->transform, &M3SpringAnimation::finished, this, [this] {
        if (d->transform->endValue().toDouble() < 0.5) this->hide();
        else applyTransform();
    });

    ui->chunkContainerLayout->installEventFilter(this);
}

QuickWidgetContainer::~QuickWidgetContainer() {
    delete ui;
    delete d;
}

QSize QuickWidgetContainer::sizeHint() const {
    QSize sizeHint;
    Chunk* chunk = qobject_cast<Chunk*>(d->parentChunk);
    if (chunk) {
        sizeHint = chunk->quickWidget()->sizeHint();
    } else {
        sizeHint = d->quickWidget->sizeHint();
    }

    return sizeHint.grownBy(QMargins(padding, padding, padding, padding));
}

void QuickWidgetContainer::showContainer() {
    Chunk* chunk = qobject_cast<Chunk*>(d->parentChunk);
    QWidget* quickWidget = chunk ? chunk->quickWidget() : d->quickWidget;
    d->content->layout()->addWidget(quickWidget);
    quickWidget->installEventFilter(this);

    d->isShowing = true;
    if (!this->isVisible()) d->transform->snapTo(0.0);
    d->transform->animateTo(1.0); // moving first, so the window starts out cut to the chunk
    calculatePosition();

    this->show();
    DesktopWm::setSystemWindow(this, DesktopWm::SystemWindowTypeSkipTaskbarOnly);
    this->activateWindow();

    d->barLock = StateManager::barManager()->acquireLock();
    emit showing();
}

void QuickWidgetContainer::hideContainer() {
    d->transform->animateTo(0.0);
    d->isShowing = false;

    if (d->barLock) d->barLock->unlock();
    emit hiding();
}

bool QuickWidgetContainer::isShowing() {
    return d->isShowing;
}

void QuickWidgetContainer::setQuickWidget(QWidget* widget) {
    d->quickWidget = widget;
}

QRect QuickWidgetContainer::sourceRect() const {
    return QRect(d->parentChunk->mapToGlobal(QPoint(0, 0)), d->parentChunk->size());
}

QPainterPath QuickWidgetContainer::panelShape() const {
    // From the chunk's pill to the panel, in this window's coordinates
    const double k = d->transform->currentValue().toDouble();
    const QRectF from = QRectF(sourceRect()).translated(-this->pos());
    const QRectF to = QRectF(d->target).translated(-this->pos());
    const QRectF r(QPointF(lerp(from.left(), to.left(), k), lerp(from.top(), to.top(), k)), QPointF(lerp(from.right(), to.right(), k), lerp(from.bottom(), to.bottom(), k)));
    const double radius = qMin(lerp(from.height() / 2, panelCorner, qBound(0.0, k, 1.0)), qMin(r.width(), r.height()) / 2);
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    return path;
}

void QuickWidgetContainer::applyTransform() {
    // While it moves the window covers the chunk and the panel; at rest just the panel
    const bool moving = d->transform->state() == QAbstractAnimation::Running;
    const QRect frame = moving ? d->target.united(sourceRect()) : d->target;
    if (frame != this->geometry()) {
        this->setContentsMargins(d->target.left() - frame.left() + padding, d->target.top() - frame.top() + padding,
            frame.right() - d->target.right() + padding, frame.bottom() - d->target.bottom() + padding);
        this->setFixedSize(frame.size());
        this->setGeometry(frame);
    }

    d->veil->setGeometry(d->content->geometry());
    d->veil->setVisible(moving);
    d->veil->raise();
    d->veil->update();

    // Cut the window to the shape: always while it moves (the contents
    // stay inside it), at rest only without a compositor (smooth edges)
    if (Compositor::isActive() && !moving) this->clearMask();
    else this->setMask(QRegion(panelShape().toFillPolygon().toPolygon()));
    this->update();
}

void QuickWidgetContainer::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillPath(panelShape(), panelColour());
}

QColor QuickWidgetContainer::panelColour() const {
    // The chunk's tonal pill turns into the panel's surface
    const M3::Scheme s = M3::currentScheme();
    return mix(s.secondaryContainer, s.surfaceContainerHigh, d->transform->currentValue().toDouble() * 1.5);
}

void QuickWidgetContainer::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    switch (event->type()) {
        case QEvent::ActivationChange:
            if (!this->isActiveWindow()) this->hideContainer();
            break;
        default:
            break;
    }
}

void QuickWidgetContainer::calculatePosition() {
    QPoint midpoint = d->parentChunk->mapToGlobal(QPoint(d->parentChunk->width() / 2, d->parentChunk->height()));
    QRect screenGeometry = qApp->screenAt(midpoint)->geometry();

    // Below the bar, not over its second row (the Status Center fills the screen: there, below the chunk)
    int top = midpoint.y();
    const int barBottom = screenGeometry.top() + barInset + StateManager::barManager()->barHeight();
    if (barBottom < screenGeometry.center().y()) top = qMax(top, barBottom);

    QRect geom(QPoint(0, top + barInset), this->sizeHint());
    geom.moveLeft(midpoint.x() - geom.width() / 2);
    if (geom.left() < screenGeometry.left() + 9) geom.moveLeft(screenGeometry.left() + 9);
    if (geom.right() > screenGeometry.right() - 9) geom.moveRight(screenGeometry.right() - 9);
    d->target = geom;
    applyTransform();
}

bool QuickWidgetContainer::eventFilter(QObject* watched, QEvent* event) {
    if (watched == d->veil) {
        // The contents fade in once the panel has some room
        if (event->type() == QEvent::Paint) {
            QColor veil = panelColour();
            veil.setAlphaF(float(qBound(0.0, 1 - (d->transform->currentValue().toDouble() - 0.25) / 0.75, 1.0)));
            QPainter(d->veil).fillRect(d->veil->rect(), veil);
            return true;
        }
        return false;
    }
    if (d->isShowing && (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)) { // closed: its chunk's widget changes don't move it
        QTimer::singleShot(0, this, &QuickWidgetContainer::calculatePosition);
    }
    return false;
}

void QuickWidgetContainer::hideEvent(QHideEvent* event) {
    if (d->isShowing) this->hideContainer();
}
