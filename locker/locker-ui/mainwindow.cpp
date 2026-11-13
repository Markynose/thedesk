#include "mainwindow.h"

#include "claudelockchip.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDateTime>
#include <QKeyEvent>
#include <QProcess>
#include <QSettings>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <Wm/desktopwm.h>
#include <horizon/horizonscene.h>
#include <horizon/horizonview.h>
#include <lockmanager.h>
#include <m3locksurface.h>

struct MainWindowPrivate {
        QString lockCheckProcess;
        M3LockSurface* surface;
        HorizonView* horizon;
};

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent) {
    d = new MainWindowPrivate();

    d->surface = new M3LockSurface(this);
    d->surface->setGreeting(tr("Locked"));
    d->surface->setPlaceholder(tr("Password to unlock"));
    this->setCentralWidget(d->surface);
    d->horizon = d->surface->horizon();
    connect(d->surface, &M3LockSurface::submitted, this, &MainWindow::submit);
    // Under the date: what Claude Code is doing while you're away
    d->surface->setClockNote(new ClaudeLockChip(d->surface));

    // The desktop's own wallpaper, exactly as it is now: its scene and the
    // turn it has for the current desktop, so locking doesn't change it
    QString desktopBackground = QSettings(QStringLiteral("theSuite"), QStringLiteral("theShell")).value(QStringLiteral("desktop/background")).toString();
    int scene = -1; // "living"
    for (int i = 0; i < HorizonScene::SceneCount; i++) {
        if (desktopBackground == QStringLiteral("horizon:") + HorizonScene::sceneName(i)) scene = i;
    }
    d->horizon->setFixedScene(scene);
    DesktopWm::instance(); // its window system backend, for the desktops
    d->horizon->setDesktop(int(DesktopWm::currentDesktop()), int(qMax(qsizetype(1), DesktopWm::desktops().count())), false);

    // (No text on the horizon: the old wake caption and notification
    // "moons" are gone; the lock is the desktop's picture, the clock and you)

    if (qEnvironmentVariableIsSet("TD_LOCK_CHECKER")) {
        d->lockCheckProcess = qEnvironmentVariable("TD_LOCK_CHECKER");
    } else {
        d->lockCheckProcess = QStringLiteral(TD_LOCKER_CHECKER_PATH);
    }

    QTimer::singleShot(0, this, [this] {
        d->surface->open(M3LockSurface::Opening::FromLock);
    });
}

MainWindow::~MainWindow() {
    delete d;
}

bool MainWindow::checkPassword(QString password) {
    QString name = qgetenv("USER");
    if (name.isEmpty()) {
        name = qgetenv("USERNAME");
    }

    QProcess checker;
    checker.start(d->lockCheckProcess, {name});
    if (!checker.waitForStarted()) return false;
    checker.write(password.toUtf8());
    checker.write("\n");
    checker.closeWriteChannel();
    if (!checker.waitForFinished(60000)) {
        // Never treat a hung or timed out checker as success
        checker.kill();
        checker.waitForFinished();
        return false;
    }
    // exitCode() is 0 for a process that crashed or never ran; check the status too
    return checker.exitStatus() == QProcess::NormalExit && checker.error() == QProcess::UnknownError && checker.exitCode() == 0;
}

void MainWindow::submit(const QString& password) {
    // Don't probe with an empty password: with pam_faillock every probe
    // counts as a failed attempt and can lock the account out. (Accounts
    // without a password still unlock with an empty one.)
    d->surface->setBusy(true);
    d->surface->showMessage(QString());
    QTimer::singleShot(0, this, [this, password] {
        if (checkPassword(password)) {
            // Open onto the desktop the way the loading screen does, then go
            connect(d->surface, &M3LockSurface::exitFinished, qApp, [] {
                QApplication::exit(0);
            });
            d->surface->playExitToDesktop();
            return;
        }
        d->surface->clearPassword();
        d->surface->setBusy(false);
        d->surface->showError(tr("That password didn't work. Try again."));
    });
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange) {
        LockManager::instance()->raiseAll();
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    DesktopWm::instance()->setSystemWindow(this, DesktopWm::SystemWindowTypeLockScreen);
    this->activateWindow();
    d->surface->focusPassword();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched)
    Q_UNUSED(event)
    return false;
}

