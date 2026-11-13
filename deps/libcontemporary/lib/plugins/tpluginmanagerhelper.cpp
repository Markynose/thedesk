#include "tpluginmanagerhelper.h"

#include "tsettings.h"
#include <QJsonObject>

struct tPluginManagerHelperPrivate {
        struct PluginData {
                QString name;
                QIcon icon;
        };

        tSettings settings;
        QList<QUuid> loadedPlugins;
        QList<QUuid> blacklistedPlugins;
        QMap<QUuid, PluginData> pluginData;
        QMap<QUuid, QString> pluginErrors;
};

tPluginManagerHelper* tPluginManagerHelper::instance() {
    static tPluginManagerHelper* instance = new tPluginManagerHelper();
    return instance;
}

tPluginManagerHelper::PluginRegistration tPluginManagerHelper::registerPlugin(QJsonObject data) {
    auto metadata = data.value("MetaData").toObject();
    auto uuid = QUuid::fromString(metadata.value("uuid").toString());

    if (!uuid.isNull()) {
        tPluginManagerHelperPrivate::PluginData pluginData;
        pluginData.name = metadata.value("name").toString();
        pluginData.icon = QIcon::fromTheme(metadata.value("icon").toString());
        d->pluginData.insert(uuid, pluginData);
    }

    PluginRegistration reg;
    reg.uuid = uuid;
    reg.shouldLoad = !d->blacklistedPlugins.contains(reg.uuid);
    if (reg.shouldLoad && !uuid.isNull()) {
        d->loadedPlugins.append(uuid);
    }
    return reg;
}

QList<QUuid> tPluginManagerHelper::knownPlugins() {
    return d->pluginData.keys();
}

QString tPluginManagerHelper::pluginName(QUuid uuid) {
    return d->pluginData.value(uuid).name;
}

QIcon tPluginManagerHelper::pluginIcon(QUuid uuid) {
    return d->pluginData.value(uuid).icon;
}

QString tPluginManagerHelper::pluginError(QUuid uuid) {
    return d->pluginErrors.value(uuid);
}

QList<QUuid> tPluginManagerHelper::loadedPlugins() {
    return d->loadedPlugins;
}

void tPluginManagerHelper::loadPlugin(QUuid uuid) {
    if (d->loadedPlugins.contains(uuid)) return;

    d->pluginErrors.remove(uuid);
    d->loadedPlugins.append(uuid);
    emit requestLoadPlugin(uuid);
}

void tPluginManagerHelper::setPluginError(QUuid uuid, QString error) {
    d->pluginErrors.insert(uuid, error);
    d->loadedPlugins.removeOne(uuid);
    emit pluginError(uuid, error);
}

QList<QUuid> tPluginManagerHelper::blacklistedPlugins() {
    return d->blacklistedPlugins;
}

void tPluginManagerHelper::blacklist(QUuid uuid) {
    if (d->blacklistedPlugins.contains(uuid)) return;
    d->blacklistedPlugins.append(uuid);
    this->writeBlacklist();
}

void tPluginManagerHelper::unblacklist(QUuid uuid) {
    if (!d->blacklistedPlugins.contains(uuid)) return;
    d->blacklistedPlugins.removeOne(uuid);
    this->writeBlacklist();
}

bool tPluginManagerHelper::havePendingPluginChanges() {
    for (auto plugin : d->blacklistedPlugins) {
        if (d->loadedPlugins.contains(plugin)) return true;
    }
    return false;
}

tPluginManagerHelper::tPluginManagerHelper(QObject* parent) :
    QObject{parent} {
    d = new tPluginManagerHelperPrivate();
    if (d->settings.contains("plugins/blacklist")) {
        auto blacklisted = d->settings.delimitedList("plugins/blacklist");
        for (const auto& item : blacklisted) {
            d->blacklistedPlugins.append(QUuid::fromString(item));
        }
    }
}

void tPluginManagerHelper::writeBlacklist() {
    QStringList blacklisted;
    for (auto item : d->blacklistedPlugins) {
        blacklisted.append(item.toString(QUuid::WithoutBraces));
    }
    d->settings.setDelimitedList("plugins/blacklist", blacklisted);
    emit blacklistChanged();
}
