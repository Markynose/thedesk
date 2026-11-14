/****************************************
 *
 *   theDesk - a label whose changed characters roll
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#ifndef M3ROLLINGLABEL_H
#define M3ROLLINGLABEL_H

#include "libthedesk_global.h"
#include <QLabel>

class M3SpringAnimation;

/**
 * A plain-text QLabel for values that change (the bar's clock seconds, the
 * battery figure): when the text changes, the part that stays put stays and
 * the changed tail rolls up, the old one out at the top, the new one in from
 * below, on a fast spatial spring.
 */
class LIBTHEDESK_EXPORT M3RollingLabel : public QLabel {
        Q_OBJECT
    public:
        explicit M3RollingLabel(QWidget* parent = nullptr);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        QString shown, from;
        M3SpringAnimation* roll;
};

#endif // M3ROLLINGLABEL_H
