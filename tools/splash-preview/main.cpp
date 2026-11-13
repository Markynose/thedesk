/****************************************
 *
 *   theDesk - living type loading screen preview
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "livingtypeview.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

class Preview : public QWidget {
    public:
        Preview() {
            auto* layout = new QVBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            view = new LivingTypeView(this);
            layout->addWidget(view, 1);
            help = new QLabel(this);
            help->setContentsMargins(12, 6, 12, 6);
            layout->addWidget(help);
            connect(view, &LivingTypeView::exitFinished, this, [this] {
                QTimer::singleShot(900, view, &LivingTypeView::restart);
            });
            updateHelp();
        }

        LivingTypeView* view;

    protected:
        void keyPressEvent(QKeyEvent* event) override {
            switch (event->key()) {
                case Qt::Key_1:
                    view->setMode(LivingTypeView::Collage);
                    break;
                case Qt::Key_2:
                    view->setMode(LivingTypeView::Field);
                    break;
                case Qt::Key_S:
                    slow = !slow;
                    view->setTimeScale(slow ? 0.25 : 1);
                    break;
                case Qt::Key_E:
                    view->playExit();
                    break;
                case Qt::Key_X:
                    // Cycle: random, vortex, bloom, ripple, drop, sweep, cascade
                    view->setExitStyle(LivingTypeView::ExitStyle((view->exitStyle() + 2) % (LivingTypeView::ExitStyleCount + 1) - 1));
                    break;
                case Qt::Key_R:
                    view->restart();
                    break;
                case Qt::Key_Escape:
                    qApp->quit();
                    break;
                default:
                    break;
            }
            updateHelp();
        }

    private:
        QLabel* help;
        bool slow = false;

        void updateHelp() {
            help->setText(QStringLiteral("1 collage   2 field   S slow motion (%1)   E exit   X exit style (%3)   R restart   Esc quit      now: %2")
                              .arg(slow ? QStringLiteral("on") : QStringLiteral("off"), view->mode() == LivingTypeView::Collage ? QStringLiteral("collage") : QStringLiteral("field"), LivingTypeView::exitStyleName(view->exitStyle())));
        }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    Preview preview;
    if (app.arguments().contains(QStringLiteral("--field"))) {
        preview.view->setMode(LivingTypeView::Field);
        QKeyEvent refresh(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier);
        QApplication::sendEvent(&preview, &refresh);
    }
    if (app.arguments().contains(QStringLiteral("--mlm"))) preview.view->setFlagColours(true);
    int exitArg = app.arguments().indexOf(QStringLiteral("--exit"));
    if (exitArg >= 0 && exitArg + 1 < app.arguments().size()) {
        for (int style = LivingTypeView::RandomExit; style < LivingTypeView::ExitStyleCount; style++) {
            if (LivingTypeView::exitStyleName(style) == app.arguments().at(exitArg + 1)) preview.view->setExitStyle(LivingTypeView::ExitStyle(style));
        }
    }
    preview.resize(1366, 768);
    if (app.arguments().contains(QStringLiteral("--fullscreen"))) {
        preview.showFullScreen();
    } else {
        preview.show();
    }
    return app.exec();
}
