/**
 * @file KeyTaskDto.h
 * @brief 钥匙任务 DTO（跨层传输对象）
 *
 * 说明：
 * 1. UI/Controller 层不直接依赖协议实现结构（KeyTaskInfo）。
 * 2. 该 DTO 仅承载展示所需字段，避免跨层耦合。
 */
#ifndef KEYTASKDTO_H
#define KEYTASKDTO_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

struct KeyTaskDto
{
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

#endif // KEYTASKDTO_H
