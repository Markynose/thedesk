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

#include "metainfofile.h"
#include <jsonfile.h>

bool genJsonFile(QString metainfoFile, QDir outputDirectory) {
    QTextStream output(stdout);

    JsonFile file;
    MetainfoFile metainfo(metainfoFile);
    while (metainfo.moveToNext()) {
        file.insertString(metainfo.trKey(), metainfo.trContents());
    }

    if (metainfo.isError()) {
        QTextStream(stderr) << "error: Unable to parse input metainfo file\n";
        return false;
    }

    // Write the output to en.json
    QString outputFile = outputDirectory.absoluteFilePath("en.json");
    output << "Generating file " << outputFile << "...\n";

    QFile f(outputFile);
    f.open(QFile::WriteOnly);
    f.write(file.output());
    f.close();
    return true;
}

bool genMetainfoFile(QString templateFile, QDir jsonDirectory, QString outputFile) {
    QTextStream output(stdout);
    QMap<QString, JsonFile> jsonFiles;
    for (const QFileInfo& file : jsonDirectory.entryInfoList({"*.json"}, QDir::Files)) {
        JsonFile jsonFile(file.absoluteFilePath());
        jsonFiles.insert(file.baseName(), jsonFile);
    }

    QFile f(outputFile);
    f.open(QFile::WriteOnly);

    MetainfoFile metainfo(templateFile);
    metainfo.setOutput(&f);
    while (metainfo.moveToNext()) {
        // Insert the translations into the metainfo file
        for (const QString& locale : jsonFiles.keys()) {
            QString value = jsonFiles.value(locale).message(metainfo.trKey());
            metainfo.writeTranslation(locale, value);
        }
    }

    if (metainfo.isError()) {
        QTextStream(stderr) << "error: Unable to parse input metainfo file\n";
        return false;
    }
    output << "Generating file " << outputFile << "...\n";

    f.close();
    return true;
}

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addOption({
        {"j", "json-generate"},
        "Generate JSON Translation files from AppStream Metainfo files"
    });
    parser.addOption({
        {"k", "metainfo-generate"},
        "Generate AppStream Metainfo files from JSON Translation files"
    });
    parser.addOption({
        {"d", "json-directory"},
        "Directory of JSON Translation files", "json-directory"
    });
    parser.addOption({
        {"t", "metainfo-template"},
        "AppStream Metainfo template file", "desktop template"
    });
    parser.addOption({
        {"o", "metainfo-output"},
        "AppStream Metainfo output file", "desktop output"
    });
    QCommandLineOption helpOption = parser.addHelpOption();
    parser.parse(a.arguments());

    QTextStream output(stdout);
    QTextStream eoutput(stderr);
    if (parser.isSet(helpOption)) {
        output << parser.helpText() << "\n";
        return 0;
    }

    bool genJson = parser.isSet("json-generate");
    bool genmetaInfo = parser.isSet("metainfo-generate");
    if (!genJson && !genmetaInfo) {
        eoutput << "error: At least one of --json-generate or --metainfo-generate must be specified.\n";
        return 1;
    }

    if (!parser.isSet("metainfo-template")) {
        eoutput << "error: A metainfo template file must be specified with --metainfo-template.\n";
        return 1;
    }

    if (!parser.isSet("json-directory")) {
        eoutput << "error: A JSON output directory must be specified with --json-directory.\n";
        return 1;
    }

    if (!parser.isSet("metainfo-output") && genmetaInfo) {
        eoutput << "error: A metainfo output file must be specified with --metainfo-output.\n";
        return 1;
    }

    QString metainfoTemplate = parser.value("metainfo-template");
    QDir outputDirectory(parser.value("json-directory"));
    if (!outputDirectory.exists()) outputDirectory.mkpath(".");

    bool success = false;
    if (genJson) {
        // Generate the en JSON file from a .desktop file
        success = genJsonFile(metainfoTemplate, outputDirectory);
    }
    if (genmetaInfo) {
        // Generate the final .desktop file from JSON files
        success = genMetainfoFile(metainfoTemplate, outputDirectory, parser.value("metainfo-output"));
    }

    return success ? 0 : 1;
}
