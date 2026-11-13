//
// Created by Victor Tran on 12/4/2022.
//

#ifndef LIBCONTEMPORARY_TMESSAGEBOXBACKEND_H
#define LIBCONTEMPORARY_TMESSAGEBOXBACKEND_H

#include "tmessagebox.h"
#include <QMessageBox>
#include "torderedmap.h"
#include <QPushButton>

class tMessageBoxButtonInfo : public QObject {
    Q_OBJECT
public:
    tMessageBoxButtonInfo(tMessageBoxButton *parent) : QObject(parent), label{}, buttonType{}, buttonStyle{}, isDefault(false) { }

    // A button either has a label and buttonStyle, or is a standard button with a buttonType.
    QString label;
    QMessageBox::ButtonRole buttonStyle;
    QMessageBox::StandardButton buttonType;
    bool isDefault;
};

struct tMessageBoxBackendPrivate;
class tMessageBoxBackend : public QObject {
    Q_OBJECT

public:
    tMessageBoxBackend(QObject* parent);
    ~tMessageBoxBackend();

    void init(QMessageBox::Icon style,
        const QIcon &icon,
        const QString &titleText,
        const QString &messageText,
        const QString &informativeText,
        const QString &detailedText,
        const QString &checkboxText,
        const tOrderedMap<tMessageBoxButton*, tMessageBoxButtonInfo*>& buttonMap);

    void open(QWidget* parent);

signals:
    void canBeDestroyed();

private:
    QScopedPointer<tMessageBoxBackendPrivate> d;
};


#endif//LIBCONTEMPORARY_TMESSAGEBOXBACKEND_H
