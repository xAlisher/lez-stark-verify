#ifndef POT_OUTPUT_BUFFER_H
#define POT_OUTPUT_BUFFER_H

#include <QByteArray>
#include <QString>
#include <QStringList>

class PotOutputBuffer
{
public:
    QStringList append(const QByteArray& chunk)
    {
        m_complete.append(chunk);
        m_pending.append(chunk);

        QStringList lines;
        int start = 0;
        int newline = -1;
        while ((newline = m_pending.indexOf('\n', start)) >= 0) {
            lines.append(QString::fromUtf8(m_pending.constData() + start,
                                           newline - start).trimmed());
            start = newline + 1;
        }
        if (start > 0) m_pending.remove(0, start);
        return lines;
    }

    QString completeOutput() const
    {
        return QString::fromUtf8(m_complete);
    }

private:
    QByteArray m_complete;
    QByteArray m_pending;
};

#endif // POT_OUTPUT_BUFFER_H
