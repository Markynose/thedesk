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

#ifndef TTOAST_H
#define TTOAST_H

#include <QObject>
#include <QTimer>
#include <QBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPaintEvent>
#include "tpropertyanimation.h"

struct tToastPrivate;
class LIBCONTEMPORARY_EXPORT tToast : public QObject
{
    Q_OBJECT

public:
    explicit tToast(QObject *parent = 0);
    ~tToast();

    void setTitle(QString title);
    QString title();

    void setText(QString text);
    QString text();

    void setActions(QMap<QString, QString> actions);
    QMap<QString, QString> actions();

    void setTimeout(int milliseconds);
    int timeout();

signals:
    void actionClicked(QString key);
    void dismissed();
    void doDefaultOption();

public slots:
    void show(QWidget* parent);
    void announceAction(QString text);
    void dismiss();

private:
    tToastPrivate* d;

    bool canAnnounceAction = false;
    bool announcingAction = false;
    bool timerStopped = false;

    bool eventFilter(QObject *watched, QEvent *event);
    void updateToastGeometry();
};

#endif // TTOAST_H
