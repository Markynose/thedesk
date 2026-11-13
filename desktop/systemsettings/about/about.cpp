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
#include "about.h"
#include "ui_about.h"

#include "aboutcards.h"
#include "acknowledgements.h"
#include "changehostnamepopover.h"
#include <QBoxLayout>
#include <QBuffer>
#include <QDBusInterface>
#include <QFile>
#include <QHostInfo>
#include <QDesktopServices>
#include <QIcon>
#include <QUrl>
#include <Wm/desktopwm.h>
#include <statemanager.h>
#include <statuscentermanager.h>
#include <sys/sysinfo.h>
#include <tapplication.h>
#include <tlogger.h>
#include <tpopover.h>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

struct AboutPrivate {
        QDBusInterface* hostnamed;
        AboutPage* page = nullptr;
};

About::About() :
    StatusCenterPane(),
    ui(new Ui::About) {
    ui->setupUi(this);

    d = new AboutPrivate();
    d->hostnamed = new QDBusInterface("org.freedesktop.hostname1", "/org/freedesktop/hostname1", "org.freedesktop.hostname1", QDBusConnection::systemBus());

    ui->titleLabel->setBackButtonIsMenu(true);
    ui->titleLabel->setBackButtonShown(StateManager::instance()->statusCenterManager()->isHamburgerMenuRequired());
    connect(StateManager::instance()->statusCenterManager(), &StatusCenterManager::isHamburgerMenuRequiredChanged, ui->titleLabel, &tTitleLabel::setBackButtonShown);

    // Fill in details from hostnamed
    ui->deviceTypeLabel->setText(QStringLiteral("%1 %2").arg(d->hostnamed->property("HardwareVendor").toString(), d->hostnamed->property("HardwareModel").toString()));
    ui->distroWebpage->setText(d->hostnamed->property("HomeURL").toString());
    ui->distroName->setText(d->hostnamed->property("OperatingSystemPrettyName").toString());

    struct sysinfo* info = new struct sysinfo;
    if (sysinfo(info) == 0) {
        ui->availableMemory->setText(QLocale().formattedDataSize(static_cast<qint64>(info->totalram)));
        ui->availableSwap->setText(QLocale().formattedDataSize(static_cast<qint64>(info->totalswap)));
    } else {
        ui->availableMemory->setText(tr("Unknown"));
        ui->availableSwap->setText(tr("Unknown"));
    }
    delete info;

    ui->kernelVersion->setText(QSysInfo::kernelVersion());
    ui->qtVersion->setText(qVersion());

    QFile cpuInfoFile("/proc/cpuinfo");
    cpuInfoFile.open(QFile::ReadOnly);
    QByteArray cpuInfoBuf = cpuInfoFile.readAll();
    QBuffer cpuInfo(&cpuInfoBuf);
    cpuInfo.open(QBuffer::ReadOnly);
    QStringList knownCpus;
    while (!cpuInfo.atEnd()) {
        QString line = cpuInfo.readLine();
        if (line.startsWith("model name")) {
            QString cpu = line.mid(line.indexOf(":") + 1).trimmed();
            knownCpus.append(cpu);
        }
    }

    QStringList shownCpus;
    while (knownCpus.length() > 0) {
        QString cpu = knownCpus.value(0);
        int numberOfThisCpu = knownCpus.count(cpu);

        knownCpus.removeAll(cpu);

        if (numberOfThisCpu == 1) {
            shownCpus.append(cpu);
        } else {
            shownCpus.append(QString::number(numberOfThisCpu) + " × " + cpu);
        }
    }

    if (shownCpus.length() == 0) {
        ui->processorType->setText(tr("Unknown"));
    } else {
        ui->processorType->setText(shownCpus.join(" · "));
    }

    QDBusConnection::systemBus().connect("org.freedesktop.hostname1", "/org/freedesktop/hostname1", "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(updateHostname()));
    updateHostname();

    ui->videoLabel->setText(videoCard());
    ui->windowSystemLabel->setText(DesktopWm::windowSystemName());

#ifdef BLUEPRINT
    ui->versionLabel->setText(tr("theDesk %1 - Blueprint").arg("1.0"));
    ui->compileDate->setText(tr("You compiled theDesk on %1").arg(__DATE__));
#else
    ui->versionLabel->setText(tr("theDesk %1").arg(tApplication::applicationVersion()));
    ui->compileDate->setVisible(false);
#endif

    ui->copyrightNotice->setText(tr("Copyright © Victor Tran %1. Licensed under the terms of the GNU General Public License, version 3 or later.").arg(tApplication::copyrightYear()));

    const int contentWidth = StateManager::instance()->statusCenterManager()->preferredContentWidth();
    ui->sysinfoFrame->layout()->setAlignment(ui->distributionPane, Qt::AlignHCenter);
    ui->sysinfoFrame->layout()->setAlignment(ui->hardwareSoftwarePane, Qt::AlignHCenter);
    ui->centralWidget->layout()->setAlignment(ui->aboutPane, Qt::AlignHCenter);
    ui->centralWidget->layout()->setAlignment(ui->copyrightNotice, Qt::AlignHCenter);
    ui->distributionPane->setFixedWidth(contentWidth);
    ui->hardwareSoftwarePane->setFixedWidth(contentWidth);
    ui->aboutPane->setFixedWidth(contentWidth);
    ui->copyrightNotice->setFixedWidth(contentWidth);

    // The redesigned page takes the old one's place; the old labels above
    // still gather the figures it shows
    for (QWidget* old : {static_cast<QWidget*>(ui->sysinfoFrame), static_cast<QWidget*>(ui->aboutPane), static_cast<QWidget*>(ui->copyrightNotice)}) old->hide();
    d->page = new AboutPage(ui->centralWidget);
    static_cast<QBoxLayout*>(ui->centralWidget->layout())->insertWidget(0, d->page); // above the old layout's stretch
    ui->centralWidget->layout()->setAlignment(d->page, Qt::AlignHCenter | Qt::AlignTop);
    d->page->setSpecs({
        {QStringLiteral("memory"), tr("Processor"), QString(ui->processorType->text()).remove(QStringLiteral("(R)")).remove(QStringLiteral("(TM)")).replace(QStringLiteral(" CPU "), QStringLiteral(" ")).section(QStringLiteral(" @ "), 0, 0).simplified()},
        {QStringLiteral("monitor"), tr("Graphics"), ui->videoLabel->text()},
        {QStringLiteral("memory_alt"), tr("Memory"), ui->availableMemory->text()},
        {QStringLiteral("swap_horiz"), tr("Swap"), ui->availableSwap->text()},
        {QStringLiteral("terminal"), tr("Kernel"), ui->kernelVersion->text()},
        {QStringLiteral("desktop_windows"), tr("Window system"), ui->windowSystemLabel->text()},
        {QStringLiteral("code"), tr("Qt"), ui->qtVersion->text()},
        {QStringLiteral("deployed_code"), tr("theDesk"), tApplication::applicationVersion()},
    });
    d->page->addChip(QStringLiteral("help-about"), tr("Acknowledgements"), [this] {
        on_acknowledgementsButton_clicked();
    });
    d->page->addChip(QStringLiteral("tools-report-bug"), tr("Debug log"), [this] {
        on_debugLogButton_clicked();
    });
    d->page->addChip(QStringLiteral("applications-development"), tr("Source code"), [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/theCheeseboard/thedesk")));
    });
    d->page->onRename([this] {
        on_changeHostnameButton_clicked();
    });
    updateHostname();
}

void About::showEvent(QShowEvent* event) {
    StatusCenterPane::showEvent(event);
    d->page->opened();
}

About::~About() {
    delete d;
    delete ui;
}

void About::on_titleLabel_backButtonClicked() {
    StateManager::instance()->statusCenterManager()->showStatusCenterHamburgerMenu();
}

void About::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        emit displayNameChanged();
    }
}

QString About::videoCard() {
    QSurfaceFormat format;
    format.setMajorVersion(3); // Specify the version of OpenGL context
    format.setMinorVersion(2);
    format.setProfile(QSurfaceFormat::CoreProfile); // Modern OpenGL

    QOffscreenSurface offscreenSurface;
    offscreenSurface.setFormat(format); // Apply the format
    offscreenSurface.create();          // This must be called to actually create the surface

    QOpenGLContext context;
    context.setFormat(format); // Again, set the format
    if (!context.create())     // Try to create the OpenGL context
    {
        return tr("Unknown");
    }

    if (!context.makeCurrent(&offscreenSurface)) // Attempt to make the context current
    {
        return tr("Unknown");
    }

    QOpenGLFunctions* functions = context.functions(); // Get the OpenGL functions for the current context

    QString renderer = QString::fromUtf8(reinterpret_cast<const char*>(functions->glGetString(GL_RENDERER))); // Get the GPU's name

    context.doneCurrent();

    return renderer;
}

QString About::name() {
    return "SystemAbout";
}

QString About::displayName() {
    return tr("About");
}

QIcon About::icon() {
    return QIcon::fromTheme("preferences-desktop-about-symbolic");
}

QWidget* About::leftPane() {
    return nullptr;
}

void About::on_acknowledgementsButton_clicked() {
    Acknowledgements* ack = new Acknowledgements();
    tPopover* popover = new tPopover(ack);
    popover->setPopoverWidth(SC_DPI(600));
    connect(ack, &Acknowledgements::done, popover, &tPopover::dismiss);
    connect(popover, &tPopover::dismissed, ack, &Acknowledgements::deleteLater);
    connect(popover, &tPopover::dismissed, popover, &tPopover::deleteLater);
    popover->show(this->window());
}

void About::on_debugLogButton_clicked() {
    StateManager::statusCenterManager()->hide();
    tLogger::openDebugLogWindow();
}

void About::on_changeHostnameButton_clicked() {
    ChangeHostnamePopover* changeHostname = new ChangeHostnamePopover();
    tPopover* popover = new tPopover(changeHostname);
    popover->setPopoverWidth(SC_DPI(600));
    connect(changeHostname, &ChangeHostnamePopover::done, popover, &tPopover::dismiss);
    connect(popover, &tPopover::dismissed, changeHostname, &Acknowledgements::deleteLater);
    connect(popover, &tPopover::dismissed, popover, &tPopover::deleteLater);
    popover->show(this->window());
}

void About::updateHostname() {
    QString hostname = d->hostnamed->property("PrettyHostname").toString();
    if (hostname.isEmpty()) hostname = d->hostnamed->property("Hostname").toString();
    if (hostname.isEmpty()) hostname = QHostInfo::localHostName();

    ui->hostnameLabel->setText(hostname);
    if (d->page) d->page->setHostname(hostname);
}
