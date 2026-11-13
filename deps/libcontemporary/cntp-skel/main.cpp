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
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#include "projecttemplate.h"

bool noconfirm = false;

bool continueResponse(QString message = "OK to continue? [Y/n]: ") {
    QTextStream output(stdout);
    output << message;
    output.flush();

    if (noconfirm) {
        output << "y\n";
        return true;
    }

    QTextStream input(stdin);
    QString continueResponse = input.readLine();
    return continueResponse.toLower().startsWith("y") || continueResponse.isEmpty();
}

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addOption({
        {"t", "template"},
        "Template to generate", "template"
    });
    parser.addOption({
        {"p", "parameter"},
        "Input parameter for template", "name:value"
    });
    parser.addOption({"noconfirm", "Assume yes to everything"});
    parser.addPositionalArgument("projectName", "Name of the project to generate", "name");
    QCommandLineOption helpOption = parser.addHelpOption();
    parser.parse(a.arguments());

    QTextStream input(stdin);
    QTextStream output(stdout, QIODevice::WriteOnly | QIODevice::Unbuffered);
    QTextStream eoutput(stderr);
    if (parser.isSet(helpOption)) {
        output << parser.helpText() << "\n";
        return 0;
    }

    if (parser.positionalArguments().isEmpty()) {
        eoutput << "error: No project name specified.\n";
        return 1;
    }

    noconfirm = parser.isSet("noconfirm");
    QString projectName = parser.positionalArguments().first();

    QString templ = parser.value("template");
    if (!parser.isSet("template")) {
        output << "Select a template to create. Options include:\n";
        for (QString project : ProjectTemplate::templates()) {
            output << "  - " << project << "\n";
        }
        output << "\n";
        output << "Choose: ";
        output.flush();
        templ = input.readLine();
    }

    if (!ProjectTemplate::templateExists(templ)) {
        eoutput << "error: Template does not exist.\n";
        return 1;
    }

    QMap<QString, QString> commandLineInputParameters;
    for (QString parameter : parser.values("parameter")) {
        QString name = parameter.left(parameter.indexOf(":"));
        QString value = parameter.mid(parameter.indexOf(":") + 1);
        commandLineInputParameters.insert(name, value);
    }

    ProjectTemplate pt(templ);
    pt.setProjectName(projectName);

    for (QString parameter : pt.inputParameters()) {
        output << pt.inputParameterPrompt(parameter);
        output.flush();

        if (commandLineInputParameters.contains(parameter)) {
            pt.setTemplateValue(parameter, commandLineInputParameters.value(parameter));
            output << commandLineInputParameters.value(parameter) << "\n";
            output.flush();
        } else {
            pt.setTemplateValue(parameter, input.readLine());
        }
    }

    QString path = QDir::current().absoluteFilePath(projectName);
    output << "Creating new project with the following information:\n";
    output << "  Project Location: " << path << "\n";
    output << "  Project Name: " << projectName << "\n";
    output << "  Project Template: " << templ << "\n";

    for (QString parameter : pt.inputParameters()) {
        output << "  " << pt.inputParameterReadableName(parameter) << ": " << pt.templateValue(parameter) << "\n";
    }

    output.flush();

    if (!continueResponse()) {
        output << "Aborting.\n";
        return 0;
    }

    pt.create(path);
    return 0;
}
