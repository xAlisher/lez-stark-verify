#ifndef BLOCKCHAIN_PLUGIN_H
#define BLOCKCHAIN_PLUGIN_H

#include <QObject>
#include <QString>
#include <QtPlugin>          // for Q_PLUGIN_METADATA, Q_INTERFACES
#include "ZkPluginInterface.h"
#include "LogosViewPluginBase.h"

class LogosAPI;
class ZkVerifyBackend;

// Thin plugin entry point. Holds a ZkVerifyBackend and lets the
// generated view-plugin base expose it to ui-host.
class ZkPlugin : public QObject,
                         public ZkPluginInterface,
                         public ZkVerifyBackendViewPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ZkPluginInterface_iid FILE "../metadata.json")
    Q_INTERFACES(ZkPluginInterface)

public:
    explicit ZkPlugin(QObject* parent = nullptr);
    ~ZkPlugin() override;

    QString name()    const override { return "zk_guess_ui"; }
    QString version() const override { return "1.0.0"; }

    // Called by ui-host after plugin load. Creates the backend and wires
    // it up with the provided LogosAPI.
    Q_INVOKABLE void initLogos(LogosAPI* api);

private:
    ZkVerifyBackend* m_backend = nullptr;
};

#endif // BLOCKCHAIN_PLUGIN_H
