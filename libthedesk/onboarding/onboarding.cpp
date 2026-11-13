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
#include "onboarding.h"

#include <m3motion.h>
#include "ui_onboarding.h"

#include "onboardingbar.h"
#include "onboardingstepper.h"
#include "onboardingtext.h"
#include "onboardingchrome.h"
#include <QAudioDecoder>
#include <QAudioOutput>
#include <QKeyEvent>
#include <QPainter>
#include <common.h>
#include <onboardingmanager.h>
#include <onboardingpage.h>
#include <powermanager.h>
#include <statemanager.h>
#include <tscrim.h>
#include <tsettings.h>
#include <tvariantanimation.h>

#include <QAudioSink>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QShortcut>
#include <QPainterPath>
#include <m3scheme.h>
#include <horizon/horizonview.h>

#include <private/onboardingmanager_p.h>

struct OnboardingPrivate {
        QStringList preferredOnboardingOrder;
        QList<QPair<QString, OnboardingPage*>> steps;
        QList<QPair<OnboardingStepper*, OnboardingPage*>> steppers;

        QByteArray singleAudioData;
        QByteArray loopAudioData;
        QAudioSink* audioOutput = nullptr;
        QIODevice* audioOutputDevice;
        int audioPointer = 0;
        bool audioEnabled = true;

        OnboardingBar* bar;
        bool barVisible = false;

        tVariantAnimation* contentAnim;
        tVariantAnimation* barAnim;
        tVariantAnimation* fadeAnim;

        bool onboardingStarted = false;
        HorizonView* horizon = nullptr; // the living horizon behind it all
        bool textOnShow = false;        // started before being shown: play the words then
        OnboardingHero* hero = nullptr;         // every step's picture, morphing between them
        OnboardingProgress* progress = nullptr; // the wavy line across the card's top
        tSettings settings;
};

Onboarding::Onboarding(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::Onboarding) {
    ui->setupUi(this);

    Q_INIT_RESOURCE(libthedesk_resources);

    this->setWindowFlag(Qt::WindowStaysOnTopHint);
    this->setWindowFlag(Qt::FramelessWindowHint);

    this->setCursor(QCursor(Qt::BlankCursor));

    d = new OnboardingPrivate();
    d->preferredOnboardingOrder = d->settings.delimitedList("Onboarding/order");

    // M3E: the living horizon behind everything, the steps in a rounded card
    d->horizon = new HorizonView(this);
    d->horizon->lower();
    d->horizon->playIntro();
    ui->contentWrapper->installEventFilter(this);
    ui->line->setVisible(false);
    {
        // Pages fill with the card's colour (tStackedWidget wants them filled
        // for its slide); the card clips them to its corners (eventFilter)
        QPalette card = ui->contentWrapper->palette();
        for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) card.setColor(group, QPalette::Window, M3::currentScheme().surfaceContainer);
        ui->contentWrapper->setPalette(card);
    }

    ui->stepperFrame->setFixedSize(SC_DPI_T(QSize(200, 600), QSize));
    // The steps sit in the middle of their column, not stacked at the top
    if (auto* column = qobject_cast<QVBoxLayout*>(ui->stepperFrame->layout())) column->insertStretch(0, 1);
    ui->stackedWidget->setFixedSize(SC_DPI_T(QSize(800, 600), QSize));
    ui->stackedWidget->setCurrentAnimation(tStackedWidget::SlideHorizontal);
    ui->contentWrapper->setFixedHeight(0);
    ui->contentWrapper->setVisible(false);

    OnboardingManager* manager = StateManager::onboardingManager();
    manager->d->onboardingRunning = true;
    connect(manager, &OnboardingManager::nextStep, this, [=] {
        if (ui->stackedWidget->count() == ui->stackedWidget->currentIndex() + 1) {
            completeOnboarding();
        } else {
            ui->stackedWidget->setCurrentIndex(ui->stackedWidget->currentIndex() + 1);
        }
    });
    connect(manager, &OnboardingManager::previousStep, this, [=] {
        ui->stackedWidget->setCurrentIndex(ui->stackedWidget->currentIndex() - 1);
    });

    // Populate the stacked widget with all the onboarding items
    QStringList currentItems;
    for (OnboardingPage* step : qAsConst(manager->d->steps)) {
        OnboardingStepper* stepper = new OnboardingStepper(this);
        stepper->setText(step->displayName());
        stepper->setPageName(step->name());
        connect(ui->stackedWidget, &tStackedWidget::switchingFrame, this, [=](int step) {
            stepper->setCurrentStep(step + 1);
        });

        int index = Common::getInsertionIndex(d->preferredOnboardingOrder, currentItems, step->name());

        // Make sure this pane is actually configured to be in the onboarding
        if (index != -1) {
            // Add this pane at the correct index
            ui->stackedWidget->insertWidget(index, step);
            currentItems.insert(index, step->name());
            d->steps.insert(index, {step->name(), step});

            ui->stepperLayout->insertWidget(index, stepper);
            d->steppers.insert(index, {stepper, step});
        } else {
            // Hide the stepper so it doesn't show up in the background
            stepper->setVisible(false);
        }
    }

    for (int i = 0; i < d->steppers.count(); i++) {
        d->steppers.at(i).first->setStep(i + 1);
        if (i == d->steppers.count() - 1) d->steppers.at(i).first->setIsFinal(true);
    }

    // Pixel-style steps: one hero over the pages that morphs from step to
    // step, a wavy progress line, and each step page restyled to match
    for (const auto& step : std::as_const(d->steps)) {
        if (OnboardingChrome::hasHero(step.first)) OnboardingChrome::stylePage(step.second);
    }
    d->hero = new OnboardingHero(ui->contentWrapper);
    d->progress = new OnboardingProgress(ui->contentWrapper);
    d->progress->setVisible(false);
    ui->stackedWidget->installEventFilter(this);
    connect(ui->stackedWidget, &tStackedWidget::switchingFrame, this, &Onboarding::updateChrome);
    placeChrome();
    updateChrome(ui->stackedWidget->currentIndex());

    d->bar = new OnboardingBar(this);
    d->bar->setParent(this);
    connect(d->bar, &OnboardingBar::muteToggled, this, [=](bool mute) {
        if (d->audioOutput) d->audioOutput->setVolume(mute ? 0 : 100);
    });
    connect(d->bar, &OnboardingBar::closeClicked, this, [=] {
        StateManager::instance()->powerManager()->showPowerOffConfirmation();
    });
    d->bar->setVisible(false);
    d->bar->installEventFilter(this);

    d->contentAnim = new tVariantAnimation(this);
    d->contentAnim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
    d->contentAnim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
    connect(d->contentAnim, &tVariantAnimation::valueChanged, this, [=](QVariant value) {
        ui->contentWrapper->setFixedHeight(value.toInt());
    });

    d->barAnim = new tVariantAnimation(this);
    d->barAnim->setEasingCurve(M3Motion::easing(M3Motion::SpatialDefault));
    d->barAnim->setDuration(M3Motion::duration(M3Motion::SpatialDefault));
    connect(d->barAnim, &tVariantAnimation::valueChanged, this, [=](QVariant value) {
        d->bar->move(0, value.toInt());
    });

    d->fadeAnim = new tVariantAnimation(this);
    d->fadeAnim->setStartValue(0);
    d->fadeAnim->setEndValue(255);
    d->fadeAnim->setEasingCurve(M3Motion::easing(M3Motion::EffectsDefault));
    d->fadeAnim->setDuration(M3Motion::duration(M3Motion::EffectsDefault));
    connect(d->fadeAnim, &tVariantAnimation::valueChanged, this, [=] {
        this->update();
    });

    if (d->settings.value("Onboarding/onboardingAudio").toBool()) {
        auto format = QMediaDevices::defaultAudioOutput().preferredFormat();

        QEventLoop* loop = new QEventLoop();
        for (int i = 0; i < 2; i++) {
            QByteArray* array = (i == 0 ? &d->singleAudioData : &d->loopAudioData);
            QString setting = (i == 0 ? "Onboarding/audio.start" : "Onboarding/audio.loop");
            auto file = QUrl::fromLocalFile(d->settings.value(setting).toString());

            QEventLoopLocker* locker = new QEventLoopLocker(loop);
            QAudioDecoder* audioDecoder = new QAudioDecoder();
            audioDecoder->setSource(file);
            audioDecoder->setAudioFormat(format);
            connect(audioDecoder, &QAudioDecoder::bufferReady, this, [=] {
                QAudioBuffer buf = audioDecoder->read();
                array->append(buf.constData<char>(), buf.byteCount());
            });
            connect(audioDecoder, &QAudioDecoder::finished, this, [=] {
                delete locker;
                audioDecoder->deleteLater();
            });
            connect(audioDecoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this, [=](QAudioDecoder::Error error) {
                delete locker;
                audioDecoder->deleteLater();
            });
            audioDecoder->start();
        }
        loop->exec();
        loop->deleteLater();

        d->audioOutput = new QAudioSink(format);
        d->audioOutput->setBufferSize(176400);
        d->audioOutputDevice = d->audioOutput->start();

        QTimer* audioTimer = new QTimer(d->audioOutput);
        audioTimer->setInterval(100);
        connect(audioTimer, &QTimer::timeout, this, &Onboarding::writeAudio);
        audioTimer->start();

        writeAudio();
    }

    if (d->settings.value("Onboarding/onboardingVideo").toBool()) {
        this->setAttribute(Qt::WA_TranslucentBackground);
    } else {
        this->startOnboarding();
    }

    auto* terminalShortcut = new QShortcut(QKeySequence(Qt::ControlModifier | Qt::AltModifier | Qt::Key_T), this);
    connect(terminalShortcut, &QShortcut::activated, this, [this] {
        this->hide();

        auto env = QProcessEnvironment::systemEnvironment();
        env.remove("LD_LIBRARY_PATH");

        auto* proc = new QProcess(this);
        proc->setProgram("theterminal");
        proc->setArguments({"-e", "/bin/bash"});
        proc->setWorkingDirectory(QDir::rootPath());
        proc->setProcessEnvironment(env);
        proc->start();
        connect(proc, &QProcess::finished, this, [proc, this] {
            proc->deleteLater();
            this->show();
        });
    });
}

Onboarding::~Onboarding() {
    if (d->audioOutput) {
        d->audioOutput->stop();
        d->audioOutput->deleteLater();
    }
    delete d;
    delete ui;
}

void Onboarding::on_stackedWidget_currentChanged(int arg1) {
    if (d->steps.count() > arg1) d->steps.at(arg1).second->pageActivated();
    // The page arrived without its words (preparePage); its title springs in
    // letter by letter, its description rises in; the others wait hidden
    for (int i = 0; i < ui->stackedWidget->count(); i++) {
        if (i != arg1) OnboardingText::preparePage(ui->stackedWidget->widget(i));
    }
    if (!ui->stackedWidget->widget(arg1)->property("m3HoldText").toBool()) OnboardingText::animatePage(ui->stackedWidget->widget(arg1), 0);
}

void Onboarding::on_bar_closeClicked() {
    StateManager::instance()->powerManager()->showPowerOffConfirmation();
}

void Onboarding::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    if (d->textOnShow) {
        // Shown as the loading screen's exit opens onto it: the words come in
        // as it opens (OnboardingHello waits the same)
        d->textOnShow = false;
        if (!ui->stackedWidget->currentWidget()->property("m3HoldText").toBool()) OnboardingText::animatePage(ui->stackedWidget->currentWidget(), 700);
    }
}

void Onboarding::resizeEvent(QResizeEvent* event) {
    if (d->horizon) d->horizon->setGeometry(this->rect());
    // The card draws itself (eventFilter): no frame, no divider
    ui->contentWrapper->setFrameShape(QFrame::NoFrame);
    ui->line->setVisible(false);
    ui->stepperFrame->setVisible(this->geometry().width() >= SC_DPI(1000));
    updateBarVisiblity();
}

void Onboarding::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    QLinearGradient grad;
    grad.setStart(0, 0);
    grad.setFinalStop(0, d->bar->height() * 2);
    grad.setColorAt(0, QColor(0, 0, 0, 127));
    grad.setColorAt(1, QColor(0, 0, 0, 0));

    painter.setPen(Qt::transparent);
    painter.setBrush(grad);
    painter.drawRect(0, 0, this->width(), this->height());

    int opacity = d->fadeAnim->currentValue().toInt();
    if (opacity > 0) {
        QColor col = this->palette().color(QPalette::Window);
        col.setAlpha(opacity);
        painter.setBrush(col);
        painter.drawRect(0, 0, this->width(), this->height());
    }
}

void Onboarding::startOnboarding() {
    if (ui->contentWrapper->isVisible()) return;

    this->setCursor(QCursor(Qt::ArrowCursor));

    d->contentAnim->setStartValue(0);
    d->contentAnim->setEndValue(ui->contentWrapper->sizeHint().height());

    QMetaObject::Connection* c = new QMetaObject::Connection();
    *c = connect(d->contentAnim, &tVariantAnimation::finished, this, [=] {
        disconnect(*c);
        delete c;

        // Animate in the bar
        if (this->geometry().height() >= SC_DPI(600) + 2 * d->bar->height()) {
            QMetaObject::Connection* c = new QMetaObject::Connection();
            *c = connect(d->barAnim, &tVariantAnimation::finished, this, [=] {
                disconnect(*c);
                delete c;

                d->onboardingStarted = true;
                updateBarVisiblity();
            });
            showBar();
        } else {
            d->onboardingStarted = true;
            d->barVisible = true;
            updateBarVisiblity();
        }

        d->bar->setVisible(true);
    });
    d->contentAnim->start();
    ui->contentWrapper->setVisible(true);
    for (int i = 0; i < ui->stackedWidget->count(); i++) OnboardingText::preparePage(ui->stackedWidget->widget(i));
    if (this->isVisible() && !ui->stackedWidget->currentWidget()->property("m3HoldText").toBool()) OnboardingText::animatePage(ui->stackedWidget->currentWidget(), M3Motion::duration(M3Motion::SpatialDefault) / 2);
    else d->textOnShow = true; // see showEvent
}

void Onboarding::writeAudio() {
    if (!d->audioEnabled) return;
    if (!d->audioOutputDevice) return;

    int bytesFree = d->audioOutput->bytesFree();
    if (d->audioPointer < d->singleAudioData.length()) {
        QByteArray chunk = d->singleAudioData.mid(d->audioPointer, bytesFree);
        d->audioPointer += chunk.length();
        bytesFree -= chunk.length();
        d->audioOutputDevice->write(chunk);
    }

    // Prevent ourselves from locking up in this loop
    QDeadlineTimer lockTimer(50);
    while (bytesFree != 0 && d->audioPointer >= d->singleAudioData.length() && !lockTimer.hasExpired()) {
        if (d->audioPointer >= d->singleAudioData.length() + d->loopAudioData.length()) d->audioPointer -= d->loopAudioData.length();
        QByteArray chunk = d->loopAudioData.mid(d->audioPointer - d->singleAudioData.length(), bytesFree);
        d->audioPointer += chunk.length();
        bytesFree -= chunk.length();
        d->audioOutputDevice->write(chunk);
    }

    // Disable the audio
    if (lockTimer.hasExpired()) {
        d->audioEnabled = false;
    }
}

void Onboarding::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);

        for (QPair<OnboardingStepper*, OnboardingPage*> stepper : d->steppers) {
            stepper.first->setText(stepper.second->displayName());
        }
    }
}

void Onboarding::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        if (!ui->contentWrapper->isVisible()) {
            // Skip the intro sequence
            startOnboarding();
            event->accept();
        }
    } else if (event->key() == Qt::Key_Escape) {
        StateManager::instance()->powerManager()->showPowerOffConfirmation();
        event->accept();
    }
}

void Onboarding::placeChrome() {
    if (!d->hero) return;
    const QRect pages = ui->stackedWidget->geometry();
    d->hero->move(pages.left() + OnboardingChrome::margin, pages.top() + OnboardingChrome::margin);
    d->progress->setGeometry(pages.left() + OnboardingChrome::margin, pages.top() + 8, pages.width() - 2 * OnboardingChrome::margin, 12);
    d->hero->raise();
    d->progress->raise();
}

void Onboarding::updateChrome(int index) {
    if (!d->hero || index < 0 || index >= d->steps.count()) return;
    const QString name = d->steps.at(index).first;
    d->hero->showStep(name);
    d->progress->setVisible(OnboardingChrome::hasHero(name));
    d->progress->setProgress(d->steps.count() > 1 ? double(index) / (d->steps.count() - 1) : 1);
    d->hero->raise();
    d->progress->raise();
}

bool Onboarding::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->stackedWidget && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) placeChrome();
    if (watched == ui->contentWrapper && event->type() == QEvent::Resize) {
        QPainterPath card;
        card.addRoundedRect(QRectF(ui->contentWrapper->rect()), 28, 28);
        ui->contentWrapper->setMask(QRegion(card.toFillPolygon().toPolygon()));
    }
    if (watched == ui->contentWrapper && event->type() == QEvent::Paint) {
        // The M3 card the steps sit in
        QPainter painter(ui->contentWrapper);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath card;
        card.addRoundedRect(QRectF(ui->contentWrapper->rect()), 28, 28);
        painter.fillPath(card, M3::currentScheme().surfaceContainer);
        return false;
    }
    if (watched == d->bar) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            updateBarVisiblity();
        }
    }
    return false;
}

void Onboarding::completeOnboarding() {
    d->contentAnim->setStartValue(ui->contentWrapper->sizeHint().height());
    d->contentAnim->setEndValue(0);
    d->contentAnim->start();

    // Animate out the bar
    QMetaObject::Connection* c = new QMetaObject::Connection();
    *c = connect(d->barAnim, &tVariantAnimation::finished, this, [=] {
        disconnect(*c);
        delete c;

        StateManager::onboardingManager()->d->onboardingRunning = false;
        emit onboardingCompleted(true);
        this->accept();
    });
    hideBar();

    // Fade out the music
    if (d->audioOutput) {
        tVariantAnimation* volAnim = new tVariantAnimation(this);
        volAnim->setStartValue(d->audioOutput->volume());
        volAnim->setEndValue(0);
        volAnim->setEasingCurve(M3Motion::easing(M3Motion::EffectsDefault));
        volAnim->setDuration(M3Motion::duration(M3Motion::EffectsDefault));
        connect(volAnim, &tVariantAnimation::valueChanged, this, [=](QVariant value) {
            d->audioOutput->setVolume(value.toInt());
        });
        connect(volAnim, &tVariantAnimation::finished, this, [=] {
            volAnim->deleteLater();
        });
        volAnim->start();
    }

    // Fade out the background
    d->fadeAnim->start();
}

void Onboarding::showBar() {
    if (d->barVisible) return;
    d->barAnim->setStartValue(-d->bar->height());
    d->barAnim->setEndValue(0);
    d->barAnim->start();
    d->barVisible = true;
}

void Onboarding::hideBar() {
    if (!d->barVisible) return;
    d->barAnim->setStartValue(0);
    d->barAnim->setEndValue(-d->bar->height());
    d->barAnim->start();
    d->barVisible = false;
}

void Onboarding::updateBarVisiblity() {
    if (d->onboardingStarted) {
        if (this->geometry().height() < SC_DPI(600) + 2 * d->bar->height()) {
            if (d->bar->underMouse()) {
                d->bar->move(0, d->barAnim->currentValue().toInt());
            } else {
                d->bar->move(0, -d->bar->height() + 1);
            }
            d->bar->setAutoFillBackground(true);
            tScrim::scrimForWidget(this)->setBlurEnabled(true);
        } else {
            d->bar->move(0, d->barAnim->currentValue().toInt());
            d->bar->setAutoFillBackground(false);
            tScrim::scrimForWidget(this)->setBlurEnabled(false);
        }
    } else {
        d->bar->move(0, -d->bar->height());
    }

    d->bar->setFixedWidth(this->width());
    d->bar->setFixedHeight(d->bar->height());
}
