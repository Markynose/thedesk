//
// Created by Victor Tran on 12/4/2022.
//

#ifndef LIBCONTEMPORARY_TMESSAGEBOXBACKEND_WIN_H
#define LIBCONTEMPORARY_TMESSAGEBOXBACKEND_WIN_H

#include "tmessagebox.h"
#include <QHash>
#include <QMessageBox>
#include <Windows.h>
#include <commctrl.h>
#include <QPushButton>

class TaskDialogWorker : public QObject {
    Q_OBJECT
public:
    TaskDialogWorker(tMessageBox *messageBox, QHash<int, tMessageBoxButton *> &&buttonMap, const TASKDIALOGCONFIG &config);

    void showDialog();

signals:
    void finished();

private:
    tMessageBox *messageBox;
    QHash<int, tMessageBoxButton *> buttonMap;
    TASKDIALOGCONFIG config;
};

#endif //LIBCONTEMPORARY_TMESSAGEBOXBACKEND_WIN_H
