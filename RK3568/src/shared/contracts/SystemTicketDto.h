/**
 * @file SystemTicketDto.h
 * @brief 系统票数据 DTO
 */
#ifndef SYSTEMTICKETDTO_H
#define SYSTEMTICKETDTO_H

#include <QDateTime>
#include <QString>

struct SystemTicketDto
{
    QString taskId;
    QString ticketNo;
    QString taskName;
    int     taskType = 0;
    int     stepNum = 0;
    QString createTime;
    QString planTime;
    QString source;
    QString transferState;
    QString lastError;
    QString returnState;
    QString returnError;
    QString cancelState;    // none | cancel-accepted | cancel-pending | cancel-executing | cancel-done | cancel-failed
    QString residentSlotId;
    QString residentKeyId;
    QDateTime cancelRequestedAt;
    QString cancelSource;
    bool wasInKey = false;
    QString stateDetail;
    QString jsonPath;
    QDateTime receivedAt;
    bool valid = false;
};

#endif // SYSTEMTICKETDTO_H
