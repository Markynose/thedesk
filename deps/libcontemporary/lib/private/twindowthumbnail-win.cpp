#include "twindowthumbnail-win.h"
#include "nativeeventfilter.h"
#include <qt_windows.h>

tWindowThumbnailWin::tWindowThumbnailWin(QWidget *widget) : tWindowThumbnail(widget) {
    hwnd = reinterpret_cast<HWND>(widget->window()->winId());
    connect(libContemporaryPrivate::NativeEventFilter::instance(), &libContemporaryPrivate::NativeEventFilter::thumbnailCreated, this, [=](quintptr hwnd) {
        if (reinterpret_cast<HWND>(hwnd) == this->hwnd) {
            createList();
        }
    });

    connect(libContemporaryPrivate::NativeEventFilter::instance(), &libContemporaryPrivate::NativeEventFilter::thumbnailToolbarButtonClicked, this, [=](int id) {
        buttonActions[id]->activate(QAction::ActionEvent::Trigger);
    });

}

tWindowThumbnailWin::~tWindowThumbnailWin() { }

void tWindowThumbnailWin::createList() {
    auto list = winrt::create_instance<ITaskbarList4>(CLSID_TaskbarList);
    if (FAILED(list->HrInit())) {
        return;
    }

    listCreateMutex.lock();
    this->list = list;
    if (!buttonActions.empty()) {
        applyToolbarToList();
    }
    listCreateMutex.unlock();
}

QList<THUMBBUTTON> getWindowsThumbButtons(const QList<QAction*>& actions) {
    QList<THUMBBUTTON> buttons;

    for (int i = 0; i < actions.length(); i++) {
        QAction* action = actions[i];

        THUMBBUTTONFLAGS flags{};

        if (!action->isEnabled()) {
            flags |= THBF_DISABLED;
        }

        if (!action->isVisible()) {
            flags |= THBF_HIDDEN;
        }

        auto x = GetSystemMetrics(SM_CXICON);
        auto y = GetSystemMetrics(SM_CYICON);
        auto icon = action->icon().pixmap(QSize(x, y)).toImage().toHICON();
        
        auto button = THUMBBUTTON { 
            .dwMask = THB_ICON | THB_TOOLTIP | THB_TOOLTIP,
            .iId = static_cast<UINT>(i),
            .hIcon = icon,
            .dwFlags = flags,
        };
        
        action->toolTip().left(280).toWCharArray(button.szTip);

        buttons.append(button);
    }

    return buttons;
}

void tWindowThumbnailWin::updateButtons() {
    auto buttons = getWindowsThumbButtons(buttonActions);
    list->ThumbBarUpdateButtons(hwnd, buttons.length(), buttons.data());
}   

void tWindowThumbnailWin::applyToolbarToList() {
    auto buttons = getWindowsThumbButtons(buttonActions);

    for (QAction* action : buttonActions) {
        QObject::connect(action, &QAction::changed, this, &tWindowThumbnailWin::updateButtons);
    }

    winrt::check_hresult(list->ThumbBarAddButtons(hwnd, buttons.length(), buttons.data()));

    for (THUMBBUTTON button : buttons) {
        DestroyIcon(button.hIcon);
    }
}

void tWindowThumbnailWin::setToolbar(const QList<QAction*>& actions) {
    if (!buttonActions.empty()) return;

    listCreateMutex.lock();
    buttonActions = actions;
    if (list) {
        applyToolbarToList();
    }
    listCreateMutex.unlock();
}

void tWindowThumbnailWin::setProgressState(tWindowThumbnailProgressState state) {
    if (!list) {
        return;
    }

    TBPFLAG flag;

    switch (state) {
        case tWindowThumbnailProgressState::NoProgress: {
            flag = TBPF_NOPROGRESS;
            break;
        }
        case tWindowThumbnailProgressState::Normal: {
            flag = TBPF_NORMAL;
            break;
        }
        case tWindowThumbnailProgressState::Indeterminate: {
            flag = TBPF_INDETERMINATE;
            break;
        }
        case tWindowThumbnailProgressState::Paused: {
            flag = TBPF_PAUSED;
            break;
        }
        case tWindowThumbnailProgressState::Error: {
            flag = TBPF_ERROR;
            break;
        }

    }

    list->SetProgressState(hwnd, flag);
}

void tWindowThumbnailWin::setProgressValue(float percent) {
    if (!list) {
        return;
    }

    list->SetProgressValue(hwnd, static_cast<ULONGLONG>(percent * 100), 100);
}
