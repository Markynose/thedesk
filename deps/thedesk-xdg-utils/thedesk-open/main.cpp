/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2021 Victor Tran
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

#include <tapplication.h>
#include <tsettings.h>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDebug>
#include <QMimeDatabase>
#include <QScreen>
#include <Applications/application.h>
#include <MimeAssociations/mimeassociationmanager.h>
#include "fileassociationselect.h"

void openUrl(QUrl url, bool forcePrompt) {
    QMimeDatabase db;

    QMap<QString, QString> launchArgs;
    launchArgs.insert("%u", url.toString());
    launchArgs.insert("%U", url.toString());

    QString mimeType;
    if (url.isLocalFile()) {
        QMimeType mimeTypeDb = db.mimeTypeForFile(url.toLocalFile());
        mimeType = mimeTypeDb.name();

        launchArgs.insert("%f", url.toLocalFile());
        launchArgs.insert("%F", url.toLocalFile());
    } else {
        mimeType = "x-scheme-handler/" + url.scheme();
    }

    if (!forcePrompt) {
        ApplicationPointer defaultApp = MimeAssociationManager::defaultApplicationForMimeType(mimeType);
        if (defaultApp) {
            defaultApp->launch(launchArgs);
            return;
        }
    }

    qDebug() << "MIME type: " << mimeType;
    QList<ApplicationPointer> apps = MimeAssociationManager::applicationsForMimeType(mimeType);

    if (apps.count() == 1 && !forcePrompt) {
        MimeAssociationManager::setDefaultApplicationForMimeType(apps.first()->desktopEntry(), mimeType);
        apps.first()->launch(launchArgs);
    } else {
        FileAssociationSelect dialog(url, mimeType, apps);

        dialog.show();

        QRect geometry = dialog.geometry();
        geometry.moveCenter(tApplication::screenAt(QCursor::pos())->geometry().center());
        dialog.setGeometry(geometry);

        if (dialog.exec() == FileAssociationSelect::Rejected) return;

        if (dialog.shouldSetDefault()) MimeAssociationManager::setDefaultApplicationForMimeType(dialog.selectedApplication()->desktopEntry(), mimeType);

        dialog.selectedApplication()->launch(launchArgs);
    }
}

int main(int argc, char* argv[]) {
    tApplication a(argc, argv);

    a.setOrganizationName("theSuite");
    a.setOrganizationDomain("vicr123.com");
    a.setApplicationName("thedesk-open");

    tSettings::registerDefaults(a.applicationDirPath() + "/associations.conf");
    tSettings::registerDefaults("/etc/theSuite/theDesk/associations.conf");

    QCommandLineParser parser;
    parser.addPositionalArgument(a.translate("main", "url"), a.translate("main", "The URL or path of a file to open"));
    parser.addOption({"force-prompt", a.translate("main", "Show the app selection dialog, even if there is an app set as the default")});
    parser.addHelpOption();
    parser.process(a);

    //Ensure all the command line options are valid
    QTextStream out(stdout);
    QTextStream err(stderr);
    if (parser.positionalArguments().count() == 0) {
        err << "thedesk-open: " + a.translate("main", "missing operand") + "\n";
        err << a.translate("main", "Usage: %1 [options] url").arg(a.arguments().first()) + "\n";
        err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().first()) + "\n";
        return 1;
    } else if (parser.positionalArguments().count() > 1) {
        err << "thedesk-open: " + a.translate("main", "too many operands") + "\n";
        err << a.translate("main", "Usage: %1 [options] url").arg(a.arguments().first()) + "\n";
        err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().first()) + "\n";
        return 1;
    }


    QString arg = parser.positionalArguments().first();
    QString canonical = QFileInfo(arg).canonicalFilePath();
    if (QFileInfo::exists(canonical)) {
        //Open arg as a file
        openUrl(QUrl::fromLocalFile(canonical), parser.isSet("force-prompt"));
    } else {
        QUrl url = QUrl::fromUserInput(arg);
        openUrl(url, parser.isSet("force-prompt"));
    }

    return 0;
}
