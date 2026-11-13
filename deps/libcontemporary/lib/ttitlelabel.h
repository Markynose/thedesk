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
#ifndef TTITLELABEL_H
#define TTITLELABEL_H

#include "libcontemporary_global.h"
#include <QLabel>

struct tTitleLabelPrivate;
class LIBCONTEMPORARY_EXPORT tTitleLabel : public QLabel {
        Q_OBJECT
        Q_PROPERTY(bool backButtonShown READ backButtonShown WRITE setBackButtonShown NOTIFY backButtonShownChanged)
        Q_PROPERTY(bool backButtonIsMenu READ backButtonIsMenu WRITE setBackButtonIsMenu NOTIFY backButtonIsMenuChanged)
    public:
        explicit tTitleLabel(QWidget* parent = nullptr);
        ~tTitleLabel();

        QSize sizeHint() const;

        bool backButtonShown();
        void setBackButtonShown(bool backButtonShown);

        bool backButtonIsMenu();
        void setBackButtonIsMenu(bool backButtonIsMenu);

        bool drawBottomBorder();
        void setDrawBottomBorder(bool drawBottomBorder);

    signals:
        void backButtonClicked();
        void backButtonShownChanged(bool backButtonShown);
        void backButtonIsMenuChanged(bool backButtonIsMenu);
        void drawBottomBorderChanged(bool drawBottomBorder);

    private:
        tTitleLabelPrivate* d;

        void updateFont();
        void updateDirection();

        void paintEvent(QPaintEvent* event);
        void resizeEvent(QResizeEvent* event);

        // QObject interface
    protected:
        void connectNotify(const QMetaMethod& signal);

        // QObject interface
    public:
        bool event(QEvent* event);
};

#endif // TTITLELABEL_H
