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

#ifndef TRANSLATORPROXY_H
#define TRANSLATORPROXY_H

#include <QTranslator>

struct TranslatorProxyPrivate;
class TranslatorProxy : public QTranslator {
        Q_OBJECT
    public:
        explicit TranslatorProxy(QTranslator* parentTranslator, QObject* parent = nullptr);
        ~TranslatorProxy();

    signals:

    private:
        TranslatorProxyPrivate* d;

        // QTranslator interface
    public:
        QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override;
        bool isEmpty() const override;
};

#endif // TRANSLATORPROXY_H
