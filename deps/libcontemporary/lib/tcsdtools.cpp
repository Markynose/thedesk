/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#include "tcsdtools.h"

#include "tapplication.h"
#include <QDebug>
#include <QDir>
#include <QMainWindow>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <tcsdtools/csdbuttonbox.h>
#include <tcsdtools/csdsizegrip.h>

#ifdef HAVE_X11
    #include "tx11info.h"
    #include <X11/Xatom.h>
    #include <X11/Xlib.h>

    #ifdef HAVE_GSETTINGS
        #include <QGSettings>
    #endif
#endif

#ifdef Q_OS_WIN
    #include <Windows.h>
    #include <dwmapi.h>
#endif

struct tCsdGlobalPrivate {
        tCsdGlobal* instance = nullptr;
        bool enableCSDs;
};

struct ResizeWidget {
        QWidget* widget;
        CsdSizeGrip* sizeGrips[4];

        void setMarginsEnabled(bool marginsEnabled) {
            if (marginsEnabled) {
                int width = CsdSizeGrip::borderReserveWidth(widget);
                widget->setContentsMargins(width, width, width, width);
            } else {
                widget->setContentsMargins(0, 0, 0, 0);
            }
        }
};

struct tCsdToolsPrivate {
        QList<QWidget*> moveWidgets;
        QList<ResizeWidget*> resizeWidgets;

        static QList<QWidget*> csdWidgets;
#ifdef Q_OS_WIN
        static QList<HWND> csdHandles;
#endif
};

tCsdGlobalPrivate* tCsdGlobal::d = new tCsdGlobalPrivate();
QList<QWidget*> tCsdToolsPrivate::csdWidgets = QList<QWidget*>();

#ifdef Q_OS_WIN
QList<HWND> tCsdToolsPrivate::csdHandles = QList<HWND>();
#endif

tCsdGlobal::tCsdGlobal() :
    QObject(nullptr) {
    d->enableCSDs = recommendCsdForPlatform();
}

tCsdGlobal* tCsdGlobal::instance() {
    if (d->instance == nullptr) d->instance = new tCsdGlobal();
    return d->instance;
}

void tCsdGlobal::setCsdsEnabled(bool csdsEnabled) {
    tCsdGlobal::instance(); // Initialize the private struct
    d->enableCSDs = csdsEnabled;
    emit tCsdGlobal::instance()->csdsEnabledChanged(csdsEnabled);
}

bool tCsdGlobal::csdsEnabled() {
    tCsdGlobal::instance(); // Initialize the private struct
    return d->enableCSDs;
}

bool tCsdGlobal::recommendCsdForPlatform() {
    return true;
}

tCsdTools::tCsdTools(QObject* parent) :
    QObject(parent) {
    d = new tCsdToolsPrivate();
    connect(tCsdGlobal::instance(), &tCsdGlobal::csdsEnabledChanged, this, &tCsdTools::csdsEnabledChanged);

#ifdef Q_OS_WIN
    tApplication::instance()->installNativeEventFilter(this);
#endif
}

tCsdTools::~tCsdTools() {
    delete d;
}

tCsdGlobal::WindowControlSide tCsdGlobal::windowControlsEdge() {
#ifdef Q_OS_MAC
    // Window controls always on the left
    return Left;
#endif

#ifdef Q_OS_WIN
    // Window controls always on the right
    return Right;
#endif

#ifdef HAVE_X11
    if (tX11Info::isPlatformX11()) {
        // Check the current window manager
        Atom actualType;
        int actualFormat;
        unsigned long items, bytesRemain;
        unsigned char* data = nullptr;

        XGetWindowProperty(tX11Info::display(), tX11Info::appRootWindow(), XInternAtom(tX11Info::display(), "_NET_SUPPORTING_WM_CHECK", False), 0, 32, False, XA_WINDOW, &actualType, &actualFormat, &items, &bytesRemain, &data);
        if (data == nullptr) return Right;

        quint32 supportingWindow = reinterpret_cast<quint32*>(data)[0];
        XFree(data);

        // Get the name of the window manager
        XGetWindowProperty(tX11Info::display(), supportingWindow, XInternAtom(tX11Info::display(), "_NET_WM_NAME", False), 0, 1024, False, XInternAtom(tX11Info::display(), "UTF8_STRING", False), &actualType, &actualFormat, &items, &bytesRemain, &data);
        QString windowManagerName = QString::fromUtf8(reinterpret_cast<char*>(data));
        XFree(data);

        if (windowManagerName == "GNOME Shell") {
    #ifdef HAVE_GSETTINGS
            // Use GNOME settings
            QGSettings gsettings("org.gnome.desktop.wm.preferences");
            QString buttonLayout = gsettings.get("button-layout").toString();
            QString buttonsOnLeft = buttonLayout.split(":").first();
            if (buttonsOnLeft.contains("close")) {
                return Left;
            } else {
                return Right;
            }
    #endif
        } else if (windowManagerName == "KWin") {
            // Use KWin settings
            QSettings kwinSettings(QDir::homePath() + "/.config/kwinrc", QSettings::IniFormat);
            kwinSettings.beginGroup("org.kde.kdecoration2");
            if (kwinSettings.value("ButtonsOnLeft", "M").toString().contains("X")) {
                return Left;
            } else {
                return Right;
            }
        }
    }
#endif

    // Default to right hand side
    return Right;
}

void tCsdTools::installMoveAction(QWidget* widget) {
    d->moveWidgets.append(widget);
    widget->installEventFilter(this);
    connect(widget, &QWidget::destroyed, this, QOverload<QObject*>::of(&tCsdTools::removeMoveAction));
}

void tCsdTools::removeMoveAction(QWidget* widget) {
    d->moveWidgets.removeAll(widget);
    widget->removeEventFilter(this);
    disconnect(widget, &QWidget::destroyed, this, QOverload<QObject*>::of(&tCsdTools::removeMoveAction));
}

void tCsdTools::removeMoveAction(QObject* widget) {
    removeMoveAction(qobject_cast<QWidget*>(widget));
}

void tCsdTools::installResizeAction(QWidget* widget) {
    d->csdWidgets.append(widget);
#ifdef Q_OS_WIN
    auto hwnd = reinterpret_cast<HWND>(widget->winId());
    d->csdHandles.append(hwnd);
#endif
    connect(widget, &QWidget::destroyed, this, QOverload<QObject*>::of(&tCsdTools::removeResizeAction));
    widget->installEventFilter(this);

    auto resize = new ResizeWidget();
    resize->widget = widget;

#ifdef Q_OS_MAC
    macInstallResizeAction(widget);
#else
    if (tCsdGlobal::csdsEnabled()) {
        int border = CsdSizeGrip::borderReserveWidth(widget);
        widget->setContentsMargins(border, border, border, border);
    #ifdef Q_OS_WIN
        MARGINS borderless = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &borderless);

        // Force the window to recalculate its frame
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOREDRAW);
    #else
        widget->setWindowFlag(Qt::FramelessWindowHint);
        widget->setAttribute(Qt::WA_NoSystemBackground);
        widget->setAttribute(Qt::WA_TranslucentBackground);
    #endif
    }

    for (int i = 0; i < 4; i++) {
        CsdSizeGrip* grip = new CsdSizeGrip(i, widget);
        resize->sizeGrips[i] = grip;
    }
#endif
    d->resizeWidgets.append(resize);
}

void tCsdTools::removeResizeAction(QWidget* widget) {
    auto rw = getResizeWidget(widget);
    Q_ASSERT(rw);

    widget->removeEventFilter(this);
    disconnect(widget, &QWidget::destroyed, this, QOverload<QObject*>::of(&tCsdTools::removeResizeAction));

#ifdef Q_OS_MAC
    macRemoveResizeAction(widget);
#else
    for (int i = 0; i < 4; i++) {
        rw->sizeGrips[i]->deleteLater();
    }
#endif

    d->resizeWidgets.removeOne(rw);
    delete rw;

#ifdef Q_OS_WIN
    auto hwnd = reinterpret_cast<HWND>(widget->winId());
    d->csdHandles.removeOne(hwnd);
#endif
    d->csdWidgets.removeOne(widget);
}

void tCsdTools::removeResizeAction(QObject* widget) {
    removeResizeAction(qobject_cast<QWidget*>(widget));
}

ResizeWidget* tCsdTools::getResizeWidget(QWidget* widget) {
    for (ResizeWidget* rw : d->resizeWidgets) {
        if (rw->widget == widget) {
            return rw;
        }
    }
    return nullptr;
}

QWidget* tCsdTools::csdBoxForWidget(QWidget* widget) {
    return new CsdButtonBox(widget);
}

bool tCsdTools::csdsInstalled(QWidget* widget) {
    return tCsdToolsPrivate::csdWidgets.contains(widget);
}

void tCsdTools::csdsEnabledChanged(bool enabled) {
    for (ResizeWidget* rw : d->resizeWidgets) {
        bool showing = rw->widget->isVisible();
#ifdef Q_OS_WIN
        auto hwnd = reinterpret_cast<HWND>(rw->widget->winId());
#endif
        if (enabled) {
            int border = CsdSizeGrip::borderReserveWidth(rw->widget);
            rw->widget->setContentsMargins(border, border, border, border);
#ifdef Q_OS_WIN
            // Force the window to recalculate its frame
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOREDRAW);
#else
            rw->widget->setWindowFlag(Qt::FramelessWindowHint);
            rw->widget->setAttribute(Qt::WA_NoSystemBackground);
            rw->widget->setAttribute(Qt::WA_TranslucentBackground);
#endif
        } else {
            rw->widget->setContentsMargins(0, 0, 0, 0);
#ifdef Q_OS_WIN
            // Force the window to recalculate its frame
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOREDRAW);
#else
            rw->widget->setWindowFlag(Qt::FramelessWindowHint, false);
            rw->widget->setAttribute(Qt::WA_NoSystemBackground, false);
            rw->widget->setAttribute(Qt::WA_TranslucentBackground, false);
#endif
        }
        if (showing) rw->widget->show();
    }
}

QWidget* tCsdTools::widgetForPopover(QWidget* selected) {
    if (tCsdToolsPrivate::csdWidgets.contains(selected)) {
        if (QMainWindow* mw = qobject_cast<QMainWindow*>(selected)) {
            return mw->centralWidget();
        }
    }
    return selected;
}

bool tCsdTools::eventFilter(QObject* watched, QEvent* event) {
    // Ignore all handling if CSDs are disabled
    if (!tCsdGlobal::csdsEnabled()) return false;

    if (event->type() == QEvent::MouseButtonPress) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        Q_ASSERT(widget);

        QMouseEvent* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton) {
            if (d->moveWidgets.contains(widget) && e->button() == Qt::LeftButton) {
                // Move this window
                if (widget->window()->windowHandle()->startSystemMove()) return true;

#ifdef HAVE_X11
                if (tX11Info::isPlatformX11()) {
                    int x = e->globalPosition().x();
                    int y = e->globalPosition().y();
                    XClientMessageEvent event;
                    XUngrabPointer(tX11Info::display(), tX11Info::appTime());
                    event.type = ClientMessage;
                    event.message_type = XInternAtom(tX11Info::display(), "_NET_WM_MOVERESIZE", False);
                    event.window = widget->window()->winId(); // Move the parent window of the widget
                    event.format = 32;
                    event.data.l[0] = x;
                    event.data.l[1] = y;
                    event.data.l[2] = 8;
                    event.data.l[3] = Button1;
                    event.data.l[4] = 0;

                    XSendEvent(tX11Info::display(), tX11Info::appRootWindow(), False, SubstructureRedirectMask | SubstructureNotifyMask, reinterpret_cast<XEvent*>(&event));
                    return false;
                }
#endif

#ifdef Q_OS_WIN
                // Use Windows APIs to move the window
                ReleaseCapture();
                SendMessage(reinterpret_cast<HWND>(widget->window()->winId()), WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return true;
#endif

#ifdef Q_OS_MAC
                macHandleDrag(e->globalPos(), widget->window());
                return true;
#endif

                // Move window using Qt methods
                qWarning() << "Unsupported platform; moving window manually.";
                widget->setProperty("tcsdtools_action", "move");
                widget->setProperty("tcsdtools_mousepoint", widget->window()->mapFromGlobal(e->globalPosition()));

                return true; // Prevent further handling of this event
            }
        }
    } else if (event->type() == QEvent::MouseMove) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        Q_ASSERT(widget);

        QMouseEvent* e = static_cast<QMouseEvent*>(event);
        if (d->moveWidgets.contains(widget) && widget->property("tcsdtools_action").toString() == "move") {
            // Move this window using Qt methods
            QPoint oldPos = widget->property("tcsdtools_mousepoint").toPoint();
            widget->window()->move(e->globalPosition().toPoint() - oldPos);
            return true; // Prevent further handling of this event
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        Q_ASSERT(widget);

        QMouseEvent* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::RightButton) {
#ifdef Q_OS_WIN
            // Show the system menu
            auto hwnd = reinterpret_cast<HWND>(widget->window()->winId());
            auto menu = GetSystemMenu(hwnd, false);
            auto cmd = TrackPopupMenu(menu, TPM_RETURNCMD, e->globalPosition().x(), e->globalPosition().y(), 0, hwnd, NULL);
            if (cmd > 0) {
                SendMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
            }
#endif
        } else {
            if (d->moveWidgets.contains(widget) && widget->property("tcsdtools_action").toString() == "move") {
                // Stop moving the window
                widget->setProperty("tcsdtools_action", "none");
            }
        }
    } else if (event->type() == QEvent::Resize || event->type() == QEvent::WindowStateChange) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        Q_ASSERT(widget);

        ResizeWidget* rw = getResizeWidget(widget);
        if (rw != nullptr) {
            bool marginsEnabled = true;
#ifndef Q_OS_WIN
            if (widget->isMaximized() || widget->isFullScreen()) marginsEnabled = false;
#endif
            rw->setMarginsEnabled(marginsEnabled);
        }
    } else if (event->type() == QEvent::Show) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        Q_ASSERT(widget);

        QTimer::singleShot(0, [this, widget] {
            if (d->csdWidgets.contains(widget)) {
                this->removeResizeAction(widget);
                this->installResizeAction(widget);
            }
        });
    }
    return false;
}

bool tCsdTools::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        auto msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCCALCSIZE && tCsdGlobal::csdsEnabled()) {
            if (d->csdHandles.contains(msg->hwnd)) {
                result = 0;
                return true;
            }
        }
    }
#endif
    return false;
}
