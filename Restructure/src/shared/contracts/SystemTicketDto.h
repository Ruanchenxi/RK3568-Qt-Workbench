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
    enum class AdminDeleteStage {
        None,                          // 未进入管理员删除流程
        KeyClearedAwaitingTermination, // 钥匙已清理，等待 HTTP 通知工作台
        TerminationFailed              // HTTP 通知工作台失败，需人工处理
    };

    QString taskId;
    QString ticketNo;
    QString taskName;
    int     taskType = 0;
    int     stepNum = 0;
    QString createTime;
    QString planTime;
    QString source;
    QString transferTriggerSource;
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
    AdminDeleteStage adminDeleteStage = AdminDeleteStage::None;
    QString adminDeleteError;
    // 最近一次 Q_TASK 在钥匙中亲眼确认此任务的毫秒时间戳（0 表示尚未确认）
    qint64 keyConfirmedAtMs = 0;
};

#endif // SYSTEMTICKETDTO_H
