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
#include <QCommandLineParser>
#include <MimeAssociations/mimeassociationmanager.h>

int main(int argc, char* argv[]) {
    tApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument("operation", "one of get, check or set");
    parser.addPositionalArgument("property", "Property to operate on");
    parser.addPositionalArgument("[value]", "Value to set");
    parser.addHelpOption();
    parser.process(a);

    //Ensure all the command line options are valid
    QTextStream out(stdout);
    QTextStream err(stderr);

    QString operation = parser.positionalArguments().at(0);
    QString property = parser.positionalArguments().at(1);
    if (operation == "get") {
        if (property == "default-web-browser") {
            ApplicationPointer defaultBrowser = MimeAssociationManager::defaultApplicationForMimeType("x-scheme-handler/http");
            if (!defaultBrowser) {
                err << "thedesk-xdg-settings: " + a.translate("main", "no default browser set") + "\n";
                return 1;
            } else {
                out << defaultBrowser->desktopEntry() << ".desktop\n";
            }
        } else {
            err << "thedesk-xdg-settings: " + a.translate("main", "unknown property") + "\n";
            err << a.translate("main", "Usage: %1 operation property [value]").arg(a.arguments().at(0)) + "\n";
            err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().at(0)) + "\n";
            return 1;
        }
    } else if (operation == "check") {
        QString value = parser.positionalArguments().at(2);
        if (property == "default-web-browser") {
            ApplicationPointer defaultHttp = MimeAssociationManager::defaultApplicationForMimeType("x-scheme-handler/http");
            ApplicationPointer defaultHttps = MimeAssociationManager::defaultApplicationForMimeType("x-scheme-handler/https");
            if (!defaultHttp || !defaultHttps) {
                out << "no\n";
                return 0;
            }

            if (defaultHttp->desktopEntry() != defaultHttps->desktopEntry()) {
                out << "no\n";
                return 0;
            }

            if (value != defaultHttp->desktopEntry() + ".desktop") {
                out << "no\n";
                return 0;
            }

            out << "yes\n";
            return 0;
        } else {
            err << "thedesk-xdg-settings: " + a.translate("main", "unknown property") + "\n";
            err << a.translate("main", "Usage: %1 operation property [value]").arg(a.arguments().at(0)) + "\n";
            err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().at(0)) + "\n";
            return 1;
        }
    } else if (operation == "set") {
        QString value = parser.positionalArguments().at(2);
        if (!value.endsWith(".desktop")) {
            err << "thedesk-xdg-settings: " + a.translate("main", "invalid application name") + "\n";
            err << a.translate("main", "Usage: %1 set property [value]").arg(a.arguments().at(0)) + "\n";
            err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().at(0)) + "\n";
            return 1;
        }

        if (property == "default-web-browser") {
            Application app(value.chopped(8));
            if (!app.isValid()) {
                err << "thedesk-xdg-settings: " + a.translate("main", "invalid application name") + "\n";
                err << a.translate("main", "Usage: %1 set property [value]").arg(a.arguments().at(0)) + "\n";
                err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().at(0)) + "\n";
                return 1;
            }

            MimeAssociationManager::setDefaultApplicationForMimeType(app.desktopEntry(), "x-scheme-handler/http");
            MimeAssociationManager::setDefaultApplicationForMimeType(app.desktopEntry(), "x-scheme-handler/https");
        } else {
            err << "thedesk-xdg-settings: " + a.translate("main", "unknown property") + "\n";
            err << a.translate("main", "Usage: %1 operation property [value]").arg(a.arguments().at(0)) + "\n";
            err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().at(0)) + "\n";
            return 1;
        }
    } else {
        err << "thedesk-xdg-settings: " + a.translate("main", "unknown operation") + "\n";
        err << a.translate("main", "Usage: %1 operation property [value]").arg(a.arguments().at(0)) + "\n";
        err << "       " + a.translate("main", "%1 -h for more information.").arg(a.arguments().at(0)) + "\n";
        return 1;
    }

    return 0;
}
