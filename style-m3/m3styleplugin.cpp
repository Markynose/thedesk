/****************************************
 *
 *   theDesk - Material 3 Expressive widget style
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 * *************************************/
#include "m3styleplugin.h"

#include "m3style.h"

M3StylePlugin::M3StylePlugin(QObject* parent) :
    QStylePlugin(parent) {
}

QStyle* M3StylePlugin::create(const QString& key) {
    if (key.compare(QStringLiteral("Material3"), Qt::CaseInsensitive) == 0) return new M3Style();
    return nullptr;
}
