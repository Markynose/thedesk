#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include <QObject>

class PluginInterface {
    public:
        virtual ~PluginInterface() {}

        virtual void activate() = 0;
        virtual void deactivate() = 0;
};

//#define PluginInterface_iid "com.vicr123.thebeat.PluginInterface/4.0"
// Q_DECLARE_INTERFACE(PluginInterface, PluginInterface_iid);

#endif // PLUGININTERFACE_H
