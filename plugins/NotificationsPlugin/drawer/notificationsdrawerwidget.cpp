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
#include "notificationsdrawerwidget.h"
#include <QCursor>

#include <m3motion.h>
#include "ui_notificationsdrawerwidget.h"

#include <tvariantanimation.h>
#include <QIcon>
#include <QGraphicsOpacityEffect>
#include <QPushButton>
#include "notificationtracker.h"
#include "notification.h"
#include <m3scheme.h>
#include <QPainterPath>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <QElapsedTimer>

struct NotificationsDrawerWidgetPrivate {
    NotificationTracker* tracker;
    tVariantAnimation* timeout;
    tVariantAnimation* actionsWidgetHeight;
    NotificationPtr n;

    QList<QPushButton*> actions;

    QGraphicsOpacityEffect* effect;
    bool shouldTimeoutRun = false;

    // The card's sideways position: springs in, follows a swipe, flings out
    M3SpringAnimation* slide = nullptr;
    bool pressed = false, swiping = false, flung = false;
    double pressX = 0;          // global x where the press started
    double lastX = 0, velocity = 0; // px/s, for a flick
    QElapsedTimer moveClock;
};

NotificationsDrawerWidget::NotificationsDrawerWidget(NotificationPtr notification, NotificationTracker* tracker, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::NotificationsDrawerWidget) {
    ui->setupUi(this);

    d = new NotificationsDrawerWidgetPrivate();
    d->n = notification;
    d->tracker = tracker;

    ui->summaryLabel->setText(notification->summary());
    ui->bodyLabel->setText(notification->body());
    connect(notification, &Notification::summaryChanged, ui->summaryLabel, &QLabel::setText);
    connect(notification, &Notification::bodyChanged, ui->bodyLabel, &QLabel::setText);

    connect(notification, &Notification::dismissed, this, &NotificationsDrawerWidget::animateDismiss);

    d->timeout = new tVariantAnimation(this);
    d->timeout->setStartValue(1.0);
    d->timeout->setEndValue(0.0);
    d->timeout->setEasingCurve(QEasingCurve::Linear);
    d->timeout->setForceAnimation(true);
    if (notification->timeout() == 0) {
        d->timeout->setDuration(-1);
    } else {
        d->timeout->setDuration(notification->timeout());
    }
    connect(d->timeout, &tVariantAnimation::finished, this, &NotificationsDrawerWidget::animateDismiss);
    connect(d->tracker, &NotificationTracker::pauseTimeouts, this, [ = ] {
        d->timeout->pause();
    });
    connect(d->tracker, &NotificationTracker::resumeTimeouts, this, [ = ] {
        if (d->shouldTimeoutRun && d->n->timeout() != 0 && !needsAnswer()) {
            d->timeout->start();
        }
    });

    connect(notification, &Notification::timeoutChanged, this, [ = ](qint32 timeout) {
        if (timeout == 0) {
            d->timeout->setDuration(-1);
        } else {
            d->timeout->setDuration(timeout);
        }
    });

    ui->appIconLabel->setPixmap(QIcon::fromTheme(notification->application()->getProperty("Icon").toString()).pixmap(SC_DPI_T(QSize(16, 16), QSize)));
    ui->appNameLabel->setText(notification->application()->getProperty("Name").toString());
    connect(notification, &Notification::applicationChanged, this, [ = ] {
        ui->appIconLabel->setPixmap(QIcon::fromTheme(notification->application()->getProperty("Icon").toString()).pixmap(SC_DPI_T(QSize(16, 16), QSize)));
        ui->appNameLabel->setText(notification->application()->getProperty("Name").toString());
    });

    ui->actionsWidget->setFixedHeight(0);
    connect(notification, &Notification::actionsChanged, this, &NotificationsDrawerWidget::setupActions);
    setupActions();
    // A critical notification with actions (a pairing code, a call) waits
    // for its answer: it stays, its buttons showing
    if (needsAnswer()) d->timeout->setDuration(-1);

    d->actionsWidgetHeight = new tVariantAnimation(this);
    d->actionsWidgetHeight->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
    d->actionsWidgetHeight->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
    connect(d->actionsWidgetHeight, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        ui->actionsWidget->setFixedHeight(value.toInt());
        ui->mainFrame->setFixedHeight(ui->mainFrame->sizeHint().height());
        this->updateGeometry();
    });
    connect(d->actionsWidgetHeight, &tVariantAnimation::finished, this, [ = ] {
        ui->mainFrame->setFixedHeight(ui->mainFrame->sizeHint().height());
        this->updateGeometry();
    });

    ui->buttonBox->setParent(ui->mainFrame);
    ui->buttonBox->move(this->layoutDirection() == Qt::RightToLeft ? 0 : ui->mainFrame->width() - ui->buttonBox->width(), 0);
    ui->buttonBox->setVisible(false);

    ui->mainFrame->installEventFilter(this);

    // M3 card: painted in the event filter (SurfaceContainerHigh, CornerMedium)
    ui->mainFrame->setAutoFillBackground(false);
    ui->mainFrame->setFrameShape(QFrame::NoFrame);
    {
        M3::Scheme s = M3::currentScheme();
        auto recolour = [](QWidget* w, QColor c) {
            QPalette pal = w->palette();
            for (auto group : {QPalette::Active, QPalette::Inactive}) pal.setColor(group, QPalette::WindowText, c);
            w->setPalette(pal);
        };
        QFont app = ui->appNameLabel->font();
        app.setBold(false);
        app.setWeight(QFont::Medium);
        app.setPixelSize(12); // LabelMedium
        ui->appNameLabel->setFont(app);
        recolour(ui->appNameLabel, s.onSurfaceVariant);
        QFont summary = ui->summaryLabel->font();
        summary.setBold(false);
        summary.setWeight(QFont::DemiBold);
        summary.setPixelSize(15);
        ui->summaryLabel->setFont(summary);
        recolour(ui->summaryLabel, s.onSurface);
        QFont body = ui->bodyLabel->font();
        body.setPixelSize(14); // BodyMedium
        ui->bodyLabel->setFont(body);
        recolour(ui->bodyLabel, s.onSurfaceVariant);
    }
    connect(d->timeout, &tVariantAnimation::valueChanged, ui->mainFrame, qOverload<>(&QWidget::update));
    ui->actionsLayout->setContentsMargins(12, 0, 12, 14);
    ui->actionsLayout->setSpacing(8);

    d->effect = new QGraphicsOpacityEffect(this);
    d->effect->setEnabled(false);
    this->setGraphicsEffect(d->effect);
}

NotificationsDrawerWidget::~NotificationsDrawerWidget() {
    delete ui;
    delete d;
}

void NotificationsDrawerWidget::slideSpring() {
    if (d->slide) return;
    d->slide = new M3SpringAnimation(this);
    d->slide->setToken(M3Motion::SpatialDefault); // expressive: a little overshoot
    connect(d->slide, &M3SpringAnimation::valueChanged, this, [this](QVariant value) {
        const double x = value.toDouble();
        ui->mainFrame->move(qRound(x), SC_DPI(9));
        // Swiped away: fading with the distance
        const double away = qAbs(x - SC_DPI(9)) / qMax(1, ui->mainFrame->width());
        if (d->swiping || d->flung) {
            d->effect->setEnabled(true);
            d->effect->setOpacity(qBound(0.0, 1 - away * 1.2, 1.0));
        }
    });
}

void NotificationsDrawerWidget::show() {
    QWidget::show();

    // Springs in from the side
    slideSpring();
    d->slide->snapTo(double(this->layoutDirection() == Qt::RightToLeft ? ui->mainFrame->width() : -ui->mainFrame->width()));
    d->slide->animateTo(double(SC_DPI(9)));
    QTimer::singleShot(M3Motion::duration(M3Motion::SpatialDefault), this, [this] {
        d->shouldTimeoutRun = true;
        if (d->n->timeout() != 0 && !needsAnswer() && !d->swiping && !d->flung) {
            d->timeout->start();
        }
    });
}

void NotificationsDrawerWidget::animateDismiss() {
    d->shouldTimeoutRun = false;

    tVariantAnimation* opacityAnim = new tVariantAnimation(this);
    opacityAnim->setStartValue(d->effect->isEnabled() ? d->effect->opacity() : 1.0); // (a swipe already faded it)
    opacityAnim->setEndValue(0.0);
    opacityAnim->setEasingCurve(M3Motion::easing(M3Motion::EffectsDefault));
    opacityAnim->setDuration(M3Motion::duration(M3Motion::EffectsDefault));
    connect(opacityAnim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
        d->effect->setOpacity(value.toDouble());
    });
    connect(opacityAnim, &tVariantAnimation::finished, this, [ = ] {
        opacityAnim->deleteLater();

        tVariantAnimation* anim = new tVariantAnimation(this);
        anim->setStartValue(this->height());
        anim->setEndValue(0);
        anim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
        anim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
        connect(anim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
            this->setFixedHeight(value.toInt());
        });
        connect(anim, &tVariantAnimation::finished, this, [ = ] {
            anim->deleteLater();
            emit dismiss();
        });
        anim->start();
    });
    d->effect->setEnabled(true);
    opacityAnim->start();
}

QSize NotificationsDrawerWidget::sizeHint() const {
    QSize sizeHint = QWidget::sizeHint();
    sizeHint.setHeight(ui->mainFrame->sizeHint().height() + SC_DPI(9));
    return sizeHint;
}

void NotificationsDrawerWidget::resizeEvent(QResizeEvent* event) {
    ui->mainFrame->setFixedWidth(this->width() - SC_DPI(18));
    ui->mainFrame->setFixedHeight(ui->mainFrame->sizeHint().height());
    ui->buttonBox->move(this->layoutDirection() == Qt::RightToLeft ? 0 : ui->mainFrame->width() - ui->buttonBox->width(), 0);
}

bool NotificationsDrawerWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->mainFrame && event->type() == QEvent::Paint) {
        QPainter painter(ui->mainFrame);
        painter.setRenderHint(QPainter::Antialiasing);
        M3::Scheme s = M3::currentScheme();
        QPainterPath card;
        card.addRoundedRect(QRectF(ui->mainFrame->rect()), 12, 12);
        painter.fillPath(card, s.surfaceContainerHigh);
        // Time left before it goes: a thin Primary line along the bottom
        if (d->n->timeout() != 0 && d->timeout->state() != QAbstractAnimation::Stopped) {
            double left = qBound(0.0, d->timeout->currentValue().toDouble(), 1.0);
            QRectF track(16, ui->mainFrame->height() - 6, ui->mainFrame->width() - 32, 3);
            QPainterPath line;
            line.addRoundedRect(QRectF(track.left(), track.top(), track.width() * left, track.height()), 1.5, 1.5);
            painter.fillPath(line, s.primary);
        }
        return true;
    }
    if (watched == ui->mainFrame && !d->flung) {
        // Swipe sideways to dismiss: the card follows the pointer and fades;
        // let go past a third of its width (or flick it) and it flies off,
        // otherwise it springs back
        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                auto* mouse = static_cast<QMouseEvent*>(event);
                if (mouse->button() != Qt::LeftButton) break;
                slideSpring();
                d->pressed = true;
                d->swiping = false;
                d->pressX = d->lastX = mouse->globalPosition().x();
                d->velocity = 0;
                d->moveClock.start();
                event->accept();
                return true; // the card takes the drag
            }
            case QEvent::MouseMove: {
                if (!d->pressed) break;
                auto* mouse = static_cast<QMouseEvent*>(event);
                const double x = mouse->globalPosition().x();
                const double dx = x - d->pressX;
                if (!d->swiping && qAbs(dx) > 8) {
                    d->swiping = true;
                    d->tracker->pauseTimeouts();
                }
                if (d->swiping) {
                    const qint64 ms = qMax<qint64>(1, d->moveClock.restart());
                    d->velocity = 0.6 * d->velocity + 0.4 * (x - d->lastX) * 1000.0 / ms;
                    d->lastX = x;
                    d->slide->snapTo(SC_DPI(9) + dx);
                }
                return true;
            }
            case QEvent::MouseButtonRelease: {
                if (!d->pressed) break;
                d->pressed = false;
                if (!d->swiping) return false;
                const double dx = d->lastX - d->pressX;
                const double width = ui->mainFrame->width();
                if (qAbs(dx) > width / 3 || qAbs(d->velocity) > 900) {
                    // Off it goes, the way it was going, then the gap closes
                    d->flung = true;
                    const double direction = (qAbs(d->velocity) > 900 ? d->velocity : dx) > 0 ? 1 : -1;
                    d->slide->animateTo(SC_DPI(9) + direction * (width + SC_DPI(40)));
                    d->tracker->resumeTimeouts();
                    QTimer::singleShot(260, this, [this] {
                        d->n->dismiss(Notification::NotificationUserDismissed);
                    });
                } else {
                    d->swiping = false;
                    d->slide->animateTo(double(SC_DPI(9)));
                    d->effect->setOpacity(1);
                    d->effect->setEnabled(false);
                    // Still hovered: the hover keeps them paused (Leave
                    // resumes them)
                    if (!ui->mainFrame->rect().contains(ui->mainFrame->mapFromGlobal(QCursor::pos()))) d->tracker->resumeTimeouts();
                }
                return true;
            }
            default:
                break;
        }
    }
    if (watched == ui->mainFrame) {
        switch (event->type()) {
            case QEvent::Enter:
                showButtons();
                break;
            case QEvent::Leave:
                hideButtons();
                break;
            default:
                break;
        }
    }
    return false;
}

void NotificationsDrawerWidget::setupActions() {
    for (QPushButton* button : d->actions) {
        ui->actionsLayout->removeWidget(button);
        button->deleteLater();
    }
    d->actions.clear();

    for (Notification::Action action : d->n->actions()) {
        QPushButton* button = new QPushButton();
        button->setText(action.text);
        button->setIcon(action.icon);
        connect(button, &QPushButton::clicked, this, [ = ] {
            emit d->n->actionInvoked(action);
            this->animateDismiss();
        });
        ui->actionsLayout->addWidget(button);
        d->actions.append(button);
    }
    if (needsAnswer()) {
        d->timeout->setDuration(-1);
        ui->actionsWidget->setFixedHeight(ui->actionsWidget->sizeHint().height());
    }
}

void NotificationsDrawerWidget::showButtons() {
    d->tracker->pauseTimeouts();
    ui->buttonBox->setVisible(true);

    d->actionsWidgetHeight->stop();
    d->actionsWidgetHeight->setStartValue(ui->actionsWidget->height());
    d->actionsWidgetHeight->setEndValue(ui->actionsWidget->sizeHint().height());
    d->actionsWidgetHeight->start();
}

bool NotificationsDrawerWidget::needsAnswer() {
    return d->n->urgency() == Notification::Critical && !d->n->actions().isEmpty();
}

void NotificationsDrawerWidget::hideButtons() {
    d->tracker->resumeTimeouts();
    ui->buttonBox->setVisible(false);
    if (needsAnswer()) return; // its buttons stay

    d->actionsWidgetHeight->stop();
    d->actionsWidgetHeight->setStartValue(ui->actionsWidget->height());
    d->actionsWidgetHeight->setEndValue(0);
    d->actionsWidgetHeight->start();
}

void NotificationsDrawerWidget::on_closeButton_clicked() {
    d->n->dismiss(Notification::NotificationUserDismissed);
}

void NotificationsDrawerWidget::on_dismissButton_clicked() {
    this->animateDismiss();
}
