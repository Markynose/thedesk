#ifndef TPLUGINMANAGER_H
#define TPLUGINMANAGER_H

#include "../tlogger.h"
#include "tpluginmanagerhelper.h"
#include <QDir>
#include <QDirIterator>
#include <QPluginLoader>
#include <tapplication.h>

template<typename T>
class tPluginManager {
    public:
        static tPluginManager<T>* instance() {
            auto name = QString(typeid(T).name());
            if (instances.contains(name)) return reinterpret_cast<tPluginManager<T>*>(instances.value(name));
            auto instance = new tPluginManager<T>();
            instances.insert(name, instance);
            return instance;
        }

        void setLibraryDirectory(QString libraryDirectory) {
            if (!this->libraryDirectory.isEmpty()) return;
            QObject::connect(tPluginManagerHelper::instance(), &tPluginManagerHelper::requestLoadPlugin, [this](QUuid uuid) {
                this->load(uuid);
            });

            this->libraryDirectory = libraryDirectory;

            // Discover all available plugins
            QStringList searchPaths = {
                QDir::cleanPath(qApp->applicationDirPath() + "/../plugins")};

#ifdef T_OS_UNIX_NOT_MAC
            searchPaths.append(QString(SYSTEM_LIBRARY_DIRECTORY).append(QStringLiteral("/%1/plugins").arg(libraryDirectory)));
#elif defined(Q_OS_WIN)
            searchPaths.append(qApp->applicationDirPath() + "/../../plugins");
            searchPaths.append(qApp->applicationDirPath() + "/plugins");
            searchPaths.append(qApp->applicationDirPath());
#elif defined(Q_OS_MAC)
            searchPaths.append(tApplication::macOSBundlePath() + "/Contents/Resources/Plugins");
            searchPaths.append(tApplication::macOSBundlePath() + "/../../plugins");
#endif

            QStringList seenPlugins;

            for (const QString& searchPath : searchPaths) {
                QDirIterator iterator(searchPath, {"*.so", "*.dll", "*.dylib"}, QDir::NoFilter, QDirIterator::Subdirectories);
                while (iterator.hasNext()) {
                    iterator.next();
                    if (seenPlugins.contains(iterator.fileName())) continue;
                    QPluginLoader loader(iterator.filePath());
                    auto reg = tPluginManagerHelper::instance()->registerPlugin(loader.metaData());
                    if (!reg.uuid.isNull()) {
                        knownPlugins.insert(reg.uuid, iterator.filePath());
                        if (reg.shouldLoad) load(reg.uuid);
                    }
                }
            }
        }

        bool load(QUuid uuid) {
            if (!knownPlugins.contains(uuid)) return false;
            if (loadedPlugins.contains(uuid)) return false;

            auto pluginPath = knownPlugins.value(uuid);
            QPluginLoader loader(pluginPath);
            QObject* instance = loader.instance();
            T* plugin = qobject_cast<T*>(instance);
            if (plugin) {
                plugin->activate();
                loadedPlugins.append(uuid);
                return true;
            } else {
                tWarn("PluginManager") << "Could not load plugin " << pluginPath;
                tWarn("PluginManager") << loader.errorString();
                tPluginManagerHelper::instance()->setPluginError(uuid, loader.errorString());
                return false;
            }
        }

    private:
        tPluginManager() {}

        QString libraryDirectory;
        QMap<QUuid, QString> knownPlugins;
        QList<QUuid> loadedPlugins;
        static inline QMap<QString, void*> instances = QMap<QString, void*>();
};

#endif // TPLUGINMANAGER_H
