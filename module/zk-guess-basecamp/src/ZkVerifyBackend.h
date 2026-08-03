#ifndef ZK_VERIFY_BACKEND_H
#define ZK_VERIFY_BACKEND_H

#include <QObject>
#include <QString>

#include "rep_ZkVerifyBackend_source.h"

class LogosAPI;

// Verifies a RISC0 STARK receipt by driving the bundled `zk-verify` tool
// (verify is pure — no r0vm — so it runs on the node in ms).
class ZkVerifyBackend : public ZkVerifyBackendSimpleSource
{
    Q_OBJECT
public:
    explicit ZkVerifyBackend(LogosAPI* logosAPI, QObject* parent = nullptr);

public slots:
    void verifyReceipt(QString name) override;

private:
    QString toolPath() const;
    QString receiptPath(const QString& name) const;

    LogosAPI* m_logosAPI = nullptr;
};

#endif // ZK_VERIFY_BACKEND_H
