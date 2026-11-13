/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2022 Victor Tran
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
#include "tsettingswindow.h"
#include "ui_tsettingswindow.h"

#include "tsettingspane.h"
#include "tsettingswindowdelegate.h"
#include "tsubtitlelabel.h"
#include <QScroller>
#include <QStandardItemModel>

struct tSettingsWindowPrivate {
        QStandardItemModel* leftModel;

        QList<QWidget*> rightSide;
        bool addedFirst = false;

        typedef std::tuple<int, QString, QString> StaticSection;
        typedef std::tuple<int, QString, std::function<tSettingsPane*()>> StaticPane;

        static QList<StaticSection> staticSections;
        static QList<StaticPane> staticPanes;
};

QList<tSettingsWindowPrivate::StaticSection> tSettingsWindowPrivate::staticSections = QList<tSettingsWindowPrivate::StaticSection>();
QList<tSettingsWindowPrivate::StaticPane> tSettingsWindowPrivate::staticPanes = QList<tSettingsWindowPrivate::StaticPane>();

tSettingsWindow::tSettingsWindow(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::tSettingsWindow) {
    ui->setupUi(this);

    ui->titleLabel->setBackButtonShown(true);
    this->setWindowTitle(tr("Settings"));

    d = new tSettingsWindowPrivate();

    d->leftModel = new QStandardItemModel();

    ui->contentsView->setModel(d->leftModel);
    ui->contentsView->setItemDelegate(new tSettingsWindowDelegate(this));
    ui->contentsView->setFixedWidth(300);
}

tSettingsWindow::~tSettingsWindow() {
    delete d;
    delete ui;
}

void tSettingsWindow::appendSection(QString text) {
    auto item = new QStandardItem(text);
    d->leftModel->appendRow(item);
}

void tSettingsWindow::appendPane(tSettingsPane* pane) {
    if (d->addedFirst) {
        auto line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFixedHeight(1);
        ui->paneLayout->addWidget(line);
    }

    auto item = new QStandardItem(pane->paneName());
    d->leftModel->appendRow(item);

    auto titleContainer = new QWidget(this);
    titleContainer->setFixedWidth(600);

    auto titleLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    titleLayout->setContentsMargins(9, 9, 9, 0);
    titleLayout->setAlignment(Qt::AlignLeft);

    auto* title = new tSubtitleLabel(this);
    title->setText(pane->paneName());
    titleLayout->addWidget(title);
    titleContainer->setLayout(titleLayout);
    ui->paneLayout->addWidget(titleContainer);

    pane->setFixedWidth(600);
    ui->paneLayout->addWidget(pane);

    item->setData(QVariant::fromValue(titleContainer));

    d->rightSide.append(titleContainer);
    d->rightSide.append(pane);
    d->addedFirst = true;
}

void tSettingsWindow::appendSeparator() {
    this->appendSection("");
}

void tSettingsWindow::addStaticSection(int priority, QString name, QString text) {
    for (auto section : tSettingsWindowPrivate::staticSections) {
        if (std::get<1>(section) == name) return;
    }

    tSettingsWindowPrivate::staticSections.append({priority, name, text});
}

void tSettingsWindow::addStaticPane(int priority, QString sectionName, std::function<tSettingsPane*()> paneGenerator) {
    tSettingsWindowPrivate::staticPanes.append({priority, sectionName, paneGenerator});
}

void tSettingsWindow::openStaticSettingsWindow(QWidget* parent) {
    tSettingsWindow settingsWindow(parent);

    auto sections = tSettingsWindowPrivate::staticSections;
    std::stable_sort(sections.begin(), sections.end(), [](const tSettingsWindowPrivate::StaticSection& first, const tSettingsWindowPrivate::StaticSection& second) {
        return std::get<0>(second) - std::get<0>(first) > 0;
    });
    for (const auto& [sectionPriority, sectionName, sectionText] : sections) {
        settingsWindow.appendSection(sectionText);

        QList<tSettingsWindowPrivate::StaticPane> panes;
        for (auto pane : tSettingsWindowPrivate::staticPanes) {
            if (std::get<1>(pane) == sectionName) panes.append(pane);
        }

        std::stable_sort(panes.begin(), panes.end(), [](const tSettingsWindowPrivate::StaticPane& first, const tSettingsWindowPrivate::StaticPane& second) {
            return std::get<0>(second) - std::get<0>(first) > 0;
        });
        for (const auto& [panePriority, paneSection, paneFunction] : panes) {
            settingsWindow.appendPane(paneFunction());
        }
    }

    settingsWindow.exec();
}

void tSettingsWindow::on_titleLabel_backButtonClicked() {
    this->close();
}

void tSettingsWindow::resizeEvent(QResizeEvent* event) {
    if (this->width() > 900) {
        ui->leftArea->setVisible(true);
        ui->line->setVisible(true);
        auto remainingWidth = this->width() - 900 - 1;
        ui->leftArea->setFixedWidth(remainingWidth / 2 + 300);
        ui->rightArea->setFixedWidth(remainingWidth / 2 + 600);

        for (auto widget : d->rightSide) widget->setFixedWidth(600);
    } else if (this->width() > 600) {
        ui->leftArea->setVisible(false);
        ui->line->setVisible(false);
        ui->rightArea->setFixedWidth(600);

        for (auto widget : d->rightSide) widget->setFixedWidth(600);
    } else {
        ui->rightArea->setFixedWidth(this->width());

        for (auto widget : d->rightSide) widget->setFixedWidth(this->width());
    }
}

void tSettingsWindow::on_contentsView_clicked(const QModelIndex& index) {
    if (auto* widget = index.data(Qt::UserRole + 1).value<QWidget*>()) {
        auto origin = widget->mapTo(ui->rightAreaContents, QPoint(0, 0));
        //        ui->rightArea->ensureWidgetVisible(widget);
        QScroller::scroller(ui->rightArea->viewport())->scrollTo(origin);
    }
}
