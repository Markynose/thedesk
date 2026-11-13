#ifndef TCOMPOSITETOUCHBAR_H
#define TCOMPOSITETOUCHBAR_H

#include "ttouchbar.h"

struct tCompositeTouchBarPrivate;
class tCompositeTouchBar : public tTouchBar {
        Q_OBJECT
    public:
        explicit tCompositeTouchBar(QObject* parent = nullptr);
        ~tCompositeTouchBar();

        void setCurrentTouchBar(tTouchBar* touchBar);
        tTouchBar* currentTouchBar();

    signals:

    private:
        tCompositeTouchBarPrivate* d;

        // tTouchBar interface
    public:
        void attach(QWidget* widget);
        void detach();
};

#endif // TCOMPOSITETOUCHBAR_H
