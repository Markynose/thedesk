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
#include "mprischunk.h"
#include "ui_mprischunk.h"

#include <Applications/application.h>
#include <QActionGroup>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <m3scheme.h>
#include <barmanager.h>
#include <libcontemporary_global.h>
#include <mpris/mprisengine.h>
#include <mpris/mprisplayer.h>
#include <statemanager.h>

struct MprisChunkPrivate {
        MprisPlayerPtr currentPlayer;
        QString currentPlayerService;
        ApplicationPointer currentPlayerApplication;

        QMenu* playersMenu;
        QActionGroup* playersGroup;
        QMap<QString, QAction*> playerActions;

        BarManager::BarLockPtr barLocker;
        double open = 0; // the bar: 0 closed, 1 open
        QColor pill;
};

namespace {
    constexpr int Closed = 26, Open = 30; // the pill's height, as the battery chip's
}

MprisChunk::MprisChunk() :
    Chunk(),
    ui(new Ui::MprisChunk) {
    ui->setupUi(this);

    d = new MprisChunkPrivate();
    d->playersGroup = new QActionGroup(this);

    d->playersMenu = new QMenu(this);
    d->playersMenu->addSection(tr("Media Players"));
    connect(d->playersMenu, &QMenu::aboutToShow, this, [=] {
        d->barLocker = StateManager::barManager()->acquireLock();
    });
    connect(d->playersMenu, &QMenu::aboutToHide, this, [=] {
        d->barLocker->unlock();
    });
    ui->playersButton->setMenu(d->playersMenu);
    ui->playersButton->setVisible(MprisEngine::players().count() > 1);

    connect(MprisEngine::instance(), &MprisEngine::newPlayer, this, [=](QString service, MprisPlayerPtr player) {
        Q_UNUSED(player)
        setupPlayer(service);

        if (d->currentPlayer == nullptr) setCurrentPlayer(service);
    });
    connect(MprisEngine::instance(), &MprisEngine::playerGone, this, [=](QString service) {
        QAction* action = d->playerActions.take(service);
        d->playersMenu->removeAction(action);
        action->deleteLater();
        ui->playersButton->setVisible(MprisEngine::players().count() > 1);

        if (d->currentPlayerService == service) {
            // Find another player
            if (!MprisEngine::players().isEmpty()) {
                setCurrentPlayer(MprisEngine::players().first()->service());
            } else {
                setCurrentPlayer("");
            }
        }
    });

    if (!MprisEngine::players().isEmpty()) {
        for (MprisPlayerPtr player : MprisEngine::players()) {
            setupPlayer(player->service());
        }
        setCurrentPlayer(MprisEngine::players().first()->service());
    } else {
        setCurrentPlayer("");
    }

    connect(StateManager::barManager(), &BarManager::barHeightTransitioning, this, [=](qreal percentage) {
        d->open = percentage;
        this->update();
        // The controls grow in as the bar opens (a fixed QWIDGETSIZE_MAX would ask for more room than there is,
        // and they'd be laid over the title)
        ui->buttonWidget->setFixedWidth(qRound(ui->buttonWidget->sizeHint().width() * percentage));
        ui->playersButton->setFixedWidth(qRound(28 * percentage));
        ui->stateIcon->setFixedWidth(qRound(ui->stateIcon->sizeHint().width() * (1 - percentage)));
    });

    ui->stateIcon->setFixedWidth(0);
    ui->stateIcon->setPixmap(QIcon::fromTheme("media-playback-start").pixmap(SC_DPI_T(QSize(16, 16), QSize)));

    // M3E: what's playing and its controls in one TertiaryContainer pill; the controls a connected
    // button group inset in it, play/pause the emphasized toggle (round paused, square playing)
    this->layout()->setContentsMargins(10, 0, 3, 0);
    ui->buttonWidget->layout()->setContentsMargins(4, 0, 0, 0);
    ui->playerControlsWidget->setProperty("m3ButtonGroup", QStringLiteral("connected"));
    ui->playerControlsLayout->setSpacing(2);
    for (QToolButton* button : {ui->backButton, ui->playPauseButton, ui->forwardButton, ui->playersButton}) {
        button->setProperty("m3ButtonSize", QStringLiteral("xs"));
        button->setProperty("m3IconButtonStyle", QStringLiteral("tonal"));
        button->setProperty("m3IconButtonWidth", QStringLiteral("narrow"));
        button->setFixedSize(28, 24);
        button->setIconSize(QSize(16, 16));
    }
    ui->playersButton->setProperty("m3IconButtonStyle", QStringLiteral("standard"));
    ui->playPauseButton->setProperty("m3IconButtonStyle", QStringLiteral("filled"));
    ui->playPauseButton->setProperty("m3IconButtonWidth", QStringLiteral("wide"));
    ui->playPauseButton->setProperty("m3ToggleShapeOnly", true); // stays the emphasized one, paused or playing
    ui->playPauseButton->setCheckable(true);
    ui->playPauseButton->setFixedSize(40, 24);
    applyColours();

    // Ensure that the player controls are always LTR
    //    if (this->layoutDirection() == Qt::RightToLeft) {
    ui->playerControlsWidget->setLayoutDirection(Qt::LeftToRight);
    //        ui->playerControlsLayout->setDirection(QBoxLayout::RightToLeft);
    //    }
}

MprisChunk::~MprisChunk() {
    if (StateManager::barManager()->isChunkRegistered(this)) StateManager::barManager()->removeChunk(this);
    delete d;
    delete ui;
}

void MprisChunk::setupPlayer(QString service) {
    MprisPlayerPtr player = MprisEngine::instance()->playerForInterface(service);
    QAction* action = new QAction();
    action->setText(player->identity());
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, [=] {
        setCurrentPlayer(service);
    });
    d->playerActions.insert(service, action);
    d->playersGroup->addAction(action);
    d->playersMenu->addAction(action);
    ui->playersButton->setVisible(MprisEngine::players().count() > 1);
}

void MprisChunk::setCurrentPlayer(QString player) {
    d->currentPlayer->disconnect(this);
    d->currentPlayerService = player;
    if (player == "") {
        d->currentPlayer = nullptr;
        d->currentPlayerApplication = nullptr;
        if (StateManager::barManager()->isChunkRegistered(this)) StateManager::barManager()->removeChunk(this);
    } else {
        d->currentPlayer = MprisEngine::playerForInterface(player);

        connect(d->currentPlayer.data(), &MprisPlayerInterface::desktopEntryChanged, this, &MprisChunk::updateApplication);
        connect(d->currentPlayer.data(), &MprisPlayerInterface::metadataChanged, this, &MprisChunk::updateMetadata);
        connect(d->currentPlayer.data(), &MprisPlayerInterface::playbackStatusChanged, this, &MprisChunk::updateState);

        updateApplication();
        updateMetadata();
        updateState();

        if (!StateManager::barManager()->isChunkRegistered(this)) StateManager::barManager()->addChunk(this);
        d->playerActions.value(player)->setChecked(true);
    }
}

void MprisChunk::updateMetadata() {
    MetadataMap meta = d->currentPlayer->metadata();
    QStringList parts;

    if (meta.contains("xesam:title")) parts.append(meta.value("xesam:title").toString());
    if (meta.contains("xesam:artist")) parts.append(QLocale().createSeparatedList(meta.value("xesam:artist").toStringList()));

    if (parts.isEmpty()) {
        const QString app = d->currentPlayerApplication ? d->currentPlayerApplication->getProperty("Name").toString() : QString();
        parts.append(app.isEmpty() ? d->currentPlayer->identity() : app); // no desktop entry: its own name
    }
    // Long titles elide, so the controls always have their room in the pill
    const QString text = parts.join(" · ");
    ui->metadataLabel->setText(ui->metadataLabel->fontMetrics().elidedText(text, Qt::ElideRight, 280));
    ui->metadataLabel->setToolTip(text);
}

void MprisChunk::updateApplication() {
    if (d->currentPlayer) {
        d->currentPlayerApplication = ApplicationPointer(new Application(d->currentPlayer->desktopEntry()));
    } else {
        d->currentPlayerApplication = nullptr;
    }
}

void MprisChunk::updateState() {
    switch (d->currentPlayer->playbackStatus()) {
        case MprisPlayerInterface::Playing:
            ui->playPauseButton->setChecked(true);
            ui->stateIcon->setPixmap(QIcon::fromTheme("media-playback-start").pixmap(SC_DPI_T(QSize(16, 16), QSize)));
            ui->playPauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
            break;
        case MprisPlayerInterface::Paused:
            ui->playPauseButton->setChecked(false);
            ui->stateIcon->setPixmap(QIcon::fromTheme("media-playback-pause").pixmap(SC_DPI_T(QSize(16, 16), QSize)));
            ui->playPauseButton->setIcon(QIcon::fromTheme("media-playback-start"));
            break;
        case MprisPlayerInterface::Stopped:
            ui->playPauseButton->setChecked(false);
            ui->stateIcon->setPixmap(QIcon::fromTheme("media-playback-stop").pixmap(SC_DPI_T(QSize(16, 16), QSize)));
            ui->playPauseButton->setIcon(QIcon::fromTheme("media-playback-start"));
            break;
    }
}

QString MprisChunk::name() {
    return "mpris";
}

int MprisChunk::expandedHeight() {
    return Open + 8;
}

int MprisChunk::statusBarHeight() {
    return Closed;
}

void MprisChunk::on_playPauseButton_clicked() {
    if (d->currentPlayer) d->currentPlayer->playPause();
}

void MprisChunk::on_backButton_clicked() {
    if (d->currentPlayer) d->currentPlayer->previous();
}

void MprisChunk::on_forwardButton_clicked() {
    if (d->currentPlayer) d->currentPlayer->next();
}

void MprisChunk::applyColours() {
    const M3::Scheme s = M3::currentScheme();
    d->pill = s.tertiaryContainer;
    QPalette pal = ui->metadataLabel->palette();
    pal.setColor(QPalette::WindowText, s.onTertiaryContainer);
    ui->metadataLabel->setPalette(pal);
}

void MprisChunk::changeEvent(QEvent* event) {
    Chunk::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) applyColours(); // the horizon moved the scheme on
}

void MprisChunk::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const double height = Closed + (Open - Closed) * d->open;
    const QRectF pill(0, (this->height() - height) / 2, this->width(), height);
    QPainterPath shape;
    shape.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
    painter.fillPath(shape, d->pill);
}
