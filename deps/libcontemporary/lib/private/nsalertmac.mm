//
// Created by Victor Tran on 12/4/2022.
//

#include "nsalertmac.h"
#include <QWindow>
#import <AppKit/AppKit.h>

struct NsAlertMacPrivate {
    QWidget* parentWindow;
    NSAlert* nsAlert;
    QList<QPushButton*> orders;

    QMessageBox::StandardButton clicked;
};

NsAlertMac::NsAlertMac(QObject *parent) : QObject(parent) {
    d = new NsAlertMacPrivate();
    d->nsAlert = [[NSAlert alloc] init];
}

NsAlertMac::~NsAlertMac() {
    delete d;
}

int NsAlertMac::exec() {
    NSModalResponse response = [d->nsAlert runModal];
    alertResultToQt(response);
    return d->clicked;
}

void NsAlertMac::open() {
    NSView* windowView = reinterpret_cast<NSView *>(d->parentWindow->windowHandle()->winId());
    [d->nsAlert beginSheetModalForWindow:[windowView window] completionHandler:^(NSModalResponse response) {
                         alertResultToQt(response);

                         emit accepted(d->clicked);
                         emit finished(d->clicked);
                       }];
}

void NsAlertMac::setTitle(QString title) {
    [d->nsAlert setMessageText:title.toNSString()];
}

void NsAlertMac::setText(QString text) {
    [d->nsAlert setInformativeText:text.toNSString()];
}

QPushButton *NsAlertMac::addButton(QString text) {
    [d->nsAlert addButtonWithTitle:text.toNSString()];

    QPushButton* button = new QPushButton();
    connect(this, &NsAlertMac::destroyed, button, &QPushButton::deleteLater);
    d->orders.append(button);
    return button;
}

void NsAlertMac::addStandardButton(QMessageBox::StandardButton standardButton) {
    QString text;
    switch (standardButton) {
        case QMessageBox::NoButton:
            break;
        case QMessageBox::Ok:
            text = tr("OK");
            break;
        case QMessageBox::Save:
            text = tr("Save");
            break;
        case QMessageBox::SaveAll:
            text = tr("Save All");
            break;
        case QMessageBox::Open:
            text = tr("Open");
            break;
        case QMessageBox::Yes:
            text = tr("Yes");
            break;
        case QMessageBox::YesToAll:
            text = tr("Yes to All");
            break;
        case QMessageBox::No:
            text = tr("No");
            break;
        case QMessageBox::NoToAll:
            text = tr("No to All");
            break;
        case QMessageBox::Abort:
            text = tr("Abort");
            break;
        case QMessageBox::Retry:
            text = tr("Retry");
            break;
        case QMessageBox::Ignore:
            text = tr("Ignore");
            break;
        case QMessageBox::Close:
            text = tr("Close");
            break;
        case QMessageBox::Cancel:
            text = tr("Cancel");
            break;
        case QMessageBox::Discard:
            text = tr("Don't Save");
            break;
        case QMessageBox::Help:
            text = tr("Help");
            break;
        case QMessageBox::Apply:
            text = tr("Apply");
            break;
        case QMessageBox::Reset:
            text = tr("Reset");
            break;
        case QMessageBox::RestoreDefaults:
            text = tr("Restore Defaults");
            break;
        default:
            return;
    }

    QPushButton* button = addButton(text);
    connect(button, &QPushButton::clicked, this, [=] {
        d->clicked = standardButton;
    });
}

void NsAlertMac::setParentWindow(QWidget *parent) {
    d->parentWindow = parent;
}

void NsAlertMac::alertResultToQt(int alertResult) {
    switch (alertResult) {
        case NSModalResponseOK:
            d->clicked = QMessageBox::Ok;
            break;
        case NSModalResponseCancel:
            d->clicked = QMessageBox::Cancel;
            break;
        default: {
            int index = alertResult - NSAlertFirstButtonReturn;
            d->orders.at(index)->click();
        }
    }
}
void NsAlertMac::setIcon(QMessageBox::Icon icon) {
    switch (icon) {
        case QMessageBox::NoIcon:
            break;
        case QMessageBox::Information:
        case QMessageBox::Question:
            [d->nsAlert setAlertStyle:NSAlertStyleInformational];
            break;
        case QMessageBox::Warning:
            [d->nsAlert setAlertStyle:NSAlertStyleWarning];
            break;
        case QMessageBox::Critical:
            [d->nsAlert setAlertStyle:NSAlertStyleCritical];
            break;
    }
}
