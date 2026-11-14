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
#include "statuscenter.h"
#include "ui_statuscenter.h"

#include <statemanager.h>
#include <statuscentermanager.h>
#include <statuscenterpane.h>
#include <QListWidgetItem>
#include <QPointer>
#include <tpopover.h>
#include <QShortcut>
#include <tsettings.h>
#include "statuscenterleftpane.h"
#include "common/common.h"
#include "systemsettings/systemsettings.h"

#include <quickswitch.h>
#include "statuscenterquickswitch.h"

#include <QBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <TimeDate/desktoptimedate.h>
#include <UPower/desktopupower.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <powermanager.h>

namespace {
    double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }

    QColor mix(const QColor& a, const QColor& b, double t) {
        t = qBound(0.0, t, 1.0);
        return QColor::fromRgbF(lerp(a.redF(), b.redF(), t), lerp(a.greenF(), b.greenF(), t), lerp(a.blueF(), b.blueF(), t), lerp(a.alphaF(), b.alphaF(), t));
    }

    QPainterPath rounded(const QRectF& rect, double radius) {
        QPainterPath path;
        radius = qMin(radius, qMin(rect.width(), rect.height()) / 2);
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    /**
     * A quick switch's detail as Android's dialogs do it (the Internet
     * dialog): it grows out of the tile over a scrim (a container
     * transform), holds the plugin's widget under a symbol and headline, with
     * "See all" (the switch's pane) and Done.
     */
    class DetailDialog : public QWidget {
        public:
            explicit DetailDialog(QWidget* parent) :
                QWidget(parent) {
                this->hide();
                card = new QWidget(this);
                auto* layout = new QVBoxLayout(card);
                layout->setContentsMargins(24, 24, 24, 20);
                layout->setSpacing(12);
                symbol = new QLabel(card);
                symbol->setAlignment(Qt::AlignCenter);
                headline = new QLabel(card);
                headline->setAlignment(Qt::AlignCenter);
                QFont font = headline->font();
                font.setPixelSize(24); // HeadlineSmall
                headline->setFont(font);
                host = new QVBoxLayout();
                host->setContentsMargins(0, 0, 0, 0);
                seeAll = new QPushButton(StatusCenter::tr("See all"), card);
                seeAll->setProperty("m3ButtonStyle", QStringLiteral("text"));
                done = new QPushButton(StatusCenter::tr("Done"), card);
                done->setProperty("m3ButtonStyle", QStringLiteral("filled"));
                auto* buttons = new QHBoxLayout();
                buttons->addWidget(seeAll);
                buttons->addStretch();
                buttons->addWidget(done);
                layout->addWidget(symbol);
                layout->addWidget(headline);
                layout->addLayout(host, 1);
                layout->addLayout(buttons);
                QObject::connect(done, &QPushButton::clicked, this, [this] {
                    close();
                });
                QObject::connect(seeAll, &QPushButton::clicked, this, [this] {
                    const QString pane = tile ? tile->quickSwitch()->detailPane() : QString();
                    close();
                    if (!pane.isEmpty()) StateManager::statusCenterManager()->setPane(pane);
                });

                // The contents fade in once the box has some room
                veil = new QWidget(card);
                veil->setAttribute(Qt::WA_TransparentForMouseEvents);
                veil->installEventFilter(this);

                grow = new M3SpringAnimation(this);
                grow->setToken(M3Motion::SpatialDefault);
                grow->snapTo(0.0);
                fade = new M3SpringAnimation(this);
                fade->setToken(M3Motion::EffectsDefault);
                fade->snapTo(0.0);
                for (auto* spring : {grow, fade}) QObject::connect(spring, &M3SpringAnimation::valueChanged, this, &DetailDialog::apply);
                QObject::connect(fade, &M3SpringAnimation::finished, this, [this] {
                    if (fade->endValue().toDouble() < 0.5) release();
                });
            }

            bool isOpen() const {
                return this->isVisible() && fade->endValue().toDouble() > 0.5;
            }

            void open(StatusCenterQuickSwitch* tile) {
                if (this->isVisible()) release();
                this->tile = tile;
                detail = tile->quickSwitch()->detail();
                detailDone = QObject::connect(tile->quickSwitch(), &QuickSwitch::detailDone, this, [this] {
                    close();
                });

                const M3::Scheme s = M3::currentScheme();
                QPalette palette = card->palette();
                for (auto role : {QPalette::Window, QPalette::Base}) palette.setColor(role, s.surfaceContainerHigh);
                card->setPalette(palette);
                const QIcon icon = detail->windowIcon();
                symbol->setVisible(!icon.isNull());
                if (!icon.isNull()) {
                    QPixmap pixmap = icon.pixmap(QSize(24, 24), this->devicePixelRatioF());
                    QPainter tint(&pixmap);
                    tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    tint.fillRect(pixmap.rect(), s.secondary);
                    tint.end();
                    symbol->setPixmap(pixmap);
                }
                headline->setText(detail->windowTitle().isEmpty() ? tile->quickSwitch()->title() : detail->windowTitle());
                seeAll->setVisible(!tile->quickSwitch()->detailPane().isEmpty());
                host->addWidget(detail);
                detail->show();

                this->setGeometry(this->parentWidget()->rect());
                const QSize hint = card->sizeHint();
                const int width = qBound(460, hint.width(), 560);
                const int height = qBound(240, card->heightForWidth(width) > 0 ? card->heightForWidth(width) : hint.height(), this->height() - 96);
                to = QRect(0, 0, width, height);
                to.moveCenter(this->rect().center());
                card->setGeometry(to);
                this->raise();
                this->show();
                grow->animateTo(1.0);
                fade->animateTo(1.0);
                done->setFocus();
            }

            void close() {
                if (!this->isVisible()) return;
                grow->animateTo(0.0);
                fade->animateTo(0.0);
            }

        protected:
            void paintEvent(QPaintEvent*) override {
                const M3::Scheme s = M3::currentScheme();
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                QColor scrim = s.scrim;
                scrim.setAlphaF(float(0.4 * qBound(0.0, fade->currentValue().toDouble(), 1.0)));
                painter.fillPath(rounded(this->rect(), 28), scrim);
                // the tile's colour becomes the dialog's surface
                const QColor from = tile && tile->quickSwitch()->isChecked() ? s.primary : s.surfaceContainerHighest;
                painter.fillPath(box(), mix(from, s.surfaceContainerHigh, fade->currentValue().toDouble()));
            }

            void mousePressEvent(QMouseEvent* event) override {
                if (!box().contains(event->position())) close();
            }

            void resizeEvent(QResizeEvent*) override {
                if (!this->isVisible()) return;
                to.moveCenter(this->rect().center());
                card->setGeometry(to);
                apply();
            }

            bool eventFilter(QObject* watched, QEvent* event) override {
                if (watched == veil && event->type() == QEvent::Paint) {
                    QColor colour = M3::currentScheme().surfaceContainerHigh;
                    colour.setAlphaF(float(1 - qBound(0.0, fade->currentValue().toDouble() * 1.6 - 0.6, 1.0)));
                    QPainter(veil).fillRect(veil->rect(), colour);
                    return true;
                }
                return false;
            }

        private:
            QPainterPath box() const {
                const double k = grow->currentValue().toDouble();
                const QRectF from = tile ? QRectF(tile->mapTo(this->parentWidget(), QPoint(0, 0)), tile->size()) : QRectF(to);
                const QRectF r(QPointF(lerp(from.left(), to.left(), k), lerp(from.top(), to.top(), k)), QPointF(lerp(from.right(), to.right(), k), lerp(from.bottom(), to.bottom(), k)));
                return rounded(r, lerp(20, 28, qBound(0.0, k, 1.0)));
            }

            void apply() {
                // The real widgets show only inside the growing box
                card->setMask(QRegion(box().translated(-to.topLeft()).toFillPolygon().toPolygon()));
                veil->setGeometry(card->rect());
                veil->setVisible(fade->currentValue().toDouble() < 0.99);
                veil->raise();
                veil->update();
                this->update();
            }

            void release() {
                this->hide();
                QObject::disconnect(detailDone);
                if (detail) {
                    host->removeWidget(detail);
                    detail->hide();
                    detail->setParent(nullptr); // the plugin owns it
                }
                if (tile && this->parentWidget()->isVisible()) tile->setFocus();
                detail.clear();
            }

            QWidget* card;
            QWidget* veil;
            QLabel* symbol;
            QLabel* headline;
            QVBoxLayout* host;
            QPushButton* seeAll;
            QPushButton* done;
            M3SpringAnimation* grow;
            M3SpringAnimation* fade;
            QRect to;
            QPointer<StatusCenterQuickSwitch> tile;
            QPointer<QWidget> detail;
            QMetaObject::Connection detailDone;
    };
} // namespace

struct StatusCenterPrivate {
    StatusCenterLeftPane* leftPane;
    bool leftPaneAttached = false;

    QList<QPair<QString, StatusCenterPane*>> loadedPanes;
    QMap<StatusCenterPane*, QListWidgetItem*> paneItems;


    QList<QPair<QuickSwitch*, StatusCenterQuickSwitch*>> switchItems;

    QStringList preferredPaneOrder;
    QStringList preferredSwitchOrder;

    DetailDialog* detail;
    DesktopUPower* upower;

    tSettings settings;
};

StatusCenter::StatusCenter(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::StatusCenter) {
    ui->setupUi(this);

    d = new StatusCenterPrivate();

    ui->stackedWidget->setCurrentAnimation(tStackedWidget::Lift);
    d->detail = new DetailDialog(this);

    // The header: Android's big clock and the date; battery, settings, power and close
    QFont clockFont = ui->clockLabel->font();
    clockFont.setPixelSize(44);
    ui->clockLabel->setFont(clockFont);
    QFont dateFont = ui->dateLabel->font();
    dateFont.setPixelSize(16);
    ui->dateLabel->setFont(dateFont);
    QPalette datePalette = ui->dateLabel->palette();
    datePalette.setColor(QPalette::WindowText, M3::currentScheme().onSurfaceVariant);
    ui->dateLabel->setPalette(datePalette);
    DesktopTimeDate::makeTimeLabel(ui->clockLabel, DesktopTimeDate::Time);
    DesktopTimeDate::makeTimeLabel(ui->dateLabel, DesktopTimeDate::StandardDate);
    for (QToolButton* button : {ui->settingsButton, ui->powerButton, ui->closeButton}) button->setProperty("m3IconButtonStyle", QStringLiteral("tonal"));
    ui->batteryButton->setProperty("m3ButtonStyle", QStringLiteral("tonal"));
    ui->batteryButton->setProperty("m3ButtonSize", QStringLiteral("xs"));
    d->upower = new DesktopUPower(this);
    auto updateBattery = [this] {
        // just the percentage, as the closed bar shows it
        const QStringList parts = d->upower->overallStateDescription().split(QStringLiteral(" · "));
        const auto percent = std::find_if(parts.cbegin(), parts.cend(), [](const QString& part) {
            return part.endsWith(QLatin1Char('%'));
        });
        ui->batteryButton->setVisible(d->upower->shouldShowOverallState() && percent != parts.cend());
        ui->batteryButton->setText(percent == parts.cend() ? QString() : *percent);
        ui->batteryButton->setIcon(d->upower->overallStateIcon());
    };
    connect(d->upower, &DesktopUPower::overallStateChanged, this, updateBattery);
    updateBattery();
    connect(ui->batteryButton, &QPushButton::clicked, this, [] {
        StateManager::statusCenterManager()->setPane(QStringLiteral("PowerSettings"));
    });
    connect(ui->settingsButton, &QToolButton::clicked, this, [this] {
        selectPane(QStringLiteral("SystemSettings"));
    });
    connect(ui->powerButton, &QToolButton::clicked, this, [] {
        StateManager::statusCenterManager()->hide();
        StateManager::powerManager()->showPowerOffConfirmation();
    });

    d->preferredPaneOrder = d->settings.delimitedList("StatusCenter/panesOrder");
    d->preferredSwitchOrder = d->settings.delimitedList("StatusCenter/quickSwitchOrder");

    d->preferredPaneOrder.removeAll("SystemSettings");
    d->preferredPaneOrder.append("SystemSettings");

    d->leftPane = new StatusCenterLeftPane(this);

    if (this->width() <= SC_DPI(1024)) {
        d->leftPaneAttached = true;
        detachLeftPane();
    } else {
        d->leftPaneAttached = false;
        attachLeftPane();
    }

    connect(StateManager::statusCenterManager(), &StatusCenterManager::paneAdded, this, [ = ](StatusCenterPane * pane, StatusCenterManager::PaneType type) {
        if (type == StatusCenterManager::Informational) {
            this->addPane(pane);
        }
    });
    connect(StateManager::statusCenterManager(), &StatusCenterManager::paneRemoved, this, &StatusCenter::removePane);
    connect(StateManager::statusCenterManager(), &StatusCenterManager::showHamburgerMenu, this, &StatusCenter::showHamburgerMenu);
    connect(StateManager::statusCenterManager(), &StatusCenterManager::rootMenu, d->leftPane, &StatusCenterLeftPane::popMenu);
    connect(StateManager::statusCenterManager(), &StatusCenterManager::switchAdded, this, &StatusCenter::addSwitch);
    connect(StateManager::statusCenterManager(), &StatusCenterManager::switchRemoved, this, &StatusCenter::removeSwitch);

    for (StatusCenterPane* pane : StateManager::statusCenterManager()->panes()) {
        if (StateManager::statusCenterManager()->paneType(pane) == StatusCenterManager::Informational) this->addPane(pane);
    }
    for (QuickSwitch* sw : StateManager::statusCenterManager()->switches()) {
        this->addSwitch(sw);
    }

    connect(d->leftPane, &StatusCenterLeftPane::indexChanged, this, QOverload<int>::of(&StatusCenter::selectPane));
    connect(d->leftPane, &StatusCenterLeftPane::enterMenu, this, &StatusCenter::enterMenu);

    connect(StateManager::statusCenterManager(), QOverload<QString>::of(&StatusCenterManager::paneRequest), this, [ = ](QString paneName) {
        for (StatusCenterPane* pane : StateManager::statusCenterManager()->panes()) {
            if (pane->name() == paneName) {
                if (StateManager::statusCenterManager()->paneType(pane) == StatusCenterManager::Informational) {
                    selectPane(pane);
                } else {
                    selectPane("SystemSettings");
                }
            }
        }
    });
    connect(StateManager::statusCenterManager(), QOverload<StatusCenterPane*>::of(&StatusCenterManager::paneRequest), this, [ = ](StatusCenterPane * pane) {
        if (StateManager::statusCenterManager()->paneType(pane) == StatusCenterManager::Informational) {
            selectPane(pane);
        } else {
            selectPane("SystemSettings");
        }
    });

    //Add internal items to the Status Center
    StateManager::instance()->statusCenterManager()->addPane(new SystemSettings(d->leftPane), StatusCenterManager::Informational);

    // Esc closes a detail first, then the Status Center
    QShortcut* escapeShortcut = new QShortcut(Qt::Key_Escape, this);
    connect(escapeShortcut, &QShortcut::activated, this, [this] {
        if (d->detail->isOpen()) d->detail->close();
        else ui->closeButton->click();
    });
}

StatusCenter::~StatusCenter() {
    delete d;
    delete ui;
}

void StatusCenter::on_closeButton_clicked() {
    StateManager::instance()->statusCenterManager()->hide();
}

void StatusCenter::playEntrance() {
    // the tiles drop in one after another as the shade opens
    for (int i = 0; i < d->switchItems.count(); i++) d->switchItems.at(i).second->playEntrance(120 + i * 40);
}

void StatusCenter::closeDetail() {
    d->detail->close();
}

void StatusCenter::resizeEvent(QResizeEvent* event) {
    d->detail->setGeometry(this->rect());
    if (this->width() <= SC_DPI(1024)) {
        detachLeftPane();
    } else {
        attachLeftPane();
    }
}

void StatusCenter::attachLeftPane() {
    if (d->leftPaneAttached) return;

    ui->leftPaneContainer->addWidget(d->leftPane);
    d->leftPane->setAttached(true);
    StateManager::statusCenterManager()->setIsHamburgerMenuRequired(false);
    d->leftPaneAttached = true;
}

void StatusCenter::detachLeftPane() {
    if (!d->leftPaneAttached) return;

    ui->leftPaneContainer->removeWidget(d->leftPane);
    d->leftPane->move(-d->leftPane->width(), 0);
    d->leftPane->setAttached(false);
    StateManager::statusCenterManager()->setIsHamburgerMenuRequired(true);
    d->leftPaneAttached = false;
}

void StatusCenter::showHamburgerMenu() {
    if (!tPopover::popoverForWidget(ui->mainWidget)) {
        tPopover* popover = new tPopover(d->leftPane);
        popover->setPopoverWidth(d->leftPane->width());
        popover->setPopoverSide(tPopover::Leading);
        connect(StateManager::statusCenterManager(), &StatusCenterManager::hideHamburgerMenu, popover, &tPopover::dismiss);
        connect(popover, &tPopover::dismissed, popover, &tPopover::deleteLater);
        popover->show(ui->mainWidget);
    }
}

void StatusCenter::selectPane(StatusCenterPane* pane) {
    ui->stackedWidget->setCurrentWidget(pane);
    // Another pane's list may still be open (System Settings at one of its
    // pages, say, when the network icon opens Network): go back to the main
    // list first, so the side list and the page shown always agree
    if (d->leftPane->peekMenu() != pane->leftPane()) {
        d->leftPane->popToRoot();
        if (pane->leftPane()) d->leftPane->pushMenu(pane->leftPane());
    }
    d->paneItems.value(pane)->setSelected(true);
}

void StatusCenter::selectPane(QString pane) {
    for (QPair<QString, StatusCenterPane*> loadedPane : d->loadedPanes) {
        if (loadedPane.first == pane) selectPane(loadedPane.second);
    }
}

void StatusCenter::selectPane(int index) {
    selectPane(d->loadedPanes.at(index).second);
}

void StatusCenter::enterMenu(int index) {
    StatusCenterPane* pane = d->loadedPanes.at(index).second;
    if (pane->leftPane()) {
        if (d->leftPane->peekMenu() != pane->leftPane()) d->leftPane->pushMenu(pane->leftPane());
    } else {
        StateManager::statusCenterManager()->hideStatusCenterHamburgerMenu();
    }
}

void StatusCenter::addPane(StatusCenterPane* pane) {
    QListWidgetItem* item = new QListWidgetItem();
    item->setText(pane->displayName());
    item->setIcon(pane->icon());

    connect(pane, &StatusCenterPane::displayNameChanged, this, [ = ] {
        if (d->paneItems.contains(pane)) {
            d->paneItems.value(pane)->setText(pane->displayName());
        }
    });
    connect(pane, &StatusCenterPane::iconChanged, this, [ = ] {
        if (d->paneItems.contains(pane)) {
            d->paneItems.value(pane)->setIcon(pane->icon());
        }
    });
    if (pane->leftPane()) {
        item->setData(Qt::UserRole, true);
    }
    d->paneItems.insert(pane, item);

    QStringList currentItems;
    for (QPair<QString, StatusCenterPane*> item : d->loadedPanes) {
        currentItems.append(item.first);
    }
    int index = Common::getInsertionIndex(d->preferredPaneOrder, currentItems, pane->name());
    if (index == -1) {
        //Add it at the end
        ui->stackedWidget->addWidget(pane);
        d->leftPane->appendItem(item);
        d->loadedPanes.append({pane->name(), pane});
    } else {
        //Add this chunk at the correct index
        ui->stackedWidget->insertWidget(index, pane);
        d->leftPane->insertItem(index, item);
        d->loadedPanes.insert(index, {pane->name(), pane});
    }
}

void StatusCenter::removePane(StatusCenterPane* pane) {
    for (int i = 0; i < d->loadedPanes.count(); i++) {
        if (d->loadedPanes.at(i).second == pane) {
            QListWidgetItem* item = d->paneItems.take(pane);
            d->leftPane->removeItem(item);
            delete item;

            ui->stackedWidget->removeWidget(pane);
            d->loadedPanes.removeAt(i);
            return;
        }
    }
}

void StatusCenter::addSwitch(QuickSwitch* sw) {
    QStringList currentItems;
    for (QPair<QuickSwitch*, StatusCenterQuickSwitch*> item : d->switchItems) {
        currentItems.append(item.first->name());
    }
    StatusCenterQuickSwitch* item = new StatusCenterQuickSwitch(sw);
    connect(item, &StatusCenterQuickSwitch::detailRequested, this, [this, item] {
        QuickSwitch* sw = item->quickSwitch();
        if (sw->detail()) d->detail->open(item);
        else StateManager::statusCenterManager()->setPane(sw->detailPane());
    });
    int index = Common::getInsertionIndex(d->preferredSwitchOrder, currentItems, sw->name());
    if (index == -1) {
        //Add it at the end
        ui->quickSwitchLayout->addWidget(item);
        d->switchItems.append({sw, item});
    } else {
        //Add this switch at the correct index
        ui->quickSwitchLayout->insertWidget(index, item);
        d->switchItems.insert(index, {sw, item});
    }

}

void StatusCenter::removeSwitch(QuickSwitch* sw) {
    for (int i = 0; i < d->switchItems.count(); i++) {
        if (d->switchItems.at(i).first == sw) {
            ui->quickSwitchLayout->removeWidget(d->switchItems.at(i).second);
            d->switchItems.at(i).second->deleteLater();
            d->switchItems.removeAt(i);
            return;
        }
    }
}
