#include "customshortcuts.h"

#include <QDir>
#include <QKeySequence>
#include <QProcess>
#include <Wm/desktopwm.h>
#include <Wm/desktopwmwindow.h>
#include <keygrab.h>
#include <tlogger.h>
#include <tsettings.h>

struct CustomShortcutsPrivate {
        tSettings settings;
        QList<KeyGrab*> grabs;
};

CustomShortcuts* CustomShortcuts::instance() {
    static auto* instance = new CustomShortcuts();
    return instance;
}

CustomShortcuts::CustomShortcuts(QObject* parent) :
    QObject{parent} {
    d = new CustomShortcutsPrivate();
    connect(&d->settings, &tSettings::settingChanged, this, [this](QString key) {
        if (key.startsWith(QStringLiteral("CustomShortcuts/"))) reload();
    });
    reload();
}

CustomShortcuts::~CustomShortcuts() {
    qDeleteAll(d->grabs);
    delete d;
}

void CustomShortcuts::reload() {
    qDeleteAll(d->grabs);
    d->grabs.clear();

    for (const QString& sequenceText : d->settings.keysInGroup(QStringLiteral("CustomShortcuts"))) {
        QString action = d->settings.value(QStringLiteral("CustomShortcuts/") + sequenceText).toString().trimmed();
        QKeySequence sequence = QKeySequence::fromString(sequenceText, QKeySequence::PortableText);
        if (sequence.isEmpty() || action.isEmpty()) {
            tWarn("CustomShortcuts") << "Ignoring invalid custom shortcut" << sequenceText << "=" << action;
            continue;
        }

        auto* grab = new KeyGrab(sequence, this);
        connect(grab, &KeyGrab::activated, this, [this, action] {
            trigger(action);
        });
        d->grabs.append(grab);
    }
}

void CustomShortcuts::trigger(const QString& action) {
    if (action == QStringLiteral("@close-window") || action == QStringLiteral("@kill-window")) {
        DesktopWmWindowPtr window = DesktopWm::activeWindow();
        if (!window) return;
        if (action == QStringLiteral("@close-window")) {
            window->close();
        } else {
            window->kill();
        }
        return;
    }

    if (action.startsWith('@')) {
        tWarn("CustomShortcuts") << "Unknown built-in action" << action;
        return;
    }

    QStringList arguments = QProcess::splitCommand(action);
    if (arguments.isEmpty()) return;
    QString program = arguments.takeFirst();
    if (!QProcess::startDetached(program, arguments, QDir::homePath())) {
        tWarn("CustomShortcuts") << "Unable to launch" << action;
    }
}
