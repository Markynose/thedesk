#ifndef TPLUGINMANAGERHELPER_H
#define TPLUGINMANAGERHELPER_H

#include <QObject>
#include <QUuid>
#include "libcontemporary_global.h"

struct tPluginManagerHelperPrivate;
class LIBCONTEMPORARY_EXPORT tPluginManagerHelper : public QObject {
        Q_OBJECT
    public:
        static tPluginManagerHelper* instance();

        struct PluginRegistration {
            QUuid uuid;
            bool shouldLoad;
        };

        PluginRegistration registerPlugin(QJsonObject data);

        QList<QUuid> knownPlugins();
        QString pluginName(QUuid uuid);
        QIcon pluginIcon(QUuid uuid);
        QString pluginError(QUuid uuid);

        QList<QUuid> loadedPlugins();
        void loadPlugin(QUuid uuid);
        void setPluginError(QUuid uuid, QString error);

        QList<QUuid> blacklistedPlugins();
        void blacklist(QUuid uuid);
        void unblacklist(QUuid uuid);

        bool havePendingPluginChanges();

    signals:
        void requestLoadPlugin(QUuid uuid);
        void blacklistChanged();
        void pluginError(QUuid uuid, QString error);

    private:
        explicit tPluginManagerHelper(QObject* parent = nullptr);
        tPluginManagerHelperPrivate* d;

        void writeBlacklist();
};

#endif // TPLUGINMANAGERHELPER_H
