#include "licensetextpopover.h"
#include "ui_licensetextpopover.h"

#include <QDesktopServices>
#include <QFile>
#include <tapplication.h>

LicenseTextPopover::LicenseTextPopover(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::LicenseTextPopover) {
    ui->setupUi(this);

    Q_INIT_RESOURCE(libcontemporary_licenses);

    QString license;
    switch (tApplication::applicationLicense()) {
        case tApplication::Gpl3:
        case tApplication::Gpl3OrLater:
            license = QStringLiteral("gpl3.html");
            break;
        case tApplication::Gpl2:
        case tApplication::Gpl2OrLater:
            license = QStringLiteral("gpl2.html");
            break;
        case tApplication::Lgpl3:
        case tApplication::Lgpl3OrLater:
            license = QStringLiteral("lgpl3.html");
            break;
        case tApplication::Lgpl2_1:
        case tApplication::Lgpl2_1OrLater:
            license = QStringLiteral("lgpl2.1.html");
            break;
        case tApplication::Other:
            break;
    }

    QFile file(QStringLiteral(":/the-libs/licenses/%1").arg(license));
    file.open(QFile::ReadOnly);
    ui->textBrowser->setHtml(file.readAll());
}

LicenseTextPopover::~LicenseTextPopover() {
    delete ui;
}

void LicenseTextPopover::on_titleLabel_backButtonClicked() {
    emit done();
}

void LicenseTextPopover::on_textBrowser_anchorClicked(const QUrl& arg1) {
    QDesktopServices::openUrl(arg1);
}
