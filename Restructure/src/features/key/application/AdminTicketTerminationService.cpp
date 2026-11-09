#include "features/key/application/AdminTicketTerminationService.h"

#include <utility>

#include "features/key/application/TicketStore.h"
#include "features/key/application/TicketTerminationClient.h"
#include "platform/logging/BusinessLogService.h"

AdminTicketTerminationService::AdminTicketTerminationService(
        TicketStore *ticketStore,
        AccessTokenProvider tokenProvider,
        QObject *parent)
    : QObject(parent)
    , m_ticketStore(ticketStore)
    , m_client(new TicketTerminationClient(this))
    , m_tokenProvider(std::move(tokenProvider))
{
    connect(m_client, &TicketTerminationClient::logAppended,
            this, &AdminTicketTerminationService::logAppended);
    connect(m_client, &TicketTerminationClient::terminationSucceeded,
            this, &AdminTicketTerminationService::handleSucceeded);
    connect(m_client, &TicketTerminationClient::terminationFailed,
            this, &AdminTicketTerminationService::handleFailed);
}

void AdminTicketTerminationService::terminate(const QString &taskId,
                                              int keySessionId)
{
    const QString normalizedTaskId = taskId.trimmed();
    if (normalizedTaskId.isEmpty()) {
        return;
    }
    if (m_inFlightTaskIds.contains(normalizedTaskId)) {
        emit logAppended(
                QStringLiteral("[ADMIN-TERMINATION] skipped: request already in flight taskId=%1")
                        .arg(normalizedTaskId));
        return;
    }

    const QString accessToken = m_tokenProvider ? m_tokenProvider().trimmed() : QString();
    if (accessToken.isEmpty()) {
        m_ticketStore->updateAdminDeleteStage(
                normalizedTaskId,
                SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination);
        m_ticketStore->updateAdminDeleteError(
                normalizedTaskId, QStringLiteral("管理员登录令牌为空"));
        emit statusMessage(
                QStringLiteral("工作台通知失败：管理员登录令牌为空（任务 ID: %1）")
                        .arg(normalizedTaskId));
        return;
    }

    m_ticketStore->updateAdminDeleteStage(
            normalizedTaskId,
            SystemTicketDto::AdminDeleteStage::KeyClearedAwaitingTermination);
    m_ticketStore->updateAdminDeleteError(normalizedTaskId, QString());
    m_inFlightTaskIds.insert(normalizedTaskId);
    m_keySessionIds.insert(normalizedTaskId, keySessionId);
    m_client->terminate(normalizedTaskId, accessToken);
}

void AdminTicketTerminationService::handleSucceeded(const QString &taskId)
{
    BusinessLogEvent event;
    event.flow = QStringLiteral("admin-delete");
    event.phase = QStringLiteral("backend-termination");
    event.result = QStringLiteral("completed");
    event.taskId = taskId;
    event.slot = QStringLiteral("A");
    event.keySessionId = m_keySessionIds.value(taskId, -1);
    BusinessLogService::write(event);

    m_inFlightTaskIds.remove(taskId);
    m_keySessionIds.remove(taskId);
    m_ticketStore->removeTicket(taskId);
    emit terminationCompleted(taskId);
    emit statusMessage(QStringLiteral("系统票已终止并删除：%1").arg(taskId));
}

void AdminTicketTerminationService::handleFailed(const QString &taskId,
                                                 const QString &reason)
{
    BusinessLogEvent event;
    event.flow = QStringLiteral("admin-delete");
    event.phase = QStringLiteral("backend-termination");
    event.result = QStringLiteral("failed");
    event.taskId = taskId;
    event.slot = QStringLiteral("A");
    event.keySessionId = m_keySessionIds.value(taskId, -1);
    event.reason = reason;
    BusinessLogService::write(event, BusinessLogSeverity::Error);

    m_inFlightTaskIds.remove(taskId);
    m_keySessionIds.remove(taskId);
    m_ticketStore->updateAdminDeleteStage(
            taskId, SystemTicketDto::AdminDeleteStage::TerminationFailed);
    m_ticketStore->updateAdminDeleteError(taskId, reason);
    emit statusMessage(
            QStringLiteral("工作台通知失败，请手动关闭任务 ID: %1（错误：%2）")
                    .arg(taskId, reason));
}
