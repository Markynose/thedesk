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
#include "barwindow.h"
#include <compositor.h>
#include <gatewaymanager.h>

#include <m3motion.h>
#include <m3scheme.h>
#include <QPainterPath>
#include "ui_barwindow.h"

#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <Wm/desktopwm.h>

#include "mainbarwidget.h"
#include "statuscenter/statuscenter.h"
#include <tvariantanimation.h>

#include <tsettings.h>

#include <barmanager.h>
#include <statemanager.h>
#include <statuscentermanager.h>

#include <Gestures/gesturedaemon.h>
#include <QWindow>
#include <Screens/screendaemon.h>
#include <Screens/systemscreen.h>
#include <tpopover.h>

#include "gateway/gateway.h"

#include <keygrab.h>

#include <QGuiApplication>
#include <Wm/desktopwmwindow.h>
#if QT_CONFIG(xcb)
    #include <xcb/xcb.h>
#endif

#include <horizon/focustide.h>

namespace {
    // Focus tide along the bottom edge of the bar: fills left to right with
    // a glowing head. Hidden when no tide is running.
    // Focus tide as M3 Expressive's wavy progress indicator (Compose
    // LinearWavyProgressIndicator; LinearProgressIndicatorTokens: 3 dp
    // amplitude, 40 dp wavelength, 4 dp stroke, 4 dp gap, stop dot). The wave
    // drifts slowly; it only animates while a tide runs.
    class TideLine : public QWidget {
        public:
            explicit TideLine(QWidget* parent) :
                QWidget(parent) {
                this->setAttribute(Qt::WA_TransparentForMouseEvents);
                this->setAttribute(Qt::WA_TranslucentBackground);
                this->hide();
                drift.setInterval(50); // 20 fps is plenty for a slow drift
                QObject::connect(&drift, &QTimer::timeout, this, [this] {
                    phase += 0.05 * 2 * M_PI; // the paint drifts a quarter wavelength a second
                    this->update();
                });
                QObject::connect(FocusTide::instance(), &FocusTide::progressChanged, this, [this](double progress) {
                    this->progress = progress;
                    this->setVisible(progress >= 0);
                    if (progress >= 0 && !drift.isActive()) drift.start();
                    if (progress < 0) drift.stop();
                    // Not over the Gateway while it's open
                    if (!StateManager::gatewayManager()->isOpen()) this->raise();
                    this->update();
                });
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                if (progress < 0) return;
                M3::Scheme s = M3::currentScheme();
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);
                const double amplitude = 3, wavelength = 40, stroke = 4, gap = 4;
                const double mid = this->height() / 2.0;
                const double width = this->width();
                double end = width * qBound(0.0, progress, 1.0);

                // Active part: the wave, with round caps
                QPainterPath wave;
                double shift = std::fmod(phase * 0.25, 2 * M_PI);
                for (double x = stroke / 2; x <= qMax(stroke / 2, end); x += 1) {
                    double y = mid + amplitude * std::sin(2 * M_PI * x / wavelength + shift);
                    if (x == stroke / 2) wave.moveTo(x, y);
                    else wave.lineTo(x, y);
                }
                if (end > stroke) {
                    p.setPen(QPen(s.primary, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    p.drawPath(wave);
                }

                // Track: flat, after the gap; stop dot at the end
                double trackStart = end + gap + stroke / 2;
                if (trackStart < width - stroke / 2) {
                    p.setPen(QPen(s.secondaryContainer, stroke, Qt::SolidLine, Qt::RoundCap));
                    p.drawLine(QPointF(trackStart, mid), QPointF(width - stroke / 2, mid));
                    p.setPen(Qt::NoPen);
                    p.setBrush(s.primary);
                    p.drawEllipse(QPointF(width - stroke / 2, mid), stroke / 2, stroke / 2);
                }
            }

        private:
            double progress = -1;
            double phase = 0;
            QTimer drift;
    };

    // The bar is an unmanaged dock window. Window managers such as bspwm
    // ignore activation requests (_NET_ACTIVE_WINDOW) for unmanaged windows,
    // so take X input focus directly while the Status Center needs the keyboard.
    // Which X window has the keyboard now
    quint32 currentInputFocus() {
#if QT_CONFIG(xcb)
        auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        if (!x11 || !x11->connection()) return 0;
        xcb_get_input_focus_reply_t* reply = xcb_get_input_focus_reply(x11->connection(), xcb_get_input_focus(x11->connection()), nullptr);
        if (!reply) return 0;
        const quint32 focus = reply->focus;
        free(reply);
        return focus;
#else
        return 0;
#endif
    }

    // Gives the keyboard back to a window if it's still on screen, else to
    // whatever is under the pointer
    void restoreInputFocus(quint32 window) {
#if QT_CONFIG(xcb)
        auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        if (!x11 || !x11->connection()) return;
        xcb_connection_t* c = x11->connection();
        bool viewable = false;
        if (window > 1) {
            xcb_get_window_attributes_reply_t* attributes = xcb_get_window_attributes_reply(c, xcb_get_window_attributes(c, window), nullptr);
            if (attributes) {
                viewable = attributes->map_state == XCB_MAP_STATE_VIEWABLE;
                free(attributes);
            }
        }
        xcb_set_input_focus(c, XCB_INPUT_FOCUS_POINTER_ROOT, viewable ? window : XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
        xcb_flush(c);
#else
        Q_UNUSED(window)
#endif
    }

    void forceInputFocus(QWidget* widget) {
#if QT_CONFIG(xcb)
        auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        if (!x11 || !x11->connection()) return;
        xcb_set_input_focus(x11->connection(), XCB_INPUT_FOCUS_PARENT, static_cast<xcb_window_t>(widget->winId()), XCB_CURRENT_TIME);
        xcb_flush(x11->connection());
#else
        Q_UNUSED(widget)
#endif
    }
} // namespace

namespace {
    // M3E floating toolbar (FloatingToolbarTokens): a SurfaceContainer pill
    // that floats off the screen edge. Compose uses 16 dp; 8 suits a 768 px
    // high screen better.
    constexpr int floatInset = 8;
    constexpr double floatCorner = 16; // CornerLarge: a pill while collapsed, gentler expanded (user: 28 was too round)

    QPainterPath floatingShape(const QRectF& rect) {
        QPainterPath path;
        double radius = qMin(floatCorner, rect.height() / 2);
        path.addRoundedRect(rect, radius, radius);
        return path;
    }
} // namespace

struct BarWindowPrivate {
        MainBarWidget* mainBarWidget;
        StatusCenter* statusCenterWidget;

        M3SpringAnimation* heightAnim;
        M3SpringAnimation* barStatusCenterTransitionAnim;

        QGraphicsOpacityEffect* mainBarOpacity;
        QGraphicsOpacityEffect* statusCenterOpacity;

        SystemScreen* oldPrimaryScreen = nullptr;

        QList<DesktopWmWindowPtr> maximisedWindows;

        bool barExpanding = true;
        bool statusCenterShown = false;
        bool barPendingShow = false;

        GestureInteractionPtr lastGesture;

        QWidget* tideLine = nullptr;

        // Window that had focus before the Status Center took the keyboard
        DesktopWmWindowPtr focusBeforeStatusCenter;
        quint32 inputFocusBeforeStatusCenter = 0; // the X window with the keyboard

        tSettings settings;
};

BarWindow::BarWindow(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::BarWindow) {
    ui->setupUi(this);
    applyToolbarPalette();
    d = new BarWindowPrivate();
    this->setWindowTitle(QStringLiteral("theDesk bar")); // picom's rounding rule
    connect(Compositor::instance(), &Compositor::activeChanged, this, &BarWindow::applyShape);

    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setMouseTracking(true);

    d->mainBarWidget = new MainBarWidget(this);
    connect(d->mainBarWidget, &MainBarWidget::expandedHeightChanged, this, &BarWindow::barHeightChanged);
    connect(d->mainBarWidget, &MainBarWidget::statusBarHeightChanged, this, &BarWindow::barHeightChanged);

    d->statusCenterWidget = new StatusCenter(this);

    d->mainBarOpacity = new QGraphicsOpacityEffect(d->mainBarWidget);
    d->mainBarOpacity->setEnabled(false);
    d->mainBarWidget->setGraphicsEffect(d->mainBarOpacity);

    d->statusCenterOpacity = new QGraphicsOpacityEffect(d->statusCenterWidget);
    d->statusCenterOpacity->setEnabled(false);
    d->statusCenterWidget->setGraphicsEffect(d->statusCenterOpacity);
    d->statusCenterWidget->setVisible(false);

    // M3 Expressive springs (m3/m3motion.h): retargeting keeps momentum
    d->heightAnim = new M3SpringAnimation(this);
    d->heightAnim->setToken(M3Motion::SpatialDefault);
    connect(d->heightAnim, &M3SpringAnimation::valueChanged, this, [=](QVariant value) {
        this->setFixedHeight(value.toInt());  // no divider line any more: the window is exactly the content
    });

    connect(GestureDaemon::instance(), &GestureDaemon::gestureBegin, this, [=](GestureInteractionPtr gesture) {
        if (gesture->isValidInteraction(GestureTypes::Swipe, GestureTypes::Down, 3)) {
            if (d->barExpanding) {
                trackStatusCenterPullDownGesture(gesture);
            } else {
                trackBarPullDownGesture(gesture);
            }
        } else if (gesture->isValidInteraction(GestureTypes::Swipe, GestureTypes::Up, 3)) {
            if (d->statusCenterShown) {
                trackStatusCenterPullUpGesture(gesture);
            }
        }
    });

    // 0 = bar, 1 = Status Center. A crossfade, so the effects spring (no bounce)
    d->barStatusCenterTransitionAnim = new M3SpringAnimation(this);
    d->barStatusCenterTransitionAnim->setToken(M3Motion::EffectsDefault);
    d->barStatusCenterTransitionAnim->setStartValue(0.0);
    d->barStatusCenterTransitionAnim->setEndValue(0.0);
    connect(d->barStatusCenterTransitionAnim, &M3SpringAnimation::valueChanged, this, [=](QVariant value) {
        double percentage = qBound(0.0, value.toDouble(), 1.0);
        if (qFuzzyIsNull(percentage)) { // Fully show bar view
            d->statusCenterWidget->setVisible(false);
            d->mainBarWidget->setVisible(true);
            d->statusCenterOpacity->setEnabled(false);
            d->mainBarOpacity->setEnabled(false);
            this->setFixedHeight(d->mainBarWidget->expandedHeight());
            ui->line->setVisible(true);
        } else if (qFuzzyCompare(percentage, 1)) { // Fully show Status Center view
            d->statusCenterWidget->setVisible(true);
            d->mainBarWidget->setVisible(false);
            d->statusCenterOpacity->setEnabled(false);
            d->mainBarOpacity->setEnabled(false);
            this->setFixedHeight(d->statusCenterWidget->height());
            ui->line->setVisible(false);
        } else {
            if (percentage < 0.5) { // Animate the bar
                d->mainBarOpacity->setOpacity(1 - (percentage * 2));
                d->mainBarOpacity->setEnabled(true);
                d->mainBarWidget->setVisible(true);
                d->statusCenterWidget->setVisible(false);
                d->statusCenterOpacity->setEnabled(false);
            } else { // Animate the Status Center
                d->statusCenterOpacity->setOpacity(percentage * 2 - 1);
                d->statusCenterOpacity->setEnabled(true);
                d->statusCenterWidget->setVisible(true);
                d->mainBarWidget->setVisible(false);
                d->mainBarOpacity->setEnabled(false);
            }
            this->setFixedHeight((d->statusCenterWidget->height() - d->mainBarWidget->expandedHeight()) * percentage + d->mainBarWidget->expandedHeight());
            ui->line->setVisible(true);
        }
    });

    // Tell the window manager that this is a "taskbar" type window
    this->setWindowFlag(Qt::FramelessWindowHint);
    DesktopWm::instance()->setSystemWindow(this, DesktopWm::SystemWindowTypeTaskbar);
    DesktopWm::instance()->blurWindow(this);
    this->barHeightChanged();

    connect(qApp, &QApplication::primaryScreenChanged, this, &BarWindow::updatePrimaryScreen);
    updatePrimaryScreen();

    connect(StateManager::statusCenterManager(), &StatusCenterManager::showStatusCenter, this, &BarWindow::showStatusCenter);
    connect(StateManager::statusCenterManager(), &StatusCenterManager::hideStatusCenter, this, &BarWindow::hideStatusCenter);
    connect(StateManager::barManager(), &BarManager::barLockedChanged, this, [=](bool isBarLocked) {
        if (!isBarLocked) {
            //            showBar();
            //        } else {
            if (!this->geometry().contains(QCursor::pos())) hideBar();
        }
    });

    connect(&d->settings, &tSettings::settingChanged, this, [=](QString key, QVariant value) {
        if (key == "Appearance/translucent") {
            this->update();
        }
    });
    connect(DesktopWm::instance(), &DesktopWm::windowAdded, this, &BarWindow::trackWindow);
    connect(DesktopWm::instance(), &DesktopWm::windowRemoved, this, [=](DesktopWmWindowPtr window) {
        if (d->maximisedWindows.contains(window)) {
            d->maximisedWindows.removeAll(window);
            this->update();
        }
    });
    for (const DesktopWmWindowPtr& window : DesktopWm::openWindows()) {
        trackWindow(window);
    }

    KeyGrab* statusCenterGrab = new KeyGrab(QKeySequence(Qt::MetaModifier | Qt::Key_Tab), "statusCenter");
    connect(statusCenterGrab, &KeyGrab::activated, this, [=] {
        StateManager::statusCenterManager()->show();
    });

    ui->line->raise();

    // Initialise the Gateway
    Gateway::instance();
}

BarWindow::~BarWindow() {
    delete ui;
    delete d;
}

void BarWindow::setFixedHeight(int height) {
    QWidget::setFixedHeight(height);

    // Work around a Qt bug that causes the native window not to be updated on Wayland for some reason
    this->windowHandle()->setGeometry(this->geometry());
}

void BarWindow::setFixedWidth(int width) {
    QWidget::setFixedWidth(width);

    // Work around a Qt bug that causes the native window not to be updated on Wayland for some reason
    this->windowHandle()->setGeometry(this->geometry());
}

void BarWindow::applyShape() {
    // With a compositor, picom rounds the toolbar (antialiased, the shadow
    // following; a rule matches the window title). Without one, cut the
    // window to the toolbar's shape
    if (Compositor::isActive()) this->clearMask();
    else this->setMask(QRegion(floatingShape(QRectF(this->rect())).toFillPolygon().toPolygon()));
}

void BarWindow::resizeEvent(QResizeEvent* event) {
    ui->line->setGeometry(0, this->height() - 1, this->width(), this->height());
    ui->line->setFixedHeight(0); // the floating toolbar has no divider
    if (!d->tideLine) d->tideLine = new TideLine(this);
    double inner = qMin(floatCorner, this->height() / 2.0);
    d->tideLine->setGeometry(int(inner), this->height() - 12, this->width() - 2 * int(inner), 10);

    applyShape();
    d->mainBarWidget->setFixedWidth(this->width());
    d->mainBarWidget->barHeightChanged(this->height());
    d->statusCenterWidget->setFixedWidth(this->width());

    StateManager::barManager()->setBarHeight(this->height());
}

void BarWindow::enterEvent(QEnterEvent* event) {
    if (d->lastGesture && d->lastGesture->isActive()) return;
    d->barPendingShow = true;
    if ((this->layoutDirection() == Qt::RightToLeft && mapFromGlobal(QCursor::pos()).x() > d->mainBarWidget->currentAppWidgetX()) ||
        (this->layoutDirection() == Qt::LeftToRight && mapFromGlobal(QCursor::pos()).x() < d->mainBarWidget->currentAppWidgetX())) showBar();
}

void BarWindow::leaveEvent(QEvent* event) {
    if (d->lastGesture && d->lastGesture->isActive()) return;
    hideBar();
}

void BarWindow::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillPath(floatingShape(QRectF(this->rect())), this->palette().color(QPalette::Window));
}

bool BarWindow::event(QEvent* event) {
    // Not changeEvent: Qt hands ApplicationPaletteChange to event() only, and
    // the bar's own full palette means no PaletteChange follows it, so the
    // bar kept the colours it started with while the horizon moved on
    if (event->type() == QEvent::ApplicationPaletteChange) applyToolbarPalette();
    return QWidget::event(event);
}

void BarWindow::applyToolbarPalette() {
    // The toolbar is SurfaceContainer; the bar's own widgets paint with
    // Window, so they match it
    M3::Scheme scheme = M3::currentScheme();
    QPalette pal = qApp->palette();
    QColor container = scheme.primary.rgb() == pal.color(QPalette::Highlight).rgb() ? scheme.surfaceContainer : pal.color(QPalette::Window);
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) pal.setColor(group, QPalette::Window, container);
    this->setPalette(pal);
}

void BarWindow::trackWindow(DesktopWmWindowPtr window) {
    window->disconnect(this);
    DesktopWm::instance()->disconnect(window);
    connect(window, &DesktopWmWindow::geometryChanged, this, [=] {
        trackWindow(window);
    });
    connect(window, &DesktopWmWindow::windowStateChanged, this, [=] {
        trackWindow(window);
    });
    connect(DesktopWm::instance(), &DesktopWm::currentDesktopChanged, window, [=] {
        trackWindow(window);
    });

    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    if (window->isMaximised() && !window->isMinimized() && screenGeometry.contains(window->geometry()) && window->isOnCurrentDesktop()) {
        if (d->maximisedWindows.contains(window)) return;
        d->maximisedWindows.append(window);
        this->update();
    } else if (!window->isMaximised() || window->isMinimized() || !window->isOnCurrentDesktop()) {
        if (!d->maximisedWindows.contains(window)) return;
        d->maximisedWindows.removeAll(window);
        this->update();
    }
}

void BarWindow::updatePrimaryScreen() {
    if (d->oldPrimaryScreen) {
        disconnect(d->oldPrimaryScreen, &SystemScreen::geometryChanged, this, &BarWindow::updatePrimaryScreen);
        disconnect(d->oldPrimaryScreen, &SystemScreen::rotationChanged, this, &BarWindow::updatePrimaryScreen);
    }

    auto* primaryScreen = ScreenDaemon::instance()->primayScreen();
    connect(primaryScreen, &SystemScreen::geometryChanged, this, &BarWindow::updatePrimaryScreen);
    connect(primaryScreen, &SystemScreen::rotationChanged, this, &BarWindow::updatePrimaryScreen);
    d->oldPrimaryScreen = primaryScreen;

    if (primaryScreen) {
        // Qt adjusts the size of the window for us, so we'll need to adjust for that
        auto primaryGeometry = primaryScreen->geometry();
        primaryGeometry.setSize(primaryGeometry.size() / primaryScreen->qtScreen()->devicePixelRatio());

        this->setFixedWidth(primaryGeometry.width() - 2 * floatInset);
        this->move(primaryGeometry.topLeft() + QPoint(floatInset, floatInset));
        d->statusCenterWidget->setFixedHeight(primaryGeometry.height() - 2 * floatInset);
    }

    // Refresh the state of all the windows
    for (const DesktopWmWindowPtr& window : DesktopWm::openWindows()) {
        trackWindow(window);
    }

    barHeightChanged();
}

void BarWindow::barHeightChanged() {
    DesktopWm::setScreenMarginForWindow(this, qApp->primaryScreen(), Qt::TopEdge, (d->mainBarWidget->statusBarHeight() + 2 * floatInset) * (ScreenDaemon::instance()->primayScreen() ? ScreenDaemon::instance()->primayScreen()->qtScreen()->devicePixelRatio() : 1));
    d->mainBarWidget->setFixedHeight(d->mainBarWidget->expandedHeight());

    if (!d->statusCenterShown) {
        animateHeight(d->barExpanding ? d->mainBarWidget->expandedHeight() : d->mainBarWidget->statusBarHeight());
    }
}

void BarWindow::showStatusCenter() {
    QSignalBlocker blocker(d->heightAnim);
    d->heightAnim->stop();

    // Stay above tiled/floating windows (the WM does not restack docks)
    this->raise();
    if (!d->statusCenterShown) {
        d->focusBeforeStatusCenter = DesktopWm::activeWindow();
        d->inputFocusBeforeStatusCenter = currentInputFocus();
    }

    StateManager::statusCenterManager()->setIsShowingStatusCenter(true);
    d->barStatusCenterTransitionAnim->animateTo(1.0);

    // Tell the window manager that this is now a standard system window
    DesktopWm::instance()->setSystemWindow(this);
    this->activateWindow();
    forceInputFocus(this);
    d->statusCenterWidget->setFocus();
    d->statusCenterShown = true;
}

void BarWindow::hideStatusCenter() {
    const bool wasShown = d->statusCenterShown;
    StateManager::statusCenterManager()->setIsShowingStatusCenter(false);
    d->barStatusCenterTransitionAnim->animateTo(0.0);

    // Tell the window manager that this is now a "taskbar" type window
    DesktopWm::instance()->setSystemWindow(this, DesktopWm::SystemWindowTypeTaskbar);
    d->mainBarWidget->setFocus();
    d->statusCenterShown = false;

    // Give the keyboard back to the window that had it. The bar took X focus
    // itself (bspwm ignores unmanaged windows), so bspwm still thinks that
    // window is focused and ignores a request to activate it: hand X focus
    // back directly too (else apps got no keys until the Gateway opened).
    // Only if it was open: a pull-down let go too early, or a hide while
    // closed, must leave the focused app alone
    if (wasShown) {
        const bool ownWindow = d->inputFocusBeforeStatusCenter == quint32(this->winId());
        restoreInputFocus(ownWindow ? 0 : d->inputFocusBeforeStatusCenter);
        if (d->focusBeforeStatusCenter) d->focusBeforeStatusCenter->activate();
    }
    d->focusBeforeStatusCenter.clear();
    d->inputFocusBeforeStatusCenter = 0;
}

void BarWindow::showBar() {
    d->barPendingShow = false;
    // The expanded bar overlaps windows; make sure it is drawn above them,
    // except the Gateway's sheet, which stays on top while it's open
    if (!StateManager::gatewayManager()->isOpen()) this->raise();
    // If we're showing the status bar, don't touch the height
    if (!StateManager::statusCenterManager()->isShowingStatusCenter()) {
        animateHeight(d->mainBarWidget->expandedHeight());

        d->barExpanding = true;
    }
}

void BarWindow::hideBar() {
    // If we're showing the status bar, don't touch the height
    if (!StateManager::statusCenterManager()->isShowingStatusCenter() && !StateManager::barManager()->isBarLocked()) {
        animateHeight(d->mainBarWidget->statusBarHeight());

        d->barExpanding = false;
    }
}

void BarWindow::animateHeight(int target) {
    // Spring from wherever the bar is; a running spring keeps its momentum
    if (d->heightAnim->state() != QAbstractAnimation::Running) d->heightAnim->setStartValue(this->height());
    d->heightAnim->animateTo(target);
}

void BarWindow::trackBarPullDownGesture(GestureInteractionPtr gesture) {
    if (d->statusCenterShown || d->barExpanding) return;

    // Capture this gesture!
    d->lastGesture = gesture;

    int from = this->height();
    int to = d->mainBarWidget->expandedHeight();
    d->heightAnim->stop();
    d->barExpanding = true;

    connect(gesture.data(), &GestureInteraction::interactionUpdated, this, [=] {
        d->heightAnim->snapTo(int(from + (to - from) * gesture->percentage()));
    });
    connect(gesture.data(), &GestureInteraction::interactionEnded, this, [=] {
        if (gesture->extrapolatePercentage(100) > 0.7) {
            showBar();
            QTimer::singleShot(3000, this, [=] {
                if (gesture == d->lastGesture && !this->underMouse()) hideBar();
            });
        } else {
            hideBar();
        }
    });
}

void BarWindow::trackStatusCenterPullDownGesture(GestureInteractionPtr gesture) {
    if (d->statusCenterShown) return;

    // Capture this gesture!
    d->lastGesture = gesture;

    d->barStatusCenterTransitionAnim->snapTo(0.0);

    connect(gesture.data(), &GestureInteraction::interactionUpdated, this, [=] {
        d->barStatusCenterTransitionAnim->snapTo(gesture->percentage());
    });
    connect(gesture.data(), &GestureInteraction::interactionEnded, this, [=] {
        if (gesture->extrapolatePercentage(100) > 0.7) {
            showStatusCenter();
        } else {
            hideStatusCenter();
        }
    });
}

void BarWindow::trackStatusCenterPullUpGesture(GestureInteractionPtr gesture) {
    if (!d->statusCenterShown) return;

    // Only accept this gesture if there is not currently a popover active
    QList<QWidget*> childWidgets;
    childWidgets.append(this);
    while (!childWidgets.isEmpty()) {
        QWidget* w = childWidgets.takeFirst();
        if (tPopover::popoverForPopoverWidget(w)) return;
        for (QObject* o : w->children()) {
            if (qobject_cast<QWidget*>(o)) childWidgets.append(qobject_cast<QWidget*>(o));
        }
    }

    // Capture this gesture!
    d->lastGesture = gesture;

    d->barStatusCenterTransitionAnim->snapTo(1.0);

    connect(gesture.data(), &GestureInteraction::interactionUpdated, this, [=] {
        d->barStatusCenterTransitionAnim->snapTo(1.0 - gesture->percentage());
    });
    connect(gesture.data(), &GestureInteraction::interactionEnded, this, [=] {
        if (gesture->extrapolatePercentage(100) > 0.3) {
            hideStatusCenter();
        } else {
            showStatusCenter();
        }
    });
}

void BarWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->pos().x() < d->mainBarWidget->currentAppWidgetX() && d->barPendingShow) showBar();
}
