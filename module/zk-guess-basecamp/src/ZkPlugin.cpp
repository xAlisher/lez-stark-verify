#include "ZkPlugin.h"
#include "ZkVerifyBackend.h"


#include <QDebug>

ZkPlugin::ZkPlugin(QObject* parent)
    : QObject(parent)
{
}

ZkPlugin::~ZkPlugin() = default;

void ZkPlugin::initLogos(LogosAPI* api)
{
    if (m_backend) return;
    m_backend = new ZkVerifyBackend(api, this);
    setBackend(m_backend);
    qDebug() << "ZkPlugin: backend initialized";
}
