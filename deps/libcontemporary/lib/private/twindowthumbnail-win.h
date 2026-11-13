#ifndef LIBCONTEMPORARY_TWINDOWTHUMBNAIL_WIN_H
#define LIBCONTEMPORARY_TWINDOWTHUMBNAIL_WIN_H

#include "../twindowthumbnail.h"
#include <QWidget>
#include <QList>
#include <QAction>
#include <QMutex>
#include <ShObjIdl.h>
#include <winrt/base.h>

class tWindowThumbnailWin final : public tWindowThumbnail {
    Q_OBJECT
public:
    tWindowThumbnailWin(QWidget* widget);
    virtual ~tWindowThumbnailWin() override;

    virtual void setToolbar(const QList<QAction*>& actions) override;
    virtual void setProgressState(tWindowThumbnailProgressState state) override;
    virtual void setProgressValue(float percent) override;
private:
    void createList();
    void applyToolbarToList();
    void updateButtons();


    winrt::com_ptr<ITaskbarList4> list;
    QList<QAction*> buttonActions;
    QMutex listCreateMutex;
    HWND hwnd;
};

#endif //LIBCONTEMPORARY_TWINDOWTHUMBNAIL_WIN_H
