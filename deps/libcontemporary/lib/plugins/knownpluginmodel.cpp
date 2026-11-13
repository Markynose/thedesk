#include "knownpluginmodel.h"

#include <QIcon>
#include "tpluginmanagerhelper.h"

KnownPluginModel::KnownPluginModel(QObject* parent) :
    QAbstractListModel(parent) {
}

int KnownPluginModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;

    return tPluginManagerHelper::instance()->knownPlugins().length();
}

QVariant KnownPluginModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    auto uuid = tPluginManagerHelper::instance()->knownPlugins().at(index.row());
    switch (role) {
        case Qt::DisplayRole:
            return tPluginManagerHelper::instance()->pluginName(uuid);
        case Qt::DecorationRole:
            return tPluginManagerHelper::instance()->pluginIcon(uuid);
        case UuidRole:
            return uuid;
        case LoadedRole:
        case BlacklistedRole:
            return tPluginManagerHelper::instance()->blacklistedPlugins().contains(uuid);
    }

    return QVariant();
}
