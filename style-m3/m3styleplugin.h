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
#ifndef M3STYLEPLUGIN_H
#define M3STYLEPLUGIN_H

#include <QStylePlugin>

class M3StylePlugin : public QStylePlugin {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QStyleFactoryInterface" FILE "plugin.json")
    public:
        explicit M3StylePlugin(QObject* parent = nullptr);
        QStyle* create(const QString& key) override;
};

#endif // M3STYLEPLUGIN_H
