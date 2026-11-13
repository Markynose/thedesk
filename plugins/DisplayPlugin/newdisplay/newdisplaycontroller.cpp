/****************************************
 *
 *   theDesk - a newly connected display
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "newdisplaycontroller.h"

#include "displaychooser.h"
#include "displayhandoff.h"
#include "displaywaitingscreen.h"
#include "xrandrlayout.h"
#include <aroundyou.h>
#include <m3motion.h>
#include <QApplication>
#include <QFile>
#include <QPointer>
#include <QScreen>
#include <QSet>
#include <QTimer>
#include <Screens/screendaemon.h>
#include <Screens/systemscreen.h>
#include <tlogger.h>
#include <tpopover.h>
#include <transparentdialog.h>
#include <tsettings.h>

struct NewDisplayControllerPrivate {
        QSet<QString> published; // displays shown in Around you
        tSettings settings;
        QTimer* settle = nullptr;
        QTimer* waitForScreens = nullptr;
        int waitedMs = 0;

        QString handledKey; // the set of displays already dealt with
        bool asking = false;
        QList<QPointer<DisplayWaitingScreen>> waiting;
        QPointer<TransparentDialog> dialog;
        QPointer<tPopover> popover;
        bool chosen = false;

        // Handing the choice over: the main screen it was made on, and the
        // wait for the displays to come back as chosen
        QPointer<QScreen> mainQt;
        QTimer* revealWait = nullptr;
        QString revealSeen;
        int revealStable = 0, revealWaited = 0;
};

namespace {
    NewDisplayController* current = nullptr;

    XRandrLayout::Layout layoutFor(NewDisplayController::Choice choice) {
        switch (choice) {
            case NewDisplayController::Mirror:
                return XRandrLayout::Mirror;
            case NewDisplayController::OnlyMain:
                return XRandrLayout::OnlyMain;
            case NewDisplayController::OnlyOther:
                return XRandrLayout::OnlyOthers;
            case NewDisplayController::Extend:
            default:
                return XRandrLayout::Extend;
        }
    }

    QString settingFor(const QString& key) {
        return QStringLiteral("NewDisplay/") + key;
    }

    // Every screen Qt has, where and how big: for telling when a change
    // of displays has settled
    QString screensNow() {
        QStringList parts;
        for (QScreen* screen : qApp->screens()) {
            const QRect g = screen->geometry();
            parts.append(QStringLiteral("%1:%2,%3,%4,%5").arg(screen->name()).arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height()));
        }
        parts.sort();
        return parts.join(QLatin1Char(' '));
    }

    QString nameOf(QScreen* screen) {
        QString name = screen->model().trimmed();
        if (name.isEmpty()) name = screen->name();
        return name;
    }
} // namespace

NewDisplayController::NewDisplayController(QObject* parent) :
    QObject(parent) {
    d = new NewDisplayControllerPrivate();
    current = this;

    // RandR sends a burst of changes for one plug: act once it settles
    d->settle = new QTimer(this);
    d->settle->setSingleShot(true);
    d->settle->setInterval(700);
    connect(d->settle, &QTimer::timeout, this, &NewDisplayController::screensChanged);
    connect(ScreenDaemon::instance(), &ScreenDaemon::screensUpdated, d->settle, qOverload<>(&QTimer::start));
    connect(qApp, &QGuiApplication::screenAdded, d->settle, qOverload<>(&QTimer::start));
    connect(qApp, &QGuiApplication::screenRemoved, d->settle, qOverload<>(&QTimer::start));

    // A display just switched on becomes a QScreen a moment later
    d->waitForScreens = new QTimer(this);
    d->waitForScreens->setInterval(200);
    connect(d->waitForScreens, &QTimer::timeout, this, [this] {
        d->waitedMs += 200;
        int on = 0;
        for (const XRandrLayout::Output& output : XRandrLayout::outputs()) {
            if (output.on) on++;
        }
        if (qApp->screens().count() < on && d->waitedMs < 5000) return;
        d->waitForScreens->stop();
        ask();
    });

    d->settle->start();

    // Test hook: TD_NEWDISPLAY_TEST=<seconds> asks about the screens there
    // are, as if a display had just been connected
    if (qEnvironmentVariableIsSet("TD_NEWDISPLAY_TEST")) {
        QTimer::singleShot(int(qEnvironmentVariable("TD_NEWDISPLAY_TEST").toDouble() * 1000), this, &NewDisplayController::ask);
    }
}

NewDisplayController::~NewDisplayController() {
    if (current == this) current = nullptr;
    finish();
    delete d;
}

NewDisplayController* NewDisplayController::instance() {
    return current;
}

SystemScreen* NewDisplayController::mainScreen() {
    const QString name = XRandrLayout::mainOutput();
    for (SystemScreen* screen : ScreenDaemon::instance()->screens()) {
        if (screen->qtScreen() && screen->qtScreen()->name() == name) return screen;
    }
    return ScreenDaemon::instance()->primayScreen();
}

bool NewDisplayController::isLaptop() {
    // SMBIOS chassis types: portable, laptop, notebook, sub notebook,
    // hand held, tablet, convertible, detachable
    QFile file(QStringLiteral("/sys/class/dmi/id/chassis_type"));
    if (!file.open(QFile::ReadOnly)) return false;
    static const QList<int> portable = {8, 9, 10, 11, 14, 30, 31, 32};
    return portable.contains(file.readAll().trimmed().toInt());
}

bool NewDisplayController::askAgain() {
    if (d->asking) return true;
    int connected = 0;
    for (const XRandrLayout::Output& output : XRandrLayout::outputs()) {
        if (output.connected) connected++;
    }
    if (connected < 2) return false;
    ask();
    return true;
}

void NewDisplayController::publish(bool announce) {
    // Every display but the main one, in Around you
    const QString mainName = XRandrLayout::mainOutput();
    const QString saved = d->settings.value(settingFor(XRandrLayout::arrangementKey())).toString();
    QSet<QString> seen;
    for (const XRandrLayout::Output& output : XRandrLayout::outputs()) {
        if (!output.connected || output.name == mainName) continue;
        const QString id = QStringLiteral("display:") + (output.edidHash.isEmpty() ? output.name : output.edidHash);
        seen.insert(id);
        QString name;
        for (QScreen* screen : qApp->screens()) {
            if (screen->name() == output.name) name = screen->model().trimmed();
        }
        if (name.isEmpty()) name = output.name.startsWith(QStringLiteral("HDMI")) ? tr("TV or display") : tr("Display");
        AroundYouDevice device;
        device.id = id;
        device.symbol = output.name.startsWith(QStringLiteral("HDMI")) ? QStringLiteral("tv") : QStringLiteral("desktop_windows");
        device.name = name;
        device.state = !output.on                                  ? tr("Off")
                       : saved == QStringLiteral("mirror")         ? tr("Mirroring")
                       : saved == QStringLiteral("only-other")     ? tr("The only display on")
                                                                   : tr("Extended");
        device.activate = [this] {
            askAgain();
        };
        // In the sheet: dragged onto this computer, how to use it; dragged
        // away, switched off (only the main screen)
        device.connected = output.on;
        device.choices = {
            {tr("Extend"), [this] { apply(Extend); }},
            {tr("Mirror"), [this] { apply(Mirror); }},
            {output.name.startsWith(QStringLiteral("HDMI")) ? tr("Only TV") : tr("Only this display"), [this] { apply(OnlyOther); }}
        };
        device.disconnect = [this] {
            apply(OnlyMain);
        };
        AroundYou::instance()->setDevice(device, announce && !d->published.contains(id));
    }
    for (const QString& id : std::as_const(d->published)) {
        if (!seen.contains(id)) AroundYou::instance()->removeDevice(id);
    }
    d->published = seen;
}

void NewDisplayController::screensChanged() {
    if (!XRandrLayout::available()) return;
    QTimer::singleShot(0, this, [this] {
        publish(false);
    });
    const QList<XRandrLayout::Output> outputs = XRandrLayout::outputs();
    int connected = 0, on = 0;
    for (const XRandrLayout::Output& output : outputs) {
        if (output.connected) connected++;
        if (output.connected && output.on) on++;
    }
    const QString key = XRandrLayout::arrangementKey();
    tDebug("NewDisplay") << "displays:" << connected << "connected," << on << "on;" << (key == d->handledKey ? "seen" : "new set") << (d->asking ? "(asking)" : "");

    if (d->asking) {
        // What was being asked about changed (unplugged): stop asking
        if (key != d->handledKey) finish();
        else return;
    }
    if (connected == 0) return;

    // Some screen must always be on (after "only the other", unplugging it
    // left nothing lit)
    if (on == 0) {
        XRandrLayout::apply(XRandrLayout::Extend);
        d->handledKey = key;
        return;
    }

    // Our own changes come back here too: act only on a new set of displays
    if (key == d->handledKey) return;
    d->handledKey = key;
    if (connected < 2) {
        // Back to one display: make sure it's laid out alone (no space left
        // for a display that's gone)
        XRandrLayout::apply(XRandrLayout::Extend);
        return;
    }

    // Seen before: as it was chosen
    const QString saved = d->settings.value(settingFor(key)).toString();
    if (!saved.isEmpty()) {
        const Choice choice = saved == QStringLiteral("mirror") ? Mirror : saved == QStringLiteral("only-main") ? OnlyMain
                                                                       : saved == QStringLiteral("only-other")  ? OnlyOther
                                                                                                                : Extend;
        tDebug("NewDisplay") << "known displays, applying" << saved;
        XRandrLayout::apply(layoutFor(choice));
        // Set up as before, without asking: say so (Open shows it in Around you)
        QTimer::singleShot(800, this, [this] {
            publish(true);
        });
        return;
    }

    // New: switch them on beside the main screen (X leaves a new output
    // off) for the waiting screen, then ask once they are QScreens
    XRandrLayout::apply(XRandrLayout::Extend);
    d->waitedMs = 0;
    d->waitForScreens->start();
}

void NewDisplayController::ask() {
    const QString mainName = XRandrLayout::mainOutput();
    QScreen* mainQt = nullptr;
    QList<QScreen*> others;
    for (QScreen* screen : qApp->screens()) {
        if (screen->name() == mainName) mainQt = screen;
        else others.append(screen);
    }
    if (!mainQt) mainQt = qApp->primaryScreen();
    if (!mainQt) return;
    others.removeAll(mainQt);
    d->mainQt = mainQt;
    d->asking = true;
    d->chosen = false;
    const bool laptop = isLaptop();

    // The waiting screen on every other display that's on
    for (QScreen* screen : std::as_const(others)) {
        auto* waiting = new DisplayWaitingScreen(screen, laptop);
        waiting->show();
        d->waiting.append(waiting);
    }

    // Test hook on a single screen: its right half stands in for the display
    if (others.isEmpty() && qEnvironmentVariableIsSet("TD_NEWDISPLAY_TEST")) {
        auto* waiting = new DisplayWaitingScreen(mainQt, laptop);
        QRect half = mainQt->geometry();
        half.setLeft(half.center().x());
        waiting->setGeometry(half);
        waiting->show();
        d->waiting.append(waiting);
    }

    // The question, as a sheet on the main screen
    d->dialog = new TransparentDialog();
    d->dialog->setScreen(mainQt);
    d->dialog->showFullScreen();

    QPointer<TransparentDialog> dialog = d->dialog;
    const QString otherName = others.count() == 1 ? nameOf(others.first()) : QString();
    QTimer::singleShot(300, this, [this, dialog, otherName, laptop] {
        if (!dialog) return;
        auto* chooser = new DisplayChooser(otherName, laptop);
        auto* popover = new tPopover(chooser);
        d->popover = popover;
        popover->setPopoverSide(tPopover::Bottom);
        popover->setPopoverWidth(qMin(chooser->preferredHeight(), dialog->height() - 48));
        connect(chooser, &DisplayChooser::chosen, this, [this, popover, chooser](int choice) {
            d->chosen = true;
            const QRect tile = chooser->tileRect(choice);
            popover->dismiss();
            handOver(static_cast<Choice>(choice), tile);
        });
        connect(popover, &tPopover::dismissed, this, [this, popover, chooser, dialog] {
            // Closed without a choice: extended, as the displays are now
            if (!d->chosen) {
                d->chosen = true;
                apply(Extend);
            }
            popover->deleteLater();
            chooser->deleteLater();
            if (dialog) dialog->deleteLater();
        });
        popover->show(dialog);
    });
}

void NewDisplayController::remember(Choice choice) {
    static const char* names[] = {"extend", "mirror", "only-main", "only-other"};
    d->handledKey = XRandrLayout::arrangementKey();
    d->settings.setValue(settingFor(d->handledKey), QString::fromLatin1(names[int(choice)]));
}

void NewDisplayController::handOver(Choice choice, const QRect& tile) {
    QList<DisplayWaitingScreen*> targets;
    for (const auto& waiting : std::as_const(d->waiting)) {
        if (waiting) targets.append(waiting);
    }
    // Nothing to travel to (the other display goes dark, or none waits), or
    // calmer motion: just as chosen
    if (choice == OnlyMain || targets.isEmpty() || !d->mainQt || !tile.isValid() || !M3Motion::expressive()) {
        apply(choice);
        return;
    }
    remember(choice);

    // Mirror: the drop parts in two, and one half stays: the main screen
    // gets the same shape, and both open together
    if (choice == Mirror) {
        auto* twin = new DisplayWaitingScreen(d->mainQt, isLaptop(), DisplayWaitingScreen::Twin);
        twin->show();
        d->waiting.append(twin);
        targets.append(twin);
    }

    auto* handoff = new DisplayHandoff(tile, d->mainQt, targets, this);
    connect(handoff, &DisplayHandoff::landed, this, [this, choice] {
        if (!d->asking) return; // unplugged on the way
        for (const auto& waiting : std::as_const(d->waiting)) {
            if (waiting) waiting->land();
        }
        // The shape takes the drop in, then the displays become as chosen
        QTimer::singleShot(380, this, [this, choice] {
            if (d->asking) reveal(choice);
        });
    });
    handoff->start();
}

void NewDisplayController::reveal(Choice choice) {
    // Extended already (a new display is switched on beside the main one to
    // be asked about): nothing changes, the waiting screens just open
    bool extended = choice == Extend;
    if (extended) {
        const QList<QScreen*> screens = qApp->screens();
        int connected = 0;
        for (const XRandrLayout::Output& output : XRandrLayout::outputs()) {
            if (output.connected) connected++;
            if (output.connected && !output.on) extended = false;
        }
        if (screens.count() != connected) extended = false;
        for (int i = 0; i < screens.count(); i++) {
            for (int j = i + 1; j < screens.count(); j++) {
                if (screens.at(i)->geometry().intersects(screens.at(j)->geometry())) extended = false;
            }
        }
    }
    XRandrLayout::apply(layoutFor(choice)); // leaves alone what's already so
    if (extended) {
        for (const auto& waiting : std::as_const(d->waiting)) {
            if (waiting) waiting->open();
        }
        d->waiting.clear();
        d->asking = false;
        QTimer::singleShot(800, this, [this] {
            publish(false);
        });
        return;
    }

    // The displays go dark and come back: wait until Qt's screens have
    // stood still for a moment (at most 5 s), then cover and open them
    if (!d->revealWait) {
        d->revealWait = new QTimer(this);
        d->revealWait->setInterval(150);
    }
    d->revealWait->disconnect();
    d->revealSeen = screensNow();
    d->revealStable = 0;
    d->revealWaited = 0;
    connect(d->revealWait, &QTimer::timeout, this, [this, choice] {
        d->revealWaited += 150;
        const QString now = screensNow();
        d->revealStable = now == d->revealSeen ? d->revealStable + 1 : 0;
        d->revealSeen = now;
        if (d->revealStable < 5 && d->revealWaited < 5000) return;
        d->revealWait->stop();
        coverAndOpen(choice);
    });
    d->revealWait->start();
}

void NewDisplayController::coverAndOpen(Choice choice) {
    // The screens that show the result: the other displays when extending,
    // every screen left otherwise (mirrored ones share one picture: once)
    QList<QScreen*> screens;
    for (QScreen* screen : qApp->screens()) {
        if (choice == Extend && screen == d->mainQt) continue;
        bool covered = false;
        for (QScreen* other : std::as_const(screens)) {
            if (other->geometry().contains(screen->geometry())) covered = true;
        }
        if (!covered) screens.append(screen);
    }

    // Waiting screens that came through the change stay; the rest are
    // covers, the shape as the drop left it
    QList<QPointer<DisplayWaitingScreen>> open;
    for (QScreen* screen : std::as_const(screens)) {
        QPointer<DisplayWaitingScreen> standing;
        for (const auto& waiting : std::as_const(d->waiting)) {
            if (waiting && waiting->shownOn() == screen && !standing) standing = waiting;
        }
        if (!standing) {
            standing = new DisplayWaitingScreen(screen, isLaptop(), DisplayWaitingScreen::Cover);
            standing->show();
        }
        standing->setGeometry(screen->geometry());
        standing->raise();
        open.append(standing);
    }
    for (const auto& waiting : std::as_const(d->waiting)) {
        if (waiting && !open.contains(waiting)) waiting->deleteLater();
    }
    d->waiting.clear();
    d->asking = false;

    // A beat for a new cover to be on screen (and for eyes to come back to
    // a display that was dark), then they open
    QTimer::singleShot(450, this, [open] {
        for (const auto& waiting : open) {
            if (waiting) waiting->open();
        }
    });
    QTimer::singleShot(800, this, [this] {
        publish(false);
    });
}

void NewDisplayController::apply(Choice choice) {
    finish(); // the waiting screens go before the displays change under them
    remember(choice);
    XRandrLayout::apply(layoutFor(choice));
    QTimer::singleShot(800, this, [this] {
        publish(false);
    });
}

void NewDisplayController::finish() {
    for (const auto& waiting : std::as_const(d->waiting)) {
        if (waiting) waiting->dismiss();
    }
    d->waiting.clear();
    if (d->popover && !d->chosen) {
        d->chosen = true; // closing it isn't a choice
        d->popover->dismiss();
    }
    d->asking = false;
}
