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
#include "statuscenterleftpane.h"
#include "ui_statuscenterleftpane.h"

#include "leftpanedelegate.h"
#include <QAbstractItemView>
#include <QHelpEvent>
#include <QPainter>
#include <QToolButton>
#include <QToolTip>
#include <libcontemporary_global.h>
#include <m3motion.h>
#include <m3scheme.h>
#include <ttitlelabel.h>

namespace {
    // NavigationRail (M3E): 96 wide folded, the list's 300 open
    constexpr int OpenWidth = 300, RailWidth = 96;

    // Folded, a row is just its symbol: its name comes up as a tooltip
    class RailTips : public QObject {
        public:
            using QObject::QObject;
            bool eventFilter(QObject* watched, QEvent* event) override {
                if (event->type() != QEvent::ToolTip) return false;
                auto* viewport = qobject_cast<QWidget*>(watched);
                auto* view = viewport ? qobject_cast<QAbstractItemView*>(viewport->parentWidget()) : nullptr;
                if (!view) return false;
                for (QWidget* w = view; w; w = w->parentWidget()) {
                    if (w->property("m3Rail").isValid()) {
                        if (w->property("m3Rail").toDouble() < 0.5) return false;
                        break;
                    }
                }
                auto* help = static_cast<QHelpEvent*>(event);
                const QModelIndex index = view->indexAt(help->pos());
                if (!index.isValid()) return false;
                QToolTip::showText(help->globalPos(), index.data(Qt::DisplayRole).toString(), viewport);
                return true;
            }
    };
} // namespace

struct StatusCenterLeftPanePrivate {
        bool attached = false;
        bool folded = false;
        M3SpringAnimation* rail;
        QToolButton* railButton;
        RailTips* tips;
};

StatusCenterLeftPane::StatusCenterLeftPane(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::StatusCenterLeftPane) {
    ui->setupUi(this);

    d = new StatusCenterLeftPanePrivate();
    ui->mainList->setIconSize(SC_DPI_T(QSize(32, 32), QSize));
    ui->stackedWidget->setCurrentAnimation(tStackedWidget::SlideHorizontal);
    ui->mainList->setItemDelegate(new LeftPaneDelegate(this));

    // A navigation rail; its menu button opens it out into the list for a while
    d->tips = new RailTips(this);
    d->rail = new M3SpringAnimation(this);
    d->rail->setToken(M3Motion::SpatialDefault);
    d->folded = true;
    d->rail->snapTo(1.0);
    connect(d->rail, &M3SpringAnimation::valueChanged, this, &StatusCenterLeftPane::followRail);
    d->railButton = new QToolButton(this);
    d->railButton->setAutoRaise(true);
    d->railButton->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    d->railButton->setToolTip(tr("Fold the list"));
    connect(d->railButton, &QToolButton::clicked, this, [this] {
        d->folded = !d->folded;
        d->rail->animateTo(d->folded ? 1.0 : 0.0);
    });
    // The button sits in the title's row, so the title reads after it; sub-lists have a back button there instead
    ui->label->setContentsMargins(64, 0, 0, 0);
    connect(ui->stackedWidget, &tStackedWidget::currentChanged, this, [this] {
        setAttached(d->attached);
        // A pane's own list inside another's (Users, Input in System Settings) opens out:
        // a rail has no room for its header and names
        const double rail = d->folded && ui->stackedWidget->currentIndex() < 2 ? 1.0 : 0.0;
        if (d->rail->endValue().toDouble() != rail) d->rail->animateTo(rail);
    });
    followRail();
}

void StatusCenterLeftPane::followRail() {
    const double rail = qBound(0.0, d->rail->currentValue().toDouble(), 1.2);
    this->setProperty("m3Rail", qMin(1.0, rail));
    d->railButton->setToolTip(rail > 0.5 ? tr("Open the list") : tr("Fold the list"));
    setAttached(d->attached);
    // the titles fade as the rail folds; every list redraws its rows
    const M3::Scheme s = M3::currentScheme();
    for (tTitleLabel* title : this->findChildren<tTitleLabel*>()) {
        QPalette palette = title->palette();
        QColor ink = s.onSurface;
        ink.setAlphaF(1 - qBound(0.0, rail * 2, 1.0));
        palette.setColor(QPalette::WindowText, ink);
        title->setPalette(palette);
    }
    for (QAbstractItemView* view : this->findChildren<QAbstractItemView*>()) {
        view->viewport()->installEventFilter(d->tips);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setVerticalScrollBarPolicy(rail > 0.5 ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded); // a rail scrolls without a bar
        view->viewport()->setAutoFillBackground(false); // on the shade's own surface
        view->doItemsLayout(); // rows ask for less width as it folds
        view->viewport()->update();
    }
}

StatusCenterLeftPane::~StatusCenterLeftPane() {
    delete d;
    delete ui;
}

void StatusCenterLeftPane::appendItem(QListWidgetItem* item) {
    ui->mainList->addItem(item);
}

void StatusCenterLeftPane::insertItem(int index, QListWidgetItem* item) {
    ui->mainList->insertItem(index, item);
}

void StatusCenterLeftPane::removeItem(QListWidgetItem* item) {
    ui->mainList->removeItemWidget(item);
}

void StatusCenterLeftPane::setSelected(QListWidgetItem* item) {
    item->setSelected(true);
}

void StatusCenterLeftPane::setAttached(bool attached) {
    d->attached = attached;
    this->layout()->setContentsMargins(0, attached ? 16 : 0, 0, 0);
    // folded, it's a rail (the popover, when detached, stays a list)
    const double rail = attached ? qBound(0.0, d->rail->currentValue().toDouble(), 1.2) : 0.0;
    const int width = qRound(SC_DPI(OpenWidth) + (SC_DPI(RailWidth) - SC_DPI(OpenWidth)) * rail);
    this->setFixedWidth(width);
    d->railButton->setVisible(attached && ui->stackedWidget->currentIndex() == 0);
    d->railButton->setFixedSize(40, 40);
    placeRailButton();
}

void StatusCenterLeftPane::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    placeRailButton();
}

void StatusCenterLeftPane::placeRailButton() {
    // in the title's top row (a flexible title grows below it)
    const int y = ui->stackedWidget->y() + ui->label->y() + 12;
    d->railButton->move(this->layoutDirection() == Qt::RightToLeft ? this->width() - 28 - 40 : 28, y);
    d->railButton->raise();
}

void StatusCenterLeftPane::pushMenu(QWidget* menu) {
    ui->stackedWidget->addWidget(menu);
    ui->stackedWidget->setCurrentWidget(menu);
    followRail(); // its title and list fold too
}

void StatusCenterLeftPane::popMenu() {
    if (ui->stackedWidget->count() == 1) return;

    QMetaObject::Connection* connection = new QMetaObject::Connection();
    *connection = connect(ui->stackedWidget, &tStackedWidget::currentChanged, this, [=] {
        disconnect(*connection);
        delete connection;

        QWidget* lastWidget = ui->stackedWidget->widget(ui->stackedWidget->currentIndex() + 1);
        ui->stackedWidget->removeWidget(lastWidget);
    });
    ui->stackedWidget->setCurrentIndex(ui->stackedWidget->currentIndex() - 1);
}

void StatusCenterLeftPane::popToRoot() {
    ui->stackedWidget->setCurrentIndex(0, false);
    while (ui->stackedWidget->count() > 1) ui->stackedWidget->removeWidget(ui->stackedWidget->widget(ui->stackedWidget->count() - 1));
}

QWidget* StatusCenterLeftPane::peekMenu() {
    return ui->stackedWidget->currentWidget();
}

void StatusCenterLeftPane::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    } else if (event->type() == QEvent::LayoutDirectionChange) {
        this->setAttached(d->attached);
    }
}

void StatusCenterLeftPane::on_mainList_currentRowChanged(int currentRow) {
    emit indexChanged(currentRow);
}

void StatusCenterLeftPane::on_mainList_clicked(const QModelIndex& index) {
    emit enterMenu(index.row());
}
