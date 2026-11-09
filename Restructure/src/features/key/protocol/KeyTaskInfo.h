#ifndef KEYTASKINFO_H
#define KEYTASKINFO_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/**
 * One 20-byte task record returned by Q_TASK. The raw 16-byte identifier is
 * retained because DEL and task-log commands must send it without conversion.
 */
struct KeyTaskInfo {
    QByteArray taskId;
    quint8 status = 0;
    quint8 type = 0;
    quint8 source = 0;
    quint8 reserved = 0;

    QString taskIdHex() const
    {
        return QString(taskId.toHex(' ').toUpper());
    }
};

#endif // KEYTASKINFO_H
