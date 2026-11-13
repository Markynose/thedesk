/*******************************************************************************
 *
 * libcontemporary - Common libraries
 * Copyright (C) 2022 Victor Tran
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#include "tprintpopover.h"
#include "ui_tprintpopover.h"

#include <QFileDialog>
#include <QPrintPreviewWidget>
#include <QPrinter>
#include <QPrinterInfo>
#include <QStandardPaths>

struct tPrintPopoverPrivate {
        QPrinter* printer;
        QPrintPreviewWidget* printPreview;
        tPrintPopoverCustomPrintSettingsWidget* customWidget = nullptr;
};

tPrintPopover::tPrintPopover(QPrinter* printer, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::tPrintPopover) {
    ui->setupUi(this);

    d = new tPrintPopoverPrivate();
    d->printer = printer;

    d->printPreview = new QPrintPreviewWidget(d->printer);
    connect(d->printPreview, &QPrintPreviewWidget::paintRequested, this, &tPrintPopover::requestPaint);
    ui->printerLayout->addWidget(d->printPreview);

    ui->singlePixelHorizontalLine->setVisible(false);
    ui->printSettingsWidget->setFixedWidth(600);

    ui->titleLabel->setBackButtonShown(true);

    ui->printerBox->addItems(QPrinterInfo::availablePrinterNames());
    ui->printerBox->setCurrentIndex(QPrinterInfo::availablePrinterNames().indexOf(QPrinterInfo::defaultPrinterName()));
    updatePageSizes();
}

tPrintPopover::~tPrintPopover() {
    if (d->customWidget) d->customWidget->deleteLater();
    delete d;
    delete ui;
}

void tPrintPopover::setTitle(QString title) {
    ui->titleLabel->setText(title);
}

QString tPrintPopover::title() {
    return ui->titleLabel->text();
}

void tPrintPopover::setCustomPrintSettingsWidget(tPrintPopoverCustomPrintSettingsWidget* customWidget) {
    if (d->customWidget != nullptr) {
        d->customWidget->disconnect(this);
        ui->customPrintSettingsLayout->removeWidget(d->customWidget);
        ui->singlePixelHorizontalLine->setVisible(false);
        d->customWidget->deleteLater();
    }

    d->customWidget = customWidget;
    if (d->customWidget) {
        connect(d->customWidget, &tPrintPopoverCustomPrintSettingsWidget::requestPaintUpdate, d->printPreview, &QPrintPreviewWidget::updatePreview);
        ui->customPrintSettingsLayout->addWidget(d->customWidget);
        ui->singlePixelHorizontalLine->setVisible(true);
    }
}

tPrintPopoverCustomPrintSettingsWidget* tPrintPopover::customPrintSettingsWidget() {
    return d->customWidget;
}

void tPrintPopover::on_titleLabel_backButtonClicked() {
    emit done();
}

void tPrintPopover::on_printerBox_currentIndexChanged(int index) {
    d->printer->setPrinterName(ui->printerBox->itemText(index));
    updatePageSizes();
    d->printPreview->updatePreview();
}

void tPrintPopover::on_printButton_clicked() {
    requestPaint(d->printer);
    emit done();
}

void tPrintPopover::on_pageSizeBox_currentIndexChanged(int index) {
    d->printer->setPageSize(QPrinterInfo::printerInfo(d->printer->printerName()).supportedPageSizes().at(index));
    d->printPreview->updatePreview();
}

void tPrintPopover::updatePageSizes() {
    QSignalBlocker blocker(ui->pageSizeBox);
    ui->pageSizeBox->clear();

    QPrinterInfo printerInfo = QPrinterInfo::printerInfo(d->printer->printerName());
    for (const QPageSize& pageSize : printerInfo.supportedPageSizes()) {
        ui->pageSizeBox->addItem(pageSize.name());
        if (printerInfo.defaultPageSize() == pageSize) {
            d->printer->setPageSize(pageSize);
            ui->pageSizeBox->setCurrentIndex(ui->pageSizeBox->count() - 1);
        }
    }
}

void tPrintPopover::requestPaint(QPrinter* printer) {
    this->setEnabled(false);
    emit paintRequested(printer);
    this->setEnabled(true);
}

void tPrintPopover::on_grayscaleBox_toggled(bool checked) {
    d->printer->setColorMode(checked ? QPrinter::GrayScale : QPrinter::Color);
    d->printPreview->updatePreview();
}

void tPrintPopover::on_duplexBox_toggled(bool checked) {
    d->printer->setDuplex(checked ? QPrinter::DuplexLongSide : QPrinter::DuplexNone);
    d->printPreview->updatePreview();
}

tPrintPopoverCustomPrintSettingsWidget::tPrintPopoverCustomPrintSettingsWidget(QWidget* parent) :
    QWidget(parent) {
}

tPrintPopoverCustomPrintSettingsWidget::~tPrintPopoverCustomPrintSettingsWidget() {
}

void tPrintPopover::showEvent(QShowEvent* event) {
    d->printPreview->updatePreview();
}

void tPrintPopover::on_pdfButton_clicked() {
    QFileDialog* dialog = new QFileDialog(this);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    dialog->setNameFilters({tr("Portable Document Format (*.pdf)")});
    dialog->setDefaultSuffix(".pdf");
    connect(
        dialog, &QFileDialog::accepted, this, [this, dialog] {
            d->printer->setOutputFileName(dialog->selectedFiles().first());
            d->printer->setOutputFormat(QPrinter::PdfFormat);
            requestPaint(d->printer);
            emit done();
        },
        Qt::QueuedConnection);
    connect(dialog, &QFileDialog::finished, dialog, &QFileDialog::deleteLater);
    dialog->open();
}
