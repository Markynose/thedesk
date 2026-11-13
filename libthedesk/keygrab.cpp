/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "keygrab.h"

#include <QKeySequence>
#include <Wm/desktopwm.h>
#include <tsettings.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>

// Shortcuts can be overridden in the theDesk settings file:
//
//   [Shortcuts]
//   lockScreen=Meta+Shift+L     ; any QKeySequence in portable text format
//   gatewayOpen=Super_L          ; a bare Super key
//   nextlayoutAlternate=none     ; disable a shortcut
//
// Setting names are the second argument passed to KeyGrab.

struct KeyGrabPrivate {
        QKeySequence defaultSeq;
        QKeySequence seq;
        QString settingName;
        quint64 grabId = 0;
        bool isGrabbed = false;
        bool isPaused = true;
        bool disabled = false;
        tSettings* settings = nullptr;
};

namespace {
    QKeySequence parseSequence(const QString& text, bool* disabled) {
        *disabled = false;
        QString trimmed = text.trimmed();
        if (trimmed.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0 || trimmed.compare(QStringLiteral("disabled"), Qt::CaseInsensitive) == 0) {
            *disabled = true;
            return {};
        }
        if (trimmed.compare(QStringLiteral("Super_L"), Qt::CaseInsensitive) == 0 || trimmed.compare(QStringLiteral("Super"), Qt::CaseInsensitive) == 0) {
            return QKeySequence(Qt::Key_Super_L);
        }
        return QKeySequence::fromString(trimmed, QKeySequence::PortableText);
    }
} // namespace

KeyGrab::KeyGrab(QKeySequence seq, QObject* parent) :
    KeyGrab(seq, QString(), parent) {
}

KeyGrab::KeyGrab(QKeySequence defaultSeq, QString settingName, QObject* parent) :
    QObject(parent) {
    d = new KeyGrabPrivate();
    d->defaultSeq = defaultSeq;
    d->seq = defaultSeq;
    d->settingName = settingName;
    d->settings = new tSettings(this);

    if (!settingName.isEmpty()) {
        loadSetting();
        connect(d->settings, &tSettings::settingChanged, this, [this](QString key) {
            if (key != QStringLiteral("Shortcuts/") + d->settingName) return;
            bool wasActive = !d->isPaused;
            if (wasActive) pause();
            loadSetting();
            if (wasActive) resume();
        });
    }

    init();
}

namespace {
    QList<KeyGrab*>& allGrabs() {
        static QList<KeyGrab*> grabs;
        return grabs;
    }
} // namespace

KeyGrab::~KeyGrab() {
    allGrabs().removeAll(this);
    if (!d->isPaused) this->pause();
    delete d;
}

KeyGrabHub* KeyGrab::hub() {
    static KeyGrabHub* hub = new KeyGrabHub();
    return hub;
}

bool KeyGrab::trigger(const QString& settingName) {
    bool any = false;
    for (KeyGrab* grab : QList<KeyGrab*>(allGrabs())) {
        if (grab->d->settingName != settingName || grab->d->disabled) continue;
        any = true;
        emit grab->activated();
        emit hub()->activated(settingName);
    }
    return any;
}

void KeyGrab::loadSetting() {
    QString key = QStringLiteral("Shortcuts/") + d->settingName;
    d->seq = d->defaultSeq;
    d->disabled = false;
    // contains() only checks a single backing file; keysInGroup() merges the
    // defaults with the user's settings.
    if (!d->settings->keysInGroup(QStringLiteral("Shortcuts")).contains(d->settingName)) return;

    bool disabled;
    QKeySequence seq = parseSequence(d->settings->value(key).toString(), &disabled);
    if (disabled) {
        d->disabled = true;
    } else if (!seq.isEmpty()) {
        d->seq = seq;
    }
}

void KeyGrab::pause() {
    if (d->isGrabbed) DesktopWm::ungrabKey(d->grabId);
    d->isGrabbed = false;
    d->isPaused = true;
}

void KeyGrab::resume() {
    d->isPaused = false;
    if (d->disabled || d->seq.isEmpty() || d->isGrabbed) return;

    // Historically theDesk asked KGlobalAccel to quit so it could take over
    // global shortcuts. That also kills shortcuts registered by KWin and its
    // scripts (e.g. Polonium), so it is now opt-in.
    if (d->settings->value("Shortcuts/quitKGlobalAccel").toBool() && QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.kglobalaccel").value()) {
        QDBusMessage message = QDBusMessage::createMethodCall("org.kde.kglobalaccel", "/MainApplication", "org.qtproject.Qt.QCoreApplication", "quit");
        QDBusConnection::sessionBus().asyncCall(message);
    }

    QKeyCombination combination = d->seq[0];
    d->grabId = DesktopWm::grabKey(combination.key(), combination.keyboardModifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    d->isGrabbed = true;
}

void KeyGrab::init() {
    allGrabs().append(this);
    connect(DesktopWm::instance(), &DesktopWm::grabbedKeyPressed, this, [this](quint64 grabId) {
        if (!d->isGrabbed || d->grabId != grabId) return;
        emit activated();
        if (!d->settingName.isEmpty()) emit hub()->activated(d->settingName);
    });
    resume();
}
