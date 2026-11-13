#include "tapplication.h"
#include "tmessageboxbackend.h"
#include <QWindow>
#include <QTimer>
#import <AppKit/AppKit.h>

struct tMessageBoxBackendPrivate {
//    QMessageBox box;
    QHash<NSModalResponse, tMessageBoxButton*> buttonStorage;
    NSAlert* nsAlert = nil;
};

tMessageBoxBackend::tMessageBoxBackend(QObject *parent) : QObject(parent), d(new tMessageBoxBackendPrivate) {
    d->nsAlert = [[NSAlert alloc] init];
}

tMessageBoxBackend::~tMessageBoxBackend() = default;

void tMessageBoxBackend::init(QMessageBox::Icon style,
                              const QIcon &icon,
                              const QString &titleText,
                              const QString &messageText,
                              const QString &informativeText,
                              const QString &detailedText,
                              const QString &checkboxText,
                              const tOrderedMap<tMessageBoxButton *, tMessageBoxButtonInfo *> &buttonMap) {

    switch (style) {
        case QMessageBox::NoIcon:
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

    QStringList informativeTextParts = {
            messageText,
            informativeText
    };
    informativeTextParts.removeAll("");

    [d->nsAlert setMessageText:titleText.toNSString()];
    [d->nsAlert setInformativeText:informativeTextParts.join(QString("\n\n")).toNSString()];

    //Use setAccessoryView: for detailed text
    if (!detailedText.isEmpty()) {
//        NSTextField* textField = [[NSTextField alloc] init];
//        [textField setStringValue:detailedText.toNSString()];
//        [textField setRea]

        QFontMetrics metrics(QGuiApplication::font());
        QStringList text;
        QString line;
        for (QChar c : detailedText) {
            line += c;
            if (metrics.horizontalAdvance(line) > SC_DPI_W(500, nullptr) || c == '\n') {
                text.append(line.trimmed() + "-");
                line.clear();
            }
        }
        if (!line.isEmpty()) text.append(line);

        NSTextField* textField = [NSTextField textFieldWithString:text.join('\n').toNSString()];
        [textField setEditable:NO];
        [d->nsAlert setAccessoryView:textField];

//        NSView* view = [[NSView alloc] init];
//        NSButton* disclosureButton = [[NSButton alloc] init];
//        NSTextField* disclosureText = [NSTextField labelWithString:@"Disclosure Triangle should be on the left"];

//        [disclosureButton setBezelStyle:NSDisclosureBezelStyle];
//        [view addSubview:disclosureButton];
//        [view addSubview:disclosureText positioned:NSWindowBelow relativeTo:disclosureButton];

//        [d->nsAlert setAccessoryView:view];
    }
//    d->box.setWindowTitle(titleText);
//    d->box.setText(messageText);
//    d->box.setInformativeText(informativeText);
//    d->box.setDetailedText(detailedText);


    if (!checkboxText.isEmpty()) {
        [d->nsAlert setShowsSuppressionButton:YES];
        [[d->nsAlert suppressionButton] setTitle:checkboxText.toNSString()];
    }

    int buttonIndex = 0;
    for (auto it = buttonMap.begin(); it != buttonMap.end(); it++) {
        auto tButton = it->first;
        auto info = it->second;

        NSModalResponse response = -1;
        QString text;
        if (info->label.isEmpty()) {
            switch (info->buttonType) {
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
                    break;
            }
        } else {
            text = info->label;
        }

        if (response == -1) {
            response = NSAlertFirstButtonReturn + buttonIndex;
            buttonIndex++;
        }

        NSButton* button = [d->nsAlert addButtonWithTitle:text.toNSString()];
        if (info->buttonStyle == QMessageBox::DestructiveRole) {
            if (@available(macOS 11.0, *)) {
                [button setHasDestructiveAction:YES];
            }
        }

        d->buttonStorage.insert(response, tButton);
    }
}

void tMessageBoxBackend::open(QWidget *parent) {
    NSView* windowView = reinterpret_cast<NSView *>(parent->windowHandle()->winId());
    [d->nsAlert beginSheetModalForWindow:[windowView window] completionHandler:^(NSModalResponse response) {
        QTimer::singleShot(0, this, [=] {
            emit d->buttonStorage.value(response)->buttonPressed([[d->nsAlert suppressionButton] state] == NSOnState);
            emit canBeDestroyed();
        });
    }];
}
