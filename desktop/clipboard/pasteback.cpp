#include "pasteback.h"

#include <Applications/application.h>
#include <QGuiApplication>
#include <QTimer>

// Xlib last: its macros clash with Qt's names
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

namespace {
    void sendPaste(bool terminal) {
        auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        if (!x11) return;
        Display* display = x11->display();
        // shortcut: needs a layout with a Latin "v" (XKeysymToKeycode gives 0 otherwise), switch to the layout's keysyms if that matters
        const KeyCode v = XKeysymToKeycode(display, XK_v);
        if (!v) return;
        auto key = [display](KeyCode code, bool down) {
            XTestFakeKeyEvent(display, code, down, CurrentTime);
        };
        const KeyCode control = XKeysymToKeycode(display, XK_Control_L), shift = XKeysymToKeycode(display, XK_Shift_L);
        key(control, true);
        if (terminal) key(shift, true);
        key(v, true);
        key(v, false);
        if (terminal) key(shift, false);
        key(control, false);
        XFlush(display);
    }
} // namespace

void PasteBack::into(DesktopWmWindowPtr target) {
    if (!target) return;
    bool terminal = false;
    if (ApplicationPointer app = target->application()) terminal = app->getStringList(QStringLiteral("Categories")).contains(QStringLiteral("TerminalEmulator"));
    // Focus first, then the keys reach it
    QTimer::singleShot(80, target, [target, terminal] {
        target->activate();
        QTimer::singleShot(120, [terminal] {
            sendPaste(terminal);
        });
    });
}
