#include "tcommandpalettewindow.h"
#include "ui_tcommandpalettewindow.h"

#include "tcommandpalettecontroller.h"
#include "tcommandpalettescope.h"
#include "touchbar/ttouchbar.h"
#include "touchbar/ttouchbarlabelitem.h"
#include "touchbar/ttouchbarsegmentedcontrolitem.h"
#include <QKeyEvent>
#include <QPushButton>
#include <QTimer>

struct tCommandPaletteWindowPrivate {
        tCommandPaletteController* controller;
        tCommandPaletteScope* currentScope = nullptr;
        QMap<tCommandPaletteScope*, QPushButton*> scopeButtons;

        tTouchBar* touchBar;
        tTouchBarSegmentedControlItem* touchBarSegmentedControl;
};

tCommandPaletteWindow::tCommandPaletteWindow(tCommandPaletteController* controller, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::tCommandPaletteWindow) {
    ui->setupUi(this);

    d = new tCommandPaletteWindowPrivate();
    d->controller = controller;

    QStringList scopesList;
    for (auto scope : d->controller->scopes()) {
        auto button = new QPushButton(ui->scopesWidget);
        button->setCheckable(true);
        button->setText(scope->displayName());
        connect(button, &QPushButton::clicked, this, [this, scope] {
            changeScope(scope);
        });
        ui->scopesLayout->addWidget(button);
        d->scopeButtons.insert(scope, button);
        scopesList.append(scope->displayName());
    }

    auto touchBarLabel = new tTouchBarLabelItem(QStringLiteral("com.vicr123.libcontemporary.commandpalette.scopeslabel"), tr("Scopes:"), this);

    d->touchBarSegmentedControl = new tTouchBarSegmentedControlItem(QStringLiteral("com.vicr123.libcontemporary.commandpalette.scopes"), tr("Scopes"), this);
    d->touchBarSegmentedControl->setSegments(scopesList);
    connect(d->touchBarSegmentedControl, &tTouchBarSegmentedControlItem::currentSegmentChanged, this, [this](int index) {
        this->changeScope(d->controller->scopes().at(index));
    });

    d->touchBar = new tTouchBar(this);
    d->touchBar->addDefaultItem(touchBarLabel);
    d->touchBar->addDefaultItem(d->touchBarSegmentedControl);
    d->touchBar->attach(this);

    if (!d->controller->scopes().empty()) changeScope(d->controller->scopes().constFirst());

    this->setWindowFlag(Qt::FramelessWindowHint);
#ifdef Q_OS_MAC
    this->setWindowFlag(Qt::Tool);
#endif

    ui->searchBox->setText(controller->currentQuery());
    ui->searchBox->selectAll();
    ui->searchBox->installEventFilter(this);
}

tCommandPaletteWindow::~tCommandPaletteWindow() {
    delete d;
    delete ui;
}

void tCommandPaletteWindow::changeScope(tCommandPaletteScope* scope) {
    for (auto button : d->scopeButtons.values()) {
        button->setChecked(false);
    }

    d->currentScope = scope;
    scope->filter(ui->searchBox->text());
    d->scopeButtons.value(scope)->setChecked(true);
    ui->listView->setModel(d->currentScope);
    d->touchBarSegmentedControl->setCurrentSegment(d->controller->scopes().indexOf(scope));
    emit scopeChanged(d->currentScope);
}

tCommandPaletteScope* tCommandPaletteWindow::currentScope() {
    return d->currentScope;
}

tCommandPaletteController* tCommandPaletteWindow::controller() {
    return d->controller;
}

void tCommandPaletteWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::ActivationChange) {
        if (!this->isActiveWindow()) {
            this->reject();
        }
    }
}

void tCommandPaletteWindow::reject() {
    QDialog::reject();
    this->deleteLater();
}

void tCommandPaletteWindow::on_searchBox_textChanged(const QString& arg1) {
    d->controller->setCurrentQuery(arg1);
    d->currentScope->filter(arg1);
}

void tCommandPaletteWindow::on_listView_clicked(const QModelIndex& index) {
    auto currentScope = d->currentScope;
    QTimer::singleShot(0, this, [this, currentScope, index] {
        currentScope->activate(index);
    });
    this->reject();
}

void tCommandPaletteWindow::on_searchBox_returnPressed() {
    auto currentScope = d->currentScope;
    QModelIndexList indices = ui->listView->selectionModel()->selectedIndexes();
    if (indices.isEmpty() || !indices.first().isValid()) {
        if (currentScope->rowCount() > 0)
            QTimer::singleShot(0, this, [this, currentScope] {
                currentScope->activate(currentScope->index(0));
            });
    } else {
        QTimer::singleShot(0, this, [this, currentScope, indices] {
            currentScope->activate(indices.at(0));
        });
    }
    this->reject();
}

bool tCommandPaletteWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->searchBox && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Down) {
            QItemSelectionModel* selectionModel = ui->listView->selectionModel();
            int selectionRow = 0;
            if (selectionModel->hasSelection()) {
                int row = selectionModel->selectedIndexes().first().row();
                if (row != d->currentScope->rowCount() - 1) selectionRow = row + 1;
            }

            auto index = d->currentScope->index(selectionRow);
            while (~index.flags() & Qt::ItemIsEnabled) {
                selectionRow++;
                if (selectionRow == d->currentScope->rowCount()) selectionRow = 0;
                index = d->currentScope->index(selectionRow);
            }

            selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
            ui->listView->scrollTo(index, QListView::PositionAtCenter);
            return true;
        } else if (keyEvent->key() == Qt::Key_Up) {
            QItemSelectionModel* selectionModel = ui->listView->selectionModel();
            int selectionRow = d->currentScope->rowCount() - 1;
            if (selectionModel->hasSelection()) {
                int row = selectionModel->selectedIndexes().first().row();
                if (row != 0) selectionRow = row - 1;
            }

            auto index = d->currentScope->index(selectionRow);
            while (~index.flags() & Qt::ItemIsEnabled) {
                selectionRow--;
                if (selectionRow == -1) selectionRow = d->currentScope->rowCount() - 1;
                index = d->currentScope->index(selectionRow);
            }

            selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
            ui->listView->scrollTo(index, QListView::PositionAtCenter);
            return true;
        }
    }
    return false;
}
