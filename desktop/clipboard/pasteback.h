#ifndef PASTEBACK_H
#define PASTEBACK_H

#include <Wm/desktopwm.h>

// Gives focus back to a window and presses its paste keys (Ctrl+V, or
// Ctrl+Shift+V in terminals): what's on the clipboard lands where the
// person was typing
namespace PasteBack {
    void into(DesktopWmWindowPtr target);
}

#endif // PASTEBACK_H
