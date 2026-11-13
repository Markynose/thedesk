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

#include "translatorproxy.h"

struct TranslatorProxyPrivate {
        QTranslator* primaryTranslator;

        static QStringList numberExtensions;
        static QString accentReplacements;
};

QStringList TranslatorProxyPrivate::numberExtensions{
    "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
    "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty"};

QString TranslatorProxyPrivate::accentReplacements =
    QStringLiteral("AÅBβCÇDĐEÊGĜHĤIÎJĴKĶLĻMḾNÑOÖPÞRŔSŠTŢUÛVvWŴYÝZaåcçdðeéffgĝhĥiîjĵkķlļmḿnñoöpþrŕsštţuûvVwŵyýzƶ");

TranslatorProxy::TranslatorProxy(QTranslator* parentTranslator, QObject* parent) :
    QTranslator(parent) {
    d = new TranslatorProxyPrivate();
    d->primaryTranslator = parentTranslator;
}

TranslatorProxy::~TranslatorProxy() {
    d->primaryTranslator->deleteLater();
    delete d;
}

QString TranslatorProxy::translate(const char* context, const char* sourceText, const char* disambiguation, int n) const {
    QString translatedString = d->primaryTranslator->translate(context, sourceText, disambiguation, n);

    if (qEnvironmentVariable("THELIBS_ENABLE_PSEUDOTRANSLATION") == "1") {
        if (translatedString.isEmpty()) {
            translatedString = sourceText;
            if (translatedString.contains("%n")) translatedString.replace("%n", QString::number(n));
        }

        int extensionLength = translatedString.length() * 0.7;
        QStringList allExtensions = d->numberExtensions;
        QStringList usedExtensions;
        while (extensionLength > 0) {
            QString str = allExtensions.takeFirst();
            usedExtensions.append(str);
            extensionLength -= str.length() + 1;

            if (allExtensions.isEmpty()) allExtensions = d->numberExtensions;
        }

        QString accentedString;
        for (QChar character : qAsConst(translatedString)) {
            int index = d->accentReplacements.indexOf(character);
            if (index == -1) {
                accentedString.append(character);
            } else {
                accentedString.append(d->accentReplacements.at(index + 1));
            }
        }

        translatedString = QStringLiteral("[%2 %1]").arg(usedExtensions.join(" "), accentedString);
    }

    return translatedString;
}

bool TranslatorProxy::isEmpty() const {
    return d->primaryTranslator->isEmpty();
}
