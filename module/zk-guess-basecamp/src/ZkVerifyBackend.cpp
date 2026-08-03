#include "ZkVerifyBackend.h"
#include "logos_api.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

ZkVerifyBackend::ZkVerifyBackend(LogosAPI* logosAPI, QObject* parent)
    : ZkVerifyBackendSimpleSource(parent)
    , m_logosAPI(logosAPI)
{
    qDebug() << "ZkVerifyBackend: initialized";
}

// Resolve the bundled `zk-verify` tool: env override → next to the plugin → PATH.
QString ZkVerifyBackend::toolPath() const
{
    const QString env = qEnvironmentVariable("ZK_VERIFY_BIN");
    if (!env.isEmpty() && QFileInfo::exists(env))
        return env;
    const QString base = QCoreApplication::applicationDirPath();
    const QStringList candidates{
        base + QStringLiteral("/zk-verify"),
        base + QStringLiteral("/../plugins/zk_guess_ui/zk-verify"),
    };
    for (const QString& c : candidates)
        if (QFileInfo::exists(c))
            return c;
    return QStringLiteral("zk-verify"); // fall back to PATH
}

QString ZkVerifyBackend::receiptPath(const QString& name) const
{
    if (QFileInfo::exists(name))
        return name; // an absolute path was passed
    const QString file = name + QStringLiteral(".receipt");
    const QString env = qEnvironmentVariable("ZK_FIXTURES");
    if (!env.isEmpty() && QFileInfo::exists(env + "/" + file))
        return env + "/" + file;
    const QFileInfo tool(toolPath());
    return tool.absolutePath() + QStringLiteral("/fixtures/") + file;
}

void ZkVerifyBackend::verifyReceipt(QString name)
{
    const QString tool = toolPath();
    const QString rp = receiptPath(name);
    if (!QFileInfo::exists(rp)) {
        emit verifyResult(false, QString(), 0, -1, QStringLiteral("Receipt not found: %1").arg(rp));
        return;
    }

    QProcess p;
    p.start(tool, {QStringLiteral("verify"), rp});
    if (!p.waitForFinished(15000)) {
        p.kill();
        emit verifyResult(false, QString(), 0, -1, QStringLiteral("zk-verify timed out"));
        return;
    }

    const QByteArray out = p.readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    if (!doc.isObject()) {
        const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
        emit verifyResult(false, QString(), 0, -1,
                          err.isEmpty() ? QStringLiteral("zk-verify produced no result") : err);
        return;
    }

    const QJsonObject o = doc.object();
    setLastJson(QString::fromUtf8(out).trimmed());
    emit verifyResult(o.value(QStringLiteral("valid")).toBool(),
                      o.value(QStringLiteral("commitment")).toString(),
                      o.value(QStringLiteral("guess")).toInt(),
                      o.value(QStringLiteral("dir")).toInt(),
                      QString());
}
