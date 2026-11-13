//
// Created by Victor Tran on 12/4/2022.
//

#ifndef LIBCONTEMPORARY_NSALERTMAC_H
#define LIBCONTEMPORARY_NSALERTMAC_H

#include <QObject>
#include <QPushButton>
#include <QMessageBox>

struct NsAlertMacPrivate;
class NsAlertMac : public QObject {
    Q_OBJECT

public:
    NsAlertMac(QObject *parent);
    virtual ~NsAlertMac();

    void setParentWindow(QWidget* parent);
    void setTitle(QString title);
    void setText(QString text);
    QPushButton* addButton(QString text);
    void addStandardButton(QMessageBox::StandardButton standardButton);
    void setIcon(QMessageBox::Icon icon);

    int exec();
    void open();

    signals:
        void accepted(int response);
        void finished(int result);

private:
    NsAlertMacPrivate* d;

    void alertResultToQt(int alertResult);
};


#endif//LIBCONTEMPORARY_NSALERTMAC_H
