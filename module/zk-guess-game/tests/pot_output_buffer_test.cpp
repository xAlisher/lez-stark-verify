#include "pot_output_buffer.h"

#include <QByteArray>
#include <QStringList>

#include <cassert>

int main()
{
    PotOutputBuffer output;

    const QStringList firstLines = output.append(
        QByteArrayLiteral("stage: syncing\naddr 0xabc\nbal"));
    assert(firstLines == QStringList({QStringLiteral("stage: syncing"),
                                      QStringLiteral("addr 0xabc")}));

    const QStringList secondLines = output.append(QByteArrayLiteral("ance 450\n"));
    assert(secondLines == QStringList({QStringLiteral("balance 450")}));
    assert(output.completeOutput()
           == QStringLiteral("stage: syncing\naddr 0xabc\nbalance 450\n"));

    return 0;
}
