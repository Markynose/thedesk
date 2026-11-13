#include "screenshotmanager.h"

#include "dialogs/screenshotwindow.h"
#include <tapplication.h>

struct ScreenshotManagerPrivate {
        QPixmap finalResult;
        QColor clickedColor;
        bool accepted = false;
        QRect recordRegion;
        QList<ScreenshotWindow*> screenshotWindows;
        ScreenshotWindow::Type type = ScreenshotWindow::Type::ApplicationScreenshot;
};

ScreenshotManager::ScreenshotManager(QObject* parent) :
    QObject{parent} {
    d = new ScreenshotManagerPrivate();

    for (auto screen : qApp->screens()) {
        auto w = new ScreenshotWindow(screen);
        connect(w, &ScreenshotWindow::screenshotAvailable, this, &ScreenshotManager::acceptPixmap);
        connect(w, &ScreenshotWindow::colourClicked, this, &ScreenshotManager::acceptColor);
        connect(w, &ScreenshotWindow::cancelled, this, &ScreenshotManager::reject);
        connect(w, &ScreenshotWindow::recordRequested, this, [this](QRect region) {
            // The windows step aside as they do for a discard, and the
            // recording starts once they're gone
            d->recordRegion = region;
            d->accepted = false;
            auto first = d->screenshotWindows.constFirst();
            connect(first, &ScreenshotWindow::animationComplete, this, &ScreenshotManager::finished);
            for (auto window : d->screenshotWindows) window->animateDiscard();
        });
        d->screenshotWindows.append(w);
    }
}

ScreenshotManager::~ScreenshotManager() {
    for (auto window : d->screenshotWindows) {
        window->deleteLater();
    }
    delete d;
}

void ScreenshotManager::setType(ScreenshotWindow::Type type) {
    d->type = type;
    for (auto window : d->screenshotWindows) {
        window->setType(type);
    }
}

QCoro::Task<> ScreenshotManager::prepareScreenshotWindows() {
    for (auto window : d->screenshotWindows) {
        co_await window->prepareWindow();
    }
}

void ScreenshotManager::showScreenshotWindows() {
    for (auto window : d->screenshotWindows) {
        window->showFullScreen();
    }
}

QPixmap ScreenshotManager::finalPixmap() {
    return d->finalResult;
}

QColor ScreenshotManager::clickedColor() {
    return d->clickedColor;
}

QRect ScreenshotManager::recordRegion() {
    return d->recordRegion;
}

bool ScreenshotManager::accepted() {
    return d->accepted;
}

void ScreenshotManager::acceptPixmap(QPixmap pixmap) {
    d->finalResult = pixmap;
    d->accepted = true;

    auto w = d->screenshotWindows.constFirst();
    connect(w, &ScreenshotWindow::animationComplete, this, &ScreenshotManager::finished);

    for (auto window : d->screenshotWindows) {
        window->animateTake();
    }
}

void ScreenshotManager::acceptColor(QColor color) {
    d->clickedColor = color;
    d->accepted = true;

    // Skip the animation since this is a colour picker
    emit finished();
}

void ScreenshotManager::reject() {
    d->accepted = false;
    auto w = d->screenshotWindows.constFirst();
    connect(w, &ScreenshotWindow::animationComplete, this, &ScreenshotManager::finished);

    for (auto window : d->screenshotWindows) {
        window->animateDiscard();
    }
}
