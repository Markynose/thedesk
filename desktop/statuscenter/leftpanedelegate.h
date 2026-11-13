#ifndef LEFTPANEDELEGATE_H
#define LEFTPANEDELEGATE_H

#include <m3navigationdelegate.h>

// Status Center / System Settings side lists: M3 navigation rows (libthedesk)
class LeftPaneDelegate : public M3NavigationDelegate {
        Q_OBJECT
    public:
        explicit LeftPaneDelegate(QObject* parent = nullptr) :
            M3NavigationDelegate(parent) {}
};

#endif // LEFTPANEDELEGATE_H
