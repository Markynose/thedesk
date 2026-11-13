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
#include "combinedicon.h"
#include "platformicongenerator.h"
#include <QColor>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPicture>
#include <QRegularExpression>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <QTextStream>
#include <QXmlStreamWriter>

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.addOption({
        {"i", "overlay-icon"},
        "Icon to overlay on base", "icon"
    });
    parser.addOption({
        {"m", "overlay-icon-mac"},
        "Icon to overlay on macOS base", "icon"
    });
    parser.addOption({
        {"c", "colors"},
        "Theme colors for icon, seperated by a colon", "colors"
    });
    parser.addOption({
        {"s", "output-svg"},
        "SVG output file", "svg-output"
    });
    parser.addOption({
        {"n", "output-native"},
        "Native output file", "native-output"
    });
    parser.addOption({
        {"r", "output-rc"},
        "Qt Resource output file", "rc-output"
    });
    parser.addOption({
        {"b", "blueprint"},
        "Create Blueprint style icon"
    });
    QCommandLineOption helpOption = parser.addHelpOption();
    parser.parse(a.arguments());

    QTextStream output(stdout);
    QTextStream eoutput(stderr);
    if (parser.isSet(helpOption)) {
        output << parser.helpText() << "\n";
        return 0;
    }

    bool baseIcon = parser.isSet("overlay-icon");
    bool haveColors = parser.isSet("colors");
    if (!baseIcon || !haveColors) {
        eoutput << "error: Must specify --base-icon and --colors.\n";
        return 1;
    }

    QString colors = parser.value("colors");
    if (!colors.contains(":")) {
        eoutput << "error: Color input format incorrect.\n";
        return 1;
    }

    QColor color1(colors.split(":").at(0));
    QColor color2(colors.split(":").at(1));

    if (!color1.isValid() || !color2.isValid()) {
        eoutput << "error: Color input format incorrect.\n";
        return 1;
    }

    QString overlayIcon, overlayIconMac;
    QFile baseIconFile(parser.value("overlay-icon"));
    baseIconFile.open(QFile::ReadOnly);
    overlayIcon = baseIconFile.readAll();
    baseIconFile.close();

    if (parser.isSet("overlay-icon-mac")) {
        QFile baseIconFile(parser.value("overlay-icon-mac"));
        baseIconFile.open(QFile::ReadOnly);
        overlayIconMac = baseIconFile.readAll();
        baseIconFile.close();
    }

    if (parser.isSet("output-svg")) {
        eoutput << "Creating SVG icon";

        CombinedIcon crossPlatformSvgIcon;
        crossPlatformSvgIcon.setBaseIcon(CombinedIcon::CrossPlatformIcon);
        crossPlatformSvgIcon.setIconGradientColors(color1, color2);
        crossPlatformSvgIcon.setGenerateBlueprintIcon(parser.isSet("blueprint"));
        crossPlatformSvgIcon.setOverlayIcon(overlayIcon);
        crossPlatformSvgIcon.setOverlayIconMac(overlayIconMac);

        QFile svgOutput(parser.value("output-svg"));
        svgOutput.open(QFile::WriteOnly);
        svgOutput.write(crossPlatformSvgIcon.generatedIcon().toUtf8());
        svgOutput.close();
    }

    if (parser.isSet("output-native")) {
        eoutput << "Creating native icon";
        PlatformIconGenerator* nativeGenerator = PlatformIconGenerator::iconGeneratorForPlatform();
        if (nativeGenerator) {
            CombinedIcon nativeSvgIcon;
            nativeSvgIcon.setBaseIcon(CombinedIcon::PlatformSpecificIcon);
            nativeSvgIcon.setIconGradientColors(color1, color2);
            nativeSvgIcon.setGenerateBlueprintIcon(parser.isSet("blueprint"));
            nativeSvgIcon.setOverlayIcon(overlayIcon);
            nativeSvgIcon.setOverlayIconMac(overlayIconMac);

            nativeGenerator->setInputSvg(nativeSvgIcon.generatedIcon());
            nativeGenerator->setOutputFile(parser.value("output-native"));
            nativeGenerator->generateIcon();
        } else {
            eoutput << "warn: No native generator available for this platform.\n";
        }
    }

    if (parser.isSet("output-rc")) {
        eoutput << "Creating resource files";

        CombinedIcon crossPlatformSvgIcon;
        crossPlatformSvgIcon.setBaseIcon(CombinedIcon::CrossPlatformIcon);
        crossPlatformSvgIcon.setIconGradientColors(color1, color2);
        crossPlatformSvgIcon.setGenerateBlueprintIcon(parser.isSet("blueprint"));
        crossPlatformSvgIcon.setOverlayIcon(overlayIcon);
        crossPlatformSvgIcon.setOverlayIconMac(overlayIconMac);

        QFileInfo rcFileInfo(parser.value("output-rc"));
        QFile rcSvgOutput(rcFileInfo.dir().absoluteFilePath("appicon.svg"));
        rcSvgOutput.open(QFile::WriteOnly);
        rcSvgOutput.write(crossPlatformSvgIcon.generatedIcon().toUtf8());
        rcSvgOutput.close();

        QFile rcFile(rcFileInfo.absoluteFilePath());
        rcFile.open(QFile::WriteOnly);

        QXmlStreamWriter xmlWriter(&rcFile);
        xmlWriter.writeStartElement("RCC");
        xmlWriter.writeStartElement("qresource");
        xmlWriter.writeAttribute("prefix", "/libcontemporary-appassets");
        xmlWriter.writeStartElement("file");
        xmlWriter.writeCharacters("appicon.svg");
        xmlWriter.writeEndElement(); // file
        xmlWriter.writeEndElement(); // qresource
        xmlWriter.writeEndElement(); // RCC

        rcFile.close();
    }

    return 0;
}
