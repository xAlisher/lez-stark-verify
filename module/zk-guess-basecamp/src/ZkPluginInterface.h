#ifndef BLOCKCHAIN_PLUGIN_INTERFACE_H
#define BLOCKCHAIN_PLUGIN_INTERFACE_H

#include <QtPlugin>          // for Q_DECLARE_INTERFACE
#include "interface.h"

// Marker interface used by Qt's plugin loader to identify the zk UI
// plugin. The actual API surface (Q_INVOKABLE methods, properties, signals)
// lives in ZkBackend.rep — this header only carries the IID.
class ZkPluginInterface : public PluginInterface
{
public:
    virtual ~ZkPluginInterface() = default;
};

#define ZkPluginInterface_iid "org.logos.ZkPluginInterface"
Q_DECLARE_INTERFACE(ZkPluginInterface, ZkPluginInterface_iid)

#endif // BLOCKCHAIN_PLUGIN_INTERFACE_H
