#include "tmessageboxbackend.h"
#include "tapplication.h"
#include <Windows.h>
#include <commctrl.h>
#include <tlogger.h>
#include <QThread.h>

#include "tmessageboxbackend-win.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

struct tMessageBoxBackendPrivate {
    TASKDIALOGCONFIG td{0};
    QVector<TASKDIALOG_BUTTON> buttonArray;
    QHash<int, std::pair<std::wstring, tMessageBoxButton*>> buttonStorage;
    std::wstring titleText{};
    std::wstring messageText{};
    std::wstring informativeText{};
    std::wstring detailedCollapsedText{};
    std::wstring detailedText{};
    std::wstring checkboxText{};
    HICON icon;
};

tMessageBoxBackend::tMessageBoxBackend(QObject* parent) : QObject(parent), d(new tMessageBoxBackendPrivate) {}

tMessageBoxBackend::~tMessageBoxBackend() = default;

void tMessageBoxBackend::init(QMessageBox::Icon style,
    const QIcon& icon,
    const QString& titleText,
    const QString& messageText,
    const QString& informativeText,
    const QString& detailedText,
    const QString& checkboxText,
    const tOrderedMap<tMessageBoxButton*, tMessageBoxButtonInfo*>& buttonMap) {
    d->td.cbSize = sizeof(TASKDIALOGCONFIG);
    d->td.hInstance = nullptr;
    d->td.dwFlags = TDF_EXPAND_FOOTER_AREA;

    if (icon.isNull()) {
        switch (style) {
            case QMessageBox::Icon::NoIcon:
            case QMessageBox::Icon::Question:
                d->td.pszMainIcon = nullptr;
                break;
            case QMessageBox::Icon::Information:
                d->td.pszMainIcon = TD_INFORMATION_ICON;
                break;
            case QMessageBox::Icon::Warning:
                d->td.pszMainIcon = TD_WARNING_ICON;
                break;
            case QMessageBox::Icon::Critical:
                d->td.pszMainIcon = TD_ERROR_ICON;
                break;
        }
    } else {
        d->td.dwFlags |= TDF_USE_HICON_MAIN;
        d->icon = icon.pixmap(QSize(128, 128)).toImage().toHICON();
        d->td.hMainIcon = d->icon;
    }


    if (!titleText.isEmpty()) {
        d->titleText = titleText.toStdWString();
    } else {
        d->titleText = tApplication::applicationDisplayName().toStdWString();
    }

    d->td.pszWindowTitle = d->titleText.c_str();

    if (!messageText.isEmpty()) {
        d->messageText = messageText.toStdWString();
        d->td.pszMainInstruction = d->messageText.c_str();
    }

    if (!informativeText.isEmpty()) {
        d->informativeText = informativeText.toStdWString();
        d->td.pszContent = d->informativeText.c_str();
    }

    if (!detailedText.isEmpty()) {
        d->detailedText = detailedText.toStdWString();
        d->td.pszExpandedInformation = d->detailedText.c_str();
    }

    if (!checkboxText.isEmpty()) {
        d->checkboxText = checkboxText.toStdWString();
        d->td.pszVerificationText = d->checkboxText.c_str();
    }

    // Values 0-9 are reserved for default buttons
    int currentButtonId = 10;

    auto addCustomButton = [&](tMessageBoxButton * button, const QString & label) {
        TASKDIALOG_BUTTON taskDialogButton{};
        auto id = currentButtonId++;
        taskDialogButton.nButtonID = id;
        d->buttonStorage.insert(id, {label.toStdWString(), button});
        taskDialogButton.pszButtonText = d->buttonStorage[id].first.c_str();
        d->buttonArray.append(taskDialogButton);
        return id;
    };

    for (auto it = buttonMap.begin(); it != buttonMap.end(); it++) {
        auto [button, info] = *it;
        int buttonId;
        if (info->label.isEmpty()) {
            int flagValue = 0;
            switch (info->buttonType) {
                case QMessageBox::Ok:
                    flagValue = TDCBF_OK_BUTTON;
                    buttonId = IDOK;
                    break;
                case QMessageBox::Cancel:
                    flagValue = TDCBF_CANCEL_BUTTON;
                    buttonId = IDCANCEL;
                    break;
                case QMessageBox::Abort:
                    flagValue = 0x00010000;
                    buttonId = IDABORT;
                    break;
                case QMessageBox::Retry:
                    flagValue = TDCBF_RETRY_BUTTON;
                    buttonId = IDRETRY;
                    break;
                case QMessageBox::Ignore:
                    flagValue = 0x00020000;
                    buttonId = IDIGNORE;
                    break;
                case QMessageBox::Yes:
                    flagValue = TDCBF_YES_BUTTON;
                    buttonId = IDYES;
                    break;
                case QMessageBox::No:
                    flagValue = TDCBF_NO_BUTTON;
                    buttonId = IDNO;
                    break;
                case QMessageBox::Close:
                    flagValue = TDCBF_CLOSE_BUTTON;
                    buttonId = IDCLOSE;
                    break;
                case QMessageBox::Help:
                    flagValue = 0x00100000;
                    buttonId = IDHELP;
                    break;

                // The following do not have standard equivalents on Windows, so we make up custom buttons for them
                case QMessageBox::Open:
                    addCustomButton(button, tr("Open"));
                    continue;
                case QMessageBox::Save:
                    addCustomButton(button, tr("Save"));
                    continue;
                case QMessageBox::Discard:
                    addCustomButton(button, tr("Discard"));
                    continue;
                case QMessageBox::Apply:
                    addCustomButton(button, tr("Apply"));
                    continue;
                case QMessageBox::Reset:
                    addCustomButton(button, tr("Reset"));
                    continue;
                case QMessageBox::RestoreDefaults:
                    addCustomButton(button, tr("Restore defaults"));
                    continue;
                case QMessageBox::SaveAll:
                    addCustomButton(button, tr("Save all"));
                    continue;
                case QMessageBox::YesToAll:
                    addCustomButton(button, tr("Yes to all"));
                    continue;
                case QMessageBox::NoToAll:
                    addCustomButton(button, tr("No to all"));
                    continue;
                default:
                    continue;
            }
            d->td.dwCommonButtons |= flagValue;
            d->buttonStorage.insert(buttonId, {{}, button});
        } else {
            buttonId = addCustomButton(button, info->label);
        }

        if (info->isDefault) {
            d->td.nDefaultButton = buttonId;
        }
    }

    d->td.pButtons = d->buttonArray.constData();
    d->td.cButtons = d->buttonArray.count();
}

TaskDialogWorker::TaskDialogWorker(tMessageBox* messageBox, QHash<int, tMessageBoxButton*>&& buttonMap, const TASKDIALOGCONFIG& config)
    : QObject(static_cast<QObject*>(messageBox)), messageBox(messageBox), buttonMap(std::move(buttonMap)), config(config) {
}

void TaskDialogWorker::showDialog() {
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(this);
    config.pfCallback = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) -> HRESULT {
        auto worker = reinterpret_cast<TaskDialogWorker*>(lpRefData);
        switch (msg) {
            case TDN_HELP:
                if (auto helpButton = worker->buttonMap.value(IDHELP)) {
                    emit helpButton->buttonPressed(false);
                }
        }

        return S_OK;
    };


    int closingButton;
    BOOL checkBox;
    if (SUCCEEDED(TaskDialogIndirect(&config, &closingButton, nullptr, &checkBox))) {
        emit buttonMap.value(closingButton)->buttonPressed(checkBox != FALSE);
    }

    emit finished();
    if (config.hMainIcon) {
        DestroyIcon(config.hMainIcon);
    }
}


void tMessageBoxBackend::open(QWidget* parent) {
    if (parent) {
        d->td.hwndParent = reinterpret_cast<HWND>(parent->winId());
    }

    auto thread = new QThread(this);

    auto privateMap = d->buttonStorage;
    QHash<int, tMessageBoxButton*> buttonMap;

    for (auto it = privateMap.constKeyValueBegin(); it != privateMap.constKeyValueEnd(); it++) {
        auto [id, value] = *it;
        buttonMap.insert(id, value.second);
    }

    auto worker = new TaskDialogWorker(qobject_cast<tMessageBox*>(this->parent()), std::move(buttonMap), d->td);
    connect(thread, &QThread::started, worker, &TaskDialogWorker::showDialog);
    connect(worker, &TaskDialogWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, this, &tMessageBoxBackend::canBeDestroyed);
    worker->moveToThread(thread);
    thread->start();
}

